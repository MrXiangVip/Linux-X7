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

#ifndef __V4L2_CTRL_H__
#define __V4L2_CTRL_H__

#include "config.h"
#include <stdint.h>

enum {
	STREAM_IR,
	STREAM_DEPTH
};

enum {
	FRAME_TYPE_DEPTH = 1,
	FRAME_TYPE_IR = 1 << 1,
	FRAME_TYPE_ANY = 0xFF
};

typedef struct cam_ctrl_buffer {
	uint8_t *start;
	uint32_t offset;
	uint32_t length;
} cam_ctrl_buffer_t;

typedef struct cam_ctrl_reg_cfg {
	uint16_t reg_addr;
	uint16_t addr_len;
	uint16_t val;
	uint16_t val_len;
	uint32_t delay_ms;
} cam_ctrl_reg_cfg_t;

typedef struct cam_ctrl {
#if CFG_ENABLE_V4L2_SUPPORT
	int32_t v4l2_fd;
	struct v4l2_buffer frm_buf;
#endif
#if CFG_ENABLE_CAM_I2C
	int32_t i2c_fd;
#endif
	uint32_t ir_width;
	uint32_t ir_height;
	uint32_t depth_width;
	uint32_t depth_height;
#if CFG_ENABLE_PREVIEW
	uint32_t preview_width;
	uint32_t preview_height;
	uint8_t *preview_buf_ptr;
#endif
} cam_ctrl_t;

typedef struct cam_ctrl_init {
#if CFG_ENABLE_V4L2_SUPPORT
	char *v4l2_dev;
#endif
#if CFG_ENABLE_CAM_I2C
	char *i2c_dev;
	uint16_t i2c_slave_addr;
#endif
	uint32_t fmt;
	uint32_t width;
	uint32_t height;
#if CFG_ENABLE_CAM_LIBUVC
	uint16_t vid;
	uint16_t pid;
#endif
} cam_ctrl_init_t;

int32_t cam_ctrl_init(const cam_ctrl_init_t *cam_init, cam_ctrl_t *out_cam_ctrl);
int32_t cam_ctrl_start_stream(cam_ctrl_t *cam_ctrl);
int32_t cam_ctrl_stop_stream(cam_ctrl_t *cam_ctrl);
int32_t cam_ctrl_pause_stream(cam_ctrl_t *cam_ctrl);
int32_t cam_ctrl_resume_stream(cam_ctrl_t *cam_ctrl);
int32_t cam_ctrl_deinit(cam_ctrl_t *cam_ctrl);
int32_t cam_ctrl_get_frame_data(cam_ctrl_t *cam_ctrl, uint8_t *out_data, int32_t frm_type);
int32_t cam_ctrl_deq_frame_data_ptr(cam_ctrl_t *cam_ctrl, uint8_t **out_data, int32_t frm_type);
int32_t cam_ctrl_q_frame_data_ptr(cam_ctrl_t *cam_ctrl);
int32_t cam_ctrl_config_depth(cam_ctrl_t *cam_ctrl, uint32_t width, uint32_t height, uint32_t fps);
int32_t cam_ctrl_config_ir(cam_ctrl_t *cam_ctrl, uint32_t width, uint32_t height, uint32_t fps);
int32_t cam_ctrl_enable_ir_and_depth(cam_ctrl_t *cam_ctrl);
int32_t cam_ctrl_enable_ir(cam_ctrl_t *cam_ctrl);
int32_t cam_ctrl_enable_depth(cam_ctrl_t *cam_ctrl);
int32_t cam_ctrl_switch_stream(cam_ctrl_t *cam_ctrl, int32_t strm_mode);
int32_t cam_ctrl_get_frame_type(const uint8_t *in_data, int32_t width, int32_t height);
int32_t cam_ctrl_cvt_to_depth(cam_ctrl_t *cam_ctrl, uint16_t *in_raw_depth, uint16_t *out_depth,
		int32_t width, int32_t height);
int32_t cam_ctrl_cvt_to_gray(cam_ctrl_t *cam_ctrl, const uint8_t *in_raw_ir, uint8_t *out_ir,
		int32_t width, int32_t height);
int32_t cam_ctrl_cvt_to_argb(cam_ctrl_t *cam_ctrl, const uint8_t *in_raw_ir, uint8_t *out_argb,
		int32_t width, int32_t height);
#endif
