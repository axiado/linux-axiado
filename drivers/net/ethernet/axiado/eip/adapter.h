/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2022-2026 Axiado Corporation. */

#ifndef _ADAPTER_H_
#define _ADAPTER_H_

#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/in6.h>

#include "api_pcl_dtl.h"
#include "api_dmabuf.h"
#include "da_pec_internal.h"
#include "eip.h"

/* 32-bits mark that should be present in egress pkt. */
/* This is added by iptables rule over ingress pkt. */
#define IPTABLES_RULE_MARK 0x06200001

/* Total number of records in Shadow Lookup Table.
 * When DTL_EXPIRY support is disabled, the record count should be 1
 */
#if DTL_EXPIRY_ENABLED
#define SLT_RECORD_CNT 70
#else
#define SLT_RECORD_CNT 1
#endif

/* number of SLT records in UDP shadow table*/
#define SL_FIFO_RECORD_CNT 30

/* When Rejected-Flow-Rule is added for Redirecting to Ring-3,
 * shadow table/fifo stores the rule with 9999 rule id
 */
#ifdef ENABLE_REJECTED_FLOW_TO_R3
#define REJECTED_FLOW_RULE_ID 9999
#endif /* ENABLE_REJECTED_FLOW_TO_R3 */

/* User ring interface. This is used in TR while adding dtl rule. */
#define DTL_REDIRECT_INTERFACE 1

#define IP_SIZE sizeof(struct in6_addr)
#define IPV4_SIZE sizeof(struct in_addr)

#define MARKED_DATA_SIZE 4
#define MAX_PKT_SIZE 128
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define IPHDR_MORE_FRAGS 0x0020
#define IPHDR_OFFSET 0xFF1F
#define IPSEC_IKE_PORT 500
#define IPSEC_NAT_PORT 4500

/* Before adding DTL rule, SKB mark is verfied for the specified value.
 * Netfilter/iptables rules put the valid SKB-mark on pkts
 */
#define INVALID_NF_SKB_MARK 0x0

/**
 * @brief 4 byte(1 word) struct for processing mark in pkt.
 */
struct marked_pkt {
	u16 rule_id; /* Rule ID extracted from skb->mark */
	u16 dos_bkt_addr : 10;
	/* Posssible values: accept, reject and forward_pkt(don't add rule) */
	u8 pkt_res : 2;
	u8 unused : 4;
} __packed;

/**
 * @brief selectors params for outer kernel module which calls APIs
 * for DTL rule add/delete.
 */
struct selectors {
	u8 sip[IP_SIZE];
	u8 dip[IP_SIZE];
	u16 sport;
	u16 dport;
	u8 proto;
	u32 spi; /* zero if not ipsec */
	u16 epoch; /* zero if not dtls */
	u32 flags; /*zero if not DTL */
};

/**
 * @brief This represents a record in Shadow Lookup Table. After adding dtl rule
 *  into EIP-lookup table, the handle for dtl rule needs to be stored along with
 *  selectors params in Shadow Lookup Table. This dtl-handle is used when the
 *  same rule is to be deleted from EIP-lookup table.
 */
struct shadow_dtl_rec {
	/* EIP-DTL rule handle */
	pcl_dtl_hash_handle_t dtlh;
	/* 5 tuple - source/Dest IP/Port and Protocol */
	struct selectors sel;

	/* Rule id associated with the flow. This is extracted from skb mark
	 * Valid values are greater than 0
	 */
	int mrk_rule_id;
	/* Transform Record handle */
	struct tr_handle trh;

	/* Pkt cnt stats from TR. Based on previous pkt cnt
	 * is same as current pkt cnt, DTL rule is expired
	 */
	u64 prvs_pkt_cnt;

	/* True if record is empty, otehrwise false */
	bool freed;
	/* Points to the next free SLT-record. At initialisation, this points to immediate
	 * next record(i.e. i + 1). After adding new record
	 * This value is copied to slt_curr_free_idx
	 */
	int nxt_free_idx;
};

/**
 * @brief ESP header definition.
 */
struct esphdr {
	u32 spi;
	u32 seq;
};

/**
 * @brief Rule selectors to be used by DTL rules.
 */
struct rule_selectors {
	struct marked_pkt m_pkt;
	u8 data[MAX_PKT_SIZE];
	u8 dst_mac[ETH_ALEN];
	u8 src_mac[ETH_ALEN];
	u8 protocol;
	u8 src_ip[IP_SIZE];
	u8 dst_ip[IP_SIZE];
	u16 src_port;
	u16 dst_port;
	u16 size;
	u16 epoch;
	u32 spi;
	u32 flags;
};

struct dtl_params {
	int lookup_table_id;
	struct rule_selectors *rule_sel;
	struct tr_handle *tr_h;
	int dest_rid;
	pcl_dtl_hash_handle_t dtl_hash_h; /* DTL entry handle in table.*/
};

u8 add_dtl_src_rule(struct eip_public *eip_pub, struct sk_buff *skb, u8 rid);
bool handle_non_default_pkt(struct net_device *ndev, u8 *raw_pkt, u32 len);
struct eiphdr *eip_hdr(struct sk_buff *skb);
#endif /* _ADAPTER_H_ */
