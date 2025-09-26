// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  AXIADO HID + Input Events passes
 *  to HPM bridging function.
 *
 *  Copyright (C) 2025 Axiado Corporation.
 *
 *  Based on hid-generic.c
 *  Copyright (c) 1999 Andreas Gal
 *  Copyright (c) 2000-2005 Vojtech Pavlik <vojtech@suse.cz>
 *  Copyright (c) 2005 Michael Haboustak <mike-@cinci.rr.com> for Concept2, Inc
 *  Copyright (c) 2007-2008 Oliver Neukum
 *  Copyright (c) 2006-2012 Jiri Kosina
 *  Copyright (c) 2012 Henrik Rydberg
 *
 *  Based on evbug.c
 *  Copyright (c) 1999-2001 Vojtech Pavlik
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/hid.h>

static int axiado_input_event_connect(struct input_handler *handler,
				      struct input_dev *dev,
				      const struct input_device_id *id)
{
	struct input_handle *handle;
	int ret;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "axiado-input-event";

	ret = input_register_handle(handle);
	if (ret)
		goto free_handle;

	ret = input_open_device(handle);
	if (ret)
		goto unregister_handle;

	return 0;

unregister_handle:
	input_unregister_handle(handle);
free_handle:
	kfree(handle);
	return ret;
}

static void axiado_input_event_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id axiado_input_ids[] = {
	{ .driver_info = 1 }, /* Match all devices */
	{ },			/* Terminating zero entry */
};

static struct input_handler axiado_input_handler = {
	.connect = axiado_input_event_connect,
	.disconnect = axiado_input_event_disconnect,
	.name = "axiado-input-event",
	.id_table = axiado_input_ids,
};

static int hid_axiado_input_report(struct hid_device *hdev, struct hid_report *report,
				   u8 *data, int len)
{
	int ret;

	/* Forward raw HID data to bridge function */
	ret = ax_hpm_bridge_data_forward(data, len, hdev->rdesc);
	if (ret)
		return ret;

	return 0;
}

static int hid_axiado_probe(struct hid_device *hdev,
			    const struct hid_device_id *id)
{
	int ret;

	hdev->quirks |= HID_QUIRK_INPUT_PER_APP;

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	return hid_hw_start(hdev, HID_CONNECT_DEFAULT);
}

/* Generic HID device table */
static const struct hid_device_id hid_axiado_table[] = {
	{ HID_DEVICE(HID_BUS_ANY, HID_GROUP_ANY, HID_ANY_ID, HID_ANY_ID) },
	{ }
};
MODULE_DEVICE_TABLE(hid, hid_axiado_table);

static struct hid_driver hid_axiado = {
	.name = "hid-axiado",
	.id_table = hid_axiado_table,
	.probe = hid_axiado_probe,
	.raw_event = hid_axiado_input_report,
};

static int __init axiado_hid_init(void)
{
	int ret;

	ret = input_register_handler(&axiado_input_handler);
	if (ret)
		return ret;

	return hid_register_driver(&hid_axiado);
}

static void __exit axiado_hid_exit(void)
{
	hid_unregister_driver(&hid_axiado);
	input_unregister_handler(&axiado_input_handler);
}

module_init(axiado_hid_init);
module_exit(axiado_hid_exit);

MODULE_DESCRIPTION("AXIADO HID + Input Event driver for HPM bridge");
MODULE_LICENSE("GPL");
