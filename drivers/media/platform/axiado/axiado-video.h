/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (c) 2023-2025 Axiado Corporation. */

#ifndef __AXIADO_VIDEO_H__
#define __AXIADO_VIDEO_H__

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#define FRAME_NUM 300
#define BYTES_PER_PIXEL 4 /* The frame BPP */
#define to_axiado_video(x) container_of((x), struct axiado_video, v4l2_dev)

/*!
 * v4l2 frame structure.
 */
struct axiado_v4l2_frame {
	dma_addr_t paddress;
	void *vaddress;
	struct v4l2_buffer buffer;
	struct list_head queue;
	int index;
};

/*!
 * v4l2 video structure.
 */
struct axiado_video {
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct v4l2_format v2f;
	struct v4l2_control ctrl;
	struct video_device vdev;
	struct semaphore busy_lock;
	struct semaphore param_lock;
	int open_count;
	struct list_head ready_q;
	struct list_head done_q;
	struct list_head working_q;
	int enc_counter;
	int write_counter;
	struct axiado_v4l2_frame frame[FRAME_NUM];
	bool capture_on;
	bool cts;
	spinlock_t queue_int_lock;
	spinlock_t dqueue_int_lock;
	wait_queue_head_t write_queue;
	wait_queue_head_t load_queue;
	wait_queue_head_t enc_queue;

	/* standard */
	struct v4l2_streamparm streamparm;
	struct v4l2_standard standard;
	bool standard_autodetect;

	/* The frame size can vary based on the width and height selected \
	 * by the client. 'default_size' stores the default frame_size. Used \
	 * for clearing the buffers allocated. \
	 */
	__u32 default_size;
	void *addr; /* Stores the mapped address for the Physical memory */
};

#endif /* __AXIADO_VIDEO_H__ */
