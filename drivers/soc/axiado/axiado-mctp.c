// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2023-25 Axiado Corporation.
 *
 * Axiado MCTP Driver
 *
 * Based on aspeed-mctp.c
 * Copyright (c) 2020 Intel Corporation
 *
 */
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/ptr_ring.h>
#include <linux/delay.h>

#include <uapi/linux/axiado-mctp.h>
#include <linux/soc/axiado/pcie-axiado.h>
#include "axiado-mctp.h"

static struct axiado_mctp_devinfo *priv_g;
static struct kmem_cache *packet_cache;
/* Global flag is used to avoid latency introduced by the locks.
 * We need to ensure that the loop will complete within 2ns.
 */
static bool ocm_ring_poll;
DECLARE_WAIT_QUEUE_HEAD(ocm_ring_wq);

#ifdef CONFIG_DYNAMIC_DEBUG
static void axiado_mctp_debug_parse_packet(void *buffer, u32 size, bool is_rx)
{
	bool som, eom, to;
	u8 dst_eid, src_eid, tag_ctrl, msg_type, vdm_type, tag;
	u16 vendor_id;
	u8 *data = (u8 *)buffer;
	const char *dir = is_rx ? "RX" : "TX";

	if (size < MCTP_MIN_PACKET_SIZE || !data) {
		pr_warn("MCTP %s: packet too small (%u)(0x%x) bytes)\n", dir,
			size, size);
		return;
	}
	pr_debug("MCTP %s: packet size (%u bytes)\n", dir, size);
	dst_eid = data[13];
	src_eid = data[14];
	tag_ctrl = data[15];
	msg_type = data[16];
	vendor_id = data[17] | (data[18] << 8);
	vdm_type = data[19];

	som = tag_ctrl & BIT(7);
	eom = tag_ctrl & BIT(6);
	to = tag_ctrl & BIT(5);
	tag = tag_ctrl & 0x1F;

	pr_debug(
		"MCTP %s: SrcEID=0x%02x DstEID=0x%02x Type=0x%02x SOM=%d EOM=%d TO=%d Tag=0x%02x size:%u\n",
		dir, src_eid, dst_eid, msg_type, som ? 1 : 0, eom ? 1 : 0,
		to ? 1 : 0, tag, size);
}
#endif


void *axiado_mctp_packet_alloc(gfp_t flags)
{
	return kmem_cache_alloc(packet_cache, flags);
}

void axiado_mctp_packet_free(void *packet)
{
	kmem_cache_free(packet_cache, packet);
}

static struct mctp_client *axiado_mctp_client_alloc(struct axiado_mctp_devinfo *priv)
{
	struct mctp_client *client;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		goto out;

	kref_init(&client->ref);
	client->priv = priv;
	ptr_ring_init(&client->tx_queue, priv->tx_ring_count, GFP_KERNEL);
	ptr_ring_init(&client->rx_queue, priv->rx_ring_count, GFP_ATOMIC);

out:
	return client;
}

static void axiado_mctp_client_free(struct kref *ref)
{
	struct mctp_client *client = container_of(ref, typeof(*client), ref);

	ptr_ring_cleanup(&client->rx_queue, &axiado_mctp_packet_free);
	ptr_ring_cleanup(&client->tx_queue, &axiado_mctp_packet_free);

	kfree(client);
}

static void axiado_mctp_client_put(struct mctp_client *client)
{
	kref_put(&client->ref, &axiado_mctp_client_free);
}

struct mctp_client *axiado_mctp_create_client(struct axiado_mctp_devinfo *priv)
{
	struct mctp_client *client;

	client = axiado_mctp_client_alloc(priv);
	if (!client)
		return NULL;

	init_waitqueue_head(&client->wait_queue);
	client->priv = priv;

	spin_lock_bh(&priv->clients_lock);
	list_add_tail(&client->link, &priv->clients);
	spin_unlock_bh(&priv->clients_lock);

	return client;
}

void axiado_mctp_delete_client(struct mctp_client *client)
{
	struct axiado_mctp_devinfo *priv = client->priv;
	struct mctp_type_handler *handler, *tmp;

	spin_lock_bh(&priv->clients_lock);

	list_del(&client->link);

	if (priv->default_client == client)
		priv->default_client = NULL;

	list_for_each_entry_safe(handler, tmp, &priv->mctp_type_handlers,
				  link) {
		if (handler->client == client) {
			list_del(&handler->link);
			kfree(handler);
		}
	}
	spin_unlock_bh(&priv->clients_lock);

	/* Disable the tasklet to appease lockdep */
	local_bh_disable();
	axiado_mctp_client_put(client);
	local_bh_enable();
}

static int axiado_mctp_release(struct inode *inode, struct file *file)
{
	struct mctp_client *client = file->private_data;

	axiado_mctp_delete_client(client);

	return 0;
}

static ssize_t axiado_mctp_mbx_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct mctp_client *client = (struct mctp_client *)(file->private_data);
	struct axiado_mctp_devinfo *priv = client->priv;
	struct axiado_mctp_pcie_packet *rx_packet;

	if (count < AX_PCIE_VDM_MAXBYTES) {
		dev_err(priv->dev, "READ: buffer not big enough\n");
		return -EINVAL;
	}

	rx_packet = ptr_ring_consume_bh(&client->rx_queue);

	if (!rx_packet)
		return -EAGAIN;

	if (count < rx_packet->size) {
		dev_warn(priv->dev, "READ: rx data %u over buffer size\n",
				rx_packet->size);
	}
	count = sizeof(rx_packet->data);
	if (copy_to_user(buf, &rx_packet->data, count)) {
		dev_err(priv->dev, "copy to user failed\n");
		count = -EFAULT;
	}

	print_hex_dump_bytes("RX", DUMP_PREFIX_OFFSET,
			(const void *)&rx_packet->data, rx_packet->size);
#ifdef CONFIG_DYNAMIC_DEBUG
	axiado_mctp_debug_parse_packet(&rx_packet->data, rx_packet->size, true);
#endif
	axiado_mctp_packet_free(rx_packet);
	return count;
}

static u16 axiado_mtu_get(struct axiado_mctp_devinfo *priv)
{
	u16 mtu = AX_PCIE_VDM_MAXBYTES;
	/* should probe the controller and find the mtu from registers TODO XXX */
	return mtu;
}

static u16 axiado_bdf_get(struct axiado_mctp_devinfo *priv)
{
	u8 bus, dev, func;

	if (priv->vdm_port == PCIE_X1) {
		/* domain: 0 */
		bus = 0;
		dev = 1;
		func = 0;
	} else {
		/* domain: 1 */
		bus = 0;
		dev = 2;
		func = 0;
	}

	return ((bus << 8) | (dev << 3) | func);
}

static int axiado_mctp_bdf_get(struct axiado_mctp_devinfo *priv, void __user *ubuf)
{
	struct axiado_mctp_bdf bdf = { 0 };

	if ((priv->vdm_port != PCIE_X1) && (priv->vdm_port != PCIE_X2))
		return -EINVAL;

	bdf.bdf = axiado_bdf_get(priv);
	if (copy_to_user(ubuf, &bdf, sizeof(struct axiado_mctp_bdf)) != 0) {
		dev_err(priv->dev, "BDF: copy_to_user failed\n");
		return -EFAULT;
	}
	return 0;
}

static int axiado_mctp_mtu_get(struct axiado_mctp_devinfo *priv, void __user *ubuf)
{
	struct axiado_mctp_mtu mtu = { 0 };

	if ((priv->vdm_port != PCIE_X1) && (priv->vdm_port != PCIE_X2))
		return -EINVAL;

	mtu.mtu = axiado_mtu_get(priv);
	if (copy_to_user(ubuf, &mtu, sizeof(struct axiado_mctp_mtu)) != 0) {
		dev_err(priv->dev, "MTU: copy_to_user failed\n");
		return -EFAULT;
	}
	return 0;
}

static int axiado_mctp_medium_id_get(struct axiado_mctp_devinfo *priv,
				 void __user *ubuf)
{
	struct axiado_mctp_medium_id mid = { 0x09 }; /* PCIe 2.0 */

	if ((priv->vdm_port != PCIE_X1) && (priv->vdm_port != PCIE_X2))
		return -EINVAL;

	/* probe the actual link speed and assign medium id TODO XXX */
	if (copy_to_user(ubuf, &mid, sizeof(struct axiado_mctp_medium_id)) != 0) {
		dev_err(priv->dev, "MEDIUM ID GET: copy_to_user failed\n");
		return -EFAULT;
	}
	return 0;
}

static int axiado_mctp_eid_info_get(struct axiado_mctp_devinfo *priv, void __user *ubuf,
				enum mctp_address_type fmt)
{
	int ret = 0;
	struct axiado_mctp_get_eid_info get_eid;
	struct axiado_mctp_eid_info eid = { (2 << 8) | (0 << 5) | (0), 0 };
	struct axiado_mctp_eid_ext_info exteid = {
					{ (2 << 8) | (0 << 5) | (0), 0 },
					0 };
	void *user_ptr;

	if (copy_from_user(&get_eid, ubuf,
			   sizeof(struct axiado_mctp_get_eid_info)) != 0) {
		dev_err(priv->dev, "EID INFO GET. Copy to user failed\n");
		return -EFAULT;
	}

	mutex_lock(&priv->endpoints_lock);
	/* For now, only one EP is supported */
	if (get_eid.count == 0) {
		/* This is how the client asks for number of eids in the system */
		ret = 0;
		goto update_count;
	}
	if (get_eid.count > 1) {
		ret = -EINVAL;
		dev_err(priv->dev, "EID INFO GET. Invalid count\n");
		goto func_end;
	}

	user_ptr = u64_to_user_ptr(get_eid.ptr);
	if (fmt == AX_MCTP_GENERIC_ADDR_FORMAT) {
		if (copy_to_user(user_ptr, &eid,
				 sizeof(struct axiado_mctp_eid_info)) != 0) {
			ret = -EFAULT;
			dev_err(priv->dev,
				"EID INFO GET. Copy to user failed\n");
			goto func_end;
		}
	} else {
		if (copy_to_user(user_ptr, &exteid,
				 sizeof(struct axiado_mctp_eid_ext_info)) != 0) {
			dev_err(priv->dev,
				"EID INFO GET. Copy to user failed\n");
			ret = -EFAULT;
			goto func_end;
		}
	}
update_count:
	/* Update the ubuf with actual count of entries copied */
	get_eid.count = 1;
	if (copy_from_user(ubuf, &get_eid,
			   sizeof(struct axiado_mctp_get_eid_info)) != 0) {
		dev_err(priv->dev, "EID INFO GET. Copy to user failed\n");
		ret = -EFAULT;
		goto func_end;
	}
func_end:
	mutex_unlock(&priv->endpoints_lock);
	return ret;
}

static int axiado_mctp_register_default_handler(struct mctp_client *client)
{
	int ret = 0;
	struct axiado_mctp_devinfo *priv = client->priv;

	spin_lock_bh(&priv->clients_lock);

	if (!priv->default_client)
		priv->default_client = client;
	else if (priv->default_client != client)
		ret = -EBUSY;

	spin_unlock_bh(&priv->clients_lock);

	return ret;
}

int axiado_mctp_add_type_handler(struct mctp_client *client, u8 mctp_type,
			     u16 pci_vendor_id, u16 vdm_type, u16 vdm_mask)
{
	struct axiado_mctp_devinfo *priv = client->priv;
	struct mctp_type_handler *handler, *new_handler;
	int ret = 0;

	if (mctp_type <= MCTP_HDR_TYPE_BASE_LAST) {
		/* Vendor, type and type mask must be zero for types 0-5 */
		if (pci_vendor_id != 0 || vdm_type != 0 || vdm_mask != 0)
			return -EINVAL;
	} else if (mctp_type == MCTP_HDR_TYPE_VDM_PCI) {
		/* For Vendor Defined PCI type the vendor ID must be nonzero */
		if (pci_vendor_id == 0 || pci_vendor_id == 0xffff)
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	new_handler = kzalloc(sizeof(*new_handler), GFP_KERNEL);
	if (!new_handler)
		return -ENOMEM;
	new_handler->mctp_type = mctp_type;
	new_handler->pci_vendor_id = pci_vendor_id;
	new_handler->vdm_type = vdm_type & vdm_mask;
	new_handler->vdm_mask = vdm_mask;
	new_handler->client = client;

	spin_lock_bh(&priv->clients_lock);
	list_for_each_entry(handler, &priv->mctp_type_handlers, link) {
		if (handler->mctp_type == new_handler->mctp_type &&
		    handler->pci_vendor_id == new_handler->pci_vendor_id &&
		    handler->vdm_type == new_handler->vdm_type) {
			if (handler->client != new_handler->client)
				ret = -EBUSY;
			kfree(new_handler);
			goto out_unlock;
		}
	}
	list_add_tail(&new_handler->link, &priv->mctp_type_handlers);
out_unlock:
	spin_unlock_bh(&priv->clients_lock);

	return ret;
}

static int axiado_mctp_register_type_handler(struct mctp_client *client,
					 void __user *userbuf)
{
	struct axiado_mctp_devinfo *priv = client->priv;
	struct axiado_mctp_type_handler_ioctl handler;

	if (copy_from_user(&handler, userbuf, sizeof(handler))) {
		dev_err(priv->dev, "copy from user failed\n");
		return -EFAULT;
	}

	return axiado_mctp_add_type_handler(client, handler.mctp_type,
					handler.pci_vendor_id,
					handler.vendor_type,
					handler.vendor_type_mask);
}

int axiado_mctp_remove_type_handler(struct mctp_client *client, u8 mctp_type,
				u16 pci_vendor_id, u16 vdm_type, u16 vdm_mask)
{
	struct axiado_mctp_devinfo *priv = client->priv;
	struct mctp_type_handler *handler, *tmp;
	int ret = -EINVAL;

	vdm_type &= vdm_mask;

	spin_lock_bh(&priv->clients_lock);
	list_for_each_entry_safe(handler, tmp, &priv->mctp_type_handlers,
				  link) {
		if (handler->client == client &&
		    handler->mctp_type == mctp_type &&
		    handler->pci_vendor_id == pci_vendor_id &&
		    handler->vdm_type == vdm_type) {
			list_del(&handler->link);
			kfree(handler);
			ret = 0;
			break;
		}
	}
	spin_unlock_bh(&priv->clients_lock);
	return ret;
}

static int axiado_mctp_unregister_type_handler(struct mctp_client *client,
					   void __user *userbuf)
{
	struct axiado_mctp_devinfo *priv = client->priv;
	struct axiado_mctp_type_handler_ioctl handler;

	if (copy_from_user(&handler, userbuf, sizeof(handler))) {
		dev_err(priv->dev, "copy from user failed\n");
		return -EFAULT;
	}

	return axiado_mctp_remove_type_handler(client, handler.mctp_type,
					   handler.pci_vendor_id,
					   handler.vendor_type,
					   handler.vendor_type_mask);
}

static int axiado_mctp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct platform_device *pdev = to_platform_device(misc->parent);
	struct axiado_mctp_devinfo *priv = platform_get_drvdata(pdev);
	struct mctp_client *client;

	client = axiado_mctp_create_client(priv);
	if (!client)
		return -ENOMEM;

	file->private_data = client;
	axiado_mctp_register_default_handler(client);
	return 0;
}

static long axiado_mctp_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct mctp_client *client = (struct mctp_client *)(file->private_data);
	struct axiado_mctp_devinfo *priv = client->priv;
	int ret = -ENODEV;

	switch (cmd) {
	case AXIADO_MCTP_IOCTL_GET_BDF:
		ret = axiado_mctp_bdf_get(priv, (void *)arg);
		break;
	case AXIADO_MCTP_IOCTL_FILTER_EID:
		break;
	case AXIADO_MCTP_IOCTL_GET_MEDIUM_ID:
		ret = axiado_mctp_medium_id_get(priv, (void *)arg);
		break;
	case AXIADO_MCTP_IOCTL_GET_MTU:
		ret = axiado_mctp_mtu_get(priv, (void *)arg);
		break;
	case AXIADO_MCTP_IOCTL_REGISTER_DEFAULT_HANDLER:
		ret = axiado_mctp_register_default_handler(client);
		break;
	case AXIADO_MCTP_IOCTL_REGISTER_TYPE_HANDLER:
		ret = axiado_mctp_register_type_handler(client, (void *)arg);
		break;
	case AXIADO_MCTP_IOCTL_UNREGISTER_TYPE_HANDLER:
		ret = axiado_mctp_unregister_type_handler(client, (void *)arg);
		break;
	case AXIADO_MCTP_IOCTL_GET_EID_INFO:
		ret = axiado_mctp_eid_info_get(priv, (void *)arg,
					   AX_MCTP_GENERIC_ADDR_FORMAT);
		break;
	case AXIADO_MCTP_IOCTL_SET_EID_INFO:
		break;
	case AXIADO_MCTP_IOCTL_GET_EID_EXT_INFO:
		ret = axiado_mctp_eid_info_get(priv, (void *)arg,
					   AX_MCTP_EXTENDED_ADDR_FORMAT);
		break;
	case AXIADO_MCTP_IOCTL_SET_EID_EXT_INFO:
		break;
	case AXIADO_MCTP_IOCTL_SET_OWN_EID:
		break;
	default:
		break;
	}

	return ret;
}

enum axiado_pcie_dma_channel get_free_dma_channel_no(
					struct axiado_mctp_devinfo *priv,
					enum axiado_pcie_port_num port)
{
	static enum axiado_pcie_dma_channel channel_no = DMA_CHANNEL_0;

	while (axiado_pcie_is_dma_busy(priv->axiado_pcie_pdev, port, channel_no)) {
		channel_no++;
		if (channel_no > DMA_CHANNEL_7)
			channel_no = DMA_CHANNEL_0;

	}

	return channel_no++;
}

static ssize_t axiado_mctp_pci_write_downstream(struct axiado_mctp_devinfo *priv,
						const char __user *buf,
						size_t count)
{
	int ret = 0;
	struct axiado_pcie_dma_request *dreq;

	if (count > axiado_mtu_get(priv)) {
		dev_err(priv->dev, "MCTP WRITE: invalid #bytes\n");
		return -EINVAL;
	}

	if (!ocm_ring_poll) {
		ocm_ring_poll = true;
		wake_up_interruptible(&ocm_ring_wq);
	}

	dreq = kzalloc(sizeof(struct axiado_pcie_dma_request), GFP_KERNEL);
	if (!dreq) {
		ret = -ENOMEM;
		goto func_end;
	}

	/* X2 is the root port */
	dreq->dma_dir = AX_RC_VDM_HOST_TO_PDEV;
	dreq->chan_num = get_free_dma_channel_no(priv, priv->vdm_port);
	dreq->dma_mode = DIRECT_MODE;
	dreq->source_list_size = 1; /* Not applicable for DIRECT MODE */
	dreq->dest_list_size = 0; /* Not applicable for DIRECT MODE */
	dreq->dma_source_size[0] = count; /* Not applicable for DIRECT MODE */
	dreq->dma_dest_size[0] = 0; /* Not applicable for DIRECT MODE */

	dreq->dma_size = count; /* #bytes for DIRECT MODE */
	dreq->dma_source_ptr[0] = (u64)priv->vdm_info.txdh[0];
	dreq->dma_dest_ptr[0] = 0;
	mutex_lock(&priv->vdm_info.mtx);
	if (copy_from_user(priv->vdm_info.txda[0], buf, count) != 0) {
		ret = -EFAULT;
		dev_err(priv->dev, "MCTP WRITE: copy from user failed\n");
		goto func_end;
	}
	print_hex_dump_bytes("TX", DUMP_PREFIX_OFFSET, priv->vdm_info.txda[0],
			count);
#ifdef CONFIG_DYNAMIC_DEBUG
	axiado_mctp_debug_parse_packet(priv->vdm_info.txda[0], count, false);
#endif
	priv->vdm_info.txnumbyte[0] = count;
	ret = axiado_pcie_dma_xfer(priv->axiado_pcie_pdev, priv->vdm_port, dreq);

	pr_debug("PCIE TX sent packet.\n");
	if (ret != 0) {
		ret = -EAGAIN;
		dev_err(priv->dev, "MCTP WRITE: DMA operation failed\n");
		goto func_end;
	}
	ret = count;

func_end:
	mutex_unlock(&priv->vdm_info.mtx);
	kfree(dreq);
	return ret;
}

static ssize_t axiado_mctp_pci_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct mctp_client *client = (struct mctp_client *)(file->private_data);
	struct axiado_mctp_devinfo *priv = client->priv;
	ssize_t ret = 0;

	ret = axiado_mctp_pci_write_downstream(priv, buf, count);
	if (ret > 0)
		*ppos += ret;

	return ret;
}

static __poll_t axiado_mctp_poll(struct file *file, struct poll_table_struct *pt)
{
	struct mctp_client *client = (struct mctp_client *)(file->private_data);
	struct axiado_mctp_devinfo *priv = client->priv;
	__poll_t ret = 0;

	poll_wait(file, &priv->read_queue, pt);
	if (!ptr_ring_full_bh(&client->tx_queue))
		ret |= EPOLLOUT;
	if (__ptr_ring_peek(&client->rx_queue))
		ret |= EPOLLIN;

	return ret;
}

static const struct file_operations axiado_mctp_fops = {
	.owner = THIS_MODULE,
	.open = axiado_mctp_open,
	.read = axiado_mctp_mbx_read,
	.write = axiado_mctp_pci_write,
	.release = axiado_mctp_release,
	.unlocked_ioctl = axiado_mctp_ioctl,
	.poll = axiado_mctp_poll,
};

struct device_type axiado_mctp_type = {
	.name = "axiado-mctp",
};

/**
 * @func axiado_gbx_thread_func(void *arg)
 * @brief Message box receive and process function for VDM
 * @param arg instance of private data struct axiado_mctp_devinfo
 */
int axiado_gbx_thread_func(void *arg)
{
	struct axiado_mctp_devinfo *priv = (struct axiado_mctp_devinfo *)arg;
	u32 data[AX_PCIE_VDM_MAXBYTES / 4], status;
	struct axiado_mctp_pcie_packet *rx_packet;
	int idx, dws, ret;
	struct mctp_client *client = NULL;
	u32 hdr, size;

	while (kthread_should_stop() == false) {
		dws = 0;
		/* Read the mailbox status registers for any content */
		status = axiado_pcie_msgbox_read(priv->axiado_pcie_pdev,
						 AX_GBX_STATUS_OFFSET);
		/* handle error conditions first */
		if ((status & (AX_GBX_STATUS_OV | AX_GBX_STATUS_UN)) != 0) {
			dev_warn(priv->dev,
				 "Clearing Underflow/Overflow %#010x\n",
				 status);
			/* Clear the OV/UN bits */
			axiado_pcie_msgbox_write(
				priv->axiado_pcie_pdev, AX_GBX_STATUS_OFFSET,
				(status & (AX_GBX_STATUS_OV | AX_GBX_STATUS_UN)));
		}
		if ((status & AX_GBX_STATUS_EMPTY) == 1) {
			usleep_range(10000, 11000); /* Sleep minimum 10ms */
			continue;
		}
		memset(data, 0, AX_PCIE_VDM_MAXBYTES);
		/* Calculate the number of DWs in the gbx */
		dws = (status & AX_GBX_STATUS_LEVEL) >> 5;
		dev_dbg(priv->dev, "status %08x, DWs: %08x\n", status, dws);
		if (dws == 0) {
			usleep_range(10000, 11000);/* sleep minimum 10ms */
			continue;
		}
		for (idx = 0; idx < dws; idx++) {
			data[idx] = axiado_pcie_msgbox_read(priv->axiado_pcie_pdev, 0);
			status = axiado_pcie_msgbox_read(priv->axiado_pcie_pdev,
							 AX_GBX_STATUS_OFFSET);
			dev_dbg(priv->dev, "data %08x, status %08x\n",
				data[idx], status);
		}

		/* get 1st client since we didn't support multiple client.*/
		if (client == NULL) {
			client = priv->default_client;
			/* todo: support multiple clients
			 * if (!list_empty(&priv->clients)) {
			 *	client = list_first_entry(&priv->clients,struct list_head, link);
			 * } else
			 */
			if (client == 0) {
				dev_err(priv->dev,
					"Client list is empty! drop data dws=%d\n",
					dws);
				continue;
			}
		}
		/* get mssage from buffer than put to RX Queue */
		hdr = 0;
		while (data[hdr] != 0) {
			/* Ignore bit9 bit10, The VDM won't get data over 1 K */
			size = (((data[hdr] >> 24) & 0xff) * 4) +
			       PCIE_VDM_HDR_SIZE;
			if (size > AXIADO_MCTP_MTU) {
				dev_warn(priv->dev,
					 "Rx length %d > MTU size %d\n", size,
					 AXIADO_MCTP_MTU);
				size = AXIADO_MCTP_MTU;
			}

			/* add the receive data to RX Queue */
			rx_packet = axiado_mctp_packet_alloc(GFP_ATOMIC);
			if (rx_packet) {
				rx_packet->size = size;
				memcpy(&rx_packet->data, &data[hdr],
				       sizeof(rx_packet->data));
				/* ax3000 don't need to switch */
				ret = ptr_ring_produce(&client->rx_queue,
						       rx_packet);
				if (ret) {
					/* This can happen if client process does not
					 * consume packets fast enough
					 */
					dev_warn(
						priv->dev,
						"Failed to store packet in client RX queue\n");
					axiado_mctp_packet_free(rx_packet);
				} else {
					wake_up_interruptible(
						&priv->read_queue);
				}
			}
			dev_dbg(priv->dev, "Copied %d bytes to RX buffer\n",
				size);
			hdr += size / 4;
		}
	}
	return 0;
}

void axiado_enqueue_rx_packet(struct axiado_mctp_pcie_packet *rx_packet)
{
	int ret;
	struct mctp_client *client = NULL;
	struct axiado_mctp_devinfo *priv = priv_g;

	/* get 1st client since we didn't support multiple client.*/
	if (client == NULL) {
		client = priv->default_client;
		/* todo: support multiple clients
		 * if (!list_empty(&priv->clients)) {
		 *	client = list_first_entry(&priv->clients,struct list_head, link);
		 * } else
		 */
		if (client == 0) {
			dev_err(priv->dev,
				"Client list is empty! drop data, size:%d\n",
				rx_packet->size);
			axiado_mctp_packet_free(rx_packet);
			return;
		}
	}

	ret = ptr_ring_produce(&client->rx_queue, rx_packet);
	if (ret) {
		/* This can happen if client process does not consume packets fast enough */
		dev_warn(priv->dev,
			 "Failed to store packet in client RX queue\n");
		axiado_mctp_packet_free(rx_packet);
	} else {
		wake_up_interruptible(&priv->read_queue);
	}
}

static int axiado_process_ocm_buffer(void *data)
{
	struct axiado_mctp_devinfo *priv = (struct axiado_mctp_devinfo *)data;
	int idx_tail = 0;
	s32 loop_count;
	u32 *rx_buf;
	struct axiado_mctp_pcie_packet *rx_packet;

	rx_packet = axiado_mctp_packet_alloc(GFP_ATOMIC);
	if (!rx_packet) {
		pr_err("Failed to allocate MCTP packet\n");
		return -ENOMEM;
	}

	while (!kthread_should_stop()) {
		wait_event_interruptible(ocm_ring_wq, ocm_ring_poll);
		loop_count = 0;
		/* Busy polling to check for the data in OCM ring buffer,
		 * If ring buffer has data, Allocate MCTP and fill the MCTP
		 * packet and enqueue it.
		 */
		while (ocm_ring_poll) {

			if (priv->ocm_ring_tail == priv->ocm_ring_head) {
				loop_count++;
				if (loop_count <= MAX_LOOP_COUNT) {
					usleep_range(19000, 20000);
					continue;
				} else {
					break;
				}
			}
			loop_count = 0;
			rx_buf = (u32 *)(priv->ocm_ring_tail);

			memcpy_fromio(&rx_packet->data, rx_buf, sizeof(rx_packet->data));
			rx_packet->size =
				(((rx_buf[0] >> 24) & 0xff) * 4) + PCIE_VDM_HDR_SIZE;

			/* pass data to enqueue */
			axiado_enqueue_rx_packet(rx_packet);

			rx_packet = axiado_mctp_packet_alloc(GFP_ATOMIC);
			if (!rx_packet) {
				pr_err("Failed to allocate MCTP packet\n");
				return -ENOMEM;
			}

			/* clear the buffer */
			memset_io(rx_buf, 0x00, AXIADO_MCTP_MTU + PCIE_VDM_HDR_SIZE);

			idx_tail = (idx_tail + 1) % BUF_COUNT;
			priv->ocm_ring_tail = priv->ocm_base_virt + (idx_tail * BUFFER_SIZE);
		}
		/* Update once tail reaches head */
		ocm_ring_poll = false;
	}
	axiado_mctp_packet_free(rx_packet);
	return 0;
}

static int axiado_mctp_ocm_thread(void *data)
{
	struct axiado_mctp_devinfo *priv = (struct axiado_mctp_devinfo *)data;
	phys_addr_t phys_head;
	u32 *buf, *next_buf;
	int idx = 0;

	buf = (u32 *)(priv->ocm_ring_head);

	idx = (idx + BUFFER_SIZE) & RING_MASK;
	next_buf = priv->ocm_base_virt + idx;
	phys_head = priv->ocm_res->start + idx;

	while (!kthread_should_stop()) {
		wait_event_interruptible(ocm_ring_wq, ocm_ring_poll);
		/* busy loop poll on the buffer for a change in the data
		 * Once there is a change in the buffer, update the address
		 * in the KW_CSR_BASE_ADRS_PCIE_INT_X2 + 0x03F8 register to
		 * the next buffer.
		 *
		 * To meet the timing requirements, No data processing is done
		 * here. It copies data to OCM ring buffer and axiado_ocm_buffer
		 * thread will do the processing.
		 */
		while (ocm_ring_poll) {
			if (unlikely(READ_ONCE(buf[0]) != 0x00)) {
				/* Change detected */
				axiado_pcie_ocm_csr_write(priv->axiado_pcie_pdev,
							phys_head);

				buf = priv->ocm_ring_head = next_buf;
				idx = (idx + BUFFER_SIZE) & RING_MASK;
				next_buf = priv->ocm_base_virt + idx;
				phys_head = priv->ocm_res->start + idx;
			} else {
				cpu_relax(); /* Efficient busy wait */
			}
		}
	}
	return 0;
}

/*
 * @func axiado_pcie_vdm_init
 * @desc Initializes VDM resources and allocates vdm buffers
 * @param priv initialized mctp devinfo structure
 * @return 0 success
 * @return -ENOMEM failure
 */
static int axiado_pcie_vdm_init(struct axiado_mctp_devinfo *priv)
{
	int ret = 0, i;

	mutex_init(&priv->vdm_info.mtx);
	for (i = 0; i < AX_PCIE_VDM_NUMBUFS; i++) {
		priv->vdm_info.rxda[i] = devm_kzalloc(
			priv->dev, AX_PCIE_VDM_MAXBYTES, GFP_KERNEL);
		if (priv->vdm_info.rxda[i] == NULL) {
			ret = -ENOMEM;
			goto kzalloc_fail;
		}
	}
	for (i = 0; i < AX_PCIE_VDM_NUMBUFS; i++) {
		priv->vdm_info.txda[i] =
			dma_alloc_coherent(priv->dev, AX_PCIE_VDM_MAXBYTES,
					   &priv->vdm_info.txdh[i], GFP_KERNEL);
		if (priv->vdm_info.txda[i] == NULL) {
			ret = -ENOMEM;
			goto dma_alloc_fail;
		}
	}

	/* Initialize wait queue that sync txfers with the poll file ops */
	init_waitqueue_head(&priv->read_queue);

	/* Initialize the data available flag */
	priv->data_available = false;

	/* Start the mesgbox thread for VDM reception */
	priv_g = priv;
	/* All good. Return 0 */
	return ret;
dma_alloc_fail:
	while (i--)
		dma_free_coherent(priv->dev, AX_PCIE_VDM_MAXBYTES,
				  priv->vdm_info.txda[i],
				  priv->vdm_info.txdh[i]);

	i = AX_PCIE_VDM_NUMBUFS;
kzalloc_fail:
	while (i--)
		devm_kfree(priv->dev, priv->vdm_info.rxda[i]);

	return ret;
}

static int axiado_pcie_vdm_deinit(struct axiado_mctp_devinfo *priv)
{
	int i;

	for (i = 0; i < AX_PCIE_VDM_NUMBUFS; i++) {
		dma_free_coherent(priv->dev, AX_PCIE_VDM_MAXBYTES,
				priv->vdm_info.txda[i],
				priv->vdm_info.txdh[i]);
		devm_kfree(priv->dev, priv->vdm_info.rxda[i]);
	}
	return 0;
}

static void axiado_mctp_sync_init(struct axiado_mctp_devinfo *priv)
{
	INIT_LIST_HEAD(&priv->clients);
	INIT_LIST_HEAD(&priv->mctp_type_handlers);
	INIT_LIST_HEAD(&priv->endpoints);

	spin_lock_init(&priv->clients_lock);
	mutex_init(&priv->endpoints_lock);
}

static int axiado_mctp_probe(struct platform_device *pdev)
{
	struct axiado_mctp_devinfo *priv;
	struct device_node *pcie_node;
	struct platform_device *pcie_pdev;
	int ret;
	u32 portno = 0, axrc_ver = 0;
	u32 isolated_cpu;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto out;
	}
	priv->dev = &pdev->dev;
	if (!of_property_read_u32(priv->dev->of_node, "axrc-version",
				&axrc_ver))
		priv->version = (axrc_ver & 0xFF);
	priv->is_rc = (axrc_ver >> 8) ? false : true;
	if (priv->version == AX_VDM_CTRL_VER1) {
		of_property_read_u32(priv->dev->of_node, "axrc-vdm-id",
				&portno);
		priv->vdm_port = (portno == 1) ? PCIE_X1 : PCIE_X2;
	} else {
		/* Could not resolve VDM support. Cannot proceed */
		dev_err(&pdev->dev,
				"Axiado MCTP: Device Tree lookup (VDM support) fail");
		return -ENODEV;
	}

	pcie_node = of_parse_phandle(pdev->dev.of_node, "pcie-phandle", 0);
	if (!pcie_node)
		return -EINVAL;

	pcie_pdev = of_find_device_by_node(pcie_node);
	of_node_put(pcie_node);
	if (!pcie_pdev)
		return -EINVAL;

	pr_info("Successfully found platform device for pcie: %s in mctp\n",
			dev_name(&pcie_pdev->dev));

	priv->axiado_pcie_pdev = pcie_pdev;

	priv->rx_ring_count = RX_RING_COUNT;
	priv->tx_ring_count = TX_RING_COUNT;

	ocm_ring_poll = false;

	/* Initialize all lists and sync functions */
	axiado_mctp_sync_init(priv);

	priv->mctp_miscdev.parent = priv->dev;
	priv->mctp_miscdev.minor = MISC_DYNAMIC_MINOR;
	priv->mctp_miscdev.name = "axiado-mctp";
	priv->mctp_miscdev.fops = &axiado_mctp_fops;
	ret = misc_register(&priv->mctp_miscdev);
	if (ret) {
		dev_err(priv->dev, "Failed to register miscdev\n");
		goto out;
	}
	priv->mctp_miscdev.this_device->type = &axiado_mctp_type;

	priv->peci_mctp = platform_device_register_data(
			priv->dev, (portno == 1) ? "peci0-mctp" : "peci1-mctp",
			PLATFORM_DEVID_NONE, NULL, 0);
	if (IS_ERR(priv->peci_mctp)) {
		dev_err(priv->dev, "Failed to register peci-mctp device\n");
		ret = PTR_ERR(priv->peci_mctp);
		goto out1;
	}

	/* Store device obj in platform specific data */
	platform_set_drvdata(pdev, priv);

	/* Initialize the vdm related RC fields */
	ret = axiado_pcie_vdm_init(priv);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"Axiado MCTP: VDM support initialization fail. %d",
			ret);
		goto out2;
	}

	if (priv->version != AX_VDM_CTRL_VER1) {
		/**
		 * Todo : Need clarification initialisation of rx-packet-count,
		 * rx-ring-count, tx-ring-count, mctp_drv_init to inititalise
		 * work queue and tasklet, mctp_resource_init for IO mapping of
		 * PCIE, mctp_dma_init if required, mctp_channels_init to
		 * initialise rx and tx channels
		 */
		priv->gbx_thread =
			kthread_run(axiado_gbx_thread_func, priv,
					"Global Mailbox Thread");
		if (priv->gbx_thread == NULL) {
			dev_err(priv->dev,
				"Failed to create Global Mailbox Thread\n");
			goto out3;
		}

		return 0;
	}

	/* In A0/A1 platforms, VDMs over mailbox is not working due to hardware
	 * limitation. This OCM threads are workaround to consume the VDMs using
	 * on-chip memory.
	 * ocm_thread - Copy data from PCIe device to OCM. This is time critical
	 * operation. Data will be overwritten if not consumed within certain time.
	 * To achieve it, ocm_thead is bind to dedicated isolated CPU.
	 */
	priv->ocm_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ocm-base");
	if (!priv->ocm_res) {
		pr_err("axiado_mctp: Could not find ocm base address\n");
		ret = -EADDRNOTAVAIL;
		goto out3;
	}

	priv->ocm_base_virt = devm_ioremap_resource(priv->dev, priv->ocm_res);
	if (IS_ERR(priv->ocm_base_virt)) {
		pr_err("ioremap failed\n");
		ret = PTR_ERR(priv->ocm_base_virt);
		goto out3;
	}

	memset_io(priv->ocm_base_virt, 0x00, resource_size(priv->ocm_res));

	priv->ocm_ring_head = priv->ocm_ring_tail = priv->ocm_base_virt;
	axiado_pcie_ocm_csr_write(priv->axiado_pcie_pdev, priv->ocm_res->start);

	priv->ocm_thread = kthread_create(axiado_mctp_ocm_thread, priv, "ocm_thread");
	if (IS_ERR(priv->ocm_thread)) {
		pr_err("Failed to create thread\n");
		ret = PTR_ERR(priv->ocm_thread);
		goto out3;
	}

	ret = of_property_read_u32(priv->dev->of_node, "isolated-cpu",
					&isolated_cpu);
	if (ret < 0) {
		pr_err("Failed to get isolated CPU\n");
		goto out4;
	}

	kthread_bind(priv->ocm_thread, isolated_cpu);
	wake_up_process(priv->ocm_thread);

	priv->ocm_buff_thread =
		kthread_create(axiado_process_ocm_buffer, priv, "ocm_buff_thread");
	if (IS_ERR(priv->ocm_buff_thread)) {
		pr_err("Failed to create pcie buff thread\n");
		kthread_stop(priv->ocm_thread);
		ret = PTR_ERR(priv->ocm_buff_thread);
		goto out4;
	}
	wake_up_process(priv->ocm_buff_thread);

	return 0;

out4:
	kthread_stop(priv->ocm_thread);
out3:
	axiado_pcie_vdm_deinit(priv);
out2:
	platform_device_unregister(priv->peci_mctp);
out1:
	misc_deregister(&priv->mctp_miscdev);
out:
	dev_err(&pdev->dev, "Failed to probe Axiado MCTP: %d\n", ret);
	return ret;
}

static int axiado_mctp_remove(struct platform_device *pdev)
{
	struct axiado_mctp_devinfo *priv = platform_get_drvdata(pdev);

	if (priv->ocm_thread != NULL)
		kthread_stop(priv->ocm_thread);

	if (priv->ocm_buff_thread != NULL)
		kthread_stop(priv->ocm_buff_thread);

	if (priv->gbx_thread != NULL)
		kthread_stop(priv->gbx_thread);

	axiado_pcie_vdm_deinit(priv);

	platform_device_unregister(priv->peci_mctp);

	misc_deregister(&priv->mctp_miscdev);

	return 0;
}

static const struct of_device_id axiado_mctp_match_table[] = {
	{ .compatible = "axiado,ax3000-mctp" },
	{}
};
MODULE_DEVICE_TABLE(of, axiado_mctp_match_table);

static struct platform_driver axiado_mctp_driver = {
	.driver = {
		.name           = "axiado-mctp",
		.of_match_table = of_match_ptr(axiado_mctp_match_table),
	},
	.probe  = axiado_mctp_probe,
	.remove = axiado_mctp_remove,
};

static int __init axiado_mctp_init(void)
{
	packet_cache = kmem_cache_create_usercopy(
			"mctp-packet", sizeof(struct axiado_mctp_pcie_packet), 0, 0, 0,
			sizeof(struct axiado_mctp_pcie_packet), NULL);
	if (!packet_cache)
		return -ENOMEM;

	return platform_driver_register(&axiado_mctp_driver);
}

static void __exit axiado_mctp_exit(void)
{
	priv_g = NULL;
	platform_driver_unregister(&axiado_mctp_driver);
	kmem_cache_destroy(packet_cache);
}

module_init(axiado_mctp_init)
module_exit(axiado_mctp_exit)

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado MCTP driver");
MODULE_LICENSE("GPL");
