// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2025 Axiado Corporation.
 *
 * Axiado Video Driver
 *
 * Based on aspeed-video.c
 */

#include <linux/platform_device.h>
#include <linux/of_address.h>
#include "axiado-video.h"

static int frame_count = -1; /* The number of frames requested in VIDIOC_REQBUFS */
static unsigned long frame_interval; /* Stores the interval between each frame */
/* Structure used for passing to the timer_setup function */
struct v4l2_poll_frame {
	struct timer_list timer;
	struct axiado_video *video;
};
static struct v4l2_poll_frame v4l2_poll;


/* Function Declaration */
static void axiado_video_v4l2_read(struct timer_list *t);
static int axiado_streamoff(struct axiado_video *video);

/*
 * V4L2 open interface
 * @param	file *
 * @return	0 on success
 */
static int axiado_video_v4l2_open(struct file *file)
{
	struct video_device *dev;
	struct axiado_video *video;

	dev = video_devdata(file);
	if (dev == NULL)
		return -EINVAL;
	video = video_get_drvdata(dev);
	if (!video) {
		pr_err("ERROR:%s failed\n", __func__);
		return -EBADF;
	}

	down(&video->busy_lock);
	if (signal_pending(current)) {
		up(&video->busy_lock);
		return 0;
	}

	if (video->open_count++ == 0) {
		video->enc_counter = 0;
		video->write_counter = 0;
		INIT_LIST_HEAD(&video->ready_q);
		INIT_LIST_HEAD(&video->working_q);
		INIT_LIST_HEAD(&video->done_q);
	}

	file->private_data = dev;
	up(&video->busy_lock);

	return 0;
}

/*
 * Free frame buffer status
 * @param	struct axiado_video *
 * @return	void
 */
static void axiado_free_frames(struct axiado_video *video)
{
	int i;

	if (video == NULL)
		return;

	for (i = 0; i < FRAME_NUM; i++)
		video->frame[i].buffer.flags = V4L2_BUF_FLAG_MAPPED;

	video->enc_counter = 0;
	video->write_counter = 0;
	INIT_LIST_HEAD(&video->ready_q);
	INIT_LIST_HEAD(&video->working_q);
	INIT_LIST_HEAD(&video->done_q);
}

/*
 * Free frame buffer allocated by dma_alloc_coherent
 * @param	struct axiado_video *
 * @return	0 on success
 */
static int axiado_free_frame_buf(struct axiado_video *video)
{
	int i;

	if (video == NULL)
		return -EINVAL;

	for (i = 0; i < FRAME_NUM; i++) {
		if (video->frame[i].vaddress != 0) {
			dma_free_coherent(video->dev, video->default_size,
					  video->frame[i].vaddress,
					  video->frame[i].paddress);
			video->frame[i].vaddress = 0;
		}
	}

	return 0;
}

/*
 * Allocate memory for requested frame buffers
 * @param	struct axiado_video *, int
 * @return	0 on success
 */
static int axiado_allocate_frame_buf(struct axiado_video *video, int count)
{
	int i;

	if (video == NULL)
		return -EINVAL;

	for (i = 0; i < count; i++) {
		if (video->frame[i].vaddress == 0) {
			video->frame[i].vaddress = dma_alloc_coherent(
				video->dev,
				PAGE_ALIGN(video->v2f.fmt.pix.sizeimage),
				&video->frame[i].paddress,
				GFP_DMA | GFP_KERNEL);
			if (video->frame[i].vaddress == NULL) {
				pr_err("ERROR: v4l2 capture: %s failed.\n",
				       __func__);
				axiado_free_frame_buf(video);
				return -ENOBUFS;
			}
		}
		video->frame[i].buffer.index = i;
		video->frame[i].buffer.flags = V4L2_BUF_FLAG_MAPPED;
		video->frame[i].buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video->frame[i].buffer.length = video->v2f.fmt.pix.sizeimage;
		video->frame[i].buffer.memory = V4L2_MEMORY_MMAP;
		video->frame[i].buffer.m.offset = video->frame[i].paddress;
		video->frame[i].index = i;
	}
	video->default_size = video->v2f.fmt.pix.sizeimage;

	return 0;
}

/*
 * V4L2 interface - release function
 * @param	struct file *
 * @return	0 on success, -EBADF on error
 */
static int axiado_video_v4l2_release(struct file *file)
{
	struct video_device *dev;
	struct axiado_video *video;

	dev = video_devdata(file);
	if (dev == NULL)
		return -EINVAL;
	video = video_get_drvdata(dev);
	if (!video) {
		pr_info("ERROR:%s failed\n", __func__);
		return -EBADF;
	}

	down(&video->busy_lock);
	if (--video->open_count == 0) {
		file->private_data = NULL;
		axiado_free_frame_buf(video);
		file->private_data = NULL;

		/* capture off */
		wake_up_interruptible(&video->enc_queue);
		axiado_free_frames(video);
		video->enc_counter++;
	}
	up(&video->busy_lock);

	return 0;
}

/*
 * Function to return the buffer status
 * @param	struct axiado_video *, struct v4l2_buffer *
 * @return	0 on success, -EINVAL on error
 */
static int axiado_v4l2_buffer_status(struct axiado_video *video,
				    struct v4l2_buffer *buf)
{
	if (video == NULL || buf == NULL)
		return -EINVAL;
	if (buf->index >= FRAME_NUM) {
		pr_err("ERROR: v4l2 capture: %s buffers not allocated\n",
		       __func__);
		return -EINVAL;
	}

	memcpy(buf, &(video->frame[buf->index].buffer), sizeof(*buf));

	return 0;
}

/*
 * V4L2 interface - Dequeue one V4L capture buffer
 * @param	struct axiado_video *, struct v4l2_buffer *
 * @return	0 on success, -ETIME, -EBUSY, -EINVAL on error
 */
static int axiado_v4l2_dqueue(struct axiado_video *video, struct v4l2_buffer *buf)
{
	int ret = 0;
	struct axiado_v4l2_frame *frame;
	unsigned long lock_flags;

	if (video == NULL || buf == NULL)
		return -EINVAL;

	if (video->enc_counter == 0)
		wake_up_interruptible(&video->load_queue);

	if (!wait_event_interruptible_timeout(video->enc_queue,
					      video->enc_counter > 0, 2 * HZ)) {
		pr_err("v4l2 capture: %s timeout enc_counter %x\n", __func__,
		       video->enc_counter);
		return -ETIME;
	} else if (signal_pending(current)) {
		pr_err("v4l2 capture: %s interrupt received\n", __func__);
		del_timer_sync(&v4l2_poll.timer);
		axiado_streamoff(video);
		return -ERESTARTSYS;
	}

	if (down_interruptible(&video->busy_lock))
		return -EBUSY;

	spin_lock_irqsave(&video->dqueue_int_lock, lock_flags);
	if (!list_empty(&video->done_q)) {
		video->enc_counter--;
		video->write_counter--;
		frame = list_entry(video->done_q.next, struct axiado_v4l2_frame,
				   queue);
		list_del(video->done_q.next);
		if (frame->buffer.flags & V4L2_BUF_FLAG_DONE) {
			frame->buffer.flags &= ~V4L2_BUF_FLAG_DONE;
		} else if (frame->buffer.flags & V4L2_BUF_FLAG_QUEUED) {
			pr_err("ERROR: v4l2 capture: VIDIOC_DQBUF: Buffer not filled.\n");
			frame->buffer.flags &= ~V4L2_BUF_FLAG_QUEUED;
			ret = -EINVAL;
		} else if ((frame->buffer.flags & 0x7) ==
			   V4L2_BUF_FLAG_MAPPED) {
			pr_err("ERROR: v4l2 capture: VIDIOC_DQBUF: Buffer not queued.\n");
			ret = -EINVAL;
		}
	} else {
		pr_err("ERROR: v4l2 capture: %s: done_q queue empty\n",
		       __func__);
		spin_unlock_irqrestore(&video->dqueue_int_lock, lock_flags);
		up(&video->busy_lock);
		return -EINVAL;
	}

	buf->bytesused = frame->buffer.bytesused;
	buf->length = frame->buffer.length;
	buf->index = frame->index;
	buf->flags = frame->buffer.flags;
	buf->m = video->frame[frame->index].buffer.m;
	buf->timestamp = video->frame[frame->index].buffer.timestamp;
	buf->field = video->frame[frame->index].buffer.field;

	spin_unlock_irqrestore(&video->dqueue_int_lock, lock_flags);
	up(&video->busy_lock);

	return ret;
}

/*
 * V4L2 interface - ioctl function to get the data format
 * @param	struct axiado_video *, struct v4l2_streamparm *
 * @return	0 on success, on error -EINVAL is returned
 */
static int axiado_v4l2_g_fmt(struct axiado_video *video, struct v4l2_format *fmt)
{
	int ret = 0;

	if (video == NULL || fmt == NULL)
		return -EINVAL;

	pr_debug("Inside %s : type = %d\n", __func__, fmt->type);

	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix = video->v2f.fmt.pix;
		break;

	default:
		pr_err("%s : Invalid Type\n", __func__);
		ret = -EINVAL;
		break;
	}

	pr_debug("End of %s: v2f pix widthxheight %d x %d\n", __func__,
		 video->v2f.fmt.pix.width, video->v2f.fmt.pix.height);

	return ret;
}

/*
 * V4L2 interface - ioctl function to set streaming parameters
 * @param	struct axiado_video *, struct v4l2_streamparm *
 * @return	0 on success, on error -EINVAL is returned
 */
static int axiado_v4l2_s_param(struct axiado_video *video,
			      struct v4l2_streamparm *parm)
{
	struct v4l2_format vid_fmt;
	int err = 0;

	if (video == NULL || parm == NULL)
		return -EINVAL;

	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		pr_info("Invalid type\n");
		return -EINVAL;
	}

	video->streamparm.parm.capture.timeperframe.numerator =
		parm->parm.capture.timeperframe.numerator;
	video->streamparm.parm.capture.timeperframe.denominator =
		parm->parm.capture.timeperframe.denominator;
	video->standard.frameperiod.denominator =
		video->streamparm.parm.capture.timeperframe.denominator;
	/* Calculating the frame interval whenever the framerate is modified */
	frame_interval = (HZ / video->standard.frameperiod.denominator);
	pr_debug("%s : Framerate = %d fps\n", __func__,
		 video->standard.frameperiod.denominator);
	video->streamparm.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	vid_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	pr_debug("%s : width x height of input is %d x %d\n", __func__,
		 vid_fmt.fmt.pix.width, vid_fmt.fmt.pix.height);

	return err;
}

/* V4L2 interface - streamon function to start streaming frames
 * @param	struct axiado_video *
 * @return	0 on success, errno on error
 */
static int axiado_video_v4l2_streamon(struct axiado_video *video)
{
	struct axiado_v4l2_frame *frame;
	unsigned long flags;

	if (video == NULL) {
		pr_err("ERROR: v4l2 capture: %s Video parameter is NULL\n",
		       __func__);
		return -EINVAL;
	}

	if (video->capture_on) {
		pr_err("ERROR: v4l2 capture: %s Video Stream has been turned on\n",
		       __func__);
		return -EBUSY;
	}

	spin_lock_irqsave(&video->queue_int_lock, flags);
	if (!list_empty(&video->ready_q)) {
		frame = list_entry(video->ready_q.next, struct axiado_v4l2_frame,
				   queue);
		frame->buffer.flags |= V4L2_BUF_FLAG_QUEUED;
		list_del(video->ready_q.next);
		list_add_tail(&frame->queue, &video->working_q);
		video->capture_on = true;
		wake_up_interruptible(&video->write_queue);
		spin_unlock_irqrestore(&video->queue_int_lock, flags);
	} else {
		pr_err("ERROR: v4l2 capture: %s: ready_q queue 2 empty\n",
		       __func__);
		spin_unlock_irqrestore(&video->queue_int_lock, flags);
		return -EINVAL;
	}

	/* Begin the timer for initial read */
	v4l2_poll.video = video;
	timer_setup(&v4l2_poll.timer, axiado_video_v4l2_read, 0);
	mod_timer(&v4l2_poll.timer, jiffies + frame_interval);
	pr_debug("%s : Framerate = %d fps\n", __func__,
		 video->standard.frameperiod.denominator);
	return 0;
}

/*
 * V4L2 interface - ioctl function to turn off streaming
 * @param	struct axiado_video *
 * @return	0 on success and errno on error
 */
static int axiado_streamoff(struct axiado_video *video)
{
	if (video == NULL)
		return -EINVAL;
	if (video->capture_on == false)
		return 0;

	del_timer_sync(&v4l2_poll.timer);
	video->capture_on = false;
	axiado_free_frames(video);

	return 0;
}

/*
 * V4L2 interface - load frames into queue
 * @param	struct axiado_video *
 * @param	0 on success; on error -EPIPE, -ERESTARTSYS
 */
static int axiado_v4l2_load_frame(struct axiado_video *video)
{
	struct axiado_v4l2_frame *done_frame = NULL;
	struct axiado_v4l2_frame *ready_frame = NULL;
	unsigned long ns;
	ktime_t ts;

	if (video == NULL)
		return -EINVAL;

	video->write_counter++;
	spin_lock(&video->queue_int_lock);
	spin_lock(&video->dqueue_int_lock);
	if (!list_empty(&video->working_q)) {
		done_frame = list_entry(video->working_q.next,
					struct axiado_v4l2_frame, queue);
		if (done_frame->buffer.flags & V4L2_BUF_FLAG_QUEUED) {
			done_frame->buffer.flags |= V4L2_BUF_FLAG_DONE;
			done_frame->buffer.flags &= ~V4L2_BUF_FLAG_QUEUED;
			/*
			 * Set the current time to done frame buffer's
			 * timestamp. Users can use this information to judge
			 * the frame's usage.
			 */
			ts = ktime_get_real();
			ns = ktime_to_ns(ts);

			done_frame->buffer.timestamp.tv_sec = ns / NSEC_PER_SEC;
			done_frame->buffer.timestamp.tv_usec =
				(ns % NSEC_PER_SEC) / NSEC_PER_USEC;

			/* Added to the done queue */
			list_del(video->working_q.next);
			list_add_tail(&done_frame->queue, &video->done_q);

			/* Wake up the queue */
			video->cts = false;
			video->enc_counter++;
			wake_up_interruptible(&video->enc_queue);
		} else
			pr_err("ERROR: v4l2 capture: camera_callback: buffer not queued\n");
	}

	if (!list_empty(&video->ready_q)) {
		ready_frame = list_entry(video->ready_q.next,
					 struct axiado_v4l2_frame, queue);
		ready_frame->buffer.flags |= V4L2_BUF_FLAG_QUEUED;
		list_del(video->ready_q.next);
		list_add_tail(&ready_frame->queue, &video->working_q);
	}

	spin_unlock(&video->dqueue_int_lock);
	spin_unlock(&video->queue_int_lock);

	return 0;
}

/*
 * V4L2 interface - Function to read frame data from the physical memory
 * @param	struct timer_list *t
 * @return	None
 */
static void axiado_video_v4l2_read(struct timer_list *t)
{
	int ret = -EINVAL;
	static int index;
	struct axiado_video *video;
	struct v4l2_poll_frame *new_timer;

	new_timer = from_timer(new_timer, t, timer);
	video = v4l2_poll.video;
	if (video == NULL)
		return;

	/* Prepare the V4L2 buffer structure */
	if (index >= frame_count)
		index = 0;

	if (video->frame[index].vaddress) {
		/* Copy the frame data from the physical address mapped */
		memcpy(video->frame[index].vaddress, video->addr,
		       video->v2f.fmt.pix.sizeimage);

		video->frame[index].buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video->frame[index].buffer.index = index;
		video->frame[index].buffer.memory = V4L2_MEMORY_MMAP;
		video->frame[index].buffer.bytesused =
			video->v2f.fmt.pix.sizeimage;

		/* Put the frame into queue */
		ret = axiado_v4l2_load_frame(video);
		if (ret) {
			pr_err("%s : Error loading frame\n", __func__);
		} else {
			index++;
			/* Begin the countdown for next read */
			mod_timer(&new_timer->timer, jiffies + frame_interval);
		}
	} else {
		pr_err("%s : Memory not allocated ...\n", __func__);
		return;
	}
}

/*
 * V4L2 interface - write function copy data from userspace
 * @param	struct file *, const char __user *, size_t, loff_t
 * @return	0 on success, on error -EPIPE, -ERESTARTSYS, -ENOMEM
 */
static ssize_t axiado_video_v4l2_write(struct file *file,
				      const char __user *buffer, size_t count,
				      loff_t *ppos)
{
	int ret = -EINVAL;
	unsigned long frame_interval = 0;
	static int index;
	struct video_device *dev;
	struct axiado_video *video;

	dev = video_devdata(file);
	if (dev == NULL)
		return -EINVAL;
	video = video_get_drvdata(dev);
	if (video == NULL)
		return -EINVAL;

	/* Prepare the V4L2 buffer structure */
	if (index >= frame_count)
		index = 0;

	if (!wait_event_interruptible_timeout(video->write_queue,
					      video->capture_on, 10 * HZ)) {
		pr_err("ERROR: %s() timeout write_queue\n", __func__);
		return -EPIPE;
	} else if (signal_pending(current)) {
		pr_err("ERROR: %s() interrupt received\n", __func__);
		return -ERESTARTSYS;
	}

	frame_interval = (HZ / video->standard.frameperiod.denominator);
	if (!wait_event_interruptible_timeout(video->write_queue, video->cts,
					      frame_interval))
		video->cts = true;

	if (!wait_event_interruptible_timeout(
		    video->load_queue, video->write_counter < frame_count,
		    2 * HZ)) {
		pr_err("ERROR: %s() timeout load_queue\n", __func__);
		return -EPIPE;
	} else if (signal_pending(current)) {
		pr_err("ERROR: %s() interrupt received\n", __func__);
		return -ERESTARTSYS;
	}

	if (video->frame[index].vaddress) {
		ret = copy_from_user(video->frame[index].vaddress, (u8 *)buffer,
				     count);
		if (ret) {
			ret = -ENOMEM;
			goto exit1;
		}

		if (count > video->v2f.fmt.pix.sizeimage) {
			pr_err("Invalid Frame Size\n");
			return -EINVAL;
		}

		video->frame[index].buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video->frame[index].buffer.index = index;
		video->frame[index].buffer.memory = V4L2_MEMORY_MMAP;
		video->frame[index].buffer.bytesused = count;

		ret = axiado_v4l2_load_frame(video);
		if (ret) {
			pr_err("%s : Error loading frame\n", __func__);
			ret = -ENOMEM;
		} else {
			ret = count;
			index++;
		}
	} else {
		pr_err("%s : Memory not allocated ...\n", __func__);
		ret = -ENOMEM;
	}

exit1:
	return ret;
}

/*
 * V4L2 interface - ioctls calls function
 * @param	struct file*, unsigned int, void *
 * @return	0 success, -ENODEV, -1 on errors
 */
static long axiado_v4l2_do_ioctl(struct file *file, unsigned int ioctlcmd,
				void *arg)
{
	struct video_device *dev;
	struct axiado_video *video;
	unsigned long lock_flags;
	int ret = 0;

	dev = video_devdata(file);
	if (dev == NULL)
		return -EINVAL;
	video = video_get_drvdata(dev);
	if (video == NULL)
		return -EINVAL;
	if (ioctlcmd != VIDIOC_DQBUF) {
		if (down_interruptible(&video->busy_lock)) {
			pr_info("busy_lock\n");
			return -EBUSY;
		}
	}

	switch (ioctlcmd) {
	case VIDIOC_QUERYCAP: {
		struct v4l2_capability *cap = arg;

		strscpy(cap->driver, dev->name, sizeof(cap->driver));
		cap->capabilities = dev->device_caps;
		break;
	}

	case VIDIOC_REQBUFS: {
		struct v4l2_requestbuffers *req = arg;

		if ((req->count <= 0) || (req->count > FRAME_NUM)) {
			pr_err("ERROR: v4l2 capture: VIDIOC_REQBUFS: not enough buffers\n");
			req->count = FRAME_NUM;
			ret = -EINVAL;
			break;
		}

		if (req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			pr_err("ERROR: v4l2 capture: VIDIOC_REQBUFS: wrong buffer type\n");
			ret = -EINVAL;
			break;
		}
		axiado_streamoff(video);
		if (req->memory & V4L2_MEMORY_MMAP) {
			axiado_free_frame_buf(video);
			ret = axiado_allocate_frame_buf(video, req->count);
			frame_count = req->count;
		}
		break;
	}

	case VIDIOC_QUERYBUF: {
		struct v4l2_buffer *buf = arg;
		int index = buf->index;

		if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			ret = -EINVAL;
			break;
		}
		if (buf->memory & V4L2_MEMORY_MMAP) {
			memset(buf, 0, sizeof(struct v4l2_buffer));
			buf->index = index;
			buf->memory = V4L2_MEMORY_MMAP;
		}
		down(&video->param_lock);
		if (buf->memory & V4L2_MEMORY_MMAP)
			ret = axiado_v4l2_buffer_status(video, buf);
		up(&video->param_lock);
		break;
	}

	case VIDIOC_G_FMT: {
		struct v4l2_format *fmt = arg;

		ret = axiado_v4l2_g_fmt(video, fmt);
		break;
	}

	case VIDIOC_S_PARM: {
		struct v4l2_streamparm *parm = arg;

		ret = axiado_v4l2_s_param(video, parm);
		break;
	}

	case VIDIOC_S_CTRL: {
		struct v4l2_control *ctrl = arg;

		video->ctrl.id = ctrl->id;
		video->ctrl.value = ctrl->value;
		ret = 0;
		break;
	}
	case VIDIOC_QUERY_DV_TIMINGS: {
		struct v4l2_dv_timings *timings = arg;

		timings->type = V4L2_DV_BT_656_1120;
		timings->bt.width = video->v2f.fmt.pix.width;
		timings->bt.height = video->v2f.fmt.pix.height;
		ret = 0;
		break;
	}

	case VIDIOC_QBUF: {
		struct v4l2_buffer *buf = arg;
		int index = buf->index;

		spin_lock_irqsave(&video->queue_int_lock, lock_flags);
		video->frame[index].buffer.m.offset = buf->m.offset;
		if ((video->frame[index].buffer.flags & 0x7) ==
		    V4L2_BUF_FLAG_MAPPED) {
			video->frame[index].buffer.flags |=
				V4L2_BUF_FLAG_QUEUED;
			list_add_tail(&video->frame[index].queue,
				      &video->ready_q);
		} else if (video->frame[index].buffer.flags &
			   V4L2_BUF_FLAG_QUEUED) {
			pr_err("ERROR: v4l2 capture: VIDIOC_QBUF: buffer already queued\n");
			ret = -EINVAL;
		} else if (video->frame[index].buffer.flags &
			   V4L2_BUF_FLAG_DONE) {
			pr_err("ERROR: v4l2 capture: VIDIOC_QBUF: overwrite done buffer.\n");
			video->frame[index].buffer.flags &= ~V4L2_BUF_FLAG_DONE;
			video->frame[index].buffer.flags |=
				V4L2_BUF_FLAG_QUEUED;
			ret = -EINVAL;
		}
		buf->flags = video->frame[index].buffer.flags;
		spin_unlock_irqrestore(&video->queue_int_lock, lock_flags);
		break;
	}

	case VIDIOC_DQBUF: {
		struct v4l2_buffer *buf = arg;

		ret = axiado_v4l2_dqueue(video, buf);
		break;
	}

	case VIDIOC_STREAMON: {
		ret = axiado_video_v4l2_streamon(video);
		break;
	}

	case VIDIOC_STREAMOFF: {
		ret = axiado_streamoff(video);
		break;
	}

	default: {
		pr_info("Error parsing the IOCTL - |%x|\n\n", ioctlcmd);
		ret = -EINVAL;
		break;
	}
	}

	if (ioctlcmd != VIDIOC_DQBUF)
		up(&video->busy_lock);

	return ret;
}

/*
 * V4L interface - ioctl function
 * @param	struct file *, unsigned int, unsigned long
 * @return	0 on success, on error appropriate error code is returned
 */
static long axiado_v4l2_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	return video_usercopy(file, cmd, arg, axiado_v4l2_do_ioctl);
}

/*
 * V4L2 interface file operations mmap function
 * @param	struct file *, struct vm_area_struct *
 * @return	0 Success, EINTR busy lock error, ENOBUFS remap_page error
 */
static int axiado_video_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct axiado_video *video;
	struct video_device *dev;
	unsigned long size;
	int res = 0;

	if (vma == NULL)
		return -EINVAL;
	dev = video_devdata(file);
	if (dev == NULL)
		return -EINVAL;
	video = video_get_drvdata(dev);
	if (video == NULL)
		return -EINVAL;

	pr_debug("\npgoff=0x%lx, start=0x%lx, end=0x%lx\n", vma->vm_pgoff,
		 vma->vm_start, vma->vm_end);

	/* make this _really_ smp-safe */
	if (down_interruptible(&video->busy_lock))
		return -EINTR;

	size = vma->vm_end - vma->vm_start;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
			    vma->vm_page_prot)) {
		pr_err("ERROR: v4l2 capture: %s : remap_pfn_range failed\n",
		       __func__);
		res = -ENOBUFS;
	} else {
		vm_flags_clear(vma, VM_IO); /* using shared anonymous pages */
	}
	up(&video->busy_lock);

	return res;
}

static const struct v4l2_file_operations axiado_video_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = axiado_video_v4l2_open,
	.release = axiado_video_v4l2_release,
	.write = axiado_video_v4l2_write,
	.unlocked_ioctl = axiado_v4l2_ioctl,
	.mmap = axiado_video_mmap,
};

/*
 * This function is called to probe the devices if registered.
 * @param	pdev  the device structure used to give information on which device
 * @return	The function returns 0 on success and -1 on failure.
 */
static int axiado_v4l2_probe(struct platform_device *pdev)
{
	int ret;
	struct axiado_video *video;
	struct v4l2_device *v4l2_dev;
	struct video_device *vdev;
	struct device_node *v4l2_node, *pci_node;
	struct resource res;

	pr_info("V4L2 probe started\n");
	video = devm_kzalloc(&pdev->dev, sizeof(*video), GFP_KERNEL);
	if (!video)
		return -ENOMEM;

	/* Checking the device tree node */
	v4l2_node = of_find_compatible_node(NULL, NULL, "axiado,ax3000-video");
	if (!v4l2_node) {
		pr_err("%s: Cannot find v4l2 node in device tree\n", __func__);
		return -ENODEV;
	}

	/* Parse the memory-region from the device tree */
	pci_node = of_parse_phandle(v4l2_node, "memory-region", 0);
	if (!pci_node) {
		pr_err("%s: No memory-region specified\n", __func__);
		return -EINVAL;
	}

	/* Reading the Physical address from the device tree */
	if (of_address_to_resource(pci_node, 0, &res)) {
		pr_err("%s: Cannot get memory resource from device tree\n",
		       __func__);
		return -EINVAL;
	}

	/* Mapping the Physical memory address */
	video->addr = memremap(res.start, res.end - res.start, MEMREMAP_WB);
	if (!video->addr) {
		pr_err("%s: Physical address is not accessible\n", __func__);
		return -ENOMEM;
	}

	v4l2_dev = &video->v4l2_dev;
	vdev = &video->vdev;

	video->dev = &pdev->dev;
	video->capture_on = false;
	video->cts = true;
	video->v2f.fmt.pix.width = 640;
	video->v2f.fmt.pix.height = 480;

	/*
	 * IKVM calls VIDIOC_REQBUFS before VIDIOC_S_FMT ioctl. \
	 * VIDIOC_REQBUFS will allocate the memory for buffers with the \
	 * 'video->v2f.fmt.pix.sizeimage' value. VIDIOC_S_FMT is \
	 * responsible for updating the 'video->v2f.fmt.pix.sizeimage'. \
	 * Hence, it is better to set the frame format which has the \
	 * maximum possible frame size in the probe function.
	 */
	video->v2f.fmt.pix.sizeimage =
		video->v2f.fmt.pix.width * video->v2f.fmt.pix.height *
		BYTES_PER_PIXEL; /* Expected frames are with 32bpp. */
	video->v2f.fmt.pix.bytesperline =
		video->v2f.fmt.pix.width * BYTES_PER_PIXEL;
	video->v2f.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;

	video->standard.index = 0;
	video->standard.id = V4L2_STD_UNKNOWN;
	/* IKVM has a default framerate of 30FPS */
	video->standard.frameperiod.denominator = 30;
	video->standard.frameperiod.numerator = 1;
	/* Setting the time interval between each frame read */
	frame_interval = (HZ / video->standard.frameperiod.denominator);
	video->standard.framelines = 480;
	video->streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	video->streamparm.parm.capture.timeperframe =
		video->standard.frameperiod;
	video->streamparm.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;

	video->ctrl.id = V4L2_CID_JPEG_CHROMA_SUBSAMPLING;
	video->ctrl.value =
		V4L2_JPEG_CHROMA_SUBSAMPLING_420; //V4L2_JPEG_CHROMA_SUBSAMPLING_444;

	ret = v4l2_device_register(video->dev, v4l2_dev);
	if (ret) {
		dev_err(video->dev, "Failed to register v4l2 device\n");
		return ret;
	}

	vdev->fops = &axiado_video_v4l2_fops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT |
			    V4L2_CAP_READWRITE | V4L2_CAP_STREAMING;
	vdev->v4l2_dev = v4l2_dev;
	strscpy(vdev->name, "axiado-v4l2", sizeof(vdev->name));
	vdev->vfl_type = VFL_TYPE_VIDEO;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->release = video_device_release_empty;

	sema_init(&video->param_lock, 1);
	sema_init(&video->busy_lock, 1);
	init_waitqueue_head(&video->write_queue);
	init_waitqueue_head(&video->load_queue);
	init_waitqueue_head(&video->enc_queue);
	spin_lock_init(&video->queue_int_lock);
	spin_lock_init(&video->dqueue_int_lock);

	video_set_drvdata(vdev, video);
	ret = video_register_device(vdev, VFL_TYPE_VIDEO, 0);
	if (ret != 0) {
		pr_err("video_register_device failed ret=%d\n", ret);
		return ret;
	}
	pr_info("V4L2 probe finished\n");
	return 0;
}

/*
 * This function is called to remove the devices when device unregistered.
 * @param	pdev  the device structure used to give information
 * on which device remove
 * @return	The function returns 0 on success and -1 on failure.
 */
static int axiado_v4l2_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct v4l2_device *v4l2_dev = dev_get_drvdata(dev);
	struct axiado_video *video = to_axiado_video(v4l2_dev);

	axiado_free_frame_buf(video);
	axiado_free_frames(video);

	/* Unmap the memory region */
	if (video->addr)
		memunmap(video->addr);

	video_unregister_device(&video->vdev);
	v4l2_device_unregister(v4l2_dev);
	del_timer_sync(&v4l2_poll.timer);

	return 0;
}

static const struct of_device_id axiado_v4l2_of_match[] = {
	{
		.compatible = "axiado,ax3000-video",
	},
};

static struct platform_driver axiado_v4l2_driver = {
	.driver	= {
		.name = "axiado-video",
		.owner = THIS_MODULE,
		.of_match_table = axiado_v4l2_of_match,
	},
	.probe = axiado_v4l2_probe,
	.remove = axiado_v4l2_remove,
};

module_platform_driver(axiado_v4l2_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado Video platform driver");
MODULE_LICENSE("GPL");
