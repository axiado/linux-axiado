// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include "api_pec.h"
#include "clib.h"
#include "da_pec_internal.h"
#include "device_mgmt.h"
#include "device_rw.h"
#include "log.h"
#include "eip.h"
#include "adapter.h"

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/types.h>
#include <linux/if_ether.h>

pec_result_descriptor_t g_res_desc[RING_INTERFACE_CNT][MAX_RD_COUNT];
u32 g_out_token[RING_INTERFACE_CNT][MAX_RD_COUNT][IOTOKEN_OUT_WORD_COUNT];
io_token_output_dscr_t g_io_token_output_desc[RING_INTERFACE_CNT][MAX_RD_COUNT];
struct rdr_pkts_stat g_pkt_get_stat[4];

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
static void da_pec_pcl_put16(u8 *p, unsigned int offs, u16 val)
{
	p[offs] = val >> 8;
	p[offs + 1] = val & 0xff;
}

static u16 da_pec_pcl_get16(u8 *p, unsigned int offs)
{
	return (p[offs] << 8) | p[offs + 1];
}

static void da_pec_pcl_add_checksum(u8 *p)
{
	unsigned int sum, hdrlen, i;
	unsigned int csum;

	hdrlen = p[0] & 0xf;

	csum = da_pec_pcl_get16(p, 10);
	da_pec_pcl_put16(p, 10, 0);
	sum = 0;
	for (i = 0; i < hdrlen * 2; i++)
		sum += da_pec_pcl_get16(p, i * 2);
	while ((sum >> 16) != 0)
		sum = (sum >> 16) + (sum & 0xffff);
	da_pec_pcl_put16(p, 10, ~sum);
}

/**
 * @brief Get the contxt ref object
 *
 * @param out_token_dscr
 * @return int - NON_OFFLOAD_TR_CNTXT_REF on error
			   - otherwise context ref id
 */
int get_contxt_ref(io_token_output_dscr_t out_token_dscr)
{
	if (out_token_dscr.ext_p) {
		io_token_output_dscr_ext_t *ext_p;

		ext_p = (io_token_output_dscr_ext_t *)out_token_dscr.ext_p;
		return ext_p->nph_context;
	}
	LOG_WARN("no context ref set for rd\n");
	return NON_OFFLOAD_TR_CNTXT_REF;
}
#endif

/**
 * @brief Allocate buffer for eip_hdr. Attach eip-header address to skb headroom.
 * eip-dev header contains destination mac address of the pkt.
 * This header buffer should be freed up after adding the DTL rule in xmit path.
 * Destination MAC in skb is replaced witheip-0 MAC.
 * WARN: This should be used only in control path(i.e. when sending pkt to eip-0
 * ipstack)
 *
 * @param ndev nedev ptr
 * @param skb buffer without having ingress pkt data into it.
 * @param buf Ingress pkt buffer having mac address into it.
 * @param app_id Destination netdev interface for the new flow.
 */
void set_eip_hdr(struct net_device *ndev, struct sk_buff *skb,
		 unsigned char *buf, int app_id)
{
	struct eiphdr eiph;
	struct ethhdr *eth = NULL;
	struct eip_priv *priv = NULL;
	u16 vlan_id = 0;

	priv = netdev_priv(ndev);
	memset(&eiph, 0, sizeof(struct eiphdr));
	/* Set App id(Destination Interface) */
	eiph.app_id = app_id;

	/* Extract mac from skb and put it into eiphdr */
	eth = (struct ethhdr *)buf;
	memcpy(eiph.dst_mac, eth->h_source, 6);
	if (htons(eth->h_proto) == ETH_P_8021Q) {
		/* If the packet is VLAN-tagged (protocol type indicates 802.1Q),
		 * extract the VLAN ID from the packet buffer. This VLAN ID will be
		 * used in xmit function.
		 */
		memcpy(&vlan_id, buf + EIP_VLAN_ID_OFST, EIP_VLAN_ID_SIZE);
		eiph.vlan_id = htons(vlan_id);
	} else {
		/* If the packet is not VLAN-tagged, set the VLAN ID to a special
		 * value (0xFFFF) indicating that the packet doesn't belong to any VLAN.
		 */
		eiph.vlan_id = EIP_VLAN_ID_NONE;
	}

	/* Attach eip_dev header to skb */
	memcpy(skb->data, &eiph, sizeof(struct eiphdr));

	/* Set destination eip-0 destination mac in skb */
	memcpy(eth->h_dest, ndev->dev_addr, 6);

	/* Increase the skb->data and skb->tail by sizeof(struct eiphdr)*/
	skb_reserve(skb, sizeof(struct eiphdr));
}

/**
 * @brief eip_send_ipstack - send packet received from eip to ipstack
 *
 * @param ndev - network interface device structure
 * @param len - length of the packet
 * @param buf - pointer to packet buffer
 * @param app_id - Indicates the destination-netdev-interface.
 * @param ctx_id - context ref id for ipsec offload
 * @param forwarded - true if the packet is forwarded from the dtl interface
 * to user interface
 * @return true - if packet is successfully sent to ipstack
 * @return false  - if packet is not able to sent to ipstack
 */
bool eip_send_ipstack(struct net_device *ndev, u8 rid, int len,
		      unsigned char *buf, int ctx_id, bool forwarded)
{
	struct sk_buff *skb;
	struct eip_priv *priv = netdev_priv(ndev);
	struct eip_public *eip_pub;
	struct rtnl_link_stats64 *net_stats = &priv->stats64;
	u16 vlan_proto = 0;

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	struct eip_ipsec *ipsec = priv->eip_pub->eip_ipsec;
#endif

	if (!buf) {
		LOG_CRIT("No data to send\n");
		goto err_exit;
	}
	/* TODO: build_skb is used only for data/user ring
	 * default ring still uses dev_alloc_skb and needs memory copy as
	 * we need to add eip_hdr in skb. This can be fixed by using alloc_pages
	 * for pkt_buff memory and use build_skb with frag_size argument
	 */
	if (rid == DEFAULT_INTERFACE_ID)
		skb = dev_alloc_skb(len + sizeof(struct eiphdr) + NET_IP_ALIGN);
	else if (forwarded)
		skb = dev_alloc_skb(len + NET_IP_ALIGN);
	else
		skb = slab_build_skb(buf);
	if (!skb)
		goto err_exit;

	if (rid == DEFAULT_INTERFACE_ID) {
		skb_reserve(skb, NET_IP_ALIGN);
		eip_pub = priv->eip_pub;
		ndev = eip_pub->ndev_list[NDEV_APP_ID_DFL - 1];
		set_eip_hdr(ndev, skb, buf, priv->app_id);
		// Extract the VLAN protocol type from the packet buffer
		memcpy(&vlan_proto, buf + EIP_VLAN_PROTO_OFST, EIP_VLAN_PROTO_SIZE);

		/* If the packet is a VLAN-tagged packet (identified by VLAN protocol type),
		 * remove the VLAN tag (VLAN ID and type) from the packet buffer.
		 * This effectively converts the VLAN-tagged packet into a standard IP packet
		 * by shifting the packet content and adjusting the packet length.
		 */
		if (htons(vlan_proto) == ETH_P_8021Q) {
			memcpy(buf + EIP_VLAN_PROTO_OFST,
			       buf + EIP_VLAN_PROTO_OFST + EIP_VLAN_FIELD_SIZE,
			       len - EIP_VLAN_PROTO_OFST - EIP_VLAN_FIELD_SIZE);
			// Adjust the length to reflect the removal of the VLAN header
			len = len - EIP_VLAN_FIELD_SIZE;
		}

		// Copy the processed packet data to the socket buffer (skb) for further processing
		memcpy(skb_put(skb, len), buf, len);
	} else if (forwarded) {
		skb_reserve(skb, NET_IP_ALIGN);
		memcpy(skb_put(skb, len), buf, len);
	} else {
		skb_put(skb, len);
	}

	skb->dev = ndev;
	skb->protocol = eth_type_trans(skb, ndev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	if (ctx_id != NON_OFFLOAD_TR_CNTXT_REF) {
		struct xfrm_state *xs = get_xfrm_state(ipsec, true, ctx_id);
		struct xfrm_offload *xo;
		struct sec_path *sp;

		if (!xs) {
			LOG_WARN("xfrm state not found\n");
			goto send_packet;
		}
		xfrm_state_hold(xs);
		sp = secpath_set(skb);
		if (!sp) {
			LOG_WARN("xfrm secuirty path not set\n");
			goto send_packet;
		}
		sp->xvec[sp->len++] = xs;
		sp->olen++;
		xo = xfrm_offload(skb);
		if (!xo) {
			LOG_DEBG("xfrm offload not set\n");
			goto send_packet;
		}
		xo->flags = CRYPTO_DONE;
		xo->status = CRYPTO_SUCCESS;
	}

send_packet:
#endif
	net_stats->rx_packets++;
	net_stats->rx_bytes += len;
	netif_receive_skb(skb);
	LOG_DEBG("Pkt sent to ipstack:%s", netdev_name(ndev));

	return true;

err_exit:
	LOG_DEBG("%s:%d Unable to send packet to IP", __func__, __LINE__);
	return false;
}

/**
 * @brief eip_prepare_rds - prepare the rd for eip usage
 *
 * @param eip_ring - eip ring interface structure
 * @param rd_cnt - numbers of rds that has to be created
 * @return int - 0 on success and negative on failure
 */
int eip_prepare_rds(struct eip_ring_interface *eip_ring, int rd_cnt)
{
	u32 i, prev_nxt_to_use, nxt_to_use;
	int err = 0;
	pec_status_t pec_res = PEC_STATUS_OK;
	unsigned int count = 0;
	pec_result_descriptor_t *res;
	u32(*output_token)[IOTOKEN_OUT_WORD_COUNT];
	/* load_handles must be linear */
	dma_buf_handle_t load_handles[MAX_RD_COUNT] = { 0 };
	int allocated = 0;
	int retry = 0;
	int to_load = 0;
	int loaded = 0;

	/* Avoid too frequent, as long as lots of dscr available! */
	eip_ring->rx_to_prep += rd_cnt;
	if (eip_ring->rx_to_prep < EIP_RD_REFILL_THRESH)
		return 0;
	rd_cnt = min(eip_ring->rx_to_prep, MAX_RD_COUNT);

	/* Be cautious not to change priv trackers, until a load is success */
	prev_nxt_to_use = eip_ring->rx_nxt_to_use;

	res = &g_res_desc[eip_ring->rid][prev_nxt_to_use];
	output_token = &g_out_token[eip_ring->rid][prev_nxt_to_use];

	nxt_to_use = prev_nxt_to_use;
	for (i = 0; i < rd_cnt; i++) {
		/* TODO: remove special treatment of Default interface */
		if (eip_ring->rid != DEFAULT_INTERFACE_ID) {
			err = eip_create_rx_buffer(eip_ring, nxt_to_use);
			if (err) {
				LOG_CRIT("error in creating the rx buffer\n");
				break;
			}
		} else if (eip_ring->rid == DEFAULT_INTERFACE_ID) {
			res[nxt_to_use].user_p = 0;
			res[i].dst_pkt_handle =
				eip_ring->rx_buff[nxt_to_use].handle;
			res[nxt_to_use].dst_pkt_byte_count =
				EIP_RX_BUF_SIZE;
			res[nxt_to_use].bypass_word_count = 0;
			res[nxt_to_use].output_token_p =
				output_token[nxt_to_use];
		}
		load_handles[allocated] = eip_ring->rx_buff[nxt_to_use].handle;
		allocated++;
		nxt_to_use++;
		if (nxt_to_use == MAX_RD_COUNT)
			nxt_to_use = 0;
	}
	if (allocated != rd_cnt)
		LOG_CRIT("Allocation issue in prep_rds\n");
	to_load = allocated;
	while (to_load) {
		/* keep default interface to sideload rd as it requires token processed
		 * and decides what to program during dtl addition.
		 */
		if (eip_ring->rid == DEFAULT_INTERFACE_ID) {
			pec_res = pec_inline_rd_put(eip_ring->rid, res, to_load,
						    &count);
			if (to_load > count)
				LOG_CRIT("Ring-0: to_load:%d, loaded:%d\n",
					 to_load, count);
		} else {
			pec_res = pec_scatter_preload(eip_ring->rid,
						      &load_handles[loaded],
						      to_load, &count);
		}
		loaded += count;
		to_load -= count;
		count = 0;
		retry++;
		if (pec_res != PEC_STATUS_OK) {
			if (retry >= 2) {
				LOG_CRIT("Ring:%d, expected(%d), allocated(%d), loaded(%d)\n",
					 eip_ring->rid, rd_cnt, allocated, loaded);
				LOG_CRIT("Try:%d PEC_STATUS:%d\n", retry, pec_res);
				break;
			}
		}
	}
	eip_ring->rx_to_prep -= loaded;
	if (eip_ring->rx_to_prep < 0) {
		LOG_CRIT("%s(%d): eip_ring->rx_to_prep:%d after load\n",
			 __func__, __LINE__, eip_ring->rx_to_prep);
		eip_ring->rx_to_prep = 0;
	}

	if (to_load) {
		err = -EAGAIN;
		/* mostly its PEC_BUSY, avoids log flood */
		if (pec_res != PEC_STATUS_BUSY)
			LOG_CRIT("prepare_rds: PEC ScatterPreload error %d\n",
				 pec_res);

		eip_ring->rx_nxt_to_use =
			(prev_nxt_to_use + loaded) % MAX_RD_COUNT;
		goto err_rd_put;
	}
	eip_ring->rx_nxt_to_use = (prev_nxt_to_use + loaded) % MAX_RD_COUNT;
	return 0;

err_rd_put:

#ifdef EIP_DEBUG_FREE_PRELOAD
	LOG_CRIT("%s(%d):Ring:%d:PECerr:%d\n", __func__, __LINE__, pec_res);
	LOG_CRIT("Values: exp(%d), alloc(%d), loaded(%d), try:%d\n", rd_cnt,
		 allocated, loaded, retry);
	LOG_CRIT("trackers: ring->rx_nxt_to_use:%d, prev_nxt_to_use:%d, nxt_to_use:%d\n",
		 eip_ring->rx_nxt_to_use, prev_nxt_to_use, nxt_to_use,
		allocated - loaded);

	int k = 0;

	for (k = 0; k < allocated; k++) {
		pr_err("%s(%d): allocated-handles:0x%X, ", __func__, __LINE__,
		       load_handles[k].p);
	}
	LOG_CRIT("\nAllocated total handles:%d\n", k);
#endif

	if (eip_ring->rid == DEFAULT_INTERFACE_ID)
		return err;
	i = 0;
	count = allocated - loaded;
	while (i < count) {
#ifdef EIP_DEBUG_FREE_PRELOAD
		pr_err("%s(%d): Freeing-handles:0x%X, ", __func__, __LINE__,
		       eip_ring->rx_buff[(prev_nxt_to_use + loaded + i) %
					 MAX_RD_COUNT]
			       .handle.p);
#endif
		dma_buf_release(eip_ring->rx_buff[(prev_nxt_to_use + loaded + i) %
					  MAX_RD_COUNT]
				.handle);
		eip_ring->rx_buff[(prev_nxt_to_use + loaded + i) % MAX_RD_COUNT]
			.handle.p = NULL;
		i++;
	}
	eip_ring->rx_nxt_to_use = prev_nxt_to_use;

	return err;
}

/**
 * @brief  Packet receiver, keeps on polling on for the packets from RD or gets
 * triggered by interrupt and once received, sends back to the ipstack or drops
 * the packet and then prepare the rd
 *
 * @param eip_ring eip ring interface structure
 * @return int
 */
int eip_rx_clean_rdr(struct eip_ring_interface *eip_ring)
{
	struct eip_public *eip_pub = eip_ring->eip_pub;
	struct eip_priv *priv;
	struct rtnl_link_stats64 *net_stats;
	struct eip_pkt_buff packet_buff;
	u32 nxt_to_clean;
	int app_id, i;
	bool non_default_pkt;
	int ctx_id = NON_OFFLOAD_TR_CNTXT_REF;
	u8 rid = eip_ring->rid;

	struct net_device *ndev;

	struct rdr_pkts_stat *pkt_get_stat;
	pec_result_descriptor_t *res;
	io_token_output_dscr_t *iotoken_output_dscr;
	u32(*output_token)[IOTOKEN_OUT_WORD_COUNT];

	pkt_get_stat = &g_pkt_get_stat[eip_ring->rid];
	res = g_res_desc[eip_ring->rid];
	iotoken_output_dscr = g_io_token_output_desc[eip_ring->rid];
	output_token = g_out_token[eip_ring->rid];

	zeroinit(g_res_desc[eip_ring->rid]);
	zeroinit(g_io_token_output_desc[eip_ring->rid]);
	zeroinit(g_out_token[eip_ring->rid]);
	zeroinit(pkt_get_stat->pkt_status);

	pkt_get_stat->exp_pkt_get_cnt = MAX_RD_COUNT;
	pkt_get_stat->interface_id = rid;

	if (da_pec_pe_get_multi(iotoken_output_dscr, output_token, res,
				pkt_get_stat) <= 0) {
		/* FIXME there are no pkts on ring and interrupt still happens */
		LOG_DEBG("No packets received on ring: %u\n", rid);
		return 0;
	}

	for (i = 0; i < pkt_get_stat->actual_pkt_get_cnt; i++) {
		nxt_to_clean = eip_ring->rx_nxt_to_clean;
		app_id = iotoken_output_dscr[i].app_id;

		if (!app_id || app_id > NDEV_APP_ID_MAC0) {
			LOG_CRIT("Invalid app_id: %u\n", app_id);
			return 0;
		}

		if (app_id == NDEV_APP_ID_MAC0)
			ndev = eip_pub->ndev_list[0];
		else
			ndev = eip_pub->ndev_list[app_id];

		priv = netdev_priv(ndev);
		net_stats = &priv->stats64;
		packet_buff = eip_ring->rx_buff[nxt_to_clean];
		nxt_to_clean++;
		if (nxt_to_clean == MAX_RD_COUNT)
			nxt_to_clean = 0;
		eip_ring->rx_nxt_to_clean = nxt_to_clean;

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
		ctx_id = pkt_get_stat->pkt_status[i].ctx_id;
#endif

		/* Check for RD-parse-error code */
		if (pkt_get_stat->pkt_status[i].token_parse_ret_code < 0 ||
		    pkt_get_stat->pkt_status[i].token_desc_error_code != 0) {
			LOG_CRIT("[Ring-%d].token: parse_ret_code:%d, desc_error_code:0x%08x\n",
				 rid,
				 pkt_get_stat->pkt_status[i].token_parse_ret_code,
				 pkt_get_stat->pkt_status[i]
				 .token_desc_error_code);
			net_stats->rx_errors++;
		} else {
#ifdef DEBUG
			LOG_DEBG("[Ring-%d appid-%d] Received pkt data: ", rid, app_id);
			print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 16, 1,
					(void *)packet_buff.host_address.p,
					res[i].dst_pkt_byte_count, true);
#endif
			/* TODO - below is temp fix for adding DTL rule for ingress pkt
			 * at ring-0. Ideally DTL rule should be added after iptables-set-mark.
			 */
			if (rid == DEFAULT_INTERFACE_ID) {
				non_default_pkt = handle_non_default_pkt(ndev,
									 packet_buff.host_address.p,
									 res[i].dst_pkt_byte_count);
				/* If pkt is not an ARP, send pkt to eip0 */
				if (!non_default_pkt) {
					/* Send all non-arp pkt to eip0-ipstack. */
					eip_send_ipstack(ndev, rid,
							 res[i].dst_pkt_byte_count,
							 (unsigned char *)packet_buff
							 .host_address.p,
							 ctx_id, false);
				}
			} else if (rid == REJECTED_FLOW_RULE_RING_ID) {
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
				if (app_id == HMAC_APP_ID) {
					LOG_DEBG("hmac key output\n");
					memcpy(eip_pub->hmac_key_offload_wait_q
					       .inner_key,
					       (unsigned char *)packet_buff
					       .host_address.p,
					       res[i].dst_pkt_byte_count / 2);
					memcpy(eip_pub->hmac_key_offload_wait_q
					       .outer_key,
					       (unsigned char *)packet_buff
					       .host_address.p +
					       res[i].dst_pkt_byte_count /
					       2,
					       res[i].dst_pkt_byte_count / 2);
					eip_pub->hmac_key_offload_wait_q
						.condition = 1;
					wake_up(&eip_pub->hmac_key_offload_wait_q
						.queue);
				}
#endif /* CONFIG_EIP_IPSEC_OFFLOAD */
			} else {
				/* For user ring send ingress pkts to ipstack */
				eip_send_ipstack(ndev, rid,
						 res[i].dst_pkt_byte_count,
						 (u8 *)packet_buff.host_address.p,
						 ctx_id, false);
			}
		}
		/* Reuse packet buff for default interface as alloc_skb is used
		 * instead of build_skb
		 */
		if (rid != DEFAULT_INTERFACE_ID) {
			/* Invalidate the dma-handle and record, so as it can be recycled by DDK */
			dma_buf_destroy(packet_buff.handle);
		}
	}

	/*prepare the rds */
	eip_prepare_rds(eip_ring, pkt_get_stat->actual_pkt_get_cnt);
	return pkt_get_stat->actual_pkt_get_cnt;
}

/**
 * @brief eip_tx_send - prepares the command descriptor and notifies the eip
 *
 * @param priv - eip private data structer
 * @param packet_data - packet data pointer
 * @param packet_length - length of packet data
 * @param sa_handles - pointer to sa struct
 * @return true on successful submission of cmd to eip
 * @return false  on failure
 */
bool eip_tx_send(struct eip_priv *priv, struct sk_buff *skb,
		 struct sa_handle *sa_handles, u8 rid)
{
	int rc;
	struct eip_public *eip_pub = priv->eip_pub;
	unsigned int count;
	u8 *packet_data = skb->data;
	u32 packet_length = skb->len;
	u8 offset, next_header;
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	struct xfrm_state *xs;
#endif
	bool xfrm_offloaded = false;
	struct rtnl_link_stats64 *net_stats = &priv->stats64;

	next_header = 6;
	offset = 14;

	/* logical command descriptor */
	pec_command_descriptor_t cmd;

	/*  command descriptor token data */
	io_token_input_dscr_t in_token_dscr;
	u32 input_token[IOTOKEN_IN_WORD_COUNT];
	void *in_token_dscr_ext_p = NULL;

	struct eip_ring_interface *eip_ring = &eip_pub->eip_ring[rid];
	u32 nxt_to_use = eip_ring->tx_nxt_to_use;
	struct eip_pkt_buff *packet_buffer = &eip_ring->tx_buff[nxt_to_use];

	nxt_to_use++;
	if (nxt_to_use == EIP_MAX_TX_PKT_BUFFERS)
		nxt_to_use = 0;
	eip_ring->tx_nxt_to_use = nxt_to_use;

	dma_buf_host_address_t pkt_host_address = packet_buffer->host_address;
	dma_buf_handle_t pkt_handle = packet_buffer->handle;

#ifdef DA_PEC_IOTOKEN_EXT
	io_token_input_dscr_ext_t in_token_dscr_ext;

	zeroinit(in_token_dscr_ext);
	in_token_dscr_ext_p = &in_token_dscr_ext;
#endif

	zeroinit(cmd);
	zeroinit(in_token_dscr);
	zeroinit(input_token);

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	/* TODO:
	 * One way of avoiding this memcopy
	 * 1. Get dma address of skb->buff
	 * 2. Program EIP CD with dma address of skb->buff
	 * 3. Register with EIP interrupt on CD process by EIP and then release the skb
	 */
	if (xfrm_offload(skb)) {
		LOG_DEBG("offloaded\n");
		xs = xfrm_input_state(skb);
		if (unlikely(!xs)) {
			LOG_WARN("no xs state\n");
			net_stats->tx_errors++;
			goto error_exit;
		} else {
			xfrm_offloaded = true;
			if (xs->xso.type == XFRM_DEV_OFFLOAD_CRYPTO) {
				u16 padding_bytes;
				u32 new_packet_length;
				u32 new_packet_payload;

				padding_bytes =
					packet_data[packet_length -
						    AES_SHA1_PAD_LENGTH_OFFSET];

				new_packet_length = packet_length -
						    AES_SHA1_PAD_LENGTH_OFFSET -
						    padding_bytes -
						    ESP_IPHDR_SKIP_BYTES;
				new_packet_payload =
					new_packet_length - ETH_HLEN;

				packet_data[16] =
					(new_packet_payload & 0xFF00) >> 8;
				packet_data[17] = (new_packet_payload & 0x00FF);

				/* ETH_IPHDR_COPY_BYTES will change if ip hdr has options feild set
				 * use ip hdr length to calculate this
				 */
				memcpy(pkt_host_address.p, packet_data,
				       ETH_IPHDR_COPY_BYTES);
				/* skip copying ESP_IPHDR_SKIP_BYTES and padding bytes to new pkt */
				memcpy(pkt_host_address.p +
					       ETH_IPHDR_COPY_BYTES,
				       packet_data + ETH_IPHDR_COPY_BYTES +
					       ESP_IPHDR_SKIP_BYTES,
				       new_packet_length -
					       ETH_IPHDR_COPY_BYTES);
				packet_length = new_packet_length;
				da_pec_pcl_add_checksum((u8 *)pkt_host_address.p + ETH_HLEN);
			} else {
				/* TODO: secpath is set for XFRM_DEV_OFFLOAD_PACKET as a patch
				 * So resetting before freeing the skb
				 * This step might not be needed
				 */
				secpath_reset(skb);
				memcpy(pkt_host_address.p, packet_data,
				       packet_length);
			}

			if (xs->props.mode == XFRM_MODE_TUNNEL)
				next_header = 0;
		}
	}
#endif

	/* 0x20 is required to send out ARP pkt. However the same also
	 * works for Non-ARP pkts.
	 */
	in_token_dscr_ext.hw_services = 0x20;
	cmd.token_handle = dma_buf_null_handle;
	cmd.src_pkt_handle = pkt_handle;
	cmd.src_pkt_byte_count = packet_length;
	cmd.bypass_word_count = 0;
	cmd.sa_handle1 = sa_handles->sa_handle_out;
	cmd.sa_handle2 = dma_buf_null_handle;
	cmd.sa_word_count = 128;

#ifdef DA_PEC_IOTOKEN_EXT_SRV_ICE
	in_token_dscr_ext.next_header = next_header;
	in_token_dscr_ext.offset_byte_count = offset;
#endif

	if (xfrm_offloaded) {
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
		LOG_DEBG("xfrm offloaded %d\n", xs->xso.offload_handle);
		in_token_dscr_ext.hw_services = IOTOKEN_CMD_PKT_IIP;
		cmd.sa_handle1 =
			priv->eip_pub->eip_ipsec
				->tx_ipsec_handle[xs->xso.offload_handle]
				.handle;
#endif
	} else { /* memcpy is done as part of it */
		memcpy(pkt_host_address.p, packet_data, packet_length);
	}

	LOG_DEBG("process outbound packet of size %d (input packet)\n",
		 packet_length);

#ifdef DEFAULT_IPSEC
	in_token_dscr_ext.hw_services = IOTOKEN_CMD_PKT_IIP;

#endif

	in_token_dscr.app_id = priv->app_id;
	LOG_DEBG("[Ring: %d] sending eip cmd with pkt of size %d, app_id = %d",
		 rid, packet_length, in_token_dscr.app_id);

	if (!da_pec_iotoken_create(&in_token_dscr, in_token_dscr_ext_p,
				   input_token, &cmd)) {
		net_stats->tx_errors++;
		goto error_exit;
	}

	rc = pec_packet_put(rid, &cmd, 1, &count);
	if (rc == PEC_NO_FREE_SLOTS) {
		net_stats->tx_dropped++;
		priv->dropped_no_free_slots++;
	net_stats->tx_errors++;
		if (net_ratelimit())
			LOG_CRIT("pec_packet_put No-free-slot\n");
	} else if (rc == PEC_STATUS_BUSY) {
		/* Device busy, so packet will be retried */
		net_stats->tx_dropped++;
		goto error_exit;
	} else {
		if (rc != PEC_STATUS_OK && count != 1) {
			LOG_CRIT("pec_packet_put error: %d\n", rc);
			net_stats->tx_errors++;
			goto error_exit;
		}
	}

	net_stats->tx_packets++;
	net_stats->tx_bytes += packet_length;

	return true;

error_exit:
	return false;
}
