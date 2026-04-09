/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2022-2026 Axiado Corporation. */

#ifndef _EIP_H_
#define _EIP_H_
#include "api_dmabuf.h"
#include "token_builder.h"
#include "device_types.h"
#include <linux/netdevice.h>
#include <linux/ktime.h>
#include <linux/wait.h>
#include "da_pec_internal.h"
#include "hcp.h"

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
#include "ipsec.h"
#endif

#define EIP_TX_BUF_SIZE 1500
#define EIP_RX_BUF_SIZE 1700
#define EIP_VLAN_ID_NONE 0xFFFF
#define EIP_VLAN_PROTO_OFST 12
#define EIP_VLAN_ID_OFST 14
#define EIP_VLAN_PROTO_SIZE 2
#define EIP_VLAN_ID_SIZE 2
#define EIP_VLAN_FIELD_SIZE 4

/* max rings available */
#define EIP_MAX_RING DRIVER_MAX_NOF_RING_TO_USE

/* RX-descr to use and budget */
#define EIP_NAPI_WEIGHT_FACTOR 1
#define EIP_NAPI_BUDGET (MAX_RD_COUNT * EIP_NAPI_WEIGHT_FACTOR)

/* How many tx-descr to use */
#ifdef DRIVER_MAX_TX_DESCR
#define EIP_MAX_TX_PKT_BUFFERS DRIVER_MAX_TX_DESCR
#else
#define EIP_MAX_TX_PKT_BUFFERS 16
#endif
#define EIP_MAX_CDR EIP_MAX_TX_PKT_BUFFERS

#define EIP_IRQ_NAME_LEN 16

enum NDEV_APP_ID {
	NDEV_APP_ID_MAC0 = 5,
	NDEV_APP_ID_MAC1 = 1,
	NDEV_APP_ID_MAC2 = 2,
	NDEV_APP_ID_MAC3 = 3,
	NDEV_APP_ID_MAC4 = 4,
	NDEV_APP_ID_DFL = 6,
	NDEV_APP_ID_DTL = 7
};

struct sa_handle {
	dma_buf_host_address_t sa_host_address;
	dma_buf_handle_t sa_handle_out;
	unsigned int tcr_words;
	void *tcr_data_out;
	unsigned int token_words;
	unsigned int token_header_word;
	unsigned int token_max_words_out;
	/* Virtual address for command descriptor token data */
	dma_buf_host_address_t token_host_address;
	/* Physical address for command descriptor token data */
	dma_buf_handle_t token_handle;
};

/**
 * @brief Transform record handle to be used while adding DTL rule.
 */
struct tr_handle {
	dma_buf_host_address_t virt_addr;
	dma_buf_handle_t phy_addr;
};

union ring_handler {
	struct napi_struct napi_ring;
	struct tasklet_struct tasklet_ring;
};

struct eip_ring_interface {
	struct eip_public *eip_pub;
	union ring_handler rdr_handler;

	/* Rx */
	struct eip_pkt_buff *rx_buff;
	u32 rx_nxt_to_use;
	u32 rx_nxt_to_clean;
	s32 rx_to_prep;

	/* Tx */
	struct eip_pkt_buff *tx_buff;
	u32 tx_nxt_to_use;
	u32 tx_nxt_to_clean;

	int irq;
	u8 rid;
	u8 name[EIP_IRQ_NAME_LEN];
};

/**
 * @brief Shared data among all interfaces(netdev).
 */
struct eip_public {
	/* HCP device pointer for accessing shared resources */
	struct hcp_device *hcp;

	/* Cached EIP-197 global device handle */
	device_handle_t dev_handle;

	/* list of netdev of each interface. */
	struct net_device *ndev_list[MAX_NETDEV_CNT];
	struct eip_ring_interface eip_ring[RING_INTERFACE_CNT];

	u32 rdr_napi_cnt;

	struct hmac_precompute_wq hmac_key_offload_wait_q;

	/* Shadow Look-up Table ptr */
	struct shadow_dtl_rec *slt_ptr;

	/* Shadow Look-up Table for Non-TCP Rules only */
	struct shadow_dtl_rec *sl_fifo_ptr;

	/* index that points to free record in Shadow-Lookup_table.
	 * New record is added at this index
	 */
	int slt_curr_free_idx;

	/* index that points to free record in Shadow-Lookup_table.
	 * New record is added at this index
	 */
	int sl_fifo_curr_free_idx;

	/* Common Transform Record handle for SLT for each ring. */
	struct tr_handle sl_tbl_trh[RING_INTERFACE_CNT];

	/* Common Transform Record handle for SL fifo */
	struct tr_handle sl_fifo_trh[RING_INTERFACE_CNT];
	/* Shadow Lookup Table filled record count */
	//int slt_fill_cnt;

	/* timestamp for previous scanning of SLT. */
	//u64 prv_scan_ts;
	//ktime_t prv_scan_ts;

	/* Periodically scanning of DTL lookup table */
	//struct task_struct *dtl_scan;

	/* sa_handle TODO: This can be removed later when lookup is implemented */
	struct sa_handle *sa_handle;

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	/* ipsec offload*/
	struct eip_ipsec *eip_ipsec;
#endif
	struct work_struct rxaui_phy_reset;
};

struct eip_priv {
	bool link; /*Carrier: up/down, can be bit-stuffed for other link info*/

	/* interface stats */
	struct rtnl_link_stats64 stats64;

	/* Public data. Common among all interfaces. */
	struct eip_public *eip_pub;

	/* hardware port id (1-5) */
	u8 app_id;

	__u64 dropped_no_free_slots;
};

struct eip_pkt_buff {
	dma_buf_host_address_t host_address;
	dma_buf_handle_t handle;
};

/**
 * @brief This header is added to skb of ingress pkt on eip0.
 * When the pkt comes back on eipdtl interface, this headeer is stripped of.
 * Purpose of the headeer to store-restore pkt info that are changed by ipstack.
 */
struct eiphdr {
	u8 app_id;
	u16 vlan_id;
	u8 dst_mac[6];
	u8 dst_ip[16];
};

/* ethtool statistics -- BEGIN */
#define STAT_RING_INTERFACE_CNT 16
#define STAT_RING_OFFSET_MULTIPLIER STAT_RING_INTERFACE_CNT
#define EIP_DEFAULT_RING 0
#define EIP_RING_IN_BASE 0xFFB00
#define EIP_RING_OUT_BASE 0xFFB08
#define EIP_RING_DBG_BASE 0xFFF00
#define EIP_STATS_COUNT ARRAY_SIZE(eip_stats)
/* ethtool statistics -- END */

extern int mac_phy_addr_rd(int app_id, u32 *mac_0, u32 *mac_1);
extern int mac_phy_addr_wr(int app_id, u32 mac_0, u32 mac_1);
void mac_update_promisc(int app_id, bool promisc);
u8 get_netdev_interfaces(struct hcp_device *hcp, u8 *ndevs);

void eip_build_tx_tr(u32 *base);

bool eip_create_tx_sa(bool verbose, struct sa_handle *sa_handles);
int eip_delete_tx_sa(struct sa_handle *sa_handles);

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
/* ipsec */
bool eip_create_ipsec_sa(bool verbose, struct xfrm_state *xs,
			 struct eip_priv *priv, u16 sa_idx, bool rx,
			 struct ipsec_handle *sa);

static int eip_ipsec_add_sa(struct xfrm_state *xs, struct netlink_ext_ack *);
static void eip_ipsec_del_sa(struct xfrm_state *xs);
static void eip_xfrm_update_curlft(struct xfrm_state *x);
static int eip_ipsec_add_policy(struct xfrm_policy *x,
				struct netlink_ext_ack *);
static void eip_ipsec_del_policy(struct xfrm_policy *x);
static void eip_ipsec_free_policy(struct xfrm_policy *x);
static bool eip_ipsec_offload_ok(struct sk_buff *skb, struct xfrm_state *xs);

struct xfrm_state *get_xfrm_state(struct eip_ipsec *ipsec, bool rx, int ctx_id);
void eip_ipsec_init(struct net_device *dev);

#endif

int eip_create_tx_buffers(struct eip_ring_interface *eip_ring);
void eip_delete_tx_buffers(struct eip_ring_interface *eip_ring);
int eip_create_rx_buffer(struct eip_ring_interface *eip_ring, int nxt_to_use);
int eip_create_rx_buffers(struct eip_ring_interface *eip_ring);
void eip_delete_rx_buffers(struct eip_ring_interface *eip_ring);

struct eip_public *get_eip_public(void);

static inline device_handle_t eip_get_dev_handle(struct eip_public *eip_pub)
{
	return eip_pub ? eip_pub->dev_handle : NULL;
}

static inline device_handle_t eip_get_global_dev_handle(void)
{
	return eip_get_dev_handle(get_eip_public());
}

bool eip_send_ipstack(struct net_device *ndev, u8 rid, int len,
		      unsigned char *buf, int ctx_id, bool forwarded);

int eip_prepare_rds(struct eip_ring_interface *eip_ring, int rd_cnt);

struct tr_handle *get_tr(struct eip_public *eip_pub, u8 rid, bool is_slt);

int eip_rx_clean_rdr(struct eip_ring_interface *eip_ring);

bool eip_tx_send(struct eip_priv *priv, struct sk_buff *skb,
		 struct sa_handle *sa_handles, u8 rid);

int interrupt_init(struct eip_ring_interface *eip_ring, struct net_device *ndev,
		   struct device *dev);
void eip_free_irq(struct eip_ring_interface *eip_ring);
void ack_rdr_irq(u8 rid);
void enable_rdr_irq(u8 rid);
void disable_rdr_irq(u8 rid);
int eip_rdr_poll(struct napi_struct *napi, int budget);

#endif /* _EIP_H_ */
