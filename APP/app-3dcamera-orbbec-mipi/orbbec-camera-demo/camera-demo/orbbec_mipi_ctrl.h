/*
 * Copyright 2018 NXP Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __ORBBEC_MIPI_CTRL_H__
#define __ORBBEC_MIPI_CTRL_H__

#include <stdint.h>
#include "OrbbecCAPI.h"

#define RES_480
//#define RES_540

#define MX6000_SLAVE_ADDR	0x10
#ifdef RES_480
#endif
#define MX6000_CAM_WIDTH	480
#ifdef RES_540
#define MX6000_CAM_WIDTH	540
#endif
#define MX6000_CAM_HEIGHT	640

enum {
	ZERO,
	STREAM_DEPTH,
	STREAM_IR
};

enum mx6000_ctrl_stream_mode {
	STREAM_MODE_DEPTH = 0x01,
	STREAM_MODE_SPECKLE = 0x02,
	STREAM_MODE_IR	= 0x05
};

enum mx6000_ctrl_depth_stream_resolution {
	DEPTH_STREAM_RES_480X640  = 0x15,
	DEPTH_STREAM_RES_540X640  = 0x17,
	DEPTH_STREAM_RES_960X1280 = 0x19
};

enum mx6000_ctrl_ir_stream_resolution {
	IR_STREAM_RES_480X640 = 0x15,
	IR_STREAM_RES_540X640 = 0x17,
	IR_STREAM_RES_960X1280 = 0x19
};

enum mx6000_ctrl_depth_stream_fmt {
	DEPTH_PIXEL_FORMAT_RAW12 = 0x04,
	DEPTH_PIXEL_FORMAT_RAW10 = 0x14,
};

enum mx6000_ctrl_ir_stream_fmt {
	IR_PIXEL_FORMAT_RAW12 = 0x04,
	IR_PIXEL_FORMAT_RAW10 = 0x14,
};

#define DEPTH_LOAD_PARAMETERS 	0x01
#define IR_LOAD_PARAMETERS		0x01

enum {
	MX6000_SUCCESS = 0x0,
	MX6000_DEP_RES_SUCCESS = MX6000_SUCCESS,
	MX6000_DEP_FRATE_SUCCESS = MX6000_SUCCESS,
	MX6000_DEP_FORMAT_SUCCESS = MX6000_SUCCESS,
	MX6000_DEP_LOAD_SUCCESS = MX6000_SUCCESS,
	MX6000_IR_RES_SUCCESS = MX6000_SUCCESS,
	MX6000_IR_FRATE_SUCCESS = MX6000_SUCCESS,
	MX6000_IR_FORMAT_SUCCESS = MX6000_SUCCESS,
	MX6000_IR_LOAD_SUCCESS = MX6000_SUCCESS
};


int32_t orbbec_mipi_ctrl_depth_set_video_mode(int32_t fd, uint32_t out_width, uint32_t out_height);
int32_t orbbec_mipi_ctrl_ir_set_video_mode(int32_t fd, uint32_t out_width, uint32_t out_height);
int32_t orbbec_mipi_ctrl_switch_stream(int32_t fd, int32_t strm_mode);
int32_t orbbec_mipi_ctrl_init(const char *i2c_dev, uint8_t device_addr);

inline int32_t orbbec_mipi_ctrl_shift_to_depth(uint16_t *in_buf, uint16_t *out_buf,
		uint32_t width, uint32_t height)
{
	return orbbec_Shift_2_Depth(in_buf, out_buf, width, height);
}

#endif
