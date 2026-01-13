// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
#include <net/xfrm.h>
#include <linux/netdevice.h>
#include "log.h"
#include "dtl.h"
#include "eip.h"

#define EIP_ESP_FEATURES (NETIF_F_HW_ESP)

static const struct xfrmdev_ops eip_xfrmdev_ops = {
	.xdo_dev_state_add = eip_ipsec_add_sa,
	.xdo_dev_state_delete = eip_ipsec_del_sa,
	.xdo_dev_offload_ok = eip_ipsec_offload_ok,
	.xdo_dev_policy_add = eip_ipsec_add_policy,
	.xdo_dev_policy_delete = eip_ipsec_del_policy,
	.xdo_dev_policy_free = eip_ipsec_free_policy,

};

/**
 * @brief Get the xfrm state object
 *
 * @param ipsec - ipsec handle
 * @param rx - true if it is rx ipsec handle
 * @param ctx_id - index for the sa in the table
 * @return struct xfrm_state*
 */
struct xfrm_state *get_xfrm_state(struct eip_ipsec *ipsec, bool rx, int ctx_id)
{
	if (rx) {
		if (ctx_id > MAX_RX_IPSEC_SA)
			return NULL;
		if (ipsec->rx_ipsec_handle[ctx_id].used)
			return ipsec->rx_ipsec_handle[ctx_id].xs;
	} else {
		if (ctx_id > MAX_TX_IPSEC_SA)
			return NULL;
		if (ipsec->tx_ipsec_handle[ctx_id].used)
			return ipsec->tx_ipsec_handle[ctx_id].xs;
	}
	return NULL;
}

/**
 * @brief find the free sa handle for rx and tx
 *
 * @param ipsec - eip ipsec offload
 * @param rx - true if it for rx sa otherwise false
 * @return int - zero if success otherwise negative value
 */
static int eip_ipsec_find_empty_idx(struct eip_ipsec *ipsec, bool rx)
{
	u32 i;

	if (rx) {
		if (ipsec->rx_ipsec_sa_cnt == MAX_RX_IPSEC_SA)
			return -ENOSPC;

		/* search rx sa table */
		for (i = 0; i < MAX_RX_IPSEC_SA; i++) {
			if (!ipsec->rx_ipsec_handle[i].used)
				return i;
		}
	} else {
		if (ipsec->tx_ipsec_sa_cnt == MAX_TX_IPSEC_SA)
			return -ENOSPC;

		/* search tx sa table */
		for (i = 0; i < MAX_TX_IPSEC_SA; i++) {
			if (!ipsec->tx_ipsec_handle[i].used)
				return i;
		}
	}

	return -ENOSPC;
}

/**
 * @brief eip_xfrm_update_curlft - callback function to update
 * the sa traffic stats
 *
 * @param x - pointer to transform state struct
 */
static void eip_xfrm_update_curlft(struct xfrm_state *x)
{
	LOG_DEBG("%s xfrm update\n", __func__);
}

/**
 * @brief eip_ipsec_add_policy - callback function to add spd
 *
 * @param x - pointer to transform policy
 * @return int
 */
static int eip_ipsec_add_policy(struct xfrm_policy *x,
				struct netlink_ext_ack *extack)
{
	LOG_DEBG("eip add policy\n");
	return 0;
}

/**
 * @brief eip_ipsec_del_policy - callback function to delete spd
 *
 * @param x - pointer to transform policy
 */
static void eip_ipsec_del_policy(struct xfrm_policy *x)
{
	LOG_DEBG("%s ipsec del policy\n", __func__);
}

/**
 * @brief eip_ipsec_free_policy - callback function to free spd
 *
 * @param x - pointer to transform policy
 */
static void eip_ipsec_free_policy(struct xfrm_policy *x)
{
	LOG_DEBG("%s ipsec free policy\n", __func__);
}

/**
 * @brief eip_ipsec_add_sa - program device with a security association
 *
 * @param xs - pointer to transform state struct
 * @return int - 0 on success and negative value on error
 */
static int eip_ipsec_add_sa(struct xfrm_state *xs,
			    struct netlink_ext_ack *extack)
{
	struct net_device *dev = xs->xso.dev;
	struct eip_priv *priv = netdev_priv(dev);
	struct eip_ipsec *ipsec = priv->eip_pub->eip_ipsec;
	u32 sa_idx;
	bool status = false;

	if (xs->id.proto != IPPROTO_ESP) {
		LOG_WARN("Unsupported protocol 0x%04x for ipsec offload\n",
			 xs->id.proto);
		return -EINVAL;
	}

	if (xs->props.mode != XFRM_MODE_TRANSPORT &&
	    xs->props.mode != XFRM_MODE_TUNNEL) {
		LOG_WARN("Unsupported mode for ipsec offload\n");
		return -EINVAL;
	}

	if (xs->xso.type != XFRM_DEV_OFFLOAD_PACKET) {
		LOG_WARN("Unsupported mode for ipsec offload\n");
		return -EINVAL;
	}

	if (xs->calg) {
		LOG_WARN("Compression offload not supported\n");
		return -EINVAL;
	}

	if (xs->aead) {
		LOG_WARN("AEAD offload not supported\n");
		return -EINVAL;
	}

	if (!xs->ealg)
		return -EINVAL;

	if (!xs->aalg)
		return -EINVAL;

	if (xs->xso.dir == XFRM_DEV_OFFLOAD_IN) {
		sa_idx = eip_ipsec_find_empty_idx(ipsec, true);
		if (sa_idx < 0)
			return sa_idx;
		struct ipsec_handle *sa = &ipsec->rx_ipsec_handle[sa_idx];

		status = eip_create_ipsec_sa(false, xs, priv, sa_idx, true, sa);
		if (!status)
			return -EINVAL;
		xs->xso.offload_handle = sa_idx;
		submit_dtl_rule_ipsec(xs->id, sa->handle);
		sa->xs = xs;
		sa->used = true;
		ipsec->rx_ipsec_sa_cnt++;
		LOG_DEBG("rx xfrm added %d\n", sa_idx);
	} else {
		sa_idx = eip_ipsec_find_empty_idx(ipsec, false);
		if (sa_idx < 0)
			return sa_idx;
		struct ipsec_handle *sa = &ipsec->tx_ipsec_handle[sa_idx];

		status =
			eip_create_ipsec_sa(false, xs, priv, sa_idx, false, sa);
		if (!status)
			return -EINVAL;
		xs->xso.offload_handle = sa_idx;
		sa->xs = xs;
		sa->used = true;
		ipsec->tx_ipsec_sa_cnt++;
		LOG_DEBG("tx xfrm added %d\n", sa_idx);
	}
	return 0;
}

/**
 * @brief eip_ipsec_del_sa - program device to delete security association
 *
 * @param xs - pointer to transform state struct
 */
static void eip_ipsec_del_sa(struct xfrm_state *xs)
{
	struct net_device *dev = xs->xso.dev;
	struct eip_priv *priv = netdev_priv(dev);
	struct eip_ipsec *ipsec = priv->eip_pub->eip_ipsec;
	struct ipsec_handle *ipsec_handle;
	u32 sa_idx = xs->xso.offload_handle;

	LOG_DEBG("Deleting xfrm %d\n", sa_idx);
	if (xs->xso.dir == XFRM_DEV_OFFLOAD_IN) {
		ipsec_handle = &ipsec->rx_ipsec_handle[sa_idx];
		pcl_dtl_transform_remove(0, 0, ipsec_handle->handle);
		ipsec->rx_ipsec_sa_cnt--;

	} else {
		ipsec_handle = &ipsec->tx_ipsec_handle[sa_idx];
		ipsec->tx_ipsec_sa_cnt--;
	}

	pcl_transform_un_register(ipsec_handle->handle);
	dma_buf_release(ipsec_handle->handle);
	ipsec_handle->used = false;
}

/**
 * @brief eip_ipsec_offload_ok - can this packet use the xfrm hw offload
 *
 * @param skb : socket buffer with packet pointer
 * @param xs - pointer to transform state struct
 * @return true - if device is capable of offloading the packet
 * @return false - if device is not capable of offloading the packet
 */
static bool eip_ipsec_offload_ok(struct sk_buff *skb, struct xfrm_state *xs)
{
	/* TODO: validate the packets*/
	return true;
}

/**
 * @brief eip_ipsec_init - initalises the device with xfrm callbacks
 *
 * @param dev - network device struct
 */
void eip_ipsec_init(struct net_device *dev)
{
	dev->xfrmdev_ops = &eip_xfrmdev_ops;
	dev->features |= EIP_ESP_FEATURES;
	dev->hw_enc_features |= EIP_ESP_FEATURES;
}
#endif
