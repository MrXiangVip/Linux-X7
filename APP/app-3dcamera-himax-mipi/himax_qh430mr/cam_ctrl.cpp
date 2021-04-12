/*
 * Copyright 2019 NXP Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include "cam_ctrl.h"
#include "hx_reg.h"
#include "libsimplelog.h"
#include "config.h"
#include "face_alg.h"
#include "libyuv.h"
#include "img_dbg.h"
#include "per_calc.h"
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

static uint8_t g_cap_frm_buf[CFG_CAM_WIDTH * CFG_CAM_HEIGHT * 2];

#if CFG_FACE_BIN_LIVENESS
static uint16_t g_depth_cvt_buf[CFG_DEPTH_ACTUAL_FRAME_SIZE];
#endif

#if CFG_ENABLE_IR_CLKWISE_ROTATE
static uint8_t g_ir_rot_angle_buf[CFG_IR_FRAME_SIZE] = { 0 };
#endif

#if CFG_ENABLE_DEPTH_CLKWISE_ROTATE
static uint16_t g_depth_rotate_buf[CFG_DEPTH_ACTUAL_FRAME_SIZE] = { 0 };
#endif

#if CFG_ENABLE_V4L2_SUPPORT
static cam_ctrl_buffer_t g_cam_bufs[CFG_V4L2_BUF_NUM];
#endif

#if CFG_ENABLE_CAM_I2C || CFG_CAM_BRIDGE_IF_SUPPORT
static int32_t __i2c_read_word(int32_t fd, uint16_t sub_addr, uint16_t *val)
{
	uint8_t addr[2] = { 0 };
	uint8_t data[2] = { 0 };

	addr[0] = sub_addr >> 8;
	addr[1] = sub_addr & 0xff;
	if (2 != write(fd, addr, 2)) {
		log_error("Write sub address %d failed!\n", sub_addr);
		return RET_FAIL;
	}

	if (2 != read(fd, data, 2)) {
		log_error("Read byte failed!\n");
		return RET_FAIL;
	}

	*val = (data[0] << 8) | data[1];
	log_error("reg: 0x%x, val: 0x%x!\n", sub_addr, *val);
	return RET_OK;
}

static int32_t __i2c_write_word(int32_t fd_i2c, uint16_t sub_addr, uint16_t val)
{
	uint8_t data[4] = { 0 };

	data[0] = sub_addr >> 8;
	data[1] = sub_addr & 0xff;
	data[2] = val >> 8;
	data[3] = val & 0xff;
	if (4 != write(fd_i2c, data, 4)) {
		log_error("I2C Write word %d failed!\n", sub_addr);
		return RET_FAIL;
	}

	return RET_OK;
}

static int32_t __i2c_read_byte(int32_t fd, uint16_t sub_addr, uint8_t *val)
{
	uint8_t addr[2] = { 0 };

	addr[0] = sub_addr >> 8;
	addr[1] = sub_addr & 0xff;
	if (2 != write(fd, addr, 2)) {
		log_error("Write sub address %d failed!\n", sub_addr);
		return RET_FAIL;
	}

	if (1 != read(fd, val, 1)) {
		log_error("Read byte failed!\n");
		return RET_FAIL;
	}
	log_error("reg: 0x%x, val: 0x%x!\n", sub_addr, *val);
	return RET_OK;
}

static int32_t __i2c_write_byte(int32_t fd, uint16_t sub_addr, uint8_t val)
{
	uint8_t data[3] = { 0 };

	data[0] = sub_addr >> 8;
	data[1] = sub_addr & 0xff;
	data[2] = val;
	if (write(fd, data, 3) != 3) {
		log_error("I2C write failed!\n");
		return RET_FAIL;
	} else
		return RET_OK;
}
#endif

int32_t cam_ctrl_config_depth(cam_ctrl_t *cam_ctrl, uint32_t width, uint32_t height, uint32_t fps)
{
	cam_ctrl->depth_width = width;
	cam_ctrl->depth_height = height;
	return RET_OK;
}

int32_t cam_ctrl_config_ir(cam_ctrl_t *cam_ctrl, uint32_t width, uint32_t height, uint32_t fps)
{
	cam_ctrl->ir_width = width;
	cam_ctrl->ir_height = height;
	return RET_OK;
}

int32_t cam_ctrl_enable_ir_and_depth(cam_ctrl_t *cam_ctrl)
{
	return RET_OK;
}

int32_t cam_ctrl_enable_ir(cam_ctrl_t *cam_ctrl)
{
	return RET_OK;
}

int32_t cam_ctrl_enable_depth(cam_ctrl_t *cam_ctrl)
{
	return RET_OK;
}

int32_t cam_ctrl_switch_stream(cam_ctrl_t *cam_ctrl, int32_t strm_mode)
{
	int32_t ret = 0;

	switch (strm_mode) {
	case STREAM_IR:
		ret = cam_ctrl_enable_ir(cam_ctrl);
		break;
	case STREAM_DEPTH:
		ret = cam_ctrl_enable_depth(cam_ctrl);
		break;
	default:
		break;
	}
	return ret;
}

#if CFG_CAMERA_CROP_OUTPUT
int32_t cam_ctrl_crop_img(uint8_t *in_buf, uint32_t in_buf_width,
		uint32_t in_buf_height, uint8_t *out_buf, uint32_t out_buf_width, uint32_t out_buf_height)
{
	uint32_t i = 0;
	uint8_t *tmp_in_buf = in_buf, *tmp_out_buf = out_buf;

	for (i = 0; i < out_buf_height;
			++i, tmp_in_buf += (in_buf_width << 1), tmp_out_buf += (out_buf_width << 1)) {
		memcpy(tmp_out_buf, tmp_in_buf, out_buf_width << 1);
	}
	return RET_OK;
}
#endif

#if CFG_CAMERA_SCALE_OUTPUT
static int32_t cam_ctrl_scale_img(uint8_t *in_buf, uint32_t in_buf_width,
		uint32_t in_buf_height, uint8_t *out_buf, uint32_t out_buf_width, uint32_t out_buf_height)
{
	return RET_OK;
}

static int32_t cam_ctrl_scale_img_16(uint16_t *in_buf, uint32_t in_buf_width,
		uint32_t in_buf_height, uint16_t *out_buf, uint32_t out_buf_width, uint32_t out_buf_height)
{
	libyuv::ScalePlane_16(in_buf, in_buf_width * 2, in_buf_width, in_buf_height,
			out_buf, out_buf_width * 2, out_buf_width,
			out_buf_height, libyuv::kFilterNone);
	return RET_OK;
}
#endif

#if 0
static int32_t cam_ctrl_rotateplane16(const uint16_t *src, uint16_t *dst,
							int32_t width, int32_t height,
							enum libyuv::RotationMode mod)
{
  	uint16_t *tmp_dst = dst;
	const uint16_t *tmp_src = src;
	if (libyuv::kRotate270 == mod) {
		tmp_dst += height * (width - 1);
		for (int32_t i = 0; i < width; ++i) {
			for (int32_t j = 0; j < height; ++j) {
				tmp_dst[j] = tmp_src[j * width];
			}
			++tmp_src;
			tmp_dst -= height;
		}
		return 0;
	} else {
		return -1;
	}
}
#endif

#if (CFG_ENABLE_CAM_I2C || CFG_CAM_BRIDGE_IF_SUPPORT)
static int32_t cam_ctrl_load_settings(int32_t fd_i2c, cam_ctrl_reg_cfg_t *reg_cfg, int32_t size)
{
	int32_t i = 0;
	uint16_t reg_addr = 0;
	uint16_t reg_val = 0;
	uint16_t reg_val_len = 0;
	uint32_t delay_ms = 0;
	int32_t retval = 0;

	for (i = 0; i < size; ++i, ++reg_cfg) {
		reg_addr = reg_cfg->reg_addr;
		reg_val = reg_cfg->val;
		reg_val_len = reg_cfg->val_len;
		delay_ms = reg_cfg->delay_ms;

		if (2 == reg_val_len)
			retval = __i2c_write_word(fd_i2c, reg_addr, reg_val);
		else if (1 == reg_val_len)
			retval = __i2c_write_byte(fd_i2c, reg_addr, reg_val);
		else
			return RET_FAIL;

        if (retval < 0)
			return RET_FAIL;

		if (delay_ms)
			usleep(delay_ms * 1000);
	}
	return RET_OK;
}
#endif

#if CFG_CAM_BRIDGE_IF_SUPPORT
static cam_ctrl_reg_cfg_t cam_ctrl_bridge_settings[] = {
	{0x0002, 2, 0x0001, 1, 10},
	{0x0002, 2, 0x0000, 1, 0},
	{0x0016, 2, 0x3063, 1, 0},
	{0x0018, 2, 0x0403, 1, 1000},
	{0x0018, 2, 0x0413, 1, 0},
	{0x0020, 2, 0x0011, 1, 0},
	{0x0060, 2, 0x800a, 1, 0},
	{0x0006, 2, 0x00c8, 1, 0},
	{0x0008, 2, 0x0001, 1, 0},
	{0x0004, 2, 0x8174, 1, 0},
};

static int32_t cam_ctrl_init_bridge(const cam_ctrl_init_t *cam_init)
{
	int32_t fd_i2c = 0;
	int32_t ret = 0;

	fd_i2c = open(CFG_CAM_BRIDGE_I2C_DEV, O_RDWR);
	if (fd_i2c < 0) {
		log_error("Open i2c device %s failed!\n", CFG_CAM_BRIDGE_I2C_DEV);
		return RET_FAIL;
	}

	ret = ioctl(fd_i2c, I2C_SLAVE, CFG_CAM_BRIDGE_I2C_SLAVE_ADDR);
	if (ret < 0) {
		log_error("i2c set addr failed: 0x%x: %d, Err: %d!\n", CFG_CAM_BRIDGE_I2C_SLAVE_ADDR, ret, errno);
		goto err;
	}

	if (cam_ctrl_load_settings(fd_i2c, cam_ctrl_bridge_settings, ARRAY_SIZE(cam_ctrl_bridge_settings)))
		goto err;

	return RET_OK;
err:
	if (fd_i2c)
		close(fd_i2c);
	return RET_FAIL;
}
#endif

#if CFG_ENABLE_CAM_I2C
static cam_ctrl_reg_cfg_t cam_ctrl_camera_settings[] = {
	{0xC005, 2, 0x49, 1, 0},
	{0xC073, 2, 0x65, 1, 0},
	{0xC092, 2, 0x42, 1, 0},
	{0xC093, 2, 0x64, 1, 0},
	{0xC0B9, 2, 0x1,  1, 0},
	{0xC0A3, 2, 0x64, 1, 0},
	{0xC0AA, 2, 0x6,  1, 0},
	{0xC0BF, 2, 0x3,  1, 0},
	{0xC0A0, 2, 0x1,  1, 0},
	{0xC0BF, 2, 0x3,  1, 0},
	{0xC0A0, 2, 0x3,  1, 0},
	{0xC0BF, 2, 0x3,  1, 0},
	{0xc004, 2, 0x0,  1, 0},
	{0xc0bf, 2, 0x1,  1, 0},
	{0xc003, 2, 0x0,  1, 0},
	{0xc0b6, 2, 0x1,  1, 0},
	{0xc07F, 2, 0x1,  1, 0},
	{0xc0bf, 2, 0x1,  1, 0},
	{0xc249, 2, 0x0,  1, 0},
	{0xc24A, 2, 0x12, 1, 0},
	{0xc24B, 2, 0x12, 1, 0},
	{0xc24C, 2, 0x12, 1, 0},
	{0xC0BF, 2, 0x3,  1, 0},
};

static int32_t cam_ctrl_init_camera(const cam_ctrl_init_t *cam_init)
{
	int32_t fd_i2c = 0;
	int32_t ret = 0;

	fd_i2c = open(CFG_CAM_I2C_DEV, O_RDWR);
	if (fd_i2c < 0) {
		log_error("Open i2c device %s failed!\n", CFG_CAM_I2C_DEV);
		return RET_FAIL;
	}

	//ret = ioctl(fd_i2c, I2C_SLAVE, CFG_CAM_I2C_SLAVE_ADDR);
	ret = ioctl(fd_i2c, I2C_SLAVE_FORCE, CFG_CAM_I2C_SLAVE_ADDR);
	if (ret < 0) {
		log_error("i2c set addr failed: 0x%x: %d, Err: %d!\n", CFG_CAM_I2C_SLAVE_ADDR, ret, errno);
		goto err;
	}

	if (cam_ctrl_load_settings(fd_i2c, cam_ctrl_camera_settings, ARRAY_SIZE(cam_ctrl_camera_settings)))
		goto err;

	return RET_OK;
err:
	if (fd_i2c)
		close(fd_i2c);
	return RET_FAIL;

}
#endif

int32_t cam_ctrl_init(const cam_ctrl_init_t *cam_init, cam_ctrl_t *out_cam_ctrl)
{
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	int32_t fd_v4l = 0, ret;
#if CFG_ENABLE_CAM_I2C
	int32_t fd_i2c = 0;
#endif

	if (!cam_init || !out_cam_ctrl) {
		log_error("NULL cam pointer!\n");
		return RET_FAIL;
	}

#if CFG_CAM_BRIDGE_IF_SUPPORT
	if (cam_ctrl_init_bridge(cam_init)) {
		log_error("Bridge init failed!\n");
		return RET_FAIL;
	}
#endif

#if CFG_ENABLE_CAM_I2C
	if (cam_ctrl_init_camera(cam_init)) {
		log_error("Camera init settings failed!\n");
		return RET_FAIL;
	}
#endif

	if ((fd_v4l = open(cam_init->v4l2_dev, O_RDWR, 0)) < 0) {
		log_error("unable to open %s\n", cam_init->v4l2_dev);
		goto err;
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.capturemode = 0;
	parm.parm.capture.timeperframe.denominator = 15;
	parm.parm.capture.timeperframe.numerator = 1;
	ret = ioctl(fd_v4l, VIDIOC_S_PARM, &parm);
	if (ret < 0) {
		log_error("VIDIOC_S_PARM failed: %d\n", ret);
		goto err;
	}

	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = cam_init->fmt;
	fmt.fmt.pix.width = cam_init->width;
	fmt.fmt.pix.height = cam_init->height;
	if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0) {
		log_error("set format failed: %d\n", ret);
		goto err;
	}
	out_cam_ctrl->v4l2_fd = fd_v4l;
#if CFG_ENABLE_CAM_I2C
	out_cam_ctrl->i2c_fd = fd_i2c;
#endif

#if CFG_CAM_USE_HX_PP_LIB
	if (hx_init(CFG_HIMAX_CAM_CONFIG_PATH)) {
		log_error("hx library init failed!");
		goto err;
	}
	hx_cfg_input(CFG_DEPTH_FRAME_ACTUAL_HEIGHT, CFG_DEPTH_FRAME_ACTUAL_WIDTH);
	hx_cfg_upscale(CFG_FACE_LIB_FRAME_WIDTH, CFG_FACE_LIB_FRAME_HEIGHT);
#endif

	return RET_OK;
err:
	if (fd_v4l > 0)
		close(fd_v4l);
#if CFG_ENABLE_CAM_I2C
	if (fd_i2c > 0)
		close(fd_i2c);
#endif
	return RET_FAIL;
}

int32_t cam_ctrl_start_stream(cam_ctrl_t *cam_ctrl)
{
	uint32_t i;
	struct v4l2_buffer buf;
	enum v4l2_buf_type type;
	struct v4l2_requestbuffers req;
	int32_t v4l2_fd = cam_ctrl->v4l2_fd;

	memset(&g_cam_bufs, 0, sizeof(struct cam_ctrl_buffer) * CFG_V4L2_BUF_NUM);

	memset(&req, 0, sizeof(struct v4l2_requestbuffers));
	req.count = CFG_V4L2_BUF_NUM;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	// REQBUFS
	if (ioctl(v4l2_fd, VIDIOC_REQBUFS, &req) < 0) {
		log_error("VIDIOC_REQBUFS failed!\n");
		return RET_FAIL;
	}

	// QUERYBUF
	for (i = 0; i < CFG_V4L2_BUF_NUM; i++) {
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf) < 0) {
			log_error("VIDIOC_QUERYBUF error\n");;
			return RET_FAIL;
		}

		g_cam_bufs[i].length = buf.length;
		g_cam_bufs[i].offset = (size_t)buf.m.offset;
		g_cam_bufs[i].start = (uint8_t *)mmap(NULL, g_cam_bufs[i].length,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			v4l2_fd, g_cam_bufs[i].offset);
		if (NULL == g_cam_bufs[i].start) {
			log_error("mmap failed!");
		}
		memset(g_cam_bufs[i].start, 0xFF, g_cam_bufs[i].length);
	}

	// QBUF all buffers ready for streamon
	for (i = 0; i < CFG_V4L2_BUF_NUM; i++)
	{
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.length = g_cam_bufs[i].length;
		buf.m.offset = g_cam_bufs[i].offset;

		if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0) {
			log_error("VIDIOC_QBUF error\n");
			return RET_FAIL;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(v4l2_fd, VIDIOC_STREAMON, &type) < 0) {
		log_error("VIDIOC_STREAMON error\n");
		return -1;
	}

	return RET_OK;
}

int32_t cam_ctrl_stop_stream(cam_ctrl_t *cam_ctrl)
{
	enum v4l2_buf_type type;
	int32_t i;
	int32_t v4l2_fd = cam_ctrl->v4l2_fd;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	// stream off first
	ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type);

	// unmap buffers
	for (i = 0; i < CFG_V4L2_BUF_NUM; i++) {
		if (g_cam_bufs[i].start) {
			munmap(g_cam_bufs[i].start, g_cam_bufs[i].length);
			g_cam_bufs[i].start = NULL;
		}
	}

	return RET_OK;
}

int32_t cam_ctrl_resume_stream(cam_ctrl_t *cam_ctrl)
{
	enum v4l2_buf_type type;
	int32_t v4l2_fd = cam_ctrl->v4l2_fd;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	// stream off first
	ioctl(v4l2_fd, VIDIOC_STREAMON, &type);

	return RET_OK;
}

int32_t cam_ctrl_pause_stream(cam_ctrl_t *cam_ctrl)
{
	enum v4l2_buf_type type;
	int32_t v4l2_fd = cam_ctrl->v4l2_fd;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	// stream off first
	ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type);

	return RET_OK;
}

int32_t cam_ctrl_deinit(cam_ctrl_t *cam_ctrl)
{
	int32_t v4l2_fd = cam_ctrl->v4l2_fd;
#if CFG_ENABLE_CAM_I2C
	int32_t i2c_fd = cam_ctrl->i2c_fd;
#endif

	if (v4l2_fd) {
		close(v4l2_fd);
	}
#if CFG_ENABLE_CAM_I2C
	if (i2c_fd) {
		close(i2c_fd);
	}
#endif

#if CFG_CAM_USE_HX_PP_LIB
	hx_deinit();
#endif

	return RET_OK;
}

static int32_t cam_ctrl_buf_pre_process(cam_ctrl_t *cam_ctrl, uint8_t *in_raw_data)
{
	int32_t i = 0;

	for (i = 0; i < CFG_CAM_WIDTH * CFG_CAM_HEIGHT; i += 16) {
		int32_t i_2 = i * 2;
		in_raw_data[i] = in_raw_data[i_2];
		in_raw_data[i + 1] = in_raw_data[i_2 + 2];
		in_raw_data[i + 2] = in_raw_data[i_2 + 4];
		in_raw_data[i + 3] = in_raw_data[i_2 + 6];
		in_raw_data[i + 4] = in_raw_data[i_2 + 8];
		in_raw_data[i + 5] = in_raw_data[i_2 + 10];
		in_raw_data[i + 6] = in_raw_data[i_2 + 12];
		in_raw_data[i + 7] = in_raw_data[i_2 + 14];
		in_raw_data[i + 8] = in_raw_data[i_2 + 16];
		in_raw_data[i + 9] = in_raw_data[i_2 + 18];
		in_raw_data[i + 10] = in_raw_data[i_2 + 20];
		in_raw_data[i + 11] = in_raw_data[i_2 + 22];
		in_raw_data[i + 12] = in_raw_data[i_2 + 24];
		in_raw_data[i + 13] = in_raw_data[i_2 + 26];
		in_raw_data[i + 14] = in_raw_data[i_2 + 28];
		in_raw_data[i + 15] = in_raw_data[i_2 + 30];
	}
	return 0;
}

int32_t cam_ctrl_deq_frame_data_ptr(cam_ctrl_t *cam_ctrl, uint8_t **out_data, int32_t frm_type)
{
	int32_t v4l2_fd = cam_ctrl->v4l2_fd;
	int32_t loop_for_img = 1;

	do {
		uint8_t *buf_ptr = NULL;
		struct v4l2_buffer *buf = &cam_ctrl->frm_buf;
		int32_t got_frm_type = 0;

		// DQBUF for ready buffer
		memset(buf, 0, sizeof(struct v4l2_buffer));
		buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf->memory = V4L2_MEMORY_MMAP;
		if (ioctl(v4l2_fd, VIDIOC_DQBUF, buf) < 0) {
			log_error("VIDIOC_DQBUF failed\n");
			return RET_FAIL;
		}

		memcpy(g_cap_frm_buf, g_cam_bufs[buf->index].start, CFG_CAM_WIDTH * CFG_CAM_HEIGHT * 2);
		//buf_ptr = g_cam_bufs[buf->index].start;
		buf_ptr = g_cap_frm_buf;

#if CFG_PER_TIME_CAL_FRAME_PROCESS
		per_calc_start();
#endif
		// Preprocessing, 16bit to 8bit
		cam_ctrl_buf_pre_process(cam_ctrl, buf_ptr);
#if CFG_PER_TIME_CAL_FRAME_PROCESS
		per_calc_end();
		log_info("16bit => 8bit: %d usec\n", per_calc_get_result());
#endif

		if (frm_type == FRAME_TYPE_ANY) {
			*out_data = buf_ptr;
			loop_for_img = 0;
		} else {
			got_frm_type = cam_ctrl_get_frame_type(buf_ptr, CFG_CAM_WIDTH, CFG_CAM_HEIGHT);
			if (frm_type == got_frm_type) {
				*out_data = buf_ptr;
				loop_for_img = 0;
			} else {
				cam_ctrl_q_frame_data_ptr(cam_ctrl);
			}
			//log_debug("frame type: %d\n", got_frm_type);
		}
	} while (loop_for_img);

	return RET_OK;
}

int32_t cam_ctrl_q_frame_data_ptr(cam_ctrl_t *cam_ctrl)
{
	// QBUF return the used buffer back
	if (ioctl(cam_ctrl->v4l2_fd, VIDIOC_QBUF, &cam_ctrl->frm_buf) < 0) {
		log_error("VIDIOC_QBUF failed\n");
		return RET_FAIL;
	} else {
		return RET_OK;
	}
}

int32_t cam_ctrl_get_frame_data(cam_ctrl_t *cam_ctrl, uint8_t *out_data, int32_t frm_type)
{
	struct v4l2_buffer buf;
	int32_t v4l2_fd = cam_ctrl->v4l2_fd;
	int32_t loop_for_img = 1;

	do {
		uint8_t *buf_ptr = NULL;

		// DQBUF for ready buffer
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(v4l2_fd, VIDIOC_DQBUF, &buf) < 0) {
			log_error("VIDIOC_DQBUF failed\n");
			return RET_FAIL;
		}

		buf_ptr = g_cam_bufs[buf.index].start;
		if (frm_type == FRAME_TYPE_ANY
				|| frm_type == cam_ctrl_get_frame_type(buf_ptr, CFG_CAM_WIDTH, CFG_CAM_HEIGHT)) {
			memcpy(out_data, buf_ptr, buf.length);
			loop_for_img = 0;
		}

		// QBUF return the used buffer back
		if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0) {
			log_error("VIDIOC_QBUF failed\n");
			return RET_FAIL;
		}
	} while (loop_for_img);

	return RET_OK;
}

int32_t cam_ctrl_get_frame_type(const uint8_t *in_data, int32_t width, int32_t height)
{
	return (uint8_t)*(in_data + width * height - 1);
}

static inline void cam_ctrl_scale_col_4_16(uint16_t *dst_ptr, const uint16_t *src_ptr,
		int32_t dst_width)
{
  	int32_t j;

	for (j = 0; j < dst_width - 1; j += 4) {
		dst_ptr[3] = dst_ptr[2] = dst_ptr[1] = dst_ptr[0] = src_ptr[0];
		src_ptr += 1;
		dst_ptr += 4;
	}
}

static int32_t cam_ctrl_extract_depth_frame(cam_ctrl_t *cam_ctrl, uint16_t *in_raw_depth, uint16_t *out_depth_frm)
{
	int32_t x = 0, y = 0;

	for (y = 0; y < CFG_DEPTH_FRAME_ACTUAL_HEIGHT; ++y) {
		for (x = 0; x < CFG_DEPTH_FRAME_ACTUAL_WIDTH; ++x) {
			out_depth_frm[y * CFG_DEPTH_FRAME_ACTUAL_WIDTH + x] = in_raw_depth[y * 6 * CFG_DEPTH_CAM_WIDTH / 2 + x];
		}
	}
	return 0;
}

int32_t cam_ctrl_cvt_to_depth(cam_ctrl_t *cam_ctrl, uint16_t *in_raw_depth, uint16_t *out_depth,
						int32_t lib_width, int32_t lib_height)
{
#if CFG_ENABLE_DEPTH_CLKWISE_ROTATE
	enum libyuv::RotationMode dep_rot_angle = libyuv::kRotate0;
#endif

#if CFG_PER_TIME_CAL_FRAME_PROCESS
		per_calc_start();
#endif
	/* Need to convert: 1280x800 => 640x400 => 400x640 */
	// Extract data from 1280x800
	cam_ctrl_extract_depth_frame(cam_ctrl, in_raw_depth, g_depth_cvt_buf);
#if CFG_PER_TIME_CAL_FRAME_PROCESS
	per_calc_end();
	log_info("Extract depth: %d usec\n", per_calc_get_result());
#endif

	// Rotate to 400x640
#if CFG_ENABLE_DEPTH_CLKWISE_ROTATE
	#if (CFG_DEPTH_CLKWISE_ROTATE_90 == 1)
		dep_rot_angle = libyuv::kRotate90;
	#elif (CFG_DEPTH_CLKWISE_ROTATE_180 == 1)
		dep_rot_angle = libyuv::kRotate180;
	#elif (CFG_DEPTH_CLKWISE_ROTATE_270 == 1)
		dep_rot_angle = libyuv::kRotate270;
	#endif

#if CFG_PER_TIME_CAL_FRAME_PROCESS
	per_calc_start();
#endif
	//cam_ctrl_rotateplane16(g_depth_cvt_buf, g_depth_rotate_buf, CFG_DEPTH_FRAME_ACTUAL_WIDTH, CFG_DEPTH_FRAME_ACTUAL_HEIGHT, dep_rot_angle);
	libyuv::RotatePlane_16((const uint8_t *)g_depth_cvt_buf, CFG_DEPTH_FRAME_ACTUAL_WIDTH * 2, (uint8_t *)g_depth_rotate_buf,
			CFG_DEPTH_FRAME_ACTUAL_HEIGHT * 2, CFG_DEPTH_FRAME_ACTUAL_WIDTH, CFG_DEPTH_FRAME_ACTUAL_HEIGHT, dep_rot_angle);
#if CFG_PER_TIME_CAL_FRAME_PROCESS
	per_calc_end();
	log_info("rotateplane16: %d usec\n", per_calc_get_result());
#endif // CFG_PER_TIME_CAL_FRAME_PROCESS
#endif // CFG_ENABLE_DEPTH_CLKWISE_ROTATE

#if CFG_PER_TIME_CAL_FRAME_PROCESS
		per_calc_start();
#endif

#if CFG_CAM_USE_HX_PP_LIB
	// depth from 134x214 to 400x640
	// As depth data is in 1280x800, so convert to 640x400 directly.
	if (hx_pp(g_depth_rotate_buf, out_depth)) {
		log_error("hx convert failed!\n");
		return RET_FAIL;
	}
#else
	libyuv::ScalePlane_16(g_depth_rotate_buf, CFG_DEPTH_FRAME_ACTUAL_HEIGHT, CFG_DEPTH_FRAME_ACTUAL_HEIGHT,
			CFG_DEPTH_FRAME_ACTUAL_WIDTH, out_depth, CFG_FACE_LIB_FRAME_WIDTH, CFG_FACE_LIB_FRAME_WIDTH,
			CFG_FACE_LIB_FRAME_HEIGHT, libyuv::kFilterNone);
#endif

#if CFG_PER_TIME_CAL_FRAME_PROCESS
	per_calc_end();
	log_info("hx_pp: %d usec\n", per_calc_get_result());
#endif

	return RET_OK;
}

int32_t cam_ctrl_cvt_to_gray(cam_ctrl_t *cam_ctrl, const uint8_t *in_raw_ir, uint8_t *out_ir,
									int32_t lib_width, int32_t lib_height)
{
#if CFG_ENABLE_IR_CLKWISE_ROTATE
	enum libyuv::RotationMode ir_rot_angle = libyuv::kRotate0;
#endif

#if CFG_PER_TIME_CAL_FRAME_PROCESS
		per_calc_start();
#endif
	/* Need to convert: 1280x800 => 640x400 => 400x640 */
	// Scale to 640 x 400 first
	libyuv::ScalePlane(in_raw_ir, CFG_IR_CAM_WIDTH, CFG_IR_CAM_WIDTH, CFG_IR_CAM_HEIGHT,
			g_ir_rot_angle_buf, CFG_IR_FRAME_WIDTH, CFG_IR_FRAME_WIDTH, CFG_IR_FRAME_HEIGHT, libyuv::kFilterNone);
#if CFG_PER_TIME_CAL_FRAME_PROCESS
	per_calc_end();
	log_info("ScalePlane: %d usec\n", per_calc_get_result());
#endif

#if CFG_ENABLE_IR_CLKWISE_ROTATE
	#if (CFG_IR_CLKWISE_ROTATE_90 == 1)
		ir_rot_angle = libyuv::kRotate90;
	#elif (CFG_IR_CLKWISE_ROTATE_180 == 1)
		ir_rot_angle = libyuv::kRotate180;
	#elif (CFG_IR_CLKWISE_ROTATE_270 == 1)
		ir_rot_angle = libyuv::kRotate270;
	#endif

#if CFG_PER_TIME_CAL_FRAME_PROCESS
		per_calc_start();
#endif
	// Rotate to 400 x 640
	libyuv::RotatePlane((const uint8_t *)g_ir_rot_angle_buf, CFG_IR_FRAME_WIDTH, out_ir,
			lib_width, CFG_IR_FRAME_WIDTH, CFG_IR_FRAME_HEIGHT, ir_rot_angle);
#endif

#if CFG_PER_TIME_CAL_FRAME_PROCESS
	per_calc_end();
	log_info("RotatePlane: %d usec\n", per_calc_get_result());
#endif

#if CFG_ENABLE_PREVIEW
	cam_ctrl->preview_width = lib_width;
	cam_ctrl->preview_height = lib_height;
	cam_ctrl->preview_buf_ptr = out_ir;
#endif

	return RET_OK;
}

int32_t cam_ctrl_cvt_to_argb(cam_ctrl_t *cam_ctrl, const uint8_t *in_raw_ir, uint8_t *out_argb,
									int32_t width, int32_t height)
{
	const uint8_t *p_in_ir = in_raw_ir;
	uint8_t *p_out_argb = out_argb;

	for (int32_t i = 0; i < width * height; ++i, ++p_in_ir, p_out_argb += 4) {
		*p_out_argb = *p_in_ir;
		*(p_out_argb + 1) = *p_in_ir;
		*(p_out_argb + 2) = *p_in_ir;
		*(p_out_argb + 3) = 0xff;
	}
	return RET_OK;
}

