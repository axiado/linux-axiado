// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 Axiado Corporation. All rights reserved.
 *
 * Axiado AX3005 IRoT MCTP Transport Driver
 *
 */

#include <linux/atomic.h>
#include <linux/build_bug.h>
#include <linux/cleanup.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/mailbox_client.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/wordpart.h>
#include <net/mctp.h>
#include <net/mctpdevice.h>

/* Every field on the wire is a little-endian 32-bit word. */
typedef __le32 irot_u32;

/*
 * Command codes. Even == request, odd == response. Zero is reserved as
 * "empty"/none and is never sent on the wire.
 */
enum irot_command_code {
	irot_cc_ping			= 2,
	irot_cc_ping_rsp		= 3,
	irot_cc_get_mctp_buffers	= 4,
	irot_cc_mctp_buffer		= 5,
	irot_cc_mctp			= 6,
	irot_cc_mctp_done		= 7,
};

/* 64-bit physical address split into two u32 to avoid alignment/padding. */
struct irot_msg_address {
	irot_u32 low;
	irot_u32 high;
};

struct irot_msg_span {
	struct irot_msg_address address;
	irot_u32 size;
};

struct irot_msg_data_buffers {
	irot_u32 mtu_limit;
	struct irot_msg_span host_read;	 /* BMC reads RX packets from here */
	struct irot_msg_span host_write; /* BMC writes TX packets into here */
};

struct irot_msg_data_mctp {
	irot_u32 counter;		/* echoed back in the ACK */
	struct irot_msg_span packet;	/* must lie inside the matching region */
};

union irot_msg_data {
	irot_u32 args[7];
	irot_u32 value;				/* ping echo / mctp_done counter */
	struct irot_msg_data_buffers buffers;
	struct irot_msg_data_mctp mctp;
};

struct irot_msg {
	irot_u32 command;
	union irot_msg_data data;
};

#define IROT_MSG_INIT { .command = 0, .data = { .args = { 0 } } }

/* The mailbox transfers exactly 32 bytes per message. */
static_assert(sizeof(struct irot_msg) == 32);

#define IROT_COMPATIBLE_STATIC		"axiado,ax3005-mctp-irot"
#define IROT_COMPATIBLE_NEGOTIATED	"axiado,ax3005-mctp-irot-v2"
#define IROT_DRIVER_NAME		"ax3005-mctp-irot"
#define IROT_DRIVER_DESC		"Axiado MCTP transport driver"

/*
 * Each slot carries exactly 32 bytes of irot_msg,
 * wrapped in a 16-byte controller header (48-byte FIFO frame).
 */
#define IROT_MBOX_CHAN_TX		"tx"
#define IROT_MBOX_CHAN_RX		"rx"
#define IROT_MBOX_PAYLOAD_BYTES		32
#define IROT_MBOX_TX_TOUT_MS		250

#define IROT_NETDEV_NAME		"mctpirot%d"

/*
 * IROT_MCTP_MAX_MTU bounds only the negotiated mode, where the firmware
 * reports an mtu_limit of its own and nothing else bounds the answer. In
 * DT-declared mode a window holds exactly one packet, so the MTU is the
 * smaller of the two windows.
 */
#define IROT_TX_QUEUE_SIZE		20
#define IROT_BUFFER_RETRIES		3
#define IROT_BUFFER_TIMEOUT_S		5
#define IROT_MCTP_MIN_MTU		68
#define IROT_MCTP_MAX_MTU		65536

/*
 * RX rings, both drained by the RX kthread:
 *   IROT_RX_RING_SIZE     inbound packets awaiting delivery
 *   IROT_RX_ACK_RING_SIZE counters whose payload was dropped or duplicated but
 *                         which still owe the peer a cc_mctp_done
 */
#define IROT_RX_RING_SIZE		8
#define IROT_RX_ACK_RING_SIZE		8
#define IROT_TX_ACK_TIMEOUT_MS		5000

static_assert(sizeof(struct irot_msg) <= IROT_MBOX_PAYLOAD_BYTES,
	      "one IPC message must fit in IROT_MBOX_PAYLOAD_BYTES (32)");

static_assert(IROT_TX_QUEUE_SIZE >= 1, "TX queue must hold >= 1 skb");
static_assert(IROT_MCTP_MIN_MTU >= 68, "MCTP MTU floor is 68");
static_assert(IROT_MCTP_MAX_MTU >= IROT_MCTP_MIN_MTU,
	      "MTU ceiling must not be below the floor");
static_assert(IROT_RX_RING_SIZE >= 1 && IROT_RX_ACK_RING_SIZE >= 1,
	      "RX rings must hold >= 1 entry");
static_assert(IROT_TX_ACK_TIMEOUT_MS >= IROT_MBOX_TX_TOUT_MS,
	      "ACK timeout must outlast the mailbox send timeout");
static_assert(sizeof(IROT_COMPATIBLE_STATIC) > 1,
	      "set a real compatible string");

/*
 * Mailbox FIFO frame defined by the firmware ABI: a 16-byte controller header
 * followed by the 32-byte iROT message. The controller is length-prefixed --
 * it transmits @len bytes and delivers the whole frame to the RX callback.
 */
#define IROT_MBOX_FW_HDR_SIZE	16

struct irot_mbox_frame {
	__le32 len;		/* total bytes, always 48 (0x30) */
	__le32 type;		/* 0 */
	__le32 priority;	/* 0 */
	__le32 flag;		/* 0 */
	struct irot_msg irot;
} __packed;

static_assert(sizeof(struct irot_mbox_frame) ==
	      IROT_MBOX_FW_HDR_SIZE + sizeof(struct irot_msg),
	      "irot_mbox_frame must be header + one IPC message");
static_assert(sizeof(struct irot_mbox_frame) == 48,
	      "irot_mbox_frame must be exactly 48 bytes");

/**
 * struct irot_variant - the only per-peer differences in this driver
 * @desc:              human-readable name, for the probe banner
 * @negotiate_buffers: run the cc_get_mctp_buffers handshake and take the
 *                     window addresses from the reply. When false the windows
 *                     come from the DT memory-region phandles, which are then
 *                     mandatory.
 * @peer_retransmits:  the peer resends a packet it never saw acknowledged.
 *                     This single fact drives the whole RX drop policy:
 *                       true  - withhold the ACK for anything undeliverable
 *                               and let it come back; dedup by counter so the
 *                               retransmit is not delivered twice; a full RX
 *                               ring is ordinary backpressure.
 *                       false - answer every counter even when the payload is
 *                               dropped, because nothing will come back; and
 *                               running out of room to record that obligation
 *                               is fatal to the link.
 * @first_tx_counter:  initial value of the rolling TX sequence number.
 */
struct irot_variant {
	const char *desc;
	bool negotiate_buffers;
	bool peer_retransmits;
	u32  first_tx_counter;
};

/*
 * A shared-memory window in host byte order. Wire spans are little-endian and
 * split across two words; convert once at the boundary and keep this in driver
 * state, so the validation and data paths do no endian juggling.
 */
struct irot_region {
	u64 base;
	u32 size;
};

/* One inbound packet notification awaiting the RX thread. */
struct irot_rx_entry {
	u32 counter;
	u64 base;
	u32 size;
};

struct irot_priv;

/*
 * The net_device and the driver state have independent lifetimes: the netdev is
 * freed with free_netdev(), irot_priv is devm-managed. This wrapper bridges the
 * two and lets xmit notice that the driver is going away.
 */
struct irot_netdev_priv {
	spinlock_t lock;	/* protects @p against concurrent teardown */
	struct irot_priv *p;
};

/* All driver state for one IRoT MCTP endpoint. */
struct irot_priv {
	struct device		*dev;
	struct net_device	*ndev;
	const struct irot_variant *var;

	/* mailbox */
	struct mbox_client	 cl;
	struct mbox_chan	*chan_tx;
	struct mbox_chan	*chan_rx;
	struct mutex		 tx_chan_lock;	/* serialises senders on chan_tx */
	struct irot_mbox_frame	 tx_frame;	/* owned by tx_chan_lock */

	/* shared memory in use (write-once during probe) */
	struct irot_region	 host_write;	/* AP writes, IRoT reads */
	struct irot_region	 host_read;	/* IRoT writes, AP reads */
	void			*tx_addr;	/* devm_memremap(host_write) */
	void			*rx_addr;	/* devm_memremap(host_read)  */
	u32			 mtu_limit;	/* advertised link MTU */
	u32			 read_mtu;	/* min(mtu_limit, host_read.size)  */
	u32			 write_mtu;	/* min(mtu_limit, host_write.size) */

	/* device tree */
	struct irot_region	 dt_write;	/* memory-region index 0 */
	struct irot_region	 dt_read;	/* memory-region index 1 */
	bool			 have_dt_windows;

	/* TX path */
	struct sk_buff_head	 tx_queue;
	wait_queue_head_t	 tx_wq;		/* skb queued, or stall ended */
	wait_queue_head_t	 tx_done_wq;	/* cc_mctp_done arrived */
	struct task_struct	*tx_thread;

	spinlock_t		 tx_state_lock;	/* the TX slot handshake */
	bool			 pending_write;	/* window owned by a transfer */
	bool			 tx_stalled;	/* ACK overdue, link down */
	u32			 pending_tx_counter;

	/* RX path */
	wait_queue_head_t	 rx_wq;
	struct task_struct	*rx_thread;
	spinlock_t		 rx_lock;	/* RX rings and callback state */

	struct irot_rx_entry	 rx_ring[IROT_RX_RING_SIZE];
	unsigned int		 rx_head, rx_tail, rx_count;

	u32			 rx_ack_ring[IROT_RX_ACK_RING_SIZE];
	unsigned int		 rx_ack_head, rx_ack_tail, rx_ack_count;

	bool			 rx_stalled;	/* ACK obligation lost */

	bool			 pending_ping;
	u32			 pending_ping_value;

	/* Dedup cursor: last counter handed to the stack (rx_lock) */
	u32			 prev_rx_counter;
	bool			 has_prev_rx_counter;

	/* buffer negotiation (rx_lock, probe path) */
	struct semaphore	 probe_sem;	/* count=0; up() on cc_mctp_buffer */
	struct irot_region	 nego_write;
	struct irot_region	 nego_read;
	u32			 nego_mtu_limit;
	bool			 nego_valid;

	/*
	 * Set once mctp_register_netdev() has succeeded. Until then the netdev
	 * exists but is not registered, so the carrier and queue must be left
	 * alone; the mailbox callback is already live by that point.
	 */
	bool			 netdev_live;
};

static void irot_region_from_span(struct irot_region *r,
				  const struct irot_msg_span *s)
{
	r->base = ((u64)le32_to_cpu(s->address.high) << 32) |
		  le32_to_cpu(s->address.low);
	r->size = le32_to_cpu(s->size);
}

static void irot_span_from_region(struct irot_msg_span *s,
				  const struct irot_region *r)
{
	s->address.low  = cpu_to_le32(lower_32_bits(r->base));
	s->address.high = cpu_to_le32(upper_32_bits(r->base));
	s->size         = cpu_to_le32(r->size);
}

/*
 * Bounds-check a packet against its containing window and return its offset
 * in @off. True only if the packet lies wholly inside: no underflow, no
 * overflow, no wrap. On false the packet must be dropped.
 */
static bool irot_region_offset(const struct irot_region *win, u64 base,
			       u32 size, u64 *off)
{
	if (base < win->base)
		return false;

	*off = base - win->base;

	if (*off > win->size)
		return false;

	/* Reject wrap and overflow: size beyond what is left in the window */
	if (size > win->size - *off)
		return false;

	return true;
}

/*
 * The same containment test one level up: irot_region_offset() bounds a packet
 * inside a window, this bounds a firmware-supplied window inside the window the
 * device tree reserved for it.
 */
static bool irot_region_contains(const struct irot_region *outer,
				 const struct irot_region *inner)
{
	u64 off;

	if (!inner->size)
		return false;

	return irot_region_offset(outer, inner->base, inner->size, &off);
}

/*
 * Transmit one 32-byte IPC message. The client sets tx_block, so
 * mbox_send_message() waits for the controller; the frame therefore lives in
 * driver state rather than on the caller's stack, owned by tx_chan_lock, which
 * also keeps the two kthreads from racing on the one blocking channel.
 *
 * Never call this from the RX callback: it sleeps.
 */
static int irot_send(struct irot_priv *p, const struct irot_msg *msg)
{
	guard(mutex)(&p->tx_chan_lock);

	memset(&p->tx_frame, 0, sizeof(p->tx_frame));
	p->tx_frame.len = cpu_to_le32(sizeof(p->tx_frame));
	memcpy(&p->tx_frame.irot, msg, sizeof(*msg));

	return mbox_send_message(p->chan_tx, &p->tx_frame);
}

static int irot_send_done(struct irot_priv *p, u32 counter)
{
	struct irot_msg ack = IROT_MSG_INIT;

	ack.command    = cpu_to_le32(irot_cc_mctp_done);
	ack.data.value = cpu_to_le32(counter);

	return irot_send(p, &ack);
}

static void irot_tx_mark_done(struct irot_priv *p, u32 counter)
{
	unsigned long flags;

	spin_lock_irqsave(&p->tx_state_lock, flags);

	if (!p->pending_write || p->pending_tx_counter != counter) {
		spin_unlock_irqrestore(&p->tx_state_lock, flags);
		return;
	}

	p->pending_write = false;
	wake_up(&p->tx_done_wq);

	/*
	 * A stalled TX kthread is parked on tx_wq with an empty queue, so it
	 * needs this wakeup to notice that the window is free again. Not racy
	 * against a concurrent stall: tx_stalled is set under this lock and
	 * irot_tx_stall_ended() is evaluated before the kthread sleeps again.
	 */
	if (p->tx_stalled)
		wake_up(&p->tx_wq);

	spin_unlock_irqrestore(&p->tx_state_lock, flags);
}

static int irot_tx_mark_pending(struct irot_priv *p, u32 counter)
{
	guard(spinlock_irqsave)(&p->tx_state_lock);

	if (p->pending_write)
		return -EBUSY;

	p->pending_tx_counter = counter;
	p->pending_write = true;

	return 0;
}

static bool irot_tx_idle(struct irot_priv *p)
{
	guard(spinlock_irqsave)(&p->tx_state_lock);

	return !p->pending_write;
}

static bool irot_tx_stalled(struct irot_priv *p)
{
	guard(spinlock_irqsave)(&p->tx_state_lock);

	return p->tx_stalled;
}

static bool irot_tx_stall_ended(struct irot_priv *p)
{
	guard(spinlock_irqsave)(&p->tx_state_lock);

	return p->tx_stalled && !p->pending_write;
}

/*
 * The peer owes us an ACK and has not sent it: take the link down and discard
 * what is queued. The write slot stays marked pending on purpose, because the
 * peer may still be reading the window and reusing it would corrupt the
 * transfer. Only a late cc_mctp_done releases it.
 */
static void irot_tx_stall(struct irot_priv *p)
{
	unsigned long flags;
	u32 counter;
	u32 dropped;

	spin_lock_irqsave(&p->tx_state_lock, flags);
	p->tx_stalled = true;
	counter = p->pending_tx_counter;
	spin_unlock_irqrestore(&p->tx_state_lock, flags);

	if (p->netdev_live) {
		netif_carrier_off(p->ndev);
		netif_stop_queue(p->ndev);
	}

	dropped = skb_queue_len(&p->tx_queue);
	skb_queue_purge(&p->tx_queue);

	p->ndev->stats.tx_errors++;
	p->ndev->stats.tx_dropped += dropped;

	dev_err(p->dev,
		"no mctp_done ACK for TX counter %u within %ums, taking link down (dropped %u queued packets)\n",
		counter, IROT_TX_ACK_TIMEOUT_MS, dropped);
}

static void irot_tx_stall_recover(struct irot_priv *p)
{
	unsigned long flags;
	bool recovered = false;

	spin_lock_irqsave(&p->tx_state_lock, flags);
	if (p->tx_stalled && !p->pending_write) {
		p->tx_stalled = false;
		recovered = true;
	}
	spin_unlock_irqrestore(&p->tx_state_lock, flags);

	if (!recovered)
		return;

	dev_info(p->dev, "TX transfer completed, link up\n");

	if (p->netdev_live) {
		netif_carrier_on(p->ndev);
		netif_wake_queue(p->ndev);
	}
}

/*
 * Bounded wait for the outstanding cc_mctp_done. Returns 0 when the slot is
 * free, -ETIMEDOUT when the peer is overdue, -ENODEV when the kthread is being
 * stopped.
 */
static int irot_tx_wait_ack(struct irot_priv *p)
{
	long rc;

	rc = wait_event_idle_timeout(p->tx_done_wq,
				     kthread_should_stop() || irot_tx_idle(p),
				     msecs_to_jiffies(IROT_TX_ACK_TIMEOUT_MS));

	if (kthread_should_stop())
		return -ENODEV;

	/*
	 * rc == 0 means the timeout expired, but the condition may still have
	 * become true on the last jiffy, so re-check before declaring the
	 * transport broken.
	 */
	if (!rc && !irot_tx_idle(p))
		return -ETIMEDOUT;

	return 0;
}

/*
 * RX ring helpers. All four run with rx_lock held and return false when the
 * ring is full or empty. irot_push_ack() records a counter that owes the peer
 * an ACK but whose payload was dropped or duplicated.
 */
static bool irot_push_ack(struct irot_priv *p, u32 counter)
{
	if (p->rx_ack_count >= IROT_RX_ACK_RING_SIZE)
		return false;

	p->rx_ack_ring[p->rx_ack_tail] = counter;
	p->rx_ack_tail = (p->rx_ack_tail + 1) % IROT_RX_ACK_RING_SIZE;
	p->rx_ack_count++;

	return true;
}

static bool irot_pop_ack(struct irot_priv *p, u32 *counter)
{
	if (!p->rx_ack_count)
		return false;

	*counter = p->rx_ack_ring[p->rx_ack_head];
	p->rx_ack_head = (p->rx_ack_head + 1) % IROT_RX_ACK_RING_SIZE;
	p->rx_ack_count--;

	return true;
}

static bool irot_push_rx(struct irot_priv *p, const struct irot_rx_entry *e)
{
	if (p->rx_count >= IROT_RX_RING_SIZE)
		return false;

	p->rx_ring[p->rx_tail] = *e;
	p->rx_tail = (p->rx_tail + 1) % IROT_RX_RING_SIZE;
	p->rx_count++;

	return true;
}

static bool irot_pop_rx(struct irot_priv *p, struct irot_rx_entry *e)
{
	if (!p->rx_count)
		return false;

	*e = p->rx_ring[p->rx_head];
	p->rx_head = (p->rx_head + 1) % IROT_RX_RING_SIZE;
	p->rx_count--;

	return true;
}

/*
 * cc_mctp_buffer: the negotiated windows have arrived. Write-once; a zero
 * mtu_limit means "no answer yet", which also covers a firmware that replies
 * with an empty descriptor.
 */
static void irot_cb_buffer_rsp(struct irot_priv *p, const struct irot_msg *m)
{
	u32 mtu_limit = le32_to_cpu(m->data.buffers.mtu_limit);
	unsigned long flags;

	spin_lock_irqsave(&p->rx_lock, flags);
	if (!p->nego_valid && mtu_limit) {
		irot_region_from_span(&p->nego_write, &m->data.buffers.host_write);
		irot_region_from_span(&p->nego_read, &m->data.buffers.host_read);
		p->nego_mtu_limit = mtu_limit;
		p->nego_valid = true;
	}
	spin_unlock_irqrestore(&p->rx_lock, flags);

	up(&p->probe_sem);
}

/*
 * cc_mctp: an inbound packet is sitting in host_read. Decides, without
 * sleeping, what we owe the peer for this counter. The whole policy turns on
 * whether the peer retransmits; see struct irot_variant.
 */
static void irot_cb_mctp(struct irot_priv *p, const struct irot_msg *m)
{
	bool retransmits = p->var->peer_retransmits;
	struct irot_rx_entry e;
	unsigned long flags;
	bool stall = false;
	bool wake = false;

	e.counter = le32_to_cpu(m->data.mctp.counter);
	e.size    = le32_to_cpu(m->data.mctp.packet.size);
	e.base    = ((u64)le32_to_cpu(m->data.mctp.packet.address.high) << 32) |
		    le32_to_cpu(m->data.mctp.packet.address.low);

	spin_lock_irqsave(&p->rx_lock, flags);

	if (p->rx_stalled) {
		dev_core_stats_rx_dropped_inc(p->ndev);
		goto out;
	}

	/*
	 * A counter we have already delivered. Only a retransmitting peer can
	 * produce one, so only then is it a duplicate rather than a wrap: ACK
	 * it without handing the same packet to the stack twice. If there is
	 * no room to record the ACK, withholding it is safe -- it comes back.
	 */
	if (retransmits && p->has_prev_rx_counter &&
	    e.counter == p->prev_rx_counter) {
		wake = irot_push_ack(p, e.counter);
		goto out;
	}

	if (irot_push_rx(p, &e)) {
		p->prev_rx_counter = e.counter;
		p->has_prev_rx_counter = true;
		wake = true;
		goto out;
	}

	dev_core_stats_rx_dropped_inc(p->ndev);

	if (retransmits) {
		/*
		 * Ordinary backpressure: say nothing and the peer sends it
		 * again. Deliberately do not advance the dedup cursor, so the
		 * retransmit counts as fresh.
		 */
		goto out;
	}

	if (irot_push_ack(p, e.counter)) {
		wake = true;
		goto out;
	}

	/*
	 * Both rings are full and nothing will come back: we have lost an ACK
	 * obligation and the peer will wait for it forever. Do not continue
	 * silently.
	 */
	p->rx_stalled = true;
	stall = true;

out:
	spin_unlock_irqrestore(&p->rx_lock, flags);

	if (wake)
		wake_up(&p->rx_wq);

	if (stall) {
		if (p->netdev_live) {
			netif_carrier_off(p->ndev);
			netif_stop_queue(p->ndev);
		}
		p->ndev->stats.rx_errors++;

		dev_err_ratelimited(p->dev,
				    "RX queues exhausted at counter %u, taking link down\n",
				    e.counter);
	}
}

/*
 * Runs in the mailbox controller's delivery context: never sleeps and never
 * calls irot_send(), a reply is always deferred to the RX kthread. @data points
 * at the controller's frame buffer and is only valid for the duration of this
 * call, so everything needed is copied out here.
 */
static void irot_rx_callback(struct mbox_client *cl, void *data)
{
	const struct irot_mbox_frame *frame = data;
	const struct irot_msg *m;
	struct irot_priv *p;
	unsigned long flags;

	if (!cl || !data)
		return;

	p = container_of(cl, struct irot_priv, cl);
	m = &frame->irot;

	switch (le32_to_cpu(m->command)) {
	case irot_cc_mctp_buffer:
		irot_cb_buffer_rsp(p, m);
		break;

	case irot_cc_mctp:
		irot_cb_mctp(p, m);
		break;

	case irot_cc_mctp_done:
		irot_tx_mark_done(p, le32_to_cpu(m->data.value));
		break;

	case irot_cc_ping:
		/*
		 * Do not reply from here: mbox_send_message() sleeps. Defer the
		 * response to the RX kthread.
		 */
		spin_lock_irqsave(&p->rx_lock, flags);
		p->pending_ping = true;
		p->pending_ping_value = le32_to_cpu(m->data.value);
		spin_unlock_irqrestore(&p->rx_lock, flags);
		wake_up(&p->rx_wq);
		break;

	default:
		dev_warn_ratelimited(p->dev, "unknown iROT command %u\n",
				     le32_to_cpu(m->command));
		break;
	}
}

/*
 * tx_block = true: mbox_send_message() blocks until the controller reports
 * tx-done. irot_send() relies on that, and on tx_chan_lock, for the lifetime
 * of the frame it hands over.
 */
static int irot_request_mbox(struct irot_priv *p)
{
	p->cl.dev          = p->dev;
	p->cl.rx_callback  = irot_rx_callback;
	p->cl.tx_block     = true;
	p->cl.knows_txdone = false;
	p->cl.tx_tout      = IROT_MBOX_TX_TOUT_MS;

	p->chan_tx = mbox_request_channel_byname(&p->cl, IROT_MBOX_CHAN_TX);
	if (IS_ERR(p->chan_tx)) {
		int rc = PTR_ERR(p->chan_tx);

		/* Leave no ERR_PTR behind for the teardown paths to trip on */
		p->chan_tx = NULL;
		return dev_err_probe(p->dev, rc,
				     "Failed to request \"%s\" mailbox channel\n",
				     IROT_MBOX_CHAN_TX);
	}

	p->chan_rx = mbox_request_channel_byname(&p->cl, IROT_MBOX_CHAN_RX);
	if (IS_ERR(p->chan_rx)) {
		int rc = PTR_ERR(p->chan_rx);

		p->chan_rx = NULL;
		mbox_free_channel(p->chan_tx);
		p->chan_tx = NULL;
		return dev_err_probe(p->dev, rc,
				     "Failed to request \"%s\" mailbox channel\n",
				     IROT_MBOX_CHAN_RX);
	}

	return 0;
}

/*
 * Read the "memory-region" phandle pair.
 *
 * of_reserved_mem_region_to_resource() is the right accessor rather than
 * of_address_to_resource(): it resolves the phandle, rejects an unavailable
 * node, and takes base and size from the registered reserved_mem entry. That
 * works for the "no-map" regions these windows use, which are carved out of
 * RAM and are not describable as a translatable bus address.
 *
 * Absent is not an error here. In negotiated mode the property is optional
 * (and then nothing bounds the firmware's answer); static mode requires it and
 * says so in irot_shmem_from_dt().
 */
static int irot_parse_dt_windows(struct irot_priv *p)
{
	static const char * const label[2] = {
		"host_write (AP -> IRoT)",
		"host_read (IRoT -> AP)",
	};
	struct irot_region *win[2] = { &p->dt_write, &p->dt_read };
	struct device_node *np = p->dev->of_node;
	int i, count, rc;

	if (!np || !of_property_present(np, "memory-region"))
		return 0;

	count = of_reserved_mem_region_count(np);
	if (count != 2)
		return dev_err_probe(p->dev, count < 0 ? count : -EINVAL,
				     "memory-region needs exactly 2 reserved regions (got %d)\n",
				     count);

	for (i = 0; i < 2; i++) {
		struct resource res;
		u64 size;

		rc = of_reserved_mem_region_to_resource(np, i, &res);
		if (rc)
			return dev_err_probe(p->dev, rc,
					     "memory-region %d [%s] is not a usable reserved region\n",
					     i, label[i]);

		size = resource_size(&res);
		if (!size || size > U32_MAX)
			return dev_err_probe(p->dev, -EINVAL,
					     "memory-region %d [%s] size %llu out of range\n",
					     i, label[i], size);

		win[i]->base = res.start;
		win[i]->size = (u32)size;
	}

	p->have_dt_windows = true;

	dev_dbg(p->dev,
		"DT windows: write 0x%llx+%u, read 0x%llx+%u\n",
		p->dt_write.base, p->dt_write.size,
		p->dt_read.base, p->dt_read.size);

	return 0;
}

/*
 * Map both windows write-combining. Shared by both provisioning modes: by this
 * point host_write, host_read and mtu_limit are filled in, however they were
 * obtained. The per-window copy bounds are derived separately from the
 * advertised MTU, because a window may be smaller than it, and they are what
 * the data paths enforce.
 */
static int irot_map_regions(struct irot_priv *p)
{
	p->rx_addr = devm_memremap(p->dev, p->host_read.base,
				   p->host_read.size, MEMREMAP_WC);
	if (IS_ERR(p->rx_addr))
		return dev_err_probe(p->dev, PTR_ERR(p->rx_addr),
				     "Failed to map host_read window\n");

	p->tx_addr = devm_memremap(p->dev, p->host_write.base,
				   p->host_write.size, MEMREMAP_WC);
	if (IS_ERR(p->tx_addr))
		return dev_err_probe(p->dev, PTR_ERR(p->tx_addr),
				     "Failed to map host_write window\n");

	p->read_mtu  = min(p->mtu_limit, p->host_read.size);
	p->write_mtu = min(p->mtu_limit, p->host_write.size);

	dev_info(p->dev,
		 "shared windows: read 0x%llx+%u, write 0x%llx+%u, mtu_limit=%u read_mtu=%u write_mtu=%u\n",
		 p->host_read.base, p->host_read.size,
		 p->host_write.base, p->host_write.size,
		 p->mtu_limit, p->read_mtu, p->write_mtu);

	return 0;
}

/*
 * Bound the firmware's answer by the DT windows.
 *
 * The IRoT hands us bare physical addresses. Without a reference to check them
 * against, a buggy or compromised peer can point host_read or host_write at
 * any RAM and this driver will map it write-combining and copy packets through
 * it. irot_region_offset() only bounds each packet inside those windows, so
 * the windows themselves have to be bounded here.
 *
 * When the DT declares memory-region, the negotiated windows must lie inside
 * the reserved ones or probe fails. When it does not -- a legacy DT -- the
 * addresses are taken on trust and the absence of a check is logged.
 */
static int irot_validate_negotiated(struct irot_priv *p)
{
	if (!p->have_dt_windows) {
		dev_warn(p->dev,
			 "no memory-region in DT: negotiated windows are unvalidated\n");
		return 0;
	}

	if (!irot_region_contains(&p->dt_write, &p->host_write))
		return dev_err_probe(p->dev, -ERANGE,
				     "negotiated host_write 0x%llx+%u outside reserved window 0x%llx+%u\n",
				     p->host_write.base, p->host_write.size,
				     p->dt_write.base, p->dt_write.size);

	if (!irot_region_contains(&p->dt_read, &p->host_read))
		return dev_err_probe(p->dev, -ERANGE,
				     "negotiated host_read 0x%llx+%u outside reserved window 0x%llx+%u\n",
				     p->host_read.base, p->host_read.size,
				     p->dt_read.base, p->dt_read.size);

	dev_dbg(p->dev, "negotiated windows validated against DT\n");
	return 0;
}

/*
 * Negotiate the shared windows: send cc_get_mctp_buffers and wait for
 * cc_mctp_buffer, up to IROT_BUFFER_RETRIES attempts of IROT_BUFFER_TIMEOUT_S
 * each. The answer is bounds-checked before anything is mapped.
 */
static int irot_discover_buffers(struct irot_priv *p)
{
	struct irot_msg req = IROT_MSG_INIT;
	unsigned long flags;
	bool got = false;
	int i, rc;
	u32 cap;

	req.command = cpu_to_le32(irot_cc_get_mctp_buffers);

	for (i = 0; i < IROT_BUFFER_RETRIES && !got; i++) {
		dev_dbg(p->dev, "buffer discovery attempt %d/%d\n",
			i + 1, IROT_BUFFER_RETRIES);

		rc = irot_send(p, &req);
		if (rc < 0)
			return dev_err_probe(p->dev, rc,
					     "Failed to send cc_get_mctp_buffers\n");

		/* Woken by irot_cb_buffer_rsp(), or timed out */
		rc = down_timeout(&p->probe_sem,
				  msecs_to_jiffies(IROT_BUFFER_TIMEOUT_S * 1000));
		if (rc)
			dev_dbg(p->dev, "no cc_mctp_buffer within %us (attempt %d)\n",
				IROT_BUFFER_TIMEOUT_S, i + 1);

		spin_lock_irqsave(&p->rx_lock, flags);
		got = p->nego_valid;
		if (got) {
			p->host_write = p->nego_write;
			p->host_read  = p->nego_read;
			p->mtu_limit  = p->nego_mtu_limit;
		}
		spin_unlock_irqrestore(&p->rx_lock, flags);
	}

	if (!got)
		return dev_err_probe(p->dev, -ETIMEDOUT,
				     "No cc_mctp_buffer after %d attempts\n",
				     IROT_BUFFER_RETRIES);

	/*
	 * A window holds exactly one packet, so it also caps the MTU. Without
	 * this clamp a firmware over-reporting mtu_limit would leave max_mtu
	 * above write_mtu, and every packet an operator sent in that range
	 * would be dropped instead of being refused here.
	 */
	cap = min_t(u32, min(p->host_read.size, p->host_write.size),
		    IROT_MCTP_MAX_MTU);
	if (cap < IROT_MCTP_MIN_MTU)
		return dev_err_probe(p->dev, -EINVAL,
				     "negotiated windows hold %u bytes, below the MCTP minimum MTU %u\n",
				     cap, IROT_MCTP_MIN_MTU);

	if (p->mtu_limit < IROT_MCTP_MIN_MTU)
		return dev_err_probe(p->dev, -EINVAL,
				     "negotiated mtu_limit %u below the MCTP minimum %u\n",
				     p->mtu_limit, IROT_MCTP_MIN_MTU);

	if (p->mtu_limit > cap) {
		dev_warn(p->dev,
			 "negotiated mtu_limit %u exceeds what the windows hold (%u), clamping\n",
			 p->mtu_limit, cap);
		p->mtu_limit = cap;
	}

	rc = irot_validate_negotiated(p);
	if (rc)
		return rc;

	return irot_map_regions(p);
}

/*
 * Take the shared windows straight from the device tree, filling exactly the
 * state irot_discover_buffers() fills so that everything downstream is
 * identical between the two modes.
 *
 * There is no firmware mtu_limit here: the AP owns the windows, so the MTU is
 * what they hold. A window carries exactly one packet and each direction is
 * bounded by its own window, so the advertised MTU is the smaller of the two.
 * A peer whose reassembly buffer is smaller than that is an operator matter,
 * not a hardware description: "ip link set ... mtu N" covers it.
 */
static int irot_shmem_from_dt(struct irot_priv *p)
{
	u32 cap;

	if (!p->have_dt_windows)
		return dev_err_probe(p->dev, -ENODEV,
				     "this binding requires two memory-region phandles\n");

	p->host_write = p->dt_write;
	p->host_read  = p->dt_read;

	cap = min(p->host_read.size, p->host_write.size);
	if (cap < IROT_MCTP_MIN_MTU)
		return dev_err_probe(p->dev, -EINVAL,
				     "shared windows hold %u bytes, below the MCTP minimum MTU %u\n",
				     cap, IROT_MCTP_MIN_MTU);

	p->mtu_limit = cap;

	return irot_map_regions(p);
}

/*
 * Answer for a packet the RX thread could not deliver.
 *
 * A retransmitting peer must not be told the packet arrived: withhold the ACK
 * and it comes back, where the dedup path in irot_cb_mctp() acknowledges the
 * retransmit without delivering it. A peer that never retransmits must be
 * answered or its write slot wedges.
 */
static void irot_rx_ack_dropped(struct irot_priv *p, u32 counter)
{
	int rc;

	if (p->var->peer_retransmits)
		return;

	rc = irot_send_done(p, counter);
	if (rc < 0) {
		dev_warn_ratelimited(p->dev,
				     "Failed to ACK dropped RX counter %u: %d\n",
				     counter, rc);
		p->ndev->stats.rx_errors++;
	}
}

/*
 * Hand one inbound packet to the network stack. On false the caller applies the
 * per-peer drop policy; the counter is answered either way by the caller.
 */
static bool irot_rx_deliver(struct irot_priv *p, const struct irot_rx_entry *e)
{
	struct sk_buff *skb;
	u64 off;

	if (!e->size || e->size > p->read_mtu ||
	    !irot_region_offset(&p->host_read, e->base, e->size, &off)) {
		dev_warn_ratelimited(p->dev,
				     "invalid RX span addr=0x%llx size=%u (window 0x%llx+%u, read_mtu %u)\n",
				     e->base, e->size, p->host_read.base,
				     p->host_read.size, p->read_mtu);
		dev_core_stats_rx_dropped_inc(p->ndev);
		return false;
	}

	skb = netdev_alloc_skb(p->ndev, e->size);
	if (!skb) {
		dev_core_stats_rx_dropped_inc(p->ndev);
		return false;
	}

	/*
	 * The peer wrote the packet and then rang the doorbell. Order our read
	 * of the window after that notification, so we cannot observe a stale
	 * or partially written payload.
	 */
	rmb();

	skb_put_data(skb, (u8 *)p->rx_addr + off, e->size);

	skb->protocol = htons(ETH_P_MCTP);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	/* No physical addressing on this point-to-point link */
	__mctp_cb(skb)->halen = 0;

	if (netif_rx(skb) != NET_RX_SUCCESS) {
		dev_core_stats_rx_dropped_inc(p->ndev);
		return false;
	}

	p->ndev->stats.rx_packets++;
	p->ndev->stats.rx_bytes += e->size;

	return true;
}

/*
 * RX worker: deferred pings, owed ACKs, inbound packets -- one of each per
 * pass. Every path that consumes an entry re-raises the wait condition if more
 * remain, so nothing is left stranded in a ring.
 */
static int irot_rx_thread(void *arg)
{
	struct irot_priv *p = arg;

	while (!kthread_should_stop()) {
		struct irot_rx_entry e;
		unsigned long flags;
		u32 ack_counter = 0;
		u32 ping_value = 0;
		bool do_ping, have_ack, have_rx;
		bool delivered;
		int rc;

		wait_event_idle(p->rx_wq,
				kthread_should_stop() ||
				READ_ONCE(p->rx_count) ||
				READ_ONCE(p->rx_ack_count) ||
				READ_ONCE(p->pending_ping));

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&p->rx_lock, flags);
		do_ping = p->pending_ping;
		ping_value = p->pending_ping_value;
		p->pending_ping = false;
		have_ack = irot_pop_ack(p, &ack_counter);
		have_rx = irot_pop_rx(p, &e);
		spin_unlock_irqrestore(&p->rx_lock, flags);

		if (do_ping) {
			struct irot_msg rsp = IROT_MSG_INIT;

			rsp.command    = cpu_to_le32(irot_cc_ping_rsp);
			rsp.data.value = cpu_to_le32(ping_value);

			rc = irot_send(p, &rsp);
			if (rc < 0)
				dev_warn_ratelimited(p->dev,
						     "Failed to send ping response: %d\n",
						     rc);
		}

		/*
		 * A counter whose payload was dropped or duplicated in the
		 * callback. Owed unconditionally: the callback only records one
		 * when this peer requires an answer, or when it is a duplicate
		 * of a packet already delivered.
		 */
		if (have_ack) {
			rc = irot_send_done(p, ack_counter);
			if (rc < 0) {
				dev_warn_ratelimited(p->dev,
						     "Failed to ACK RX counter %u: %d\n",
						     ack_counter, rc);
				p->ndev->stats.rx_errors++;
			}
		}

		if (!have_rx)
			continue;

		delivered = irot_rx_deliver(p, &e);
		if (!delivered) {
			irot_rx_ack_dropped(p, e.counter);
			continue;
		}

		rc = irot_send_done(p, e.counter);
		if (rc < 0) {
			dev_warn_ratelimited(p->dev,
					     "Failed to ACK RX counter %u: %d\n",
					     e.counter, rc);
			p->ndev->stats.rx_errors++;
		}
	}

	return 0;
}

/*
 * TX worker: dequeue skbs and write them into host_write, one outstanding write
 * at a time. The window is only reused once the peer has acknowledged the
 * previous packet, and an overdue ACK stalls the link rather than force-freeing
 * the window under the peer's feet.
 */
static int irot_tx_thread(void *arg)
{
	struct irot_priv *p = arg;
	u32 next_tx_counter = p->var->first_tx_counter;

	while (!kthread_should_stop()) {
		struct irot_msg m = IROT_MSG_INIT;
		struct sk_buff *skb;
		u32 tx_counter;
		u32 len;
		int rc;

		wait_event_idle(p->tx_wq,
				kthread_should_stop() ||
				irot_tx_stall_ended(p) ||
				!skb_queue_empty(&p->tx_queue));

		if (kthread_should_stop())
			break;

		irot_tx_stall_recover(p);

		skb = skb_dequeue(&p->tx_queue);
		if (!skb)
			continue;

		if (irot_tx_stalled(p)) {
			dev_core_stats_tx_dropped_inc(p->ndev);
			dev_kfree_skb_any(skb);
			continue;
		}

		if (p->netdev_live)
			netif_wake_queue(p->ndev);

		len = skb->len;

		if (len == 0 || len > p->write_mtu) {
			dev_warn_ratelimited(p->dev,
					     "TX invalid len=%u (write_mtu %u), dropping\n",
					     len, p->write_mtu);
			dev_core_stats_tx_dropped_inc(p->ndev);
			dev_kfree_skb_any(skb);
			continue;
		}

		/*
		 * skb_copy_bits, not memcpy(skb->data, ...): an skb may be
		 * nonlinear, and skb->data covers only the linear head.
		 */
		if (skb_copy_bits(skb, 0, p->tx_addr, len)) {
			dev_warn_ratelimited(p->dev,
					     "TX skb_copy_bits(%u) failed, dropping\n",
					     len);
			dev_core_stats_tx_dropped_inc(p->ndev);
			dev_kfree_skb_any(skb);
			continue;
		}

		/* Publish the payload before ringing the doorbell */
		wmb();

		tx_counter = next_tx_counter++;

		m.command = cpu_to_le32(irot_cc_mctp);
		m.data.mctp.counter = cpu_to_le32(tx_counter);
		/* Always at the start of host_write: one outstanding write */
		irot_span_from_region(&m.data.mctp.packet,
				      &(struct irot_region){
					      .base = p->host_write.base,
					      .size = len,
				      });

		/*
		 * Claim the window before sending, so a cc_mctp_done that
		 * arrives immediately still matches an in-flight counter.
		 */
		rc = irot_tx_mark_pending(p, tx_counter);
		if (rc) {
			dev_core_stats_tx_dropped_inc(p->ndev);
			dev_kfree_skb_any(skb);
			continue;
		}

		rc = irot_send(p, &m);
		if (rc == -ETIME) {
			/*
			 * The AX3005 mailbox controller has already copied the
			 * message into its hardware FIFO, but the FIFO did not
			 * drain before tx_tout expired. The peer may therefore
			 * still consume the notification. Keep the window
			 * claimed and do not reuse it.
			 */
			dev_err(p->dev,
				"TX mailbox completion timed out, stalling link\n");
			dev_core_stats_tx_dropped_inc(p->ndev);
			irot_tx_stall(p);
			dev_kfree_skb_any(skb);
			continue;
		}
		if (rc < 0) {
			/* Nothing left the controller: the window is ours again */
			dev_err(p->dev, "TX send failed rc=%d\n", rc);
			dev_core_stats_tx_dropped_inc(p->ndev);
			irot_tx_mark_done(p, tx_counter);
			dev_kfree_skb_any(skb);
			continue;
		}

		p->ndev->stats.tx_packets++;
		p->ndev->stats.tx_bytes += len;

		dev_kfree_skb_any(skb);

		rc = irot_tx_wait_ack(p);
		if (rc == -ETIMEDOUT)
			irot_tx_stall(p);
		else if (rc)
			break;
	}

	return 0;
}

/*
 * Hand the skb to the TX kthread. tx_queue.lock is held across the depth check,
 * the enqueue and the flow-control decision so they are one atomic step.
 * NETDEV_TX_BUSY asks the stack to requeue; nothing is dropped on that path.
 */
static netdev_tx_t irot_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct irot_netdev_priv *priv = netdev_priv(ndev);
	netdev_tx_t status = NETDEV_TX_BUSY;
	struct irot_priv *p;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	p = priv->p;
	spin_unlock_irqrestore(&priv->lock, flags);

	if (!p) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	spin_lock_irqsave(&p->tx_queue.lock, flags);
	if (skb_queue_len(&p->tx_queue) >= IROT_TX_QUEUE_SIZE) {
		netif_stop_queue(ndev);
		status = NETDEV_TX_BUSY;
	} else {
		__skb_queue_tail(&p->tx_queue, skb);
		if (skb_queue_len(&p->tx_queue) >= IROT_TX_QUEUE_SIZE)
			netif_stop_queue(ndev);
		status = NETDEV_TX_OK;
	}
	spin_unlock_irqrestore(&p->tx_queue.lock, flags);

	if (status == NETDEV_TX_OK)
		wake_up(&p->tx_wq);

	return status;
}

static int irot_ndo_open(struct net_device *ndev)
{
	netif_start_queue(ndev);
	return 0;
}

static int irot_ndo_stop(struct net_device *ndev)
{
	netif_stop_queue(ndev);
	return 0;
}

static const struct net_device_ops irot_netdev_ops = {
	.ndo_open       = irot_ndo_open,
	.ndo_stop       = irot_ndo_stop,
	.ndo_start_xmit = irot_start_xmit,
};

/*
 * A point-to-point MCTP link with no physical addressing, as for the serial and
 * USB bindings. The MTU starts at the DSP0236 floor and is raised in probe once
 * the shared windows are known.
 */
static void irot_netdev_setup(struct net_device *ndev)
{
	ndev->type            = ARPHRD_MCTP;
	ndev->addr_len        = 0;
	ndev->hard_header_len = 0;
	ndev->flags           = IFF_NOARP;
	ndev->tx_queue_len    = IROT_TX_QUEUE_SIZE;
	ndev->min_mtu         = IROT_MCTP_MIN_MTU;
	ndev->max_mtu         = IROT_MCTP_MIN_MTU;
	ndev->mtu             = IROT_MCTP_MIN_MTU;
	ndev->netdev_ops      = &irot_netdev_ops;
}

/*
 * Probe order matters:
 *   1. allocate state, init every lock and wait queue
 *   2. read the DT windows (a bound in negotiated mode, the source in static)
 *   3. allocate the netdev, so the mailbox callback has somewhere to put stats
 *   4. request the mailbox channels -- the callback goes live here
 *   5. establish the shared windows: negotiate with the peer, or take the DT
 *      values. Both paths end in irot_map_regions().
 *   6. publish the real MTU and register with the MCTP core
 *   7. start the kthreads and raise the carrier
 */
static int irot_probe(struct platform_device *pdev)
{
	struct irot_netdev_priv *priv;
	struct device *dev = &pdev->dev;
	struct net_device *ndev;
	struct irot_priv *p;
	int rc;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	p->dev = dev;

	p->var = of_device_get_match_data(dev);
	if (!p->var)
		return dev_err_probe(dev, -ENODEV,
				     "no variant data for this compatible\n");

	skb_queue_head_init(&p->tx_queue);
	init_waitqueue_head(&p->tx_wq);
	init_waitqueue_head(&p->tx_done_wq);
	init_waitqueue_head(&p->rx_wq);
	spin_lock_init(&p->tx_state_lock);
	spin_lock_init(&p->rx_lock);
	mutex_init(&p->tx_chan_lock);
	sema_init(&p->probe_sem, 0);

	rc = irot_parse_dt_windows(p);
	if (rc)
		return rc;

	ndev = alloc_netdev(sizeof(*priv), IROT_NETDEV_NAME, NET_NAME_ENUM,
			    irot_netdev_setup);
	if (!ndev)
		return -ENOMEM;

	SET_NETDEV_DEV(ndev, dev);
	p->ndev = ndev;

	priv = netdev_priv(ndev);
	spin_lock_init(&priv->lock);
	priv->p = p;

	platform_set_drvdata(pdev, p);

	rc = irot_request_mbox(p);
	if (rc)
		goto err_free_netdev;

	if (p->var->negotiate_buffers)
		rc = irot_discover_buffers(p);
	else
		rc = irot_shmem_from_dt(p);
	if (rc)
		goto err_free_mailbox;

	ndev->max_mtu = p->mtu_limit;
	ndev->mtu     = p->mtu_limit;

	rc = mctp_register_netdev(ndev, NULL, MCTP_PHYS_BINDING_VENDOR);
	if (rc) {
		dev_err(dev, "mctp_register_netdev failed: %d\n", rc);
		goto err_free_mailbox;
	}
	p->netdev_live = true;

	p->tx_thread = kthread_run(irot_tx_thread, p, "irot-tx/%s",
				   dev_name(dev));
	if (IS_ERR(p->tx_thread)) {
		rc = PTR_ERR(p->tx_thread);
		p->tx_thread = NULL;
		dev_err(dev, "failed to start TX kthread: %d\n", rc);
		goto err_unregister;
	}

	p->rx_thread = kthread_run(irot_rx_thread, p, "irot-rx/%s",
				   dev_name(dev));
	if (IS_ERR(p->rx_thread)) {
		rc = PTR_ERR(p->rx_thread);
		p->rx_thread = NULL;
		dev_err(dev, "failed to start RX kthread: %d\n", rc);
		goto err_stop_tx;
	}

	netif_carrier_on(ndev);

	dev_info(dev,
		 "%s MCTP transport ready on \"%s\": %s windows, MTU %u\n",
		 p->var->desc, ndev->name,
		 p->var->negotiate_buffers ? "negotiated" : "device-tree",
		 ndev->mtu);

	return 0;

err_stop_tx:
	kthread_stop(p->tx_thread);
	p->tx_thread = NULL;
err_unregister:
	p->netdev_live = false;
	mctp_unregister_netdev(ndev);
err_free_mailbox:
	mbox_free_channel(p->chan_rx);
	mbox_free_channel(p->chan_tx);
err_free_netdev:
	free_netdev(ndev);
	return rc;
}

static void irot_remove(struct platform_device *pdev)
{
	struct irot_priv *p = platform_get_drvdata(pdev);
	struct irot_netdev_priv *priv;
	unsigned long flags;

	if (!p)
		return;

	/*
	 * Unregister first: that runs ndo_stop and quiesces the TX path, so the
	 * stack cannot queue anything new while the rest is torn down. The
	 * net_device itself stays alive until the end because the kthreads
	 * still touch its stats.
	 */
	if (p->ndev) {
		priv = netdev_priv(p->ndev);

		spin_lock_irqsave(&priv->lock, flags);
		priv->p = NULL;
		spin_unlock_irqrestore(&priv->lock, flags);

		p->netdev_live = false;
		mctp_unregister_netdev(p->ndev);
	}

	/* Wake both workers out of any wait so they can see the stop request */
	wake_up(&p->rx_wq);
	wake_up(&p->tx_wq);
	wake_up(&p->tx_done_wq);

	if (p->rx_thread) {
		kthread_stop(p->rx_thread);
		p->rx_thread = NULL;
	}
	if (p->tx_thread) {
		kthread_stop(p->tx_thread);
		p->tx_thread = NULL;
	}

	/* No senders left, so the channels can go */
	if (p->chan_rx) {
		mbox_free_channel(p->chan_rx);
		p->chan_rx = NULL;
	}
	if (p->chan_tx) {
		mbox_free_channel(p->chan_tx);
		p->chan_tx = NULL;
	}

	skb_queue_purge(&p->tx_queue);

	if (p->ndev) {
		free_netdev(p->ndev);
		p->ndev = NULL;
	}

	/* The window mappings are devm-managed and released after this */
}

static const struct irot_variant irot_variant_negotiated = {
	.desc              = "Axiado IRoT v2",
	.negotiate_buffers = true,
	.peer_retransmits  = true,
	.first_tx_counter  = 0,
};

static const struct irot_variant irot_variant_static = {
	.desc              = "Axiado IRoT",
	.negotiate_buffers = false,
	.peer_retransmits  = false,
	.first_tx_counter  = 1,
};

static const struct of_device_id irot_of_match[] = {
	{
		.compatible = IROT_COMPATIBLE_STATIC,
		.data       = &irot_variant_static,
	},
	{
		.compatible = IROT_COMPATIBLE_NEGOTIATED,
		.data       = &irot_variant_negotiated,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, irot_of_match);

static struct platform_driver irot_driver = {
	.probe  = irot_probe,
	.remove = irot_remove,
	.driver = {
		.name           = IROT_DRIVER_NAME,
		.of_match_table = irot_of_match,
	},
};
module_platform_driver(irot_driver);

MODULE_AUTHOR("Axiado Corporation");
MODULE_DESCRIPTION(IROT_DRIVER_DESC);
MODULE_LICENSE("GPL");
