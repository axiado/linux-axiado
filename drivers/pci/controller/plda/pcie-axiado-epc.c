// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2023-2026 Axiado Corporation.
 *
 * Axiado PLDA PCIe Endpoint Controller (EPC) Driver
 *
 * Converts the PLDA PCIe IP endpoint mode to the standard
 * pci_epc framework.  VGA/Ethernet class code and BAR layout
 * are configured inline — no separate EPF driver needed.
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/pci-epc.h>
#include <linux/pci-epf.h>
#include <linux/pci_regs.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#include "pcie-ax3000.h"

#define EP_LINK_POLL_MS		500	/* fast poll until initial link-up */
#define EP_LINK_POLL_UP_MS	10000	/* slow poll once link is up */
#define MSI_TRG_SHIFT		5

/* ----------------------------------------------------------------
 * pci_epc_ops: hardware-level operations on the PLDA bridge
 * ----------------------------------------------------------------
 */

static int axiado_epc_write_header(struct pci_epc *epc, u8 fn, u8 vfn,
				   struct pci_epf_header *hdr)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	u32 ids;

	writeb_relaxed(hdr->cache_line_size, pcie->cfg + PCI_CACHE_LINE_SIZE);
	writeb_relaxed(hdr->interrupt_pin, pcie->cfg + PCI_INTERRUPT_PIN);

	/*
	 * Program bridge PCIE_PCI_IDS registers (0x98–0xA3).
	 * These are what the IP presents on the PCIe link.
	 */
	writel_relaxed((u32)hdr->deviceid << 16 | hdr->vendorid,
		       pcie->bridge + PCIE_PCI_IDS_DW0);

	ids = readl_relaxed(pcie->bridge + PCIE_PCI_IDS_DW1);
	ids &= REVISION_ID_MASK;
	ids |= (hdr->baseclass_code << 24) |
	       (hdr->subclass_code << 16) |
	       (hdr->progif_code << 8);
	writel_relaxed(ids, pcie->bridge + PCIE_PCI_IDS_DW1);

	writel_relaxed((u32)hdr->subsys_id << 16 | hdr->subsys_vendor_id,
		       pcie->bridge + PCIE_PCI_IDS_DW2);

	dev_dbg(pcie->dev,
		"EPC: write_header PF vid=0x%x did=0x%x class=%02x:%02x:%02x subsys=0x%x:0x%x\n",
		hdr->vendorid, hdr->deviceid, hdr->baseclass_code,
		hdr->subclass_code, hdr->progif_code,
		hdr->subsys_vendor_id, hdr->subsys_id);

	return 0;
}

static u32 axiado_bar_type_from_flags(unsigned long flags)
{
	u32 bar_type = 0;

	if (flags & PCI_BASE_ADDRESS_MEM_TYPE_64)
		bar_type |= PCI_BASE_ADDRESS_MEM_TYPE_64;
	if (flags & PCI_BASE_ADDRESS_MEM_PREFETCH)
		bar_type |= PCI_BASE_ADDRESS_MEM_PREFETCH;

	return bar_type;
}

/*
 * DTS ranges carry IORESOURCE_* bits, while epf_bar.flags and the bridge BAR
 * sizing registers use PCI_BASE_ADDRESS_*. The two never overlap, so passing
 * range.flags through unconverted leaves the type bits zero and makes every
 * BAR advertise as 32-bit non-prefetchable.
 */
static u32 axiado_bar_flags_from_range(unsigned long of_flags)
{
	u32 flags = PCI_BASE_ADDRESS_SPACE_MEMORY;

	if (of_flags & IORESOURCE_MEM_64)
		flags |= PCI_BASE_ADDRESS_MEM_TYPE_64;
	if (of_flags & IORESOURCE_PREFETCH)
		flags |= PCI_BASE_ADDRESS_MEM_PREFETCH;

	return flags;
}


/* Program one P2A ATR table entry. Caller holds atr_lock. */
static void axiado_epc_write_atr(struct axiado_pcie *pcie, int table_idx,
				 phys_addr_t addr, size_t size)
{
	u32 atr_base = PCIE_ATR_PCIE_WIN0 + table_idx * PCIE_ATR_TABLE_OFFSET;

	writel_relaxed(((ilog2(size) - 1) << PCIE_ATR_SRC_WIN_SIZE_SHIFT) | 1,
		       pcie->bridge + atr_base + PCIE_ATR_SRC_ADDR_LOW);
	writel_relaxed(0, pcie->bridge + atr_base + PCIE_ATR_SRC_ADDR_HIGH);
	writel_relaxed((u32)addr & PCIE_ATR_TRSL_ADDR_MASK,
		       pcie->bridge + atr_base + PCIE_ATR_TRSL_ADDR_LOW);
	writel_relaxed(upper_32_bits(addr),
		       pcie->bridge + atr_base + PCIE_ATR_TRSL_ADDR_HIGH);
	writel_relaxed(PCIE_ATR_TRSLID_AXI4_MASTER_0,
		       pcie->bridge + atr_base + PCIE_ATR_TRSL_PARAM);

	set_bit(table_idx, pcie->atr_table_map);
	pcie->atr_table_addr[table_idx] = addr;
	//All are 64 Bit
	table_idx++;
	set_bit(table_idx, pcie->atr_table_map);
}


/*
 * Program one BAR's sizing register directly, for use while the IP is still
 * held in reset and the pci_epc plumbing is not usable yet.
 */
static void axiado_epc_program_option_rom(struct axiado_pcie *pcie, u64 size, phys_addr_t addr)
{
	u32 bar_val;

	/*Set 64bit prefetch and size for bridge MEM_PREFECT 0x8 is the correct offset in csr*/
	bar_val = (~(size - 1) & PCI_BASE_ADDRESS_MEM_MASK) | PCI_BASE_ADDRESS_MEM_PREFETCH;
	writel(bar_val, pcie->bridge + PCIE_BAR_EXP_ROM_OFFSET);
	axiado_epc_write_atr(pcie, 0x6,  addr, size);

}

static int axiado_epc_set_bar(struct pci_epc *epc, u8 fn, u8 vfn,
			      struct pci_epf_bar *epf_bar)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	enum pci_barno bar = epf_bar->barno;
	u64 addr = epf_bar->phys_addr;
	u32 reg_offset, bar_val;
	int table_idx;

	switch (bar) {
	case BAR_0:
		reg_offset = PCIE_BAR_01_OFFSET;
		break;
	case BAR_2:
		reg_offset = PCIE_BAR_23_OFFSET;
		break;
	default:
		return -EINVAL;
	}

	bar_val = (~(epf_bar->size - 1) & PCI_BASE_ADDRESS_MEM_MASK) |
		  axiado_bar_type_from_flags(epf_bar->flags);
	writel_relaxed(bar_val, pcie->bridge + reg_offset);

	/*
	 * AX3005 AXI address space is 32-bit, so BAR targets are always
	 * below 4GB. Set upper BAR to all-ones for correct host sizing.
	 */
	if (epf_bar->flags & PCI_BASE_ADDRESS_MEM_TYPE_64) {
		writel_relaxed(U32_MAX, pcie->bridge + reg_offset + 4);
	}

	mutex_lock(&pcie->atr_lock);

	table_idx = find_first_zero_bit(pcie->atr_table_map,
					PCIE_ATR_MAX_TABLE_NUM);
	if ((table_idx + 1) >= PCIE_ATR_MAX_TABLE_NUM) {
		
		dev_info(pcie->dev, "EPC: set_bar MAX_TABLE BAR%d phy_addr=0x%llx\n",
			 table_idx, (u64)addr);

	}

	axiado_epc_write_atr(pcie, bar, addr, epf_bar->size);

	mutex_unlock(&pcie->atr_lock);

	dev_dbg(pcie->dev, "EPC: set_bar BAR%d size=0x%zx addr=0x%llx reg=0x%x[0x%x]\n",
		bar, epf_bar->size, (u64)addr, reg_offset, bar_val);
	return 0;
}

static void axiado_epc_clear_bar(struct pci_epc *epc, u8 fn, u8 vfn,
				 struct pci_epf_bar *epf_bar)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	u32 reg_offset;

	switch (epf_bar->barno) {
	case BAR_0:
		reg_offset = PCIE_BAR_01_OFFSET;
		break;
	case BAR_2:
		reg_offset = PCIE_BAR_23_OFFSET;
		break;
	default:
		return;
	}

	writel_relaxed(0, pcie->bridge + reg_offset);
	writel_relaxed(0, pcie->bridge + reg_offset + 4);

	dev_dbg(pcie->dev, "EPC: clear_bar PF BAR%d reg=0x%x\n",
		epf_bar->barno, reg_offset);
}

static int axiado_epc_map_addr(struct pci_epc *epc, u8 fn, u8 vfn,
			       phys_addr_t addr, u64 pci_addr, size_t size)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	int table_idx;

	mutex_lock(&pcie->atr_lock);

	table_idx = find_first_zero_bit(pcie->atr_table_map,
					PCIE_ATR_MAX_TABLE_NUM);
	if ((table_idx + 1) >= PCIE_ATR_MAX_TABLE_NUM) {
		mutex_unlock(&pcie->atr_lock);
		return -ENOMEM;
	}

	axiado_epc_write_atr(pcie, table_idx, addr, size);

	mutex_unlock(&pcie->atr_lock);

	dev_dbg(pcie->dev, "EPC: map_addr tab%d phys=0x%llx pci=0x%llx size=0x%zx\n",
		table_idx, (u64)addr, pci_addr, size);
	return 0;
}

static void axiado_epc_unmap_addr(struct pci_epc *epc, u8 fn, u8 vfn,
				  phys_addr_t addr)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	u32 atr_base;
	int i;

	mutex_lock(&pcie->atr_lock);

	for (i = 0; i < PCIE_ATR_MAX_TABLE_NUM; i++) {
		if (!test_bit(i, pcie->atr_table_map))
			continue;
		if (pcie->atr_table_addr[i] != addr)
			continue;

		atr_base = PCIE_ATR_PCIE_WIN0 + i * PCIE_ATR_TABLE_OFFSET;
		writel_relaxed(0, pcie->bridge + atr_base + PCIE_ATR_SRC_ADDR_LOW);
		clear_bit(i, pcie->atr_table_map);
		clear_bit(i+1, pcie->atr_table_map);
		pcie->atr_table_addr[i] = 0;
		mutex_unlock(&pcie->atr_lock);
		dev_dbg(pcie->dev, "EPC: unmap_addr PF tab%d phys=0x%llx\n",
			i, (u64)addr);
		return;
	}

	mutex_unlock(&pcie->atr_lock);
	dev_dbg(pcie->dev, "EPC: unmap_addr PF phys=0x%llx not found\n",
		(u64)addr);
}

static int axiado_epc_raise_msi_irq(struct pci_epc *epc, u8 fn, u8 vfn,
				    u8 interrupt_num)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	u32 msi_trg, temp;
	u32 ctrl_status_off;

	if (interrupt_num < 1) {
		dev_err(pcie->dev, "invalid MSI vector %d (must be >= 1)\n",
			interrupt_num);
		return -EINVAL;
	}

	/* EPC framework passes 1-based interrupt_num; convert to 0-based */
	interrupt_num -= 1;

	if (interrupt_num >= pcie->num_msi_vectors) {
		dev_err(pcie->dev, "invalid MSI vector %d (max %d)\n",
			interrupt_num, pcie->num_msi_vectors - 1);
		return -EINVAL;
	}

	msi_trg = 1 << (interrupt_num + MSI_TRG_SHIFT);

	ctrl_status_off = pcie->pcie_x1 ?
		REG_PCIE_X1_PF_CTRL_STATUS_ADRS_OFFSET :
		REG_PCIE_X2_PF_CTRL_STATUS_ADRS_OFFSET;

	/*
	 * Re-arm: the previous MSI leaves its pending bit set in ISTATUS_HOST
	 * and the IP only emits a new message on a fresh 0->1 transition of
	 * that bit. Clear the pending bit(s) first (write-1-to-clear) so the
	 * trigger below produces a new edge; without this only the first MSI
	 * is ever delivered.
	 */
	temp = readl(pcie->bridge + REG_PCIE_ISTATUS_HOST_ADRS_OFFSET);
	if (temp)
		writel(temp, pcie->bridge + REG_PCIE_ISTATUS_HOST_ADRS_OFFSET);

	/* Trigger the MSI request (write-only pulse in PF_CTRL_STATUS) */
	writel(msi_trg, pcie->ext + ctrl_status_off);
	dev_dbg(pcie->dev, "PF MSI vector %d trg=0x%x off=0x%x\n",
		interrupt_num, msi_trg, ctrl_status_off);
	return 0;
}

static int axiado_epc_get_msi(struct pci_epc *epc, u8 fn, u8 vfn)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	u32 flags;

	if (!pcie->msi_cap)
		return -EINVAL;

	flags = readl_relaxed(pcie->cfg + pcie->msi_cap);
	if (!(flags & (PCI_MSI_FLAGS_ENABLE << 0x10))) {
		dev_dbg(pcie->dev, "EPC: get_msi fn%d vfn%d — MSI not enabled (flags=0x%x)\n",
			fn, vfn, flags);
		return -EINVAL;
	}

	dev_dbg(pcie->dev, "EPC: get_msi fn%d vfn%d flags=0x%x MME=%d\n",
		fn, vfn, flags,
		1 << FIELD_GET(PCI_MSI_FLAGS_QSIZE << 0x10, flags));

	/* Return host-granted vector count (MME field, written by host) */
	return 1 << FIELD_GET((PCI_MSI_FLAGS_QSIZE << 0x10), flags);
}

static int axiado_epc_set_msi(struct pci_epc *epc, u8 fn, u8 vfn,
			      u8 nr_irqs)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	u32 val;
	u32 flags;
	u8 mmc = order_base_2(nr_irqs);

	/*
	 * Program the bridge capability register (PCIE_PCI_IRQ_DW0):
	 *  - Clear PF_MSI_CAP_DISABLE (bit 3) to expose MSI capability
	 *  - Clear MSIX_CAP (bit 31) to select MSI over MSI-X
	 *  - Set NUM_MSI_MSGS (bits [6:4]) to advertised vector count
	 */
	val = readl(pcie->bridge + PCIE_PCI_IRQ_DW0);
	val &= ~PF_MSI_CAP_DISABLE;
	val &= ~MSIX_CAP_MASK;
	val &= ~NUM_MSI_MSGS_MASK;
	val |= (mmc << NUM_MSI_MSGS_SHIFT) & NUM_MSI_MSGS_MASK;
	writel(val, pcie->bridge + PCIE_PCI_IRQ_DW0);

	/* Also set MMC in config space mirror so host sees it directly */
	if (pcie->msi_cap) {
		flags = readl(pcie->cfg + pcie->msi_cap);
		flags &= ~(PCI_MSI_FLAGS_QMASK << 0x10); 
		flags |= FIELD_PREP((PCI_MSI_FLAGS_QMASK<< 0x10), mmc);
		writel(flags, pcie->cfg + pcie->msi_cap);
	}

	dev_dbg(pcie->dev, "EPC: set_msi fn%d vfn%d nr_irqs=%d MMC=%d IRQ_DW0=0x%x\n",
		fn, vfn, nr_irqs, mmc, val);

	return 0;
}

static int axiado_epc_raise_irq(struct pci_epc *epc, u8 fn, u8 vfn,
				unsigned int type, u16 interrupt_num)
{
	switch (type) {
	case PCI_IRQ_MSI:
		return axiado_epc_raise_msi_irq(epc, fn, vfn, interrupt_num);
	default:
		dev_err(&epc->dev, "unsupported IRQ type %d\n", type);
		return -EINVAL;
	}
}

static int axiado_epc_start(struct pci_epc *epc)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	u32 temp;

	/* Enable LTSSM — clear DISABLE_LTSSM (bit 2) in CFGCTRL */
	temp = readl(pcie->bridge + REG_PCIE_PCIE_CFGCTRL_ADRS_OFFSET);
	dev_info(pcie->dev, "LTSSM enable: CFGCTRL 0x%x -> 0x%x\n",
		 temp, temp & PCIE_PHY_CONF_CTRL_LTSSM);
	writel(temp & PCIE_PHY_CONF_CTRL_LTSSM,
	       pcie->bridge + REG_PCIE_PCIE_CFGCTRL_ADRS_OFFSET);

	/* Start EP link monitor */
	pcie->ep_link_up = false;
	schedule_delayed_work(&pcie->ep_link_work,
			      msecs_to_jiffies(EP_LINK_POLL_MS));
	dev_info(pcie->dev, "EP link monitor started (poll %dms)\n",
		 EP_LINK_POLL_MS);

	return 0;
}

static void axiado_epc_stop(struct pci_epc *epc)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);
	u32 atr_base;
	int i;

	dev_info(pcie->dev, "EPC: stop — tearing down ATR tables\n");

	cancel_delayed_work_sync(&pcie->ep_link_work);
	pcie->ep_link_up = false;

	mutex_lock(&pcie->atr_lock);
	for (i = 0; i < PCIE_ATR_MAX_TABLE_NUM; i++) {
		if (test_bit(i, pcie->atr_table_map)) {
			atr_base = PCIE_ATR_PCIE_WIN0 +
				   i * PCIE_ATR_TABLE_OFFSET;
			writel_relaxed(0, pcie->bridge + atr_base +
				       PCIE_ATR_SRC_ADDR_LOW);
		}
	}
	bitmap_zero(pcie->atr_table_map, PCIE_ATR_MAX_TABLE_NUM);
	memset(pcie->atr_table_addr, 0, sizeof(pcie->atr_table_addr));
	mutex_unlock(&pcie->atr_lock);
}

/*
 * BAR0, BAR2 are strictly 64-bit (each consumes a pair of
 * 32-bit BAR registers). Odd-numbered BARs are reserved as the
 * upper-32 portion of the preceding 64-bit BAR.
 */
static const struct pci_epc_features axiado_epc_features = {
	.linkup_notifier = true,
	.msi_capable = true,
	.intx_capable = false,
	.bar[BAR_0] = { .only_64bit = true, },
	.bar[BAR_1] = { .type = BAR_RESERVED, },
	.bar[BAR_2] = { .only_64bit = true, },
	.bar[BAR_3] = { .type = BAR_RESERVED, },
};

static const struct pci_epc_features *
axiado_epc_get_features(struct pci_epc *epc, u8 fn, u8 vfn)
{
	struct axiado_pcie *pcie = epc_get_drvdata(epc);

	dev_dbg(pcie->dev, "EPC: get_features fn%d vfn%d msi=%d intx=%d linkup_notif=%d\n",
		fn, vfn, axiado_epc_features.msi_capable,
		axiado_epc_features.intx_capable,
		axiado_epc_features.linkup_notifier);

	return &axiado_epc_features;
}

static const struct pci_epc_ops axiado_epc_ops = {
	.write_header	= axiado_epc_write_header,
	.set_bar	= axiado_epc_set_bar,
	.clear_bar	= axiado_epc_clear_bar,
	.map_addr	= axiado_epc_map_addr,
	.unmap_addr	= axiado_epc_unmap_addr,
	.get_msi	= axiado_epc_get_msi,
	.set_msi	= axiado_epc_set_msi,
	.raise_irq	= axiado_epc_raise_irq,
	.start		= axiado_epc_start,
	.stop		= axiado_epc_stop,
	.get_features	= axiado_epc_get_features,
	.owner		= THIS_MODULE,
};

/* ----------------------------------------------------------------
 * Link polling — notify EPC framework on link state changes
 * ----------------------------------------------------------------
 */

#ifdef AXIADO_EPC_MSI_SELFTEST
/*
 * Development-only MSI self-test: fires a single MSI (vector 1) on link-up
 * so the raise_irq path can be validated end-to-end against a host stub
 * driver. Compiled out of normal builds; define AXIADO_EPC_MSI_SELFTEST to
 * enable.
 */
static void axiado_epc_msi_selftest(struct axiado_pcie *pcie)
{
	int err = pci_epc_raise_irq(pcie->epc, 0, 0, PCI_IRQ_MSI, 1);

	dev_info(pcie->dev, "MSI selftest: raise_irq vector 1 -> %d\n", err);
}
#endif

static void axiado_epc_link_work(struct work_struct *work)
{
	struct axiado_pcie *pcie =
		container_of(work, struct axiado_pcie, ep_link_work.work);
	u32 reg, ltssm;

	reg = readl_relaxed(pcie->csr + LINK_STATUS_LOW_POWER_ADDR);
	ltssm = (reg >> 8) & 0x1F;

	if (ltssm == 0x10) {
		if (!pcie->ep_link_up) {
			pcie->ep_link_up = true;
			dev_info(pcie->dev, "EP link UP LTSSM=0x%x reg=0x%08x\n",
				 ltssm, reg);

			pci_epc_linkup(pcie->epc);
#ifdef AXIADO_EPC_MSI_SELFTEST
			axiado_epc_msi_selftest(pcie);
#endif
		}
	} else {
		if (pcie->ep_link_up) {
			pcie->ep_link_up = false;
			dev_info(pcie->dev, "EP link DOWN LTSSM=0x%x reg=0x%08x\n",
				 ltssm, reg);
			pci_epc_linkdown(pcie->epc);
		} else {
			dev_dbg(pcie->dev, "EP link poll: LTSSM=0x%02x reg=0x%08x\n",
				ltssm, reg);
		}
	}

	/*
	 * Poll quickly until the initial link-up, then back off to the slower
	 * interval since further link transitions are rare.
	 */
	schedule_delayed_work(&pcie->ep_link_work,
			      msecs_to_jiffies(pcie->ep_link_up ?
					       EP_LINK_POLL_UP_MS :
					       EP_LINK_POLL_MS));
}

/* ----------------------------------------------------------------
 * EP range parser (from DTS "ranges" property)
 * ----------------------------------------------------------------
 */

static int axiado_epc_parse_ep_ranges(struct axiado_pcie *pcie)
{
	struct device_node *np = pcie->dev->of_node;
	struct of_pci_range_parser parser;
	struct of_pci_range range;
	int i = 0;

	if (of_pci_range_parser_init(&parser, np))
		return -EINVAL;

	for_each_of_pci_range(&parser, &range) {
		if (i == 0) {
			pcie->ep_bar0_addr = range.cpu_addr;
			pcie->ep_bar0_size = range.size;
			pcie->ep_bar0_flags = range.flags;
		} else if (i == 1) {
			pcie->ep_bar1_addr = range.cpu_addr;
			pcie->ep_bar1_size = range.size;
			pcie->ep_bar1_flags = range.flags;
		} else if (i == 2) {
			pcie->ep_bar2_addr = range.cpu_addr;
			pcie->ep_bar2_size = range.size;
			pcie->ep_bar2_flags = range.flags;
		}
		dev_dbg(pcie->dev, "EP range[%d]: cpu 0x%llx size 0x%llx flags 0x%x\n",
			i, range.cpu_addr, range.size, range.flags);
		i++;
	}

	if (i < 2) {
		dev_err(pcie->dev, "need at least 2 ranges for endpoint BARs, found %d\n", i);
		return -EINVAL;
	}

	pcie->ep_num_ranges = i;
	return 0;
}

/* ----------------------------------------------------------------
 * Hardware init: PHY, PLL, PERST#, reset, EQ
 * ----------------------------------------------------------------
 */

static int axiado_epc_hw_init(struct axiado_pcie *pcie)
{
	u32 temp;
	u32 reset_ctrl_off = pcie->pcie_x1 ? REG_PCIE_X1_RESET_CTRL_ADRS_OFFSET
					    : REG_PCIE_X2_RESET_CTRL_ADRS_OFFSET;
	u32 ip_ctrl_off = pcie->pcie_x1 ? REG_PCIE_X1_IP_CTRL_ADRS_OFFSET
					 : REG_PCIE_X2_IP_CTRL_ADRS_OFFSET;
	u32 gen_settings_off = pcie->pcie_x1 ? REG_PCIE_X1_GEN_SETTINGS_ADRS_OFFSET
					      : REG_PCIE_X2_GEN_SETTINGS_ADRS_OFFSET;

	/*
	 * This reset/enable sequence runs once at probe and has real
	 * timing dependencies (e.g. the PIPE_RST_N release below must
	 * reach hardware before the delay that follows it starts), so
	 * non-relaxed accessors are used throughout for ordering.
	 */

	/* PERST# pull-up via IOCTL pad */
	if (pcie->ioctl) {
		u32 offset = pcie->pcie_x1 ? REG_PAD_CFG_REG_25_ADRS_OFFSET
					    : REG_PAD_CFG_REG_26_ADRS_OFFSET;
		temp = readl(pcie->ioctl + offset);
		temp |= PCIE_PERST_PULLUP_SET;
		writel(temp, pcie->ioctl + offset);
	}

	/* POR reset: set PCIE_RSTN + HOLD_PIPE_RST */
	temp = readl(pcie->ext + reset_ctrl_off);
	temp = REG_PCIE_RESET_CTRL_PCIE_RSTN_SET(temp, 1);
	temp = REG_PCIE_RESET_CTRL_HOLD_PIPE_RST_SET(temp, 1);
	writel(temp, pcie->ext + reset_ctrl_off);

	/* AX3005 EXT gen register */
	if (pcie->pcie_x1)
		writel(REG_PCIE_X1_GEN_VAL, pcie->ext + REG_PCIE_X1_GEN_OFF);

	/* IP_CTRL: TL clock frequency, EP mode, no clock gating */
	temp = readl(pcie->ext + ip_ctrl_off);
	temp = REG_PCIE_IP_CTRL_FREQ_SET(temp, AX_PCIE_TL_CLOCK_FREQ_MHZ);
	if (pcie->is_root_port)
		temp = REG_PCIE_IP_CTRL_RP_NEP_SET(temp, 1);
	else
		temp = REG_PCIE_IP_CTRL_RP_NEP_SET(temp, 0);
	temp = REG_PCIE_IP_CTRL_TL_CLK_GATE_EN_SET(temp, 0);
	writel(temp, pcie->ext + ip_ctrl_off);

	/* GEN_SETTINGS: port type, lane reversal and width */
	if (pcie->pcie_x2) {
		temp = readl(pcie->bridge + gen_settings_off);
		temp = PCIE_K_SET_PCIE_PORT_TYPE_SET(temp, 0);
		temp = PCIE_K_SET_LANE_REVERSAL_EN_SET(temp, 1);
		temp = PCIE_K_SET_LINK_WIDTH_X2_SET(temp, 1);
		writel(temp, pcie->bridge + gen_settings_off);
	}

	/* PHYMAC_CFG: enable EQ phase 2/3 */
	temp = readl(pcie->bridge + REG_PCIE_PHYMAC_CFG_ADRS_OFFSET);
	temp |= PCIE_PHY_EQ_ENABLE;
	writel(temp, pcie->bridge + REG_PCIE_PHYMAC_CFG_ADRS_OFFSET);

	/* Gen3/Gen4 EQ presets and tuning */
	axiado_pcie_config_eq_gen3_4(pcie);

	/* Release PIPE_RST_N */
	writel(0x5, pcie->ext + reset_ctrl_off);
	usleep_range(1000, 2000);


	/* Configure MSI: enable capability, advertise vector count from DTS */
	temp = readl(pcie->bridge + PCIE_PCI_IRQ_DW0);
	temp &= ~PF_MSI_CAP_DISABLE;
	temp &= ~MSIX_CAP_MASK;
	temp &= ~NUM_MSI_MSGS_MASK;
	temp |= (order_base_2(pcie->num_msi_vectors) << NUM_MSI_MSGS_SHIFT) & NUM_MSI_MSGS_MASK;
	writel(temp, pcie->bridge + PCIE_PCI_IRQ_DW0);

	/* Enable bus master */
	temp = readl(pcie->cfg + REG_PCIE_X1_COMMAND_STATUS_ADRS_OFFSET);
	writel(temp | PCIE_CFG_BUS_MASTER_EN,
	       pcie->cfg + REG_PCIE_X1_COMMAND_STATUS_ADRS_OFFSET);

	/*
	 * Zero all BAR sizing registers so no phantom BARs are advertised.
	 * The active BARs (BAR0/BAR2) are programmed afterwards via the
	 * .set_bar callback; anything not set stays 0 and is not exposed to
	 * the host.
	 */
	writel(0, pcie->bridge + PCIE_BAR_01_OFFSET);
	writel(0, pcie->bridge + PCIE_BAR_01_OFFSET + 4);
	writel(0, pcie->bridge + PCIE_BAR_23_OFFSET);
	writel(0, pcie->bridge + PCIE_BAR_23_OFFSET + 4);

	/* Program class code into bridge register (preserving revision ID) */
	if (pcie->is_vga || pcie->is_eth) {
		u32 class_code = pcie->is_vga ? PCIE_VGA_CLASS_CODE
					      : PCIE_ETHERNET_CLASS_CODE;
		class_code |= readl(pcie->bridge + PCIE_PCI_IDS_DW1) &
			      REVISION_ID_MASK;
		writel(class_code, pcie->bridge + PCIE_PCI_IDS_DW1);
	}

	dev_dbg(pcie->dev,
		"EPC: hw_init done — bridge=%pa ext=%pa cfg=%pa csr=%pa x%d MSI=%d\n",
		&pcie->bridge_phys, &pcie->ext_phys, &pcie->cfg_phys,
		&pcie->csr_phys, pcie->lanes, pcie->num_msi_vectors);

	return 0;
}

/*
 * Fixed-function x1 boards (VGA/Ethernet) have no EPF driver, so their BARs
 * and P2A address translation are configured directly from the DTS ranges:
 * range[0]->BAR0, range[1]->BAR2, range[2]->BAR6.
 */
static int axiado_epc_setup_fixed_bar(struct axiado_pcie *pcie,
				      enum pci_barno barno, u64 addr, u64 size,
				      unsigned long of_flags)
{
	struct pci_epf_bar bar = {
		.barno = barno,
		.phys_addr = addr,
		.size  = size,
		.flags = axiado_bar_flags_from_range(of_flags),
	};
	int err;

	err = pci_epc_set_bar(pcie->epc, 0, 0, &bar);
	if (err) {
		dev_err(pcie->dev, "failed to set BAR%d: %d\n", barno, err);
		return err;
	}


	return err;
}

static int axiado_epc_setup_fixed_bars(struct axiado_pcie *pcie)
{
	int err;

	err = axiado_epc_setup_fixed_bar(pcie, BAR_0, pcie->ep_bar0_addr,
					 pcie->ep_bar0_size,
					 pcie->ep_bar0_flags);
	if (err)
		return err;

	err = axiado_epc_setup_fixed_bar(pcie, BAR_2, pcie->ep_bar1_addr,
					 pcie->ep_bar1_size,
					 pcie->ep_bar1_flags);
	if (err)
		return err;

	axiado_epc_program_option_rom(pcie, pcie->ep_bar2_size,  pcie->ep_bar2_addr);
	return err;
}

/* ----------------------------------------------------------------
 * Platform probe / remove
 * ----------------------------------------------------------------
 */

static int axiado_epc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct axiado_pcie *pcie;
	struct pci_epc *epc;
	int err;

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pcie->dev = dev;
	platform_set_drvdata(pdev, pcie);
	INIT_LIST_HEAD(&pcie->ports);
	INIT_DELAYED_WORK(&pcie->ep_link_work, axiado_epc_link_work);
	mutex_init(&pcie->atr_lock);

	epc = devm_pci_epc_create(dev, &axiado_epc_ops);
	if (IS_ERR(epc)) {
		dev_err(dev, "failed to create EPC device\n");
		return PTR_ERR(epc);
	}

	pcie->epc = epc;
	epc_set_drvdata(epc, pcie);

	err = axiado_pcie_parse_dt(pcie);
	if (err)
		return err;

	err = axiado_pcie_get_resources(pcie);
	if (err) {
		dev_err(dev, "failed to request resources: %d\n", err);
		return err;
	}

	err = axiado_epc_parse_ep_ranges(pcie);
	if (err) {
		dev_err(dev, "failed to parse endpoint ranges: %d\n", err);
		return err;
	}

	epc->max_functions = 1;

	/* PHY, PLL, PERST#, reset sequence */
	err = axiado_epc_hw_init(pcie);
	if (err) {
		dev_err(dev, "HW init failed: %d\n", err);
		return err;
	}

	/* Create a EPF device for the EPF driver to attach*/
	pcie->msi_cap = PLDA_MSI_CAP_OFFSET;
	pcie->epf = pci_epf_create("axiado-pcie-epf");
	pcie->epf->dev.of_node = pcie->dev->of_node;
	pcie->epf->epc = epc;

	//If we have a VGA and X1 instantiate it in the probe itself
	if (pcie->pcie_x1 && (pcie->is_vga)) {
		err = axiado_epc_setup_fixed_bars(pcie);
		if (err)
			return err;

		err = pci_epc_start(epc);
		if (err) {
			dev_err(dev, "failed to start EPC: %d\n", err);
			return err;
		}
		pci_epc_init_notify(epc);
	}

	dev_info(dev, "Axiado PCIe EPC ready (%s, x%d)\n",
		 pcie->is_vga ? "VGA" : pcie->is_eth ? "Ethernet" : "generic",
		 pcie->lanes);

	return 0;
}

static void axiado_epc_remove(struct platform_device *pdev)
{
	struct axiado_pcie *pcie = platform_get_drvdata(pdev);
	pci_epf_destroy(pcie->epf);
	axiado_epc_stop(pcie->epc);
}

static const struct of_device_id axiado_epc_of_match[] = {
	{ .compatible = "axiado,ax3005-pcie-ep" },
	{ }
};
MODULE_DEVICE_TABLE(of, axiado_epc_of_match);

static struct platform_driver axiado_epc_driver = {
	.driver = {
		.name = "axiado-pcie-ep",
		.of_match_table = axiado_epc_of_match,
	},
	.probe = axiado_epc_probe,
	.remove = axiado_epc_remove,
};

module_platform_driver(axiado_epc_driver);

MODULE_AUTHOR("Karthikeyan Mitran <kmitran@axiado.com>");
MODULE_AUTHOR("Shubhangini <shubhangini@axiado.com>");
MODULE_DESCRIPTION("Axiado PLDA PCIe Endpoint Controller (EPC) driver");
MODULE_LICENSE("GPL");
