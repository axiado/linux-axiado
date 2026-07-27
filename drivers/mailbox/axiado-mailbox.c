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
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define AX_TX_CHANS   8   /* 0-7 */
#define AX_RX_CHANS   8   /* 8-15 */
#define CHAN_STRIDE   0x4
#define TX_REG_STRIDE 0x40
#define RX_REG_STRIDE 0x30

/* Mailbox CSR bit definitions */
#define MBOX_CSR_EMPTY    BIT(0) /* 1 = FIFO empty (no data); 0 = data available */
#define MBOX_CSR_OVERFLOW BIT(2) /* overflow flag; write 1 to clear (W1C) */
#define MBOX_CSR_FLUSH    BIT(4) /* flush/reset; written on channel startup and shutdown */

struct axiado_mbox_data {
	u8  num_chans;
	u16 msg_size;
};

struct axiado_channel_data {
	void __iomem *mbox_reg;
	void __iomem *csr_reg;
	char name[16];
	void *rx_buffer;
	u8 channel_num;
	int irq;
	struct mbox_chan *chan;
	u16 chan_msg_size;
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
	u32 max_words = mb->drv_data->msg_size / sizeof(u32);
	u32 msg_len;

	dev_dbg(chan->mbox->dev, "Sending on %s\n", priv->name);

	if (readl(priv->csr_reg) & MBOX_CSR_OVERFLOW)
		writel(MBOX_CSR_OVERFLOW, priv->csr_reg);

	if (!(readl(priv->csr_reg) & MBOX_CSR_EMPTY)) {
		dev_warn(mb->mbox.dev, "%s: Ch-%d last data has not finished\n", __func__, idx);
		return -EBUSY;
	}

	/*
	 * mbox_chan_ops.send_data() carries no length of its own, so the
	 * client encodes one as a little-endian byte count in the first
	 * word of its message (e.g. the "len" field of
	 * struct axiado_mctp_mbox_msg). Use it to size this write, but
	 * clamp to the controller's configured maximum so a zero,
	 * corrupt, or oversized value can never push more than max_words
	 * onto the FIFO.
	 */
	msg_len = le32_to_cpu(*(u32 *)data);
	if (msg_len == 0 || msg_len > mb->drv_data->msg_size)
		msg_len = mb->drv_data->msg_size;

	num_words = min_t(u32, DIV_ROUND_UP(msg_len, sizeof(u32)), max_words);

	/*
	 * Each write to the same FIFO port register pushes one DW into the
	 * mailbox; the hardware advances its internal write pointer.
	 */
	for (data_reg = priv->mbox_reg, word_data = (u32 *)data;
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

	dev_dbg(chan->mbox->dev, " ISR priv->csr_reg =%p\n", priv->csr_reg);
	dev_dbg(chan->mbox->dev, " ISR value of priv->csr_reg =%x\n", readl((priv->csr_reg)));

	if ((readl(priv->csr_reg) & MBOX_CSR_EMPTY))
		return IRQ_NONE;

	/*
	 * Each read from the same FIFO port register pops one DW; the
	 * hardware advances its internal read pointer.
	 */
	data_reg = priv->mbox_reg;
	word_data = priv->rx_buffer;
	for (num_words = priv->chan_msg_size / sizeof(u32); num_words; num_words--) {
		if (readl(priv->csr_reg) & MBOX_CSR_EMPTY)
			break;
		*word_data = readl(data_reg);
		dev_dbg(chan->mbox->dev, " rx_thread rx_data =0x%x\n", *word_data);
		word_data++;
	}

	if (READ_ONCE(priv->chan_state))
		mbox_chan_received_data(chan, priv->rx_buffer);

	return IRQ_HANDLED;

}

static int axiado_mbox_startup(struct mbox_chan *chan)
{
	struct axiado_channel_data *priv = chan->con_priv;

	dev_dbg(chan->mbox->dev, "Startup called for channel %s\n", priv->name);
	WRITE_ONCE(priv->chan_state, 1);
	writel(MBOX_CSR_FLUSH, priv->csr_reg);
	return 0;
}

static void axiado_mbox_shutdown(struct mbox_chan *chan)
{
	struct axiado_channel_data *priv = chan->con_priv;

	dev_dbg(chan->mbox->dev, "Shutdown called for channel %s\n", priv->name);
	WRITE_ONCE(priv->chan_state, 0);
	writel(MBOX_CSR_FLUSH, priv->csr_reg);
}

static bool axiado_mbox_last_tx_done(struct mbox_chan *chan)
{
	struct axiado_channel_data *priv = chan->con_priv;

	if (!(readl(priv->csr_reg) & MBOX_CSR_EMPTY))
		return false;
	return true;
}

static const struct mbox_chan_ops axiado_mbox_chan_ops = {
	.send_data	= axiado_mbox_send_data,
	.startup	= axiado_mbox_startup,
	.shutdown	= axiado_mbox_shutdown,
	.last_tx_done	= axiado_mbox_last_tx_done,
};


static int axiado_mbox_probe(struct platform_device *pdev)
{
	struct axiado_mbox          *mb;
	const struct axiado_mbox_data *drv_data;
	struct axiado_channel_data  *ch_data;
	struct device               *dev = &pdev->dev;
	int                          ret;
	unsigned int                 i;
	int irq_idx = 0;

	if (!pdev->dev.of_node)
		return -ENODEV;

	drv_data = (const struct axiado_mbox_data *)
			device_get_match_data(&pdev->dev);
	if (!drv_data)
		return -ENODEV;

	mb = devm_kzalloc(dev, sizeof(*mb), GFP_KERNEL);
	if (!mb)
		return -ENOMEM;

	mb->mbox.dev       = dev;
	mb->mbox.num_chans = drv_data->num_chans;

	mb->mbox.chans = devm_kcalloc(&pdev->dev,
				      drv_data->num_chans,
				      sizeof(*mb->mbox.chans),
				      GFP_KERNEL);
	if (!mb->mbox.chans)
		return -ENOMEM;

	mb->tx_intr = devm_platform_ioremap_resource_byname(pdev, "tx");
	if (IS_ERR(mb->tx_intr))
		return PTR_ERR(mb->tx_intr);

	mb->rx_intr = devm_platform_ioremap_resource_byname(pdev, "rx");
	if (IS_ERR(mb->rx_intr))
		return PTR_ERR(mb->rx_intr);

	ch_data = devm_kcalloc(&pdev->dev,
			       drv_data->num_chans,
			       sizeof(*ch_data),
			       GFP_KERNEL);
	if (!ch_data)
		return -ENOMEM;

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

			dev_dbg(dev, "RX chan %d -> irq %d\n", i, ch_data[i].irq);

		}

		ch_data[i].channel_num = i;
		ch_data[i].chan_state = 0;
		ch_data[i].chan = &mb->mbox.chans[i];
		ch_data[i].chan_msg_size = drv_data->msg_size;
		snprintf(ch_data[i].name, sizeof(ch_data[i].name), "chan-%d", i);

		ch_data[i].rx_buffer = devm_kcalloc(dev,
						    drv_data->msg_size,
						    sizeof(u8),
						    GFP_KERNEL);
		if (!ch_data[i].rx_buffer)
			return -ENOMEM;

		mb->mbox.chans[i].con_priv = &ch_data[i];
	}

	platform_set_drvdata(pdev, mb);
	mb->drv_data = drv_data;
	mb->mbox.ops = &axiado_mbox_chan_ops;
	mb->mbox.txdone_irq   = false;
	mb->mbox.txdone_poll  = true;
	mb->mbox.txpoll_period = 5;

	return devm_mbox_controller_register(dev, &mb->mbox);
}

static const struct axiado_mbox_data axiado_drv_data = {
	.num_chans = 16,
	.msg_size = 256,
};

static const struct of_device_id axiado_mbox_of_match[] = {
	{ .compatible = "axiado,ax3005-mailbox", .data = &axiado_drv_data },
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
