// SPDX-License-Identifier: GPL-2.0
/*
 * Axiado MCTP over Shared Memory + Mailbox Doorbell Transport Driver
 *
 * This driver implements the MCTP transport for the Axiado AX3005 SoC,
 * carrying MCTP packets over shared memory (MEMREMAP_WC) with mailbox
 * messages used purely as doorbells and ACKs.
 *
 * Wire format note: the iROT message fields below (command, counter, size,
 * address words) are defined little-endian on the wire. Both the Processor
 * and the Co-Processor run little-endian on AX3005, so the driver accesses
 * them as native u32 and is little-endian only by contract.
 *
 */

#include <linux/errno.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <net/mctp.h>
#include <net/mctpdevice.h>
#include <uapi/linux/if_arp.h>

/**
 * enum irot_command_code - iROT IPC command identifiers.
 * @irot_cc_ping:      Ping request from Processor to Co-Processor.
 * @irot_cc_ping_rsp:  Ping response from Co-Processor to Processor.
 * @irot_cc_mctp:      MCTP packet notification; shmem descriptor in data.
 * @irot_cc_mctp_done: MCTP buffer release ACK; counter echoed in data.
 */
enum irot_command_code {
	irot_cc_ping            = 2,
	irot_cc_ping_rsp        = 3,
	irot_cc_mctp            = 6,
	irot_cc_mctp_done       = 7,
};

/**
 * struct irot_msg_address - 64-bit physical address split into two 32-bit words.
 * @low:  Lower 32 bits of the physical address.
 * @high: Upper 32 bits of the physical address (zero on AX3005).
 */
struct irot_msg_address {
	u32 low;
	u32 high;
};

/**
 * struct irot_msg_span - Describes a contiguous shmem buffer.
 * @address: Physical base address of the buffer.
 * @size:    Length in bytes of valid data in the buffer.
 */
struct irot_msg_span {
	struct irot_msg_address address;
	u32 size;
};

/**
 * struct irot_msg_data_mctp - MCTP-specific iROT message payload.
 * @counter: Rolling sequence number; echoed in irot_cc_mctp_done.
 * @packet:  Shmem span describing the MCTP packet location and length.
 */
struct irot_msg_data_mctp {
	u32 counter;
	struct irot_msg_span packet;
};

/**
 * union irot_msg_data - Generic iROT message data field (28 bytes).
 * @args:  Raw 32-bit argument array (7 words).
 * @value: Single 32-bit value (used for ping and mctp_done counter).
 * @mctp:  Structured MCTP shmem descriptor.
 */
union irot_msg_data {
	u32 args[7];
	u32 value;
	struct irot_msg_data_mctp mctp;
};

/**
 * struct irot_message - 32-byte iROT IPC message body.
 * @command: One of enum irot_command_code.
 * @data:    Command-specific payload.
 *
 * This struct is transmitted inside the mailbox FIFO after the 16-byte
 * firmware header.  It must remain exactly 32 bytes.
 */
struct irot_message {
	u32 command;
	union irot_msg_data data;
} __packed;

static_assert(sizeof(struct irot_message) == 32,
	      "irot_message must be exactly 32 bytes");

/* Zero-initialiser for struct irot_message. */
#define IROT_MESSAGE_INIT       { .command = 0, .data = { .args = { 0 } } }

/* Mailbox message format:
 * 16-byte firmware header + 32-byte irot_message = 48 bytes total
 */

#define AXIADO_MCTP_FW_HDR_SIZE         16

/* Total mailbox message size sent over the FIFO (48 bytes). */
#define AXIADO_MCTP_MBOX_MSG_SIZE       (AXIADO_MCTP_FW_HDR_SIZE + \
					 sizeof(struct irot_message))

/**
 * struct axiado_mctp_mbox_msg - Full 48-byte mailbox FIFO message.
 * @len:      Total message length in bytes (always 48).
 * @type:     Message type; always 0.
 * @priority: Message priority; always 0.
 * @flag:     Message flag; always 0.
 * @irot:     32-byte iROT command payload following the header.
 */
struct axiado_mctp_mbox_msg {
	__le32 len;
	__le32 type;
	__le32 priority;
	__le32 flag;
	struct irot_message irot;
} __packed;

static_assert(sizeof(struct axiado_mctp_mbox_msg) == 48,
	      "axiado_mctp_mbox_msg must be exactly 48 bytes");

/* Driver definitions */

/* Maximum depth of the software TX queue (SKBs). */
#define AXIADO_MCTP_QUEUE_SIZE  64

/* Minimum MCTP MTU per DMTF DSP0236 Section 8.3. */
#define AXIADO_MCTP_MIN_MTU     68

struct axiado_mctp_drv;

/**
 * struct axiado_mctp_netdev_priv - Per-netdev private data.
 * @drv: Back-pointer to the driver state.
 */
struct axiado_mctp_netdev_priv {
	struct axiado_mctp_drv *drv;
};

/**
 * struct axiado_mctp_drv - Driver instance state.
 * @dev:             Platform device pointer.
 * @ndev:            MCTP net device.
 * @mbox_client:     Mailbox client descriptor shared by TX and RX channels.
 * @tx_chan:         Mailbox TX channel (Processor to Co-Processor).
 * @rx_chan:         Mailbox RX channel (Co-Processor to Processor, interrupt-driven).
 * @mailbox_ready:   True once both mailbox channels have been acquired.
 * @tx_shmem:        WC-mapped TX shmem window (Processor writes, Co-Processor reads).
 * @rx_shmem:        WC-mapped RX shmem window (Co-Processor writes, Processor reads).
 * @tx_shmem_base:   Physical base address of TX shmem (from DTS).
 * @rx_shmem_base:   Physical base address of RX shmem (from DTS).
 * @tx_shmem_size:   Size of TX shmem window in bytes (from DTS).
 * @rx_shmem_size:   Size of RX shmem window in bytes (from DTS).
 * @mtu:             Maximum MCTP payload size = tx_shmem_size.
 * @tx_queue:        Software queue of outbound SKBs.
 * @tx_wq:           Wait queue that wakes the TX kthread on enqueue.
 * @tx_thread:       TX kthread handle.
 * @tx_done_wq:      Wait queue that wakes the TX kthread on mctp_done ACK.
 * @pending_write:   True while a TX is awaiting its mctp_done ACK.
 * @tx_state_lock:   Protects @pending_write and @pending_tx_counter.
 * @pending_tx_counter: Counter value of the in-flight TX packet.
 * @rx_wq:           Wait queue that wakes the RX kthread on mailbox arrival.
 * @rx_thread:       RX kthread handle.
 * @rx_lock:         Protects @pending_rx, @pending_rx_msg, @pending_ping and
 *                   @pending_ping_value.
 * @pending_rx_msg:  Snapshot of the most recent unprocessed RX mailbox msg.
 * @pending_rx:      True when @pending_rx_msg holds an unprocessed message.
 * @pending_ping:    True when a ping request awaits a response from the RX
 *                   kthread (the reply must not be sent from the mailbox
 *                   callback, which may run in atomic context).
 * @pending_ping_value: Ping token to echo back in the ping response.
 */
struct axiado_mctp_drv {
	struct device *dev;
	struct net_device *ndev;

	/* Mailbox channels */
	struct mbox_client mbox_client;
	struct mbox_chan *tx_chan;
	struct mbox_chan *rx_chan;
	bool mailbox_ready;

	/* Shared memory (WC-mapped via memremap, uncached on Processor) */
	void *tx_shmem;
	void *rx_shmem;
	phys_addr_t   tx_shmem_base;
	phys_addr_t   rx_shmem_base;
	size_t        tx_shmem_size;
	size_t        rx_shmem_size;
	u32           mtu;

	/* TX path */
	struct sk_buff_head tx_queue;
	wait_queue_head_t tx_wq;
	struct task_struct *tx_thread;

	wait_queue_head_t tx_done_wq;
	bool pending_write;
	spinlock_t tx_state_lock;
	u32 pending_tx_counter;

	/* RX path */
	wait_queue_head_t rx_wq;
	struct task_struct *rx_thread;
	spinlock_t rx_lock;

	struct axiado_mctp_mbox_msg pending_rx_msg;
	bool pending_rx;

	bool pending_ping;
	u32 pending_ping_value;
};

/* Internal helpers */

static void axiado_mctp_tx_mark_done(struct axiado_mctp_drv *drv, u32 counter)
{
	unsigned long flags;

	spin_lock_irqsave(&drv->tx_state_lock, flags);
	if (drv->pending_write && drv->pending_tx_counter == counter) {
		drv->pending_write = false;
		wake_up(&drv->tx_done_wq);
	}
	spin_unlock_irqrestore(&drv->tx_state_lock, flags);
}

static int axiado_mctp_tx_mark_pending(struct axiado_mctp_drv *drv,
				       u32 counter)
{
	unsigned long flags;

	spin_lock_irqsave(&drv->tx_state_lock, flags);
	if (drv->pending_write) {
		spin_unlock_irqrestore(&drv->tx_state_lock, flags);
		return -EBUSY;
	}
	drv->pending_tx_counter = counter;
	drv->pending_write = true;
	spin_unlock_irqrestore(&drv->tx_state_lock, flags);
	return 0;
}

static bool axiado_mctp_tx_idle(struct axiado_mctp_drv *drv)
{
	unsigned long flags;
	bool idle;

	spin_lock_irqsave(&drv->tx_state_lock, flags);
	idle = !drv->pending_write;
	spin_unlock_irqrestore(&drv->tx_state_lock, flags);
	return idle;
}

static void axiado_build_mbox_msg(struct axiado_mctp_mbox_msg *mbox_msg,
				  const struct irot_message *irot)
{
	memset(mbox_msg, 0, sizeof(*mbox_msg));
	mbox_msg->len = cpu_to_le32(AXIADO_MCTP_MBOX_MSG_SIZE);
	memcpy(&mbox_msg->irot, irot, sizeof(*irot));
}

/* Net device ops */

static int axiado_mctp_open(struct net_device *ndev)
{
	netif_start_queue(ndev);
	return 0;
}

static int axiado_mctp_stop(struct net_device *ndev)
{
	netif_stop_queue(ndev);
	return 0;
}

static netdev_tx_t axiado_mctp_start_xmit(struct sk_buff *skb,
					  struct net_device *ndev)
{
	struct axiado_mctp_netdev_priv *priv = netdev_priv(ndev);
	struct axiado_mctp_drv *drv = priv->drv;
	netdev_tx_t status = NETDEV_TX_BUSY;
	unsigned long flags;

	if (!drv) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	spin_lock_irqsave(&drv->tx_queue.lock, flags);
	if (skb_queue_len(&drv->tx_queue) >= AXIADO_MCTP_QUEUE_SIZE) {
		netif_stop_queue(ndev);
		status = NETDEV_TX_BUSY;
	} else {
		__skb_queue_tail(&drv->tx_queue, skb);
		if (skb_queue_len(&drv->tx_queue) == AXIADO_MCTP_QUEUE_SIZE)
			netif_stop_queue(ndev);
		status = NETDEV_TX_OK;
	}
	spin_unlock_irqrestore(&drv->tx_queue.lock, flags);

	if (status == NETDEV_TX_OK)
		wake_up(&drv->tx_wq);

	return status;
}

static const struct net_device_ops axiado_mctp_netdev_ops = {
	.ndo_open       = axiado_mctp_open,
	.ndo_stop       = axiado_mctp_stop,
	.ndo_start_xmit = axiado_mctp_start_xmit,
};

static void axiado_mctp_netdev_setup(struct net_device *ndev)
{
	ndev->type            = ARPHRD_MCTP;
	ndev->min_mtu         = AXIADO_MCTP_MIN_MTU;
	ndev->max_mtu         = AXIADO_MCTP_MIN_MTU; /* updated in probe */
	ndev->mtu             = AXIADO_MCTP_MIN_MTU; /* updated in probe */
	ndev->hard_header_len = 0;
	ndev->addr_len        = 0;
	ndev->flags           = IFF_NOARP;
	ndev->tx_queue_len    = AXIADO_MCTP_QUEUE_SIZE;
	ndev->netdev_ops      = &axiado_mctp_netdev_ops;
}

/* Mailbox RX callback (threaded IRQ, process context) */

static void axiado_mctp_rx_callback(struct mbox_client *cl, void *data)
{
	struct axiado_mctp_drv *drv;
	struct axiado_mctp_mbox_msg *mbox_msg;
	struct irot_message *irot;
	unsigned long flags;

	if (!cl || !data)
		return;

	drv = container_of(cl, struct axiado_mctp_drv, mbox_client);
	mbox_msg = (struct axiado_mctp_mbox_msg *)data;
	irot = &mbox_msg->irot;

	switch (irot->command) {
	case irot_cc_mctp:
		spin_lock_irqsave(&drv->rx_lock, flags);
		if (!drv->pending_rx) {
			memcpy(&drv->pending_rx_msg, mbox_msg,
			       sizeof(*mbox_msg));
			drv->pending_rx = true;
			wake_up(&drv->rx_wq);
		} else {
			if (drv->ndev)
				drv->ndev->stats.rx_dropped++;
		}
		spin_unlock_irqrestore(&drv->rx_lock, flags);
		break;

	case irot_cc_mctp_done:
		axiado_mctp_tx_mark_done(drv, irot->data.value);
		break;

	case irot_cc_ping:
		/*
		 * Do not reply from here: this callback may run in atomic
		 * context and mbox_send_message() can sleep (tx_block). Defer
		 * the response to the RX kthread.
		 */
		spin_lock_irqsave(&drv->rx_lock, flags);
		drv->pending_ping = true;
		drv->pending_ping_value = irot->data.value;
		wake_up(&drv->rx_wq);
		spin_unlock_irqrestore(&drv->rx_lock, flags);
		break;

	default:
		dev_warn_ratelimited(drv->dev,
				     "unknown iROT command %u\n",
				     irot->command);
		break;
	}
}

/* RX kthread */

static int axiado_mctp_rx_thread(void *arg)
{
	struct axiado_mctp_drv *drv = arg;

	while (!kthread_should_stop()) {
		struct axiado_mctp_mbox_msg mbox_msg;
		struct irot_message *irot;
		struct sk_buff *skb;
		struct mctp_skb_cb *cb;
		struct irot_message done_irot = IROT_MESSAGE_INIT;
		struct axiado_mctp_mbox_msg done_msg;
		unsigned long flags;
		bool do_ping;
		u32 ping_value;
		bool have_rx;
		u64 paddr;
		u32 psize;
		int rx_ret;

		wait_event_killable(drv->rx_wq,
				    kthread_should_stop() ||
				    drv->pending_rx || drv->pending_ping);

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&drv->rx_lock, flags);
		do_ping = drv->pending_ping;
		ping_value = drv->pending_ping_value;
		drv->pending_ping = false;
		have_rx = drv->pending_rx;
		if (have_rx) {
			memcpy(&mbox_msg, &drv->pending_rx_msg,
			       sizeof(mbox_msg));
			drv->pending_rx = false;
		}
		spin_unlock_irqrestore(&drv->rx_lock, flags);

		/* Deferred ping response (safe to send from process context) */
		if (do_ping) {
			struct irot_message rsp = IROT_MESSAGE_INIT;
			struct axiado_mctp_mbox_msg rsp_msg;

			rsp.command = irot_cc_ping_rsp;
			rsp.data.value = ping_value;
			axiado_build_mbox_msg(&rsp_msg, &rsp);
			mbox_send_message(drv->tx_chan, &rsp_msg);
		}

		if (!have_rx)
			continue;

		irot = &mbox_msg.irot;
		paddr = ((u64)irot->data.mctp.packet.address.high << 32) |
			irot->data.mctp.packet.address.low;
		psize = irot->data.mctp.packet.size;

		/*
		 * Validate the size first, then the base, then the extent.
		 * address.high is always 0 on AX3005, so paddr < 2^32 and the
		 * paddr + psize sum cannot overflow u64.
		 */
		if (psize == 0 || psize > drv->mtu ||
		    paddr < drv->rx_shmem_base ||
		    paddr + psize > drv->rx_shmem_base + drv->rx_shmem_size) {
			dev_warn_ratelimited(drv->dev,
					     "invalid RX span addr=0x%llx size=%u\n",
					     paddr, psize);
			drv->ndev->stats.rx_dropped++;
			goto send_done;
		}

		skb = netdev_alloc_skb(drv->ndev, psize);
		if (!skb) {
			drv->ndev->stats.rx_dropped++;
			goto send_done;
		}

		memcpy(skb_put(skb, psize),
		       drv->rx_shmem + (paddr - drv->rx_shmem_base),
		       psize);

		skb->dev = drv->ndev;
		skb->protocol = htons(ETH_P_MCTP);
		skb_reset_network_header(skb);

		cb = __mctp_cb(skb);
		cb->halen = 0;

		rx_ret = netif_receive_skb(skb);
		if (rx_ret == NET_RX_SUCCESS) {
			drv->ndev->stats.rx_packets++;
			drv->ndev->stats.rx_bytes += psize;
		} else {
			drv->ndev->stats.rx_dropped++;
		}

send_done:
		done_irot.command = irot_cc_mctp_done;
		done_irot.data.value = irot->data.mctp.counter;
		axiado_build_mbox_msg(&done_msg, &done_irot);
		mbox_send_message(drv->tx_chan, &done_msg);
	}

	return 0;
}

/* TX kthread */

static int axiado_mctp_tx_thread(void *arg)
{
	struct axiado_mctp_drv *drv = arg;
	u32 next_tx_counter = 1;

	while (!kthread_should_stop()) {
		struct sk_buff *skb;
		struct irot_message irot = IROT_MESSAGE_INIT;
		struct axiado_mctp_mbox_msg mbox_msg;
		u32 len;
		u32 tx_counter;
		int rc;

		wait_event_killable(drv->tx_wq,
				    kthread_should_stop() ||
				    !skb_queue_empty(&drv->tx_queue));

		if (kthread_should_stop())
			break;

		skb = skb_dequeue(&drv->tx_queue);
		if (!skb)
			continue;

		netif_wake_queue(drv->ndev);

		len = skb->len;

		if (len == 0 || len > drv->mtu) {
			dev_warn_ratelimited(drv->dev,
					     "TX invalid len=%u, dropping\n",
					     len);
			drv->ndev->stats.tx_dropped++;
			dev_kfree_skb_any(skb);
			continue;
		}

		/*
		 * Wait for previous TX ACK before writing shmem.
		 * Intentionally unbounded: irot_cc_mctp_done is guaranteed
		 * by the Co-Processor firmware protocol. A timeout would be
		 * worse — a premature expiry force-clears pending_write, the
		 * next packet overwrites TX shmem, and Co-Processor receives
		 * corrupted data with a counter mismatch.
		 */
		wait_event_killable(drv->tx_done_wq,
				    kthread_should_stop() ||
				    axiado_mctp_tx_idle(drv));

		if (kthread_should_stop()) {
			dev_kfree_skb_any(skb);
			break;
		}

		if (!drv->mailbox_ready || !drv->tx_chan || !drv->rx_chan) {
			dev_err(drv->dev, "mailbox not ready, dropping TX\n");
			drv->ndev->stats.tx_dropped++;
			dev_kfree_skb_any(skb);
			continue;
		}

		memcpy(drv->tx_shmem, skb->data, len);

		tx_counter = next_tx_counter++;
		irot.command = irot_cc_mctp;
		irot.data.mctp.counter = tx_counter;
		irot.data.mctp.packet.address.low  = (u32)drv->tx_shmem_base;
		irot.data.mctp.packet.address.high =
			(u32)(drv->tx_shmem_base >> 32);
		irot.data.mctp.packet.size = len;

		axiado_build_mbox_msg(&mbox_msg, &irot);

		rc = axiado_mctp_tx_mark_pending(drv, tx_counter);
		if (rc) {
			drv->ndev->stats.tx_dropped++;
			dev_kfree_skb_any(skb);
			continue;
		}

		rc = mbox_send_message(drv->tx_chan, &mbox_msg);
		if (rc < 0) {
			dev_err(drv->dev,
				"mbox_send_message failed rc=%d\n", rc);
			drv->ndev->stats.tx_dropped++;
			axiado_mctp_tx_mark_done(drv, tx_counter);
			dev_kfree_skb_any(skb);
			continue;
		}

		drv->ndev->stats.tx_packets++;
		drv->ndev->stats.tx_bytes += len;

		dev_kfree_skb_any(skb);
	}

	return 0;
}

/* Mailbox initialisation */

static int axiado_mctp_request_mailbox(struct axiado_mctp_drv *drv)
{
	drv->mbox_client.dev = drv->dev;
	drv->mbox_client.rx_callback = axiado_mctp_rx_callback;
	drv->mbox_client.tx_block = true;
	drv->mbox_client.knows_txdone = false;
	drv->mbox_client.tx_tout = 1000;

	drv->tx_chan = mbox_request_channel_byname(&drv->mbox_client, "tx");
	if (IS_ERR(drv->tx_chan))
		return dev_err_probe(drv->dev, PTR_ERR(drv->tx_chan),
				     "failed to request TX mailbox channel\n");

	drv->rx_chan = mbox_request_channel_byname(&drv->mbox_client, "rx");
	if (IS_ERR(drv->rx_chan)) {
		mbox_free_channel(drv->tx_chan);
		return dev_err_probe(drv->dev, PTR_ERR(drv->rx_chan),
				     "failed to request RX mailbox channel\n");
	}

	drv->mailbox_ready = true;
	return 0;
}

/* Shared memory initialisation — reads regions from DTS memory-region
 * phandles rather than hardcoding physical addresses.
 *
 * DTS example:
 *   mctp-mailbox {
 *       compatible = "axiado,ax3005-mctp-mailbox";
 *       memory-region = <&shm_ap_to_rot>, <&shm_rot_to_ap>;
 *       mbox-names = "tx", "rx";
 *       mboxes = <&mailbox 0>, <&mailbox 8>;
 *   };
 */
static int axiado_mctp_init_shmem(struct axiado_mctp_drv *drv)
{
	struct device_node *np = drv->dev->of_node;
	struct device_node *shmem_np;
	struct resource res;
	int rc;

	/* TX shmem: memory-region index 0 (Processor writes, Co-Processor reads) */
	shmem_np = of_parse_phandle(np, "memory-region", 0);
	if (!shmem_np)
		return dev_err_probe(drv->dev, -ENODEV,
				     "no TX memory-region in DTS\n");
	rc = of_address_to_resource(shmem_np, 0, &res);
	of_node_put(shmem_np);
	if (rc)
		return dev_err_probe(drv->dev, rc,
				     "failed to get TX shmem resource\n");
	drv->tx_shmem_base = res.start;
	drv->tx_shmem_size = resource_size(&res);

	/* RX shmem: memory-region index 1 (Co-Processor writes, Processor reads) */
	shmem_np = of_parse_phandle(np, "memory-region", 1);
	if (!shmem_np)
		return dev_err_probe(drv->dev, -ENODEV,
				     "no RX memory-region in DTS\n");
	rc = of_address_to_resource(shmem_np, 0, &res);
	of_node_put(shmem_np);
	if (rc)
		return dev_err_probe(drv->dev, rc,
				     "failed to get RX shmem resource\n");
	drv->rx_shmem_base = res.start;
	drv->rx_shmem_size = resource_size(&res);

	/* MTU = TX shmem window size */
	drv->mtu = (u32)drv->tx_shmem_size;

	/* Map TX shmem as write-combining */
	drv->tx_shmem = memremap(drv->tx_shmem_base,
				 drv->tx_shmem_size, MEMREMAP_WC);
	if (!drv->tx_shmem)
		return dev_err_probe(drv->dev, -ENOMEM,
				     "failed to map TX shmem\n");

	/* Map RX shmem as write-combining */
	drv->rx_shmem = memremap(drv->rx_shmem_base,
				 drv->rx_shmem_size, MEMREMAP_WC);
	if (!drv->rx_shmem) {
		memunmap(drv->tx_shmem);
		drv->tx_shmem = NULL;
		return dev_err_probe(drv->dev, -ENOMEM,
				     "failed to map RX shmem\n");
	}

	return 0;
}

/* Platform driver probe / remove */

static int axiado_mctp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct net_device *ndev;
	struct axiado_mctp_netdev_priv *priv;
	struct axiado_mctp_drv *drv;
	int rc;

	drv = devm_kzalloc(dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->dev = dev;

	skb_queue_head_init(&drv->tx_queue);
	init_waitqueue_head(&drv->tx_wq);
	init_waitqueue_head(&drv->tx_done_wq);
	init_waitqueue_head(&drv->rx_wq);
	spin_lock_init(&drv->tx_state_lock);
	spin_lock_init(&drv->rx_lock);

	rc = axiado_mctp_init_shmem(drv);
	if (rc)
		return rc;

	ndev = alloc_netdev(sizeof(*priv), "mctpirot%d",
			    NET_NAME_ENUM, axiado_mctp_netdev_setup);
	if (!ndev) {
		rc = -ENOMEM;
		goto err_shmem;
	}

	/* Set MTU from DTS shmem size */
	ndev->max_mtu = drv->mtu;
	ndev->mtu     = drv->mtu;

	SET_NETDEV_DEV(ndev, dev);
	drv->ndev = ndev;

	priv = netdev_priv(ndev);
	priv->drv = drv;

	platform_set_drvdata(pdev, drv);

	rc = mctp_register_netdev(ndev, NULL, MCTP_PHYS_BINDING_VENDOR);
	if (rc) {
		dev_err(dev, "mctp_register_netdev failed: %d\n", rc);
		free_netdev(ndev);
		goto err_shmem;
	}

	/*
	 * Acquire the mailbox channels before starting the kthreads. The RX
	 * callback may then stash a message and wake the RX kthread; that is
	 * harmless until the kthread is running.
	 */
	rc = axiado_mctp_request_mailbox(drv);
	if (rc)
		goto err_netdev;

	drv->tx_thread = kthread_run(axiado_mctp_tx_thread, drv,
				     "axiado_mctp_tx");
	if (IS_ERR(drv->tx_thread)) {
		rc = PTR_ERR(drv->tx_thread);
		drv->tx_thread = NULL;
		dev_err(dev, "failed to start TX kthread: %d\n", rc);
		goto err_mailbox;
	}

	drv->rx_thread = kthread_run(axiado_mctp_rx_thread, drv,
				     "axiado_mctp_rx");
	if (IS_ERR(drv->rx_thread)) {
		rc = PTR_ERR(drv->rx_thread);
		drv->rx_thread = NULL;
		dev_err(dev, "failed to start RX kthread: %d\n", rc);
		kthread_stop(drv->tx_thread);
		drv->tx_thread = NULL;
		goto err_mailbox;
	}

	netif_carrier_on(ndev);

	dev_info(dev, "MCTP mailbox transport ready tx_base=0x%llx rx_base=0x%llx mtu=%u\n",
		 (u64)drv->tx_shmem_base,
		 (u64)drv->rx_shmem_base,
		 drv->mtu);

	return 0;

err_mailbox:
	drv->mailbox_ready = false;
	mbox_free_channel(drv->rx_chan);
	mbox_free_channel(drv->tx_chan);
err_netdev:
	mctp_unregister_netdev(ndev);
	free_netdev(ndev);
err_shmem:
	memunmap(drv->tx_shmem);
	memunmap(drv->rx_shmem);
	return rc;
}

static void axiado_mctp_remove(struct platform_device *pdev)
{
	struct axiado_mctp_drv *drv = platform_get_drvdata(pdev);

	if (!drv)
		return;

	drv->mailbox_ready = false;

	/*
	 * Step 1: unregister the netdev first. This calls ndo_stop and
	 * quiesces the TX path so the stack can no longer queue new SKBs
	 * while the rest of the resources are torn down. The net_device is
	 * kept alive (freed at step 6) because the kthreads still touch its
	 * stats.
	 */
	if (drv->ndev)
		mctp_unregister_netdev(drv->ndev);

	/* Step 2: wake and stop the kthreads before freeing the channels. */
	wake_up(&drv->rx_wq);
	wake_up(&drv->tx_wq);
	wake_up(&drv->tx_done_wq);

	if (drv->rx_thread) {
		kthread_stop(drv->rx_thread);
		drv->rx_thread = NULL;
	}
	if (drv->tx_thread) {
		kthread_stop(drv->tx_thread);
		drv->tx_thread = NULL;
	}

	/* Step 3: free the mailbox channels (no more senders remain). */
	if (drv->rx_chan) {
		mbox_free_channel(drv->rx_chan);
		drv->rx_chan = NULL;
	}
	if (drv->tx_chan) {
		mbox_free_channel(drv->tx_chan);
		drv->tx_chan = NULL;
	}

	/* Step 4: drain any SKBs still queued. */
	skb_queue_purge(&drv->tx_queue);

	/* Step 5: unmap the shared memory. */
	if (drv->tx_shmem) {
		memunmap(drv->tx_shmem);
		drv->tx_shmem = NULL;
	}
	if (drv->rx_shmem) {
		memunmap(drv->rx_shmem);
		drv->rx_shmem = NULL;
	}

	/* Step 6: free the net_device. */
	if (drv->ndev) {
		free_netdev(drv->ndev);
		drv->ndev = NULL;
	}
}

static const struct of_device_id axiado_mctp_of_match[] = {
	{ .compatible = "axiado,ax3005-mctp-mailbox" },
	{ }
};
MODULE_DEVICE_TABLE(of, axiado_mctp_of_match);

static struct platform_driver axiado_mctp_driver = {
	.probe      = axiado_mctp_probe,
	.remove = axiado_mctp_remove,
	.driver = {
		.name           = "axiado-mctp-mailbox",
		.of_match_table = axiado_mctp_of_match,
	},
};

module_platform_driver(axiado_mctp_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Axiado AX3005 MCTP mailbox and shared memory transport driver");
MODULE_AUTHOR("Axiado Corporation");
