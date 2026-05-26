/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021-2025 Axiado Corporation (or its affiliates). All rights reserved.
 *
 * HCP (Header and Crypto Processing Engine) - Internal shared structures
 *
 * This header defines the unified data structures for the consolidated
 * HCP platform driver that integrates SHIM, MAC, PHY, and EIP-197 subsystems.
 */

#ifndef _AXIADO_HCP_H_
#define _AXIADO_HCP_H_

#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/clk.h>
#include "shim/shim_eip_common.h"

/* Maximum counts for HCP subsystems */
#define MAX_NETDEV_CNT (MAX_MAC_CNT + 2)

/* Forward declarations */
struct shim_mem_admin;
struct shim_priv;
struct eip_priv;
struct net_device;

/**
 * struct hcp_device - Main HCP device structure
 * @dev: Pointer to platform device
 * @pdev: Platform device pointer
 *
 * Memory regions:
 * @eip197_base: Virtual base address for EIP-197 crypto engine
 * @shim_base: Virtual base address for SHIM registers
 * @phy_csr_base: Virtual base address for PHY CSR
 * @eip197_phys: Physical address of EIP-197
 * @eip197_size: Size of EIP-197 memory region
 *
 * Interrupts:
 * @eip197_irq: EIP-197 crypto engine IRQ
 * @ring_irqs: Ring interrupts [0-3]
 * @mac_irqs: MAC interrupts [0-4] for 5 Ethernet MACs
 *
 * Subsystem private data:
 * @shim: SHIM subsystem private data
 * @eip: EIP subsystem private data
 *
 * Clocks:
 * @eip197_clk: EIP-197 clock (optional)
 *
 * MDIO bus:
 * @mdio_bus: MDIO bus for PHY management
 *
 * MAC configuration:
 * @mac_cfg: MAC/PHY configuration array from SHIM subsystem
 *
 * Network devices:
 * @netdevs: Array of network device pointers (up to 7: 5 MACs + DTL + DFL)
 * @num_netdevs: Number of registered network devices
 */
struct hcp_device {
	/* Device references */
	struct device *dev;
	struct platform_device *pdev;

	/* Memory regions */
	void __iomem *eip197_base;
	void __iomem *shim_base;
	void __iomem *phy_csr_base;
	phys_addr_t eip197_phys;
	resource_size_t eip197_size;
	resource_size_t shim_size;

	/* Interrupts */
	int eip197_irq;
	int ring_irqs[RING_INTERFACE_CNT];
	int mac_irqs[MAX_MAC_CNT];

	/* Subsystem private data pointers */
	void *shim_priv;
	void *eip_priv;
	void *hfifo_priv;

	/* Clocks */
	struct clk *eip197_clk;

	/* MDIO bus */
	struct mii_bus *mdio_bus;

	/* Network devices */
	struct net_device *netdevs[MAX_NETDEV_CNT];
	int num_netdevs;

	/* true for Host FIFO mode, false for EIP mode */
	bool hfifo_mode;
};

/**
 * struct hfifo_priv - Host FIFO private structure
 * @data: Host FIFO private data pointer
 * @mac_idx: MAC port index selected for Host FIFO
 */
struct hfifo_priv {
	void *data;
	u8 mac_idx;
};

/* Helper macro to access MAC configuration through shim_priv.
 * MAC configuration is part of shim subsystem's private data.
 */
#define hcp_get_mac_cfg(hcp) \
	(((struct shim_mem_admin *)((hcp)->shim_priv))->mac_cfg)

/* Subsystem initialization/cleanup functions */

/**
 * shim_subsystem_init - Initialize SHIM subsystem
 * @hcp: HCP device structure
 *
 * Initializes SHIM hardware, MACs, PHY CSR, and MDIO bus.
 * This must be called before EIP initialization.
 *
 * Return: 0 on success, negative error code on failure
 */
int shim_subsystem_init(struct hcp_device *hcp);

/**
 * shim_mac_soft_reset - Soft reset the particular MAC
 * @mac_idx: MAC index for reset
 */
void shim_mac_soft_reset(u8 mac_idx);

/**
 * shim_subsystem_exit - Cleanup SHIM subsystem
 * @hcp: HCP device structure
 */
void shim_subsystem_exit(struct hcp_device *hcp);

/**
 * eip_subsystem_init - Initialize EIP subsystem
 * @hcp: HCP device structure
 *
 * Initializes EIP-197 hardware, rings, crypto, and network devices.
 * SHIM must be initialized before calling this.
 *
 * Return: 0 on success, negative error code on failure
 */
int eip_subsystem_init(struct hcp_device *hcp);

/**
 * eip_subsystem_exit - Cleanup EIP subsystem
 * @hcp: HCP device structure
 */
void eip_subsystem_exit(struct hcp_device *hcp);
/**
 * hfifo_subsystem_init - Initialize Host FIFO subsystem
 * @hcp: HCP device structure
 *
 * Initializes the Host FIFO and network divice.
 *
 * Return: 0 on success, negative error code on failure
 */
int hfifo_subsystem_init(struct hcp_device *hcp);
/**
 * hfifo_subsystem_exit - Cleanup Host FIFO subsystem
 * @hcp: HCP device structure
 */
void hfifo_subsystem_exit(struct hcp_device *hcp);

#endif /* _AXIADO_HCP_H_ */
