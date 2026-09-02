/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021-2025 Axiado Corporation (or its affiliates). All rights reserved.
 *
 * HCP (Header and Crypto Processing Engine) - Internal shared structures
 *
 * This header defines the unified data structures for the consolidated
 * HCP platform driver Host FIFO subsystem.
 */

#ifndef _AXIADO_HFIFO_H_
#define _AXIADO_HFIFO_H_

#include <linux/netdevice.h>
#include "hcp.h"

struct hfifo_data {
	/* HCP structure */
	struct hcp_device *hcp;

	/* net device registered with system */
	struct net_device *ndev;

	/* NAPI for draining the Host FIFO RX packets */
	struct napi_struct napi_hfifo_rx;

	/* IRQ line corresponding to selected MAC-port */
	int rx_irq;

	/* hfifo selected MAC */
	u8 mac_idx;

	/* last link state */
	bool link;

	/* interface stats */
	struct rtnl_link_stats64 stats64;
};

/* MAC helpers used by the Host FIFO netdev are declared in shim_common.h */
#endif /* _AXIADO_HFIFO_H_ */
