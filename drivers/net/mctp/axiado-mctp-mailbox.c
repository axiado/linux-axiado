// SPDX-License-Identifier: GPL-2.0
/*
 * axiado-mctp-mailbox.c - Axiado MCTP over Mailbox Transport Driver
 *
 * This driver implements MCTP transport over the Axiado mailbox
 * hardware. It creates a network device (mctpmbox%d) that the Linux MCTP
 * stack uses to communicate with the target subsystem over mailbox.
 */

#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <net/mctp.h>
#include <net/mctpdevice.h>
#include <uapi/linux/if_arp.h>

/* Maximum number of packets in the TX software queue */
#define AXIADO_MCTP_QUEUE_SIZE        64

/* Mailbox FIFO packet size (hardware fixed at 252 bytes) */
#define AXIADO_MCTP_MBOX_PKT_SIZE     252

/* 16-byte firmware header prepended to every MCTP packet */
#define AXIADO_MCTP_FW_HDR_SIZE       16

/* Maximum MCTP payload = mailbox size minus firmware header */
#define AXIADO_MCTP_MAX_PAYLOAD       (AXIADO_MCTP_MBOX_PKT_SIZE - \
				       AXIADO_MCTP_FW_HDR_SIZE)

/* Minimum MCTP MTU as per MCTP spec (DSP0236) */
#define AXIADO_MCTP_MIN_MTU           68

/**
 * struct axiado_mctp_mailbox_pkt - Mailbox packet format (ax_ipc_mb_msg_t)
 * @len:      Total packet length = AXIADO_MCTP_FW_HDR_SIZE + mctp_payload_len
 * @type:     Always 0
 * @priority: Always 0
 * @flag:     Always 0
 * @payload:  MCTP packet bytes (without the 16-byte header)
 */
struct axiado_mctp_mailbox_pkt {
	__le32 len;
	__le32 type;
	__le32 priority;
	__le32 flag;
	u8     payload[AXIADO_MCTP_MAX_PAYLOAD];
};

/**
 * struct axiado_mctp_rx_entry - Queued RX packet entry
 * @list: Linked list node
 * @pkt:  Copy of the mailbox packet received from co-processor
 */
struct axiado_mctp_rx_entry {
	struct list_head list;
	struct axiado_mctp_mailbox_pkt pkt;
};

struct axiado_mctp_drv;

struct axiado_mctp_netdev_priv {
	struct axiado_mctp_drv *drv;
};

/**
 * struct axiado_mctp_drv - Driver private data
 * @dev:                Platform device
 * @ndev:               MCTP network device (mctpmbox0)
 * @mbox_client:        Mailbox client handle
 * @tx_chan:            TX mailbox channel (ch0: app processor to co-processor)
 * @rx_chan:            RX mailbox channel (ch8: co-processor to app processor)
 * @mailbox_ready:      True when both mailbox channels are acquired
 * @tx_queue:           TX software queue (sk_buff)
 * @tx_wq:              TX worker wait queue
 * @tx_thread:          TX worker thread
 * @rx_wq:              RX worker wait queue
 * @rx_thread:          RX worker thread
 * @rx_lock:            Spinlock protecting rx_queue
 * @rx_queue:           RX packet queue (filled by mailbox callback)
 * @rx_queue_len:       Current depth of rx_queue
 */
struct axiado_mctp_drv {
	struct device *dev;
	struct net_device *ndev;

	struct mbox_client mbox_client;
	struct mbox_chan *tx_chan;
	struct mbox_chan *rx_chan;
	bool mailbox_ready;

	struct sk_buff_head tx_queue;
	wait_queue_head_t tx_wq;
	struct task_struct *tx_thread;

	wait_queue_head_t rx_wq;
	struct task_struct *rx_thread;

	spinlock_t rx_lock; /* protects rx_queue and rx_queue_len */
	struct list_head rx_queue;
	int rx_queue_len;
};

/**
 * axiado_mctp_open - Open the MCTP network device
 * @ndev: Network device
 *
 * Return: Always 0
 */
static int axiado_mctp_open(struct net_device *ndev)
{
	netif_start_queue(ndev);
	return 0;
}

/**
 * axiado_mctp_stop - Stop the MCTP network device
 * @ndev: Network device
 *
 * Return: Always 0
 */
static int axiado_mctp_stop(struct net_device *ndev)
{
	netif_stop_queue(ndev);
	return 0;
}

/**
 * axiado_mctp_start_xmit - Enqueue outgoing MCTP packet
 * @skb:  Socket buffer containing the MCTP packet
 * @ndev: Network device
 *
 * Called by the MCTP stack when it wants to send a packet.
 * The packet is queued and the TX worker thread is woken.
 *
 * Return: NETDEV_TX_OK on success, NETDEV_TX_BUSY if queue full
 */
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
	ndev->type = ARPHRD_MCTP;
	ndev->min_mtu = AXIADO_MCTP_MIN_MTU;
	ndev->max_mtu = AXIADO_MCTP_MAX_PAYLOAD;
	ndev->mtu = AXIADO_MCTP_MAX_PAYLOAD;
	ndev->hard_header_len = 0;
	ndev->addr_len = 0;
	ndev->flags = IFF_NOARP;
	ndev->tx_queue_len = AXIADO_MCTP_QUEUE_SIZE;
	ndev->netdev_ops = &axiado_mctp_netdev_ops;
}

/**
 * axiado_mctp_rx_callback - Mailbox RX callback, called by mailbox controller on RX interrupt
 * @cl:   Mailbox client handle
 * @data: Pointer to received mailbox data buffer
 *
 * Runs in atomic context. Copies the received packet into a dynamically
 * allocated entry and queues it for the RX worker thread.
 */
static void axiado_mctp_rx_callback(struct mbox_client *cl, void *data)
{
	struct axiado_mctp_drv *drv;
	struct axiado_mctp_rx_entry *entry;
	unsigned long flags;

	if (!cl || !data)
		return;

	drv = container_of(cl, struct axiado_mctp_drv, mbox_client);

	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry) {
		drv->ndev->stats.rx_dropped++;
		return;
	}

	memcpy(&entry->pkt, data, sizeof(entry->pkt));

	spin_lock_irqsave(&drv->rx_lock, flags);
	list_add_tail(&entry->list, &drv->rx_queue);
	drv->rx_queue_len++;
	spin_unlock_irqrestore(&drv->rx_lock, flags);

	wake_up(&drv->rx_wq);
}

/**
 * axiado_mctp_rx_thread - RX worker thread
 * @arg: Pointer to axiado_mctp_drv
 *
 * Dequeues received mailbox packets, strips the 16-byte firmware header,
 * and delivers the raw MCTP payload to the Linux MCTP stack via
 * netif_receive_skb().
 *
 * Return: Always 0
 */
static int axiado_mctp_rx_thread(void *arg)
{
	struct axiado_mctp_drv *drv = arg;

	while (!kthread_should_stop()) {
		struct axiado_mctp_rx_entry *entry;
		struct sk_buff *skb;
		struct mctp_skb_cb *cb;
		unsigned long flags;
		u32 fw_len;
		u16 len;

		wait_event_killable(drv->rx_wq,
					kthread_should_stop() ||
					!list_empty(&drv->rx_queue));

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&drv->rx_lock, flags);
		if (list_empty(&drv->rx_queue)) {
			spin_unlock_irqrestore(&drv->rx_lock, flags);
			continue;
		}
		entry = list_first_entry(&drv->rx_queue,
					 struct axiado_mctp_rx_entry, list);
		list_del(&entry->list);
		drv->rx_queue_len--;
		spin_unlock_irqrestore(&drv->rx_lock, flags);

		fw_len = le32_to_cpu(entry->pkt.len);

		/* Validate firmware header length */
		if (fw_len < AXIADO_MCTP_FW_HDR_SIZE ||
		    fw_len > AXIADO_MCTP_MBOX_PKT_SIZE) {
			drv->ndev->stats.rx_dropped++;
			kfree(entry);
			continue;
		}

		/* MCTP payload = total length minus 16-byte firmware header */
		len = (u16)(fw_len - AXIADO_MCTP_FW_HDR_SIZE);

		if (len == 0 || len > AXIADO_MCTP_MAX_PAYLOAD) {
			drv->ndev->stats.rx_dropped++;
			kfree(entry);
			continue;
		}

		skb = netdev_alloc_skb(drv->ndev, len);
		if (!skb) {
			drv->ndev->stats.rx_dropped++;
			kfree(entry);
			continue;
		}

		memcpy(skb_put(skb, len), entry->pkt.payload, len);

		skb->dev = drv->ndev;
		skb->protocol = htons(ETH_P_MCTP);
		skb_reset_network_header(skb);

		cb = __mctp_cb(skb);
		cb->halen = 0;

		if (netif_receive_skb(skb) == NET_RX_SUCCESS) {
			drv->ndev->stats.rx_packets++;
			drv->ndev->stats.rx_bytes += len;
		} else {
			drv->ndev->stats.rx_dropped++;
		}

		kfree(entry);
	}

	return 0;
}

/**
 * axiado_mctp_tx_thread - TX worker thread
 * @arg: Pointer to axiado_mctp_drv
 *
 * Dequeues outgoing MCTP packets, prepends the 16-byte firmware header,
 * and sends them over the mailbox FIFO to co-processor.
 *
 * Note: pkt.len = AXIADO_MCTP_FW_HDR_SIZE + actual_mctp_len.
 * co-processor reads exactly (pkt.len / 4) words from the FIFO. Sending a
 * fixed len=256 caused FIFO underrun errors on co-processor for short packets.
 *
 * Return: Always 0
 */
static int axiado_mctp_tx_thread(void *arg)
{
	struct axiado_mctp_drv *drv = arg;

	while (!kthread_should_stop()) {
		struct sk_buff *skb;
		struct axiado_mctp_mailbox_pkt pkt;
		u16 len;
		u32 total_len;
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

		if (len == 0 || len > AXIADO_MCTP_MAX_PAYLOAD) {
			drv->ndev->stats.tx_dropped++;
			dev_kfree_skb_any(skb);
			continue;
		}

		if (!drv->mailbox_ready || !drv->tx_chan) {
			drv->ndev->stats.tx_dropped++;
			dev_kfree_skb_any(skb);
			continue;
		}

		total_len = AXIADO_MCTP_FW_HDR_SIZE + len;

		memset(&pkt, 0, sizeof(pkt));
		pkt.len = cpu_to_le32(total_len);
		memcpy(pkt.payload, skb->data, len);

		rc = mbox_send_message(drv->tx_chan, &pkt);
		if (rc < 0) {
			drv->ndev->stats.tx_dropped++;
			dev_kfree_skb_any(skb);
			continue;
		}

		drv->ndev->stats.tx_packets++;
		drv->ndev->stats.tx_bytes += len;

		dev_kfree_skb_any(skb);
	}

	return 0;
}

/**
 * axiado_mctp_request_mailbox - Request TX and RX mailbox channels
 * @drv: Driver private data
 *
 * Requests ch0 (TX: app-processor to co-processor) and
 * ch8 (RX: co-processor to app-processor)
 * from the ax3005-mailbox controller.
 *
 * Return: 0 on success, negative errno on failure
 */
static int axiado_mctp_request_mailbox(struct axiado_mctp_drv *drv)
{
	drv->mbox_client.dev = drv->dev;
	drv->mbox_client.rx_callback = axiado_mctp_rx_callback;
	drv->mbox_client.tx_block = true;
	drv->mbox_client.knows_txdone = false;
	drv->mbox_client.tx_tout = 1000;

	/* Request TX channel (ch0: app-processor to co-processor) */
	drv->tx_chan = mbox_request_channel_byname(&drv->mbox_client, "tx");
	if (IS_ERR(drv->tx_chan)) {
		dev_err(drv->dev, "failed to request TX mailbox channel\n");
		return PTR_ERR(drv->tx_chan);
	}

	/* Request RX channel (ch8: co-processor to app-processor) */
	drv->rx_chan = mbox_request_channel_byname(&drv->mbox_client, "rx");
	if (IS_ERR(drv->rx_chan)) {
		dev_err(drv->dev, "failed to request RX mailbox channel\n");
		mbox_free_channel(drv->tx_chan);
		return PTR_ERR(drv->rx_chan);
	}

	drv->mailbox_ready = true;
	return 0;
}

/**
 * axiado_mctp_probe - Platform driver probe
 * @pdev: Platform device
 *
 * Initializes the driver, requests mailbox channels, allocates and
 * registers the MCTP network device, and starts TX/RX worker threads.
 *
 * Return: 0 on success, negative errno on failure
 */
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
	init_waitqueue_head(&drv->rx_wq);
	spin_lock_init(&drv->rx_lock);
	INIT_LIST_HEAD(&drv->rx_queue);
	drv->rx_queue_len = 0;

	rc = axiado_mctp_request_mailbox(drv);
	if (rc)
		return rc;

	/* Allocate network device; interface name will be mctpmbox<N> */
	ndev = alloc_netdev(sizeof(*priv), "mctpmbox%d",
			    NET_NAME_ENUM, axiado_mctp_netdev_setup);
	if (!ndev) {
		mbox_free_channel(drv->tx_chan);
		mbox_free_channel(drv->rx_chan);
		return -ENOMEM;
	}

	SET_NETDEV_DEV(ndev, dev);
	drv->ndev = ndev;

	priv = netdev_priv(ndev);
	priv->drv = drv;

	platform_set_drvdata(pdev, drv);

	rc = mctp_register_netdev(ndev, NULL, MCTP_PHYS_BINDING_VENDOR);
	if (rc) {
		dev_err(dev, "mctp_register_netdev failed: %d\n", rc);
		free_netdev(ndev);
		mbox_free_channel(drv->tx_chan);
		mbox_free_channel(drv->rx_chan);
		return rc;
	}

	drv->tx_thread = kthread_run(axiado_mctp_tx_thread, drv,
				     "axiado_mctp_tx");
	if (IS_ERR(drv->tx_thread)) {
		rc = PTR_ERR(drv->tx_thread);
		drv->tx_thread = NULL;
		dev_err(dev, "failed to create TX thread: %d\n", rc);
		mctp_unregister_netdev(ndev);
		free_netdev(ndev);
		mbox_free_channel(drv->tx_chan);
		mbox_free_channel(drv->rx_chan);
		return rc;
	}

	drv->rx_thread = kthread_run(axiado_mctp_rx_thread, drv,
				     "axiado_mctp_rx");
	if (IS_ERR(drv->rx_thread)) {
		rc = PTR_ERR(drv->rx_thread);
		drv->rx_thread = NULL;
		dev_err(dev, "failed to create RX thread: %d\n", rc);
		kthread_stop(drv->tx_thread);
		drv->tx_thread = NULL;
		mctp_unregister_netdev(ndev);
		free_netdev(ndev);
		mbox_free_channel(drv->tx_chan);
		mbox_free_channel(drv->rx_chan);
		return rc;
	}

	netif_carrier_on(ndev);

	dev_info(dev, "axiado MCTP mailbox transport ready (%s)\n",
		 ndev->name);
	return 0;
}

/**
 * axiado_mctp_remove - Platform driver remove
 * @pdev: Platform device
 *
 * Stops worker threads, drains queues, frees mailbox channels,
 * and unregisters the MCTP network device.
 */
static void axiado_mctp_remove(struct platform_device *pdev)
{
	struct axiado_mctp_drv *drv = platform_get_drvdata(pdev);
	struct axiado_mctp_rx_entry *entry, *tmp;
	unsigned long flags;

	if (!drv)
		return;

	drv->mailbox_ready = false;

	if (drv->rx_chan) {
		mbox_free_channel(drv->rx_chan);
		drv->rx_chan = NULL;
	}

	wake_up(&drv->tx_wq);
	wake_up(&drv->rx_wq);

	if (drv->rx_thread) {
		kthread_stop(drv->rx_thread);
		drv->rx_thread = NULL;
	}

	if (drv->tx_thread) {
		kthread_stop(drv->tx_thread);
		drv->tx_thread = NULL;
	}

	/* TX thread has exited - no further mbox_send_message can run. */
	if (drv->tx_chan) {
		mbox_free_channel(drv->tx_chan);
		drv->tx_chan = NULL;
	}

	spin_lock_irqsave(&drv->rx_lock, flags);
	list_for_each_entry_safe(entry, tmp, &drv->rx_queue, list) {
		list_del(&entry->list);
		kfree(entry);
	}
	spin_unlock_irqrestore(&drv->rx_lock, flags);

	skb_queue_purge(&drv->tx_queue);

	if (drv->ndev) {
		mctp_unregister_netdev(drv->ndev);
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
	.probe = axiado_mctp_probe,
	.remove_new = axiado_mctp_remove,
	.driver = {
		.name = "axiado-mctp-mailbox",
		.of_match_table = axiado_mctp_of_match,
	},
};

module_platform_driver(axiado_mctp_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Axiado MCTP over Mailbox Transport Driver");
MODULE_AUTHOR("AXIADO CORPORATION");
