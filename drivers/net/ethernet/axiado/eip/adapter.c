// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * adapter.c - Adapter layer functions between eip-dev primary functions
 * and DTL/FWL rule related functions.
 *
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/netdevice.h>

#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <linux/types.h>

#include "da_pec_internal.h"
#include "eip.h"
#include "log.h"
#include "pec_api.h"
#include "adapter.h"
#include "dtl.h"

/**
 * @brief Get pointer to eip_hdr from skb.
 * @param skb ptr to sk_buff.
 * @return pointer to eiphdr in skb.
 */
struct eiphdr *eip_hdr(struct sk_buff *skb)
{
	return (struct eiphdr *)(skb->data - sizeof(struct eiphdr));
}

/**
 * @brief Inspect the skb of the pkt received from ipstack and extract
 *          rule parameters like IP addresses, port addresses, etc.
 *          As of now this supports only for ICMP, UDP and TCP pkts.
 *
 * @param p_data Exracted rule-parameters will be stored in this output struct.
 * @param skb address to the skb which will be inspected.
 * @param mac_idx (O/P) MAC index for ingress pkt. This is filled using app_id
 * extracted from skb->mark.
 *
 * @return 0 on success, negative errno on failure.
 */
static int extract_rule(struct rule_selectors *p_data,
			struct sk_buff *skb, int *mac_idx)
{
	struct ethhdr *eth = NULL;
	struct iphdr *iph = NULL;
	struct esphdr *esp_h = NULL;
	struct udphdr *u_hdr = NULL;
	struct tcphdr *tcp_h = NULL;
	struct icmphdr *icmp_h = NULL;
	struct eiphdr *eiph;
	u32 skb_mark;
	int len;

	if (skb->protocol == htons(ETH_P_IP)) {
		eth = eth_hdr(skb);
		/* Extract MAC and App Id */
		eiph = eip_hdr(skb);
		memcpy(p_data->src_mac, eiph->dst_mac, ETH_ALEN);
		memcpy(p_data->dst_mac, eth->h_source, ETH_ALEN);

		if (!eiph->app_id || eiph->app_id > NDEV_APP_ID_MAC0) {
			LOG_WARN("%s: Invalid Dest Interface(%d)", __func__,
				 eiph->app_id);
			return -EINVAL;
		}

		if (eiph->app_id == NDEV_APP_ID_MAC0)
			*mac_idx = 0;
		else
			*mac_idx = eiph->app_id;

		/* Extract IP */
		iph = ip_hdr(skb);
		memcpy(p_data->src_ip, &iph->saddr, IPV4_SIZE);
		memcpy(p_data->dst_ip, &iph->daddr, IPV4_SIZE);

		/* Extract Protocol */
		p_data->protocol = iph->protocol;
		if (iph->protocol == IPPROTO_UDP) {
			u_hdr = udp_hdr(skb);
			p_data->dst_port = htons(u_hdr->dest);
			p_data->src_port = htons(u_hdr->source);

			len = u_hdr->len;

		} else if (iph->protocol == IPPROTO_TCP) {
			tcp_h = tcp_hdr(skb);

			p_data->dst_port = htons(tcp_h->dest);
			p_data->src_port = htons(tcp_h->source);

		} else if (iph->protocol == IPPROTO_ICMP) {
			icmp_h = icmp_hdr(skb);
			/* Set destination port as 0x0 in case of ICMP. */
			p_data->dst_port = 0x0;
			/* Set source port as packing of icmp-type and icmp-code */
			p_data->src_port = (icmp_h->type << 8) | (icmp_h->code);

		} else if (iph->protocol == IPPROTO_ESP) {
			esp_h = (struct esphdr *)((u8 *)iph + iph->ihl * 4);
			p_data->spi = htonl(esp_h->spi);
			p_data->dst_port = 0x0;
			p_data->src_port = 0x0;
		} else {
			LOG_WARN("%s:ERROR: Invalid Protocol:%d ", __func__,
				 iph->protocol);
			return -EINVAL;
		}
		if (eiph->vlan_id != EIP_VLAN_ID_NONE) {
			/* If the packet has a valid VLAN ID (i.e., it's not set to the special
			 * value indicating no VLAN), add a VLAN header to the packet buffer.
			 * This re-tags the packet with its associated VLAN ID from the eip header
			 * structure before further processing or sending it out.
			 */
			skb_vlan_push(skb, ETH_P_8021Q, eiph->vlan_id);
		}

		if (skb->len < 128) {
			p_data->size = skb->len;
			memcpy(p_data->data, skb->data, skb->len);
		} else {
			p_data->size = MAX_PKT_SIZE;
			memcpy(p_data->data, skb->data, MAX_PKT_SIZE);
		}
		/* Extract mark fields */
		memcpy((struct marked_pkt *)&p_data->m_pkt, (u32 *)&skb->mark,
		       MARKED_DATA_SIZE);
	} else {
		LOG_WARN("%s(%d):Invalid protocol:%x. ignored", __func__,
			 __LINE__, skb->protocol);
		return -EINVAL;
	}

	/* Extrace and fill mark */
	LOG_DEBG("SKB-MARK: 0x%08X", skb->mark);
	skb_mark = skb->mark;
	memcpy(&p_data->m_pkt, &skb_mark, sizeof(u32));

	LOG_DEBG("%s:Extracted rules from skb.", __func__);
	return 0;
}

/**
 * @brief Compare the mark field of skb with fixed-pattern IPTABLES_RULE_MARK
 *
 * @param skb Pointer to skb received from ipstack.
 *
 * @return 0 if skb-mark is valid(matches), otherwise return non-zero.
 */
int verify_skb_mark(struct sk_buff *skb)
{
	LOG_DEBG("Actual mark = 0x%X, Expected mark = 0x%X", skb->mark,
		 IPTABLES_RULE_MARK);
	if (skb->mark == INVALID_NF_SKB_MARK) {
		LOG_DEBG("%s:%d:WARN : skb mark does not match.", __func__,
			 __LINE__);
		return 1;
	}
	return 0;
}

/**
 * @brief After adding the DTL rule successfully, the pkt should be
 *  forwarded to user-interface(eip-1,2..) ipstack. Here pkt is forwarded
 *  to the MAC's ipstack. Destination MAC in the pkt is set according to destination
 * interface mac address
 *
 * @param priv netdev private data ptr
 * @param skb skb-ptr containing pkt-data and pkt-length.
 *
 * @return 0 if success, otherwise non-zero.
 */
void eip_send_usr(struct net_device *ndev, struct sk_buff *skb)
{
	u32 pkt_len = skb->len;
	unsigned char *pkt_data = (unsigned char *)skb->data;

	if (!ndev) {
		LOG_CRIT("%s: Invalid User Interface", __func__);
		return;
	}

	/* Get netdev corresponding the MAC interface pkt received */
	memcpy(pkt_data + ETH_ALEN, pkt_data, ETH_ALEN);
	/* get dest mac from netdev and set this into pkt_data dst-mac */
	memcpy(pkt_data, ndev->dev_addr, ETH_ALEN);

	/* Send pkt-data to user interface */
	eip_send_ipstack(ndev, INVALID_INTERFACE_ID, pkt_len, pkt_data,
			 NON_OFFLOAD_TR_CNTXT_REF, true);
}

/**
 * @brief Delete DTL rule from lookup table using rule_selectors
 *
 * @param sel DTL rule parameters.
 *
 * @return EIP_STATUS_SUCCESS if DTL-rule is deleted successfully. Otherwise Non-zero.
 */
int delete_lookup_by_rule_sel(struct rule_selectors *rule_sel)
{
	struct selectors sel = { 0 };

	/* Fill rule selector struct from selector struct. */
	memcpy(sel.sip, rule_sel->src_ip, IP_SIZE);
	memcpy(sel.dip, rule_sel->dst_ip, IP_SIZE);
	sel.sport = rule_sel->src_port;
	sel.dport = rule_sel->dst_port;
	sel.proto = rule_sel->protocol;
	sel.spi = rule_sel->spi;
	sel.epoch = rule_sel->epoch;
	sel.flags = rule_sel->flags;

	return delete_lookup_rule(&sel);
}

/**
 * @brief top level function for adding DTL rules. This inspects the
 *          received pkt, verifies mark, extracts rule from pkt and adds DTL
 *          rule into the lookup table.
 *
 * @param skb pointer to skb received from ipstack.
 * @param eip_pub eip-public data.
 * @param rid ring id for which rule will be added
 */
u8 add_dtl_src_rule(struct eip_public *eip_pub, struct sk_buff *skb, u8 rid)
{
	int ret;
	int mac_idx;
	struct rule_selectors rule_sel;

	LOG_DEBG("%s:Trying to add DTL rule.", __func__);

	if (verify_skb_mark(skb))
		return 1;

	ret = extract_rule(&rule_sel, skb, &mac_idx);
	if (ret) {
		LOG_WARN("error in extracting rule from skb. ret = %d", ret);
		return 1;
	}

	ret = add_dtl_rule(eip_pub, &rule_sel, rid);
	if (ret) {
		LOG_WARN("error in adding dtl rule. ret = %d", ret);
		return 1;
	}

	/* Forward pkt to user-ipstack. If DTL rule is added successfully,
	 * and the Flow is to ACCEPT the pkt THEN ONLY, the pkt should be forwarded
	 * to user-interface ipstack. Check the skb-mark-result for ACCEPT/REJECT.
	 */
	if (rule_sel.m_pkt.pkt_res)
		eip_send_usr(eip_pub->ndev_list[mac_idx], skb);

	return 0;
}

/**
 * @brief If the pkt has to go to default interface returns true
		  otherwise send it to data interfaces(eip1, eip2..)
 * @param dev netdev struct
 * @param raw_pkt ingress pkt buffer
 * @param len size of pkt in bytes
 * @return 0 if pkt is ARP and it is successfully redirected to eip1.
 * 1 if pkt is not an ARP.
 */
bool handle_non_default_pkt(struct net_device *ndev, u8 *raw_pkt, u32 len)
{
	struct ethhdr *eth = NULL;
	struct iphdr *iph = NULL;
	struct udphdr *uh = NULL;

	eth = (struct ethhdr *)raw_pkt;
	if (ntohs(eth->h_proto) == ETH_P_ARP) {
		LOG_DEBG("Pkt is an ARP req.");
		goto send_to_data_intrf;
	}

	/* Its a temporary fix forward all fragmented packets to ring-1 as
	 * ip packet will not have transport layer details which are req for lookup
	 * Need to fix this correctly
	 */
	if (ntohs(eth->h_proto) == ETH_P_IP) {
		iph = (struct iphdr *)(raw_pkt + sizeof(struct ethhdr));
		/* If more flags is set then it is fragmented packet
		 * For the last packet in the fragment, more_fragments will be zero, but
		 * offset will be > 0
		 */
		if (iph->frag_off & (IPHDR_MORE_FRAGS | IPHDR_OFFSET)) {
			LOG_DEBG("Pkt is fragmented\n");
			goto send_to_data_intrf;
		}

		/* DHCP packet */
		if (iph->protocol == IPPROTO_UDP) {
			uh = (struct udphdr *)(raw_pkt + sizeof(struct ethhdr) +
					       sizeof(struct iphdr));
			if ((htons(uh->dest) == DHCP_CLIENT_PORT &&
			     htons(uh->source) == DHCP_SERVER_PORT) ||
			    (htons(uh->dest) == DHCP_SERVER_PORT &&
			     htons(uh->source) == DHCP_CLIENT_PORT)) {
				goto send_to_data_intrf;
			}
			/* IPSEC packet */
			if ((htons(uh->dest) == IPSEC_IKE_PORT) ||
			    (htons(uh->dest) == IPSEC_NAT_PORT)) {
				goto send_to_data_intrf;
			}
		}
	}

	return false;

send_to_data_intrf:
	/* All ring0 packets reuses pool of packet buffers and dont use the build_skb */
	eip_send_ipstack(ndev, INVALID_INTERFACE_ID, len,
			 (unsigned char *)raw_pkt, NON_OFFLOAD_TR_CNTXT_REF,
			 true);

	return true;
}
