// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2021-26 Axiado Corporation (or its affiliates).
 */


#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define AX_TX_CHANS   8   /* 0–7 */
#define AX_RX_CHANS   8   /* 8–15 */
#define CHAN_STRIDE   0x4
#define TX_REG_STRIDE 0x40
#define RX_REG_STRIDE 0x30

struct axiado_mbox_data {
	u8 num_chans;
	u8 msg_size;
};

struct axiado_channel_data {
	void __iomem *mbox_reg;
	void __iomem *csr_reg;
	void __iomem *gic_reg;
	char name[16];
	void *rx_buffer;
	u8 channel_num;
	int irq;
	struct mbox_chan *chan;
	u8 chan_msg_size;
	u8 chan_state;
};

/* mailbox side */
struct axiado_mbox {
	struct mbox_controller mbox;
	const struct axiado_mbox_data *drv_data;
	void __iomem *tx_intr;
	void __iomem *rx_intr;
};

static int axiado_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct axiado_mbox *mb = dev_get_drvdata(chan->mbox->dev);
	struct axiado_channel_data *priv = chan->con_priv;
	void __iomem *data_reg;
	u32 *word_data;
	int num_words;
	int idx = priv->channel_num;
	u32 actual_len;
	u32 actual_words;
	u32 max_words = mb->drv_data->msg_size / sizeof(u32);


	dev_dbg(chan->mbox->dev, "Sending on %s\n", priv->name);

	if (readl(priv->csr_reg) & BIT(2))
		writel(BIT(2), priv->csr_reg);

	if (!(readl(priv->csr_reg) & BIT(0))) {
		dev_warn(mb->mbox.dev, "%s: Ch-%d last data has not finished\n", __func__, idx);
		return -EBUSY;
	}
	actual_len = le32_to_cpu(*((u32 *)data));

	if (actual_len == 0 || actual_len > mb->drv_data->msg_size)
		actual_len = mb->drv_data->msg_size;

	actual_words = min_t(u32,
		(actual_len + sizeof(u32) - 1) / sizeof(u32),
		max_words);

	for (data_reg = priv->mbox_reg,
	     num_words = actual_words,
	     word_data = (u32 *)data;
	     num_words; num_words--, word_data++)
		writel(*word_data, data_reg);

	dev_dbg(mb->mbox.dev, "%s: Ch-%d sent\n", __func__, idx);

	return 0;
}

static irqreturn_t axiado_rx_thread(int irq, void *dev_id)
{
	struct axiado_channel_data *priv = dev_id;
	struct mbox_chan *chan = priv->chan;
	void __iomem *data_reg;
	int num_words;
	u32 *word_data;

	dev_dbg(chan->mbox->dev, " ISR priv->csr_reg =%px \n", priv->csr_reg);
	dev_dbg(chan->mbox->dev, " ISR value of priv->csr_reg =%x \n", readl((priv->csr_reg)));

	if ((readl(priv->csr_reg) & BIT(0)))
		return IRQ_NONE;
	disable_irq_nosync(irq);

	for (data_reg = priv->mbox_reg,
			word_data = priv->rx_buffer,
			num_words = (priv->chan_msg_size / sizeof(u32));
			num_words; num_words--, word_data++) {
		if (!(readl(priv->csr_reg) & BIT(0))) {
			*word_data = readl(data_reg);
			dev_dbg(chan->mbox->dev, " rx_thread rx_data =0x%x \n", *word_data);
		}
	}

	if (priv->chan_state == 1)
		mbox_chan_received_data(chan, priv->rx_buffer);

	enable_irq(irq);

	return IRQ_HANDLED;

}

static int axiado_mbox_startup(struct mbox_chan *chan)
{
	struct axiado_channel_data *priv = chan->con_priv;

	dev_dbg(chan->mbox->dev, "Startup called for channel %s\n", priv->name);
	priv->chan_state = 1;
	writel(BIT(5), priv->csr_reg);
	return 0;
}

static void axiado_mbox_shutdown(struct mbox_chan *chan)
{
	struct axiado_channel_data *priv = chan->con_priv;

	dev_dbg(chan->mbox->dev, "Shutdown called for channel %s\n", priv->name);
	priv->chan_state = 0;
	writel(BIT(5), priv->csr_reg);
}

static bool axiado_mbox_last_tx_done(struct mbox_chan *chan)
{
	struct axiado_channel_data *priv = chan->con_priv;

	if (!(readl(priv->csr_reg) & BIT(0)))
		return false;
	return true;
}

static const struct mbox_chan_ops axiado_mbox_chan_ops = {
	.send_data	= axiado_mbox_send_data,
	.startup	= axiado_mbox_startup,
	.shutdown	= axiado_mbox_shutdown,
	.last_tx_done	= axiado_mbox_last_tx_done,
};

static void __iomem *axiado_get_gic_base(struct device_node *np)
{
	struct device_node *gic_np;
	void __iomem *gic_base = NULL;

	gic_np = of_parse_phandle(np, "interrupt-parent", 0);
	if (!gic_np) {
		pr_err("%s: cannot find interrupt‑parent\n", __func__);
		return NULL;
	}

	gic_base = of_iomap(gic_np, 0);
	of_node_put(gic_np);
	return gic_base;
}

static int axiado_mbox_probe(struct platform_device *pdev)
{
	struct axiado_mbox          *mb;
	const struct axiado_mbox_data *drv_data;
	struct axiado_channel_data  *ch_data;
	struct device               *dev = &pdev->dev;
	struct device_node          *np  = dev->of_node;
	struct resource              regs[2];
	int                          ret;
	unsigned int                 i;
	int irq_idx = 0;

	dev_dbg(dev, " Inside Mailbox Probe\n");

	if (!pdev->dev.of_node) {
		dev_dbg(dev, " No OF node attached to platform device\n");
		return -ENODEV;
	}

	/* --------------------------------------------------------------
	 *  Driver match data (num_chans, msg_size, …)
	 * -------------------------------------------------------------- */
	drv_data = (const struct axiado_mbox_data *)
			device_get_match_data(&pdev->dev);
	if (!drv_data) {
		dev_dbg(dev, " No match data found for this device\n");
		return -ENODEV;
	}
	dev_dbg(dev, " drv_data @%px  num_chans=%u  msg_size=%u\n",
		drv_data, drv_data->num_chans, drv_data->msg_size);

	/* --------------------------------------------------------------
	 *  Allocate private driver structure
	 * -------------------------------------------------------------- */
	mb = devm_kzalloc(dev, sizeof(*mb), GFP_KERNEL);
	if (!mb) {
		dev_dbg(dev, " devm_kzalloc() failed (mb == NULL)\n");
		return -ENOMEM;
	}
	dev_dbg(dev, " Allocated mb @%px\n", mb);

	/* --------------------------------------------------------------
	 *  Fill generic mailbox controller fields that the core needs
	 * -------------------------------------------------------------- */
	mb->mbox.dev       = dev;                 /* parent device      */
	mb->mbox.num_chans = drv_data->num_chans;/* 16 (or 1 for test)*/
	dev_dbg(dev, " mbox.dev = %px  mbox.num_chans = %u\n",
		mb->mbox.dev, mb->mbox.num_chans);

	/* --------------------------------------------------------------
	 *  Allocate the array of generic channel structures
	 * -------------------------------------------------------------- */
	mb->mbox.chans = devm_kcalloc(&pdev->dev,
				      drv_data->num_chans,
				      sizeof(*mb->mbox.chans),
				      GFP_KERNEL);
	if (!mb->mbox.chans) {
		dev_dbg(dev, " devm_kcalloc() failed for mb->mbox.chans\n");
		return -ENOMEM;
	}
	dev_dbg(dev, " Allocated %u mbox.chans @%px\n",
		drv_data->num_chans, mb->mbox.chans);

	/* --------------------------------------------------------------
	 *  Translate the two address cells from the DT node
	 * -------------------------------------------------------------- */
	for (i = 0; i < 2; i++) {
		ret = of_address_to_resource(np, i, &regs[i]);
		if (ret) {
			dev_err(dev, "failed to translate reg %d\n", i);
			dev_dbg(dev, " of_address_to_resource(%u) returned %d\n",
				i, ret);
			return ret;
		}
		dev_dbg(dev, " DT reg %u -> resource start=0x%px\n",
			i, &regs[i].start);
	}

	/* --------------------------------------------------------------
	 *  Map the TX and RX interrupt registers
	 * -------------------------------------------------------------- */
	mb->tx_intr = devm_ioremap_resource(dev, &regs[0]);
	mb->rx_intr = devm_ioremap_resource(dev, &regs[1]);

	if (IS_ERR(mb->tx_intr) || IS_ERR(mb->rx_intr)) {
		dev_err(dev, "cannot map mailbox registers\n");
		dev_dbg(dev, " ioremap error: tx=%px  rx=%px\n",
			mb->tx_intr, mb->rx_intr);
		return -EBUSY;
	}
	dev_dbg(dev, " Mapped TX intr = %px   RX intr = %px\n",
		mb->tx_intr, mb->rx_intr);

	/* --------------------------------------------------------------
	 *  Allocate per‑channel private data
	 * -------------------------------------------------------------- */
	ch_data = devm_kcalloc(&pdev->dev,
			       drv_data->num_chans,
			       sizeof(*ch_data),
			       GFP_KERNEL);
	if (!ch_data) {
		dev_dbg(dev, " devm_kcalloc() failed for ch_data\n");
		return -ENOMEM;
	}
	dev_dbg(dev, " Allocated %u channel private structs @%px\n",
		drv_data->num_chans, ch_data);

	for (i = 0; i < drv_data->num_chans; i++) {
		if (i < 8) {
			ch_data[i].mbox_reg = mb->tx_intr + (i * CHAN_STRIDE);
			ch_data[i].csr_reg  = mb->tx_intr + TX_REG_STRIDE + (i * CHAN_STRIDE);
			ch_data[i].irq = -1;
		} else {
			ch_data[i].mbox_reg = mb->rx_intr + ((i-8) * CHAN_STRIDE);
			ch_data[i].csr_reg  = mb->rx_intr + RX_REG_STRIDE + ((i-8) * CHAN_STRIDE);
			ch_data[i].irq = platform_get_irq(pdev, irq_idx++);
			if (ch_data[i].irq < 0) {
				dev_err(dev, "failed to get IRQ for chan %d\n", i);
				return ch_data[i].irq;
			}

			ret = devm_request_threaded_irq(dev, ch_data[i].irq,
			       NULL,                  // no hard IRQ
			       axiado_rx_thread,      // threaded handler
			       IRQF_ONESHOT,
			       dev_name(dev),
			       &ch_data[i]);
			if (ret) {
				dev_err(dev, "IRQ request failed for chan %d\n", i);
				return ret;
			}

			dev_dbg(dev, "RX chan %d → irq %d\n", i, ch_data[i].irq);

		}

		void __iomem *gic_base = axiado_get_gic_base(np);

		if (!gic_base)
			return -ENODEV;
		ch_data[i].gic_reg = gic_base;
		ch_data[i].channel_num = i;
		ch_data[i].chan_state = 0;
		ch_data[i].chan = &mb->mbox.chans[i];
		ch_data[i].chan_msg_size = drv_data->msg_size;
		snprintf(ch_data[i].name, sizeof(ch_data[i].name), "chan-%d", i);

		ch_data[i].rx_buffer = devm_kcalloc(dev,
						    drv_data->msg_size,
						    sizeof(u8),
						    GFP_KERNEL);
		if (!ch_data[i].rx_buffer) {
			dev_dbg(dev, " devm_kcalloc() failed for rx_buffer on chan %u\n", i);
			return -ENOMEM;
		}

		mb->mbox.chans[i].con_priv = &ch_data[i];

		dev_dbg(dev, " chan %02u: mbox_reg=%px  csr_reg=%px  gic_reg=%px rx_buf=%px  name=%s\n",
			i,
			ch_data[i].mbox_reg,
			ch_data[i].csr_reg,
			ch_data[i].gic_reg,
			ch_data[i].rx_buffer,
			ch_data[i].name);
	}

	platform_set_drvdata(pdev, mb);
	mb->drv_data = drv_data;
	mb->mbox.dev = dev;
	mb->mbox.num_chans = drv_data->num_chans;
	mb->mbox.ops = &axiado_mbox_chan_ops;
	mb->mbox.txdone_irq   = false;
	mb->mbox.txdone_poll  = true;
	mb->mbox.txpoll_period = 5;

	dev_dbg(dev, " Controller ops set, txdone_poll=%d, period=%u\n",
		mb->mbox.txdone_poll, mb->mbox.txpoll_period);

	ret = devm_mbox_controller_register(dev, &mb->mbox);
	if (ret) {
		dev_dbg(dev, " devm_mbox_controller_register() failed, ret=%d\n", ret);
		return ret;
	}
	dev_dbg(dev, " Finished Successfully Mailbox Probe (controller registered)\n");
	return 0;
}

static const struct axiado_mbox_data axiado_drv_data = {
	.num_chans = 16,
	.msg_size = 0xFC,
};

static const struct of_device_id axiado_mbox_of_match[] = {
	{ .compatible = "axiado,axiado-mailbox", .data = &axiado_drv_data },
	{}
};
MODULE_DEVICE_TABLE(of, axiado_mbox_of_match);

static struct platform_driver axiado_mbox_driver = {
	.driver = {
		.name = "axiado-mailbox",
		.of_match_table = axiado_mbox_of_match,
	},
	.probe = axiado_mbox_probe,
};
module_platform_driver(axiado_mbox_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado Mailbox driver");
MODULE_LICENSE("GPL");
