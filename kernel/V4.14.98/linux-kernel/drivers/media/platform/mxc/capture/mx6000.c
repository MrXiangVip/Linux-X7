/*
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/v4l2-mediabus.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#include <linux/slab.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf-core.h>

#define MX6000_VOLTAGE_ANALOG               2800000
#define MX6000_VOLTAGE_DIGITAL_CORE         1500000
#define MX6000_VOLTAGE_DIGITAL_IO           1800000

#define MIN_FPS 15
#define MAX_FPS 30
#define DEFAULT_FPS 30

#define MX6000_XCLK_MIN 6000000
#define MX6000_XCLK_MAX 24000000

#define MX6000_CHIP_ID_HIGH_BYTE        0x300A
#define MX6000_CHIP_ID_LOW_BYTE         0x300B

//#define RES_960_1280
//#define RES_540_640
#define RES_480_640

enum mx6000_mode {
	mx6000_mode_MIN = 0,
	mx6000_mode_VGA_640_480 = 0,
	mx6000_mode_QVGA_320_240 = 1,
	mx6000_mode_NTSC_720_480 = 2,
	mx6000_mode_PAL_720_576 = 3,
	mx6000_mode_720P_1280_720 = 4,
	mx6000_mode_1080P_1920_1080 = 5,
	mx6000_mode_QSXGA_2592_1944 = 6,
	mx6000_mode_QCIF_176_144 = 7,
	mx6000_mode_XGA_1024_768 = 8,
	mx6000_mode_320p_480_320 = 9,
	mx6000_mode_854p_480_854 = 10,
	mx6000_mode_bt656_640_480 = 11,
	mx6000_mode_MAX = 11
};

enum mx6000_frame_rate {
	mx6000_15_fps,
	mx6000_30_fps
};

static int mx6000_framerates[] = {
	[mx6000_15_fps] = 15,
	[mx6000_30_fps] = 30,
};

struct mx6000_datafmt {
	u32	code;
	enum v4l2_colorspace		colorspace;
};

struct reg_value {
	u16 u16RegAddr;
	u16 u16Val;
	u16 u16Mask;
	u32 u32Delay_ms;
};

struct mx6000_mode_info {
	enum mx6000_mode mode;
	u32 width;
	u32 height;
	struct reg_value *init_data_ptr;
	u32 init_data_size;
};

struct mx6000 {
	struct v4l2_subdev		subdev;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	const struct mx6000_datafmt	*fmt;
	struct v4l2_captureparm streamcap;
	bool on;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	int red;
	int green;
	int blue;
	int ae_mode;

	u32 mclk;
	u8 mclk_source;
	struct clk *sensor_clk;
	int csi;

	void (*io_init)(void);
};

/*!
 * Maintains the information on the current state of the sesor.
 */
static struct mx6000 mx6000_data;
static int pwn_gpio, rst_gpio;
static int prev_sysclk;
static int AE_Target = 52, night_mode;
static int prev_HTS;
static int AE_high, AE_low;

/* difference between 'mx6000_global_init_setting' and
 * 'mx6000_init_setting_30fps_BT656':
 * 0x3103, 0x3a19, 0x3c07
 */
static struct reg_value mx6000_global_init_setting[] = {
};

static struct reg_value mx6000_bt656_720_480[] = {
};
static struct reg_value mx6000_init_setting_30fps_BT656[] = {

	{0x0103, 0x01, 0, 0},
	{0x3105, 0x11, 0, 0},
	{0x4004, 0x00, 0, 0},
	{0x4005, 0x02, 0, 0},
	{0x4006, 0x04, 0, 0},
	{0x4007, 0x02, 0, 0},
	{0x4008, 0x08, 0, 0},
	{0x4009, 0x04, 0, 0},
	{0x4026, 0x21, 0, 0},
	{0x4027, 0xa0, 0, 0},
	{0x4022, 0x01, 0, 0},
	{0x4023, 0xf0, 0, 0},
	{0x4024, 0x00, 0, 0},
	{0x4040, 0x40, 0, 0},
	{0x3609, 0xa0, 0, 0},
	{0x370d, 0x1c, 0, 0},
	{0x3715, 0x90, 0, 0},
	{0x3705, 0x15, 0, 0},
	{0x3717, 0x1b, 0, 0},
	{0x4803, 0x82, 0, 0},
	{0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0},
	{0x3803, 0x00, 0, 0},
	{0x3804, 0x02, 0, 0},
	{0x3805, 0x9f, 0, 0},
	{0x3806, 0x01, 0, 0},
	{0x3807, 0xeb, 0, 0},
	{0x3808, 0x02, 0, 0},
	{0x3809, 0x80, 0, 0},
	/* change to 0x01e1 */
	{0x380a, 0x01, 0, 0},
	{0x380b, 0xe1, 0, 0},
	{0x3810, 0x00, 0, 0},
	{0x3811, 0x00, 0, 0},
	{0x3812, 0x00, 0, 0},
	{0x3813, 0x00, 0, 0},
	{0x5000, 0xfd, 0, 0},
	{0x5301, 0x0a, 0, 0},
	{0x5302, 0x15, 0, 0},
	{0x5303, 0x29, 0, 0},
	{0x5304, 0x49, 0, 0},
	{0x5305, 0x58, 0, 0},
	{0x5306, 0x63, 0, 0},
	{0x5307, 0x6e, 0, 0},
	{0x5308, 0x78, 0, 0},
	{0x5309, 0x80, 0, 0},
	{0x530a, 0x89, 0, 0},
	{0x530b, 0x98, 0, 0},
	{0x530c, 0xa5, 0, 0},
	{0x530d, 0xbb, 0, 0},
	{0x530e, 0xcf, 0, 0},
	{0x530f, 0xe3, 0, 0},
	{0x5310, 0x13, 0, 0},
	{0x5217, 0x01, 0, 0},
	{0x5601, 0x12, 0, 0},
	{0x5602, 0x72, 0, 0},
	{0x5603, 0x04, 0, 0},
	{0x5604, 0x0f, 0, 0},
	{0x5605, 0x99, 0, 0},
	{0x5606, 0xa8, 0, 0},
	{0x5607, 0x76, 0, 0},
	{0x5608, 0x4f, 0, 0},
	{0x5609, 0x27, 0, 0},
	{0x560a, 0x01, 0, 0},
	{0x560b, 0x9c, 0, 0},
	{0x5003, 0xd5, 0, 0},
	{0x580a, 0xa0, 0, 0},
	{0x3a1b, 0x60, 0, 0},
	{0x3a11, 0xc8, 0, 0},
	{0x3a1e, 0x50, 0, 0},
	{0x3a0f, 0x68, 0, 0},
	{0x3a10, 0x58, 0, 0},
	{0x3a1f, 0x38, 0, 0},
	{0x4700, 0x0f, 0, 0},
	{0x4300, 0x01, 0, 0},
	{0x3000, 0x07, 0, 0},
	{0x3001, 0xff, 0, 0},
	{0x6700, 0x05, 0, 0},
	{0x6701, 0x19, 0, 0},
	{0x6702, 0xfd, 0, 0},
	{0x6703, 0xd7, 0, 0},
	{0x6704, 0xff, 0, 0},
	{0x6705, 0xff, 0, 0},
	{0x3003, 0x18, 0, 0},
	{0x3004, 0x0e, 0, 0},
	{0x3709, 0x50, 0, 0},
	{0x3811, 0x0c, 0, 0},
	{0x3813, 0x02, 0, 0},
	{0x3716, 0x39, 0, 0},
	{0x3715, 0x92, 0, 0},
	{0x370d, 0x10, 0, 0},
	{0x3011, 0x01, 0, 0},
	/* fix U and V */
	//{0x5800, 0x1a, 0, 0}
	/* set surround weight to 0 */
	{0x5908, 0x00, 0, 0},
	{0x5909, 0x00, 0, 0},
	{0x590a, 0x10, 0, 0},
	{0x590b, 0x01, 0, 0},
	{0x590c, 0x10, 0, 0},
	{0x590d, 0x01, 0, 0},
	{0x590e, 0x00, 0, 0},
	{0x590f, 0x00, 0, 0},
	/* enable gray output */
	{0x5800, 0x22, 0, 0},
	/* change gamma config */
	/*
	{0x5301, 0x14, 0, 0},
	{0x5302, 0x1e, 0, 0},
	{0x5303, 0x35, 0, 0},
	{0x5304, 0x51, 0, 0},
	{0x5305, 0x5b, 0, 0},
	{0x5306, 0x63, 0, 0},
	{0x5307, 0x6b, 0, 0},
	{0x5308, 0x72, 0, 0},
	{0x5309, 0x7a, 0, 0},
	{0x530a, 0x82, 0, 0},
	{0x530b, 0x91, 0, 0},
	{0x530c, 0x9e, 0, 0},
	{0x530d, 0xbc, 0, 0},
	{0x530e, 0xd3, 0, 0},
	{0x530f, 0xed, 0, 0},
	{0x5310, 0xff, 0, 0},
	*/
};


static struct reg_value mx6000_init_setting_30fps_VGA[] = {
	{0x3008, 0x42, 0, 0},
	{0x3103, 0x03, 0, 0}, {0x3017, 0xff, 0, 0}, {0x3018, 0xff, 0, 0},
	{0x3034, 0x1a, 0, 0}, {0x3035, 0x11, 0, 0}, {0x3036, 0x46, 0, 0},
	{0x3037, 0x13, 0, 0}, {0x3108, 0x01, 0, 0}, {0x3630, 0x36, 0, 0},
	{0x3631, 0x0e, 0, 0}, {0x3632, 0xe2, 0, 0}, {0x3633, 0x12, 0, 0},
	{0x3621, 0xe0, 0, 0}, {0x3704, 0xa0, 0, 0}, {0x3703, 0x5a, 0, 0},
	{0x3715, 0x78, 0, 0}, {0x3717, 0x01, 0, 0}, {0x370b, 0x60, 0, 0},
	{0x3705, 0x1a, 0, 0}, {0x3905, 0x02, 0, 0}, {0x3906, 0x10, 0, 0},
	{0x3901, 0x0a, 0, 0}, {0x3731, 0x12, 0, 0}, {0x3600, 0x08, 0, 0},
	{0x3601, 0x33, 0, 0}, {0x302d, 0x60, 0, 0}, {0x3620, 0x52, 0, 0},
	{0x371b, 0x20, 0, 0}, {0x471c, 0x50, 0, 0}, {0x3a13, 0x43, 0, 0},
	{0x3a18, 0x00, 0, 0}, {0x3a19, 0xf8, 0, 0}, {0x3635, 0x13, 0, 0},
	{0x3636, 0x03, 0, 0}, {0x3634, 0x40, 0, 0}, {0x3622, 0x01, 0, 0},
	{0x3c01, 0x34, 0, 0}, {0x3c04, 0x28, 0, 0}, {0x3c05, 0x98, 0, 0},
	{0x3c06, 0x00, 0, 0}, {0x3c07, 0x08, 0, 0}, {0x3c08, 0x00, 0, 0},
	{0x3c09, 0x1c, 0, 0}, {0x3c0a, 0x9c, 0, 0}, {0x3c0b, 0x40, 0, 0},
	{0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0}, {0x3814, 0x31, 0, 0},
	{0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0}, {0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0}, {0x3804, 0x0a, 0, 0},
	{0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0}, {0x3807, 0x9b, 0, 0},
	{0x3808, 0x02, 0, 0}, {0x3809, 0x80, 0, 0}, {0x380a, 0x01, 0, 0},
	{0x380b, 0xe0, 0, 0}, {0x380c, 0x07, 0, 0}, {0x380d, 0x68, 0, 0},
	{0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0}, {0x3810, 0x00, 0, 0},
	{0x3811, 0x10, 0, 0}, {0x3812, 0x00, 0, 0}, {0x3813, 0x06, 0, 0},
	{0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0}, {0x3708, 0x64, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x03, 0, 0},
	{0x3a03, 0xd8, 0, 0}, {0x3a08, 0x01, 0, 0}, {0x3a09, 0x27, 0, 0},
	{0x3a0a, 0x00, 0, 0}, {0x3a0b, 0xf6, 0, 0}, {0x3a0e, 0x03, 0, 0},
	{0x3a0d, 0x04, 0, 0}, {0x3a14, 0x03, 0, 0}, {0x3a15, 0xd8, 0, 0},
	{0x4001, 0x02, 0, 0}, {0x4004, 0x02, 0, 0}, {0x3000, 0x00, 0, 0},
	{0x3002, 0x1c, 0, 0}, {0x3004, 0xff, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x300e, 0x58, 0, 0}, {0x302e, 0x00, 0, 0}, {0x4300, 0x30, 0, 0},
	{0x501f, 0x00, 0, 0}, {0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0},
	{0x440e, 0x00, 0, 0}, {0x460b, 0x35, 0, 0}, {0x460c, 0x22, 0, 0},
	{0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0}, {0x5000, 0xa7, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x5180, 0xff, 0, 0}, {0x5181, 0xf2, 0, 0},
	{0x5182, 0x00, 0, 0}, {0x5183, 0x14, 0, 0}, {0x5184, 0x25, 0, 0},
	{0x5185, 0x24, 0, 0}, {0x5186, 0x09, 0, 0}, {0x5187, 0x09, 0, 0},
	{0x5188, 0x09, 0, 0}, {0x5189, 0x88, 0, 0}, {0x518a, 0x54, 0, 0},
	{0x518b, 0xee, 0, 0}, {0x518c, 0xb2, 0, 0}, {0x518d, 0x50, 0, 0},
	{0x518e, 0x34, 0, 0}, {0x518f, 0x6b, 0, 0}, {0x5190, 0x46, 0, 0},
	{0x5191, 0xf8, 0, 0}, {0x5192, 0x04, 0, 0}, {0x5193, 0x70, 0, 0},
	{0x5194, 0xf0, 0, 0}, {0x5195, 0xf0, 0, 0}, {0x5196, 0x03, 0, 0},
	{0x5197, 0x01, 0, 0}, {0x5198, 0x04, 0, 0}, {0x5199, 0x6c, 0, 0},
	{0x519a, 0x04, 0, 0}, {0x519b, 0x00, 0, 0}, {0x519c, 0x09, 0, 0},
	{0x519d, 0x2b, 0, 0}, {0x519e, 0x38, 0, 0}, {0x5381, 0x1e, 0, 0},
	{0x5382, 0x5b, 0, 0}, {0x5383, 0x08, 0, 0}, {0x5384, 0x0a, 0, 0},
	{0x5385, 0x7e, 0, 0}, {0x5386, 0x88, 0, 0}, {0x5387, 0x7c, 0, 0},
	{0x5388, 0x6c, 0, 0}, {0x5389, 0x10, 0, 0}, {0x538a, 0x01, 0, 0},
	{0x538b, 0x98, 0, 0}, {0x5300, 0x08, 0, 0}, {0x5301, 0x30, 0, 0},
	{0x5302, 0x10, 0, 0}, {0x5303, 0x00, 0, 0}, {0x5304, 0x08, 0, 0},
	{0x5305, 0x30, 0, 0}, {0x5306, 0x08, 0, 0}, {0x5307, 0x16, 0, 0},
	{0x5309, 0x08, 0, 0}, {0x530a, 0x30, 0, 0}, {0x530b, 0x04, 0, 0},
	{0x530c, 0x06, 0, 0}, {0x5480, 0x01, 0, 0}, {0x5481, 0x08, 0, 0},
	{0x5482, 0x14, 0, 0}, {0x5483, 0x28, 0, 0}, {0x5484, 0x51, 0, 0},
	{0x5485, 0x65, 0, 0}, {0x5486, 0x71, 0, 0}, {0x5487, 0x7d, 0, 0},
	{0x5488, 0x87, 0, 0}, {0x5489, 0x91, 0, 0}, {0x548a, 0x9a, 0, 0},
	{0x548b, 0xaa, 0, 0}, {0x548c, 0xb8, 0, 0}, {0x548d, 0xcd, 0, 0},
	{0x548e, 0xdd, 0, 0}, {0x548f, 0xea, 0, 0}, {0x5490, 0x1d, 0, 0},
	{0x5580, 0x02, 0, 0}, {0x5583, 0x40, 0, 0}, {0x5584, 0x10, 0, 0},
	{0x5589, 0x10, 0, 0}, {0x558a, 0x00, 0, 0}, {0x558b, 0xf8, 0, 0},
	{0x5800, 0x23, 0, 0}, {0x5801, 0x14, 0, 0}, {0x5802, 0x0f, 0, 0},
	{0x5803, 0x0f, 0, 0}, {0x5804, 0x12, 0, 0}, {0x5805, 0x26, 0, 0},
	{0x5806, 0x0c, 0, 0}, {0x5807, 0x08, 0, 0}, {0x5808, 0x05, 0, 0},
	{0x5809, 0x05, 0, 0}, {0x580a, 0x08, 0, 0}, {0x580b, 0x0d, 0, 0},
	{0x580c, 0x08, 0, 0}, {0x580d, 0x03, 0, 0}, {0x580e, 0x00, 0, 0},
	{0x580f, 0x00, 0, 0}, {0x5810, 0x03, 0, 0}, {0x5811, 0x09, 0, 0},
	{0x5812, 0x07, 0, 0}, {0x5813, 0x03, 0, 0}, {0x5814, 0x00, 0, 0},
	{0x5815, 0x01, 0, 0}, {0x5816, 0x03, 0, 0}, {0x5817, 0x08, 0, 0},
	{0x5818, 0x0d, 0, 0}, {0x5819, 0x08, 0, 0}, {0x581a, 0x05, 0, 0},
	{0x581b, 0x06, 0, 0}, {0x581c, 0x08, 0, 0}, {0x581d, 0x0e, 0, 0},
	{0x581e, 0x29, 0, 0}, {0x581f, 0x17, 0, 0}, {0x5820, 0x11, 0, 0},
	{0x5821, 0x11, 0, 0}, {0x5822, 0x15, 0, 0}, {0x5823, 0x28, 0, 0},
	{0x5824, 0x46, 0, 0}, {0x5825, 0x26, 0, 0}, {0x5826, 0x08, 0, 0},
	{0x5827, 0x26, 0, 0}, {0x5828, 0x64, 0, 0}, {0x5829, 0x26, 0, 0},
	{0x582a, 0x24, 0, 0}, {0x582b, 0x22, 0, 0}, {0x582c, 0x24, 0, 0},
	{0x582d, 0x24, 0, 0}, {0x582e, 0x06, 0, 0}, {0x582f, 0x22, 0, 0},
	{0x5830, 0x40, 0, 0}, {0x5831, 0x42, 0, 0}, {0x5832, 0x24, 0, 0},
	{0x5833, 0x26, 0, 0}, {0x5834, 0x24, 0, 0}, {0x5835, 0x22, 0, 0},
	{0x5836, 0x22, 0, 0}, {0x5837, 0x26, 0, 0}, {0x5838, 0x44, 0, 0},
	{0x5839, 0x24, 0, 0}, {0x583a, 0x26, 0, 0}, {0x583b, 0x28, 0, 0},
	{0x583c, 0x42, 0, 0}, {0x583d, 0xce, 0, 0}, {0x5025, 0x00, 0, 0},
	{0x3a0f, 0x30, 0, 0}, {0x3a10, 0x28, 0, 0}, {0x3a1b, 0x30, 0, 0},
	{0x3a1e, 0x26, 0, 0}, {0x3a11, 0x60, 0, 0}, {0x3a1f, 0x14, 0, 0},
	{0x3008, 0x02, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x11, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_30fps_VGA_640_480[] = {
/*
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x02, 0, 0}, {0x3809, 0x80, 0, 0},
	{0x380a, 0x01, 0, 0}, {0x380b, 0xe0, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x11, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0}, {0x3503, 0x00, 0, 0},
*/
/*
	//setting 3 for 24Mhz pclk
	{0x0002, 0x0001, 0, 10},
	{0x0002, 0x0000, 0, 0},
	{0x0010, 0xfff9, 0, 1000},
	{0x0016, 0x20bf, 0, 0},
	{0x0018, 0x0c03, 0, 1000},
	{0x0018, 0x0c13, 0, 0},
	{0x0020, 0x0028, 0, 0},
	{0x000c, 0x0101, 0, 0},
	{0x0060, 0x8010, 0, 0},
	{0x0006, 0x0020, 0, 0},
	{0x0008, 0x0060, 0, 0},
	{0x0008, 0x0020, 0, 0},
	{0x0004, 0x8175, 0, 0},
*/
/*
	//setting 2 for 90Mhz pclk
	{0x0002, 0x0001, 0, 10},
	{0x0002, 0x0000, 0, 0},
	{0x0010, 0xfff9, 0, 1000},
	{0x0016, 0x3076, 0, 0},
	{0x0018, 0x0003, 0, 1000},
	{0x0018, 0x0013, 0, 0},
	{0x0020, 0x0000, 0, 0},
	{0x000c, 0x0101, 0, 0},
	{0x0060, 0x8010, 0, 0},
	{0x0006, 0x0020, 0, 0},
	{0x0008, 0x0060, 0, 0},
	{0x0008, 0x0020, 0, 0},
	{0x0004, 0x8175, 0, 0},
*/

	//setting 4 for 70Mhz pclk
	{0x0002, 0x0001, 0, 10},
	{0x0002, 0x0000, 0, 0},
	//{0x0010, 0xfff9, 0, 1000},
	{0x0016, 0x105d, 0, 0},
	{0x0018, 0x0c03, 0, 1000},
	{0x0018, 0x0c13, 0, 0},
	{0x0020, 0x0022, 0, 0},
	{0x000c, 0x0101, 0, 0},
	{0x0060, 0x800a, 0, 0},
	{0x0006, 0x0020, 0, 0},
	{0x0008, 0x0060, 0, 0},
	//{0x0008, 0x0020, 0, 0},
	{0x0004, 0x8175, 0, 0},
/*
	//setting 5 for 50Mhz pclk
	{0x0002, 0x0001, 0, 10},
	{0x0002, 0x0000, 0, 0},
	{0x0016, 0x3107, 0, 0},
	{0x0018, 0x0c03, 0, 1000},
	{0x0018, 0x0c13, 0, 0},
	{0x0020, 0x0011, 0, 0},
	{0x0060, 0x8008, 0, 0},
	{0x0006, 0x012c, 0, 0},
	{0x0008, 0x0031, 0, 0},
	{0x0004, 0x8175, 0, 0},
*/
};

static struct reg_value mx6000_setting_15fps_VGA_640_480[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x02, 0, 0}, {0x3809, 0x80, 0, 0},
	//{0x380a, 0x01, 0, 0}, {0x380b, 0xe0, 0, 0}, {0x380c, 0x07, 0, 0},

	{0x380a, 0x01, 0, 0}, {0x380b, 0xe1, 0, 0}, {0x380c, 0x07, 0, 0},

	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x21, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0}, {0x3503, 0x00, 0, 0},
};

static struct reg_value mx6000_setting_30fps_QVGA_320_240[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x01, 0, 0}, {0x3809, 0x40, 0, 0},
	{0x380a, 0x00, 0, 0}, {0x380b, 0xf0, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x11, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_15fps_QVGA_320_240[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x01, 0, 0}, {0x3809, 0x40, 0, 0},
	{0x380a, 0x00, 0, 0}, {0x380b, 0xf0, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x21, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_30fps_NTSC_720_480[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x06, 0, 0},
	{0x3807, 0xd4, 0, 0}, {0x3808, 0x02, 0, 0}, {0x3809, 0xd0, 0, 0},
	{0x380a, 0x01, 0, 0}, {0x380b, 0xe0, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x11, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_15fps_NTSC_720_480[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x06, 0, 0},
	{0x3807, 0xd4, 0, 0}, {0x3808, 0x02, 0, 0}, {0x3809, 0xd0, 0, 0},
	{0x380a, 0x01, 0, 0}, {0x380b, 0xe0, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x21, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_30fps_PAL_720_576[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x60, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x09, 0, 0}, {0x3805, 0x7e, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x02, 0, 0}, {0x3809, 0xd0, 0, 0},
	{0x380a, 0x02, 0, 0}, {0x380b, 0x40, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x11, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_15fps_PAL_720_576[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x60, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x09, 0, 0}, {0x3805, 0x7e, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x02, 0, 0}, {0x3809, 0xd0, 0, 0},
	{0x380a, 0x02, 0, 0}, {0x380b, 0x40, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x21, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_30fps_720P_1280_720[] = {
	{0x3035, 0x21, 0, 0}, {0x3036, 0x69, 0, 0}, {0x3c07, 0x07, 0, 0},
	{0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0}, {0x3814, 0x31, 0, 0},
	{0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0}, {0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0}, {0x3803, 0xfa, 0, 0}, {0x3804, 0x0a, 0, 0},
	{0x3805, 0x3f, 0, 0}, {0x3806, 0x06, 0, 0}, {0x3807, 0xa9, 0, 0},
	{0x3808, 0x05, 0, 0}, {0x3809, 0x00, 0, 0}, {0x380a, 0x02, 0, 0},
	{0x380b, 0xd0, 0, 0}, {0x380c, 0x07, 0, 0}, {0x380d, 0x64, 0, 0},
	{0x380e, 0x02, 0, 0}, {0x380f, 0xe4, 0, 0}, {0x3813, 0x04, 0, 0},
	{0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0}, {0x3709, 0x52, 0, 0},
	{0x370c, 0x03, 0, 0}, {0x3a02, 0x02, 0, 0}, {0x3a03, 0xe0, 0, 0},
	{0x3a14, 0x02, 0, 0}, {0x3a15, 0xe0, 0, 0}, {0x4004, 0x02, 0, 0},
	{0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0}, {0x4713, 0x03, 0, 0},
	{0x4407, 0x04, 0, 0}, {0x460b, 0x37, 0, 0}, {0x460c, 0x20, 0, 0},
	{0x4837, 0x16, 0, 0}, {0x3824, 0x04, 0, 0}, {0x5001, 0x83, 0, 0},
	{0x3503, 0x00, 0, 0},
};

static struct reg_value mx6000_setting_15fps_720P_1280_720[] = {
	{0x3035, 0x41, 0, 0}, {0x3036, 0x69, 0, 0}, {0x3c07, 0x07, 0, 0},
	{0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0}, {0x3814, 0x31, 0, 0},
	{0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0}, {0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0}, {0x3803, 0xfa, 0, 0}, {0x3804, 0x0a, 0, 0},
	{0x3805, 0x3f, 0, 0}, {0x3806, 0x06, 0, 0}, {0x3807, 0xa9, 0, 0},
	{0x3808, 0x05, 0, 0}, {0x3809, 0x00, 0, 0}, {0x380a, 0x02, 0, 0},
	{0x380b, 0xd0, 0, 0}, {0x380c, 0x07, 0, 0}, {0x380d, 0x64, 0, 0},
	{0x380e, 0x02, 0, 0}, {0x380f, 0xe4, 0, 0}, {0x3813, 0x04, 0, 0},
	{0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0}, {0x3709, 0x52, 0, 0},
	{0x370c, 0x03, 0, 0}, {0x3a02, 0x02, 0, 0}, {0x3a03, 0xe0, 0, 0},
	{0x3a14, 0x02, 0, 0}, {0x3a15, 0xe0, 0, 0}, {0x4004, 0x02, 0, 0},
	{0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0}, {0x4713, 0x03, 0, 0},
	{0x4407, 0x04, 0, 0}, {0x460b, 0x37, 0, 0}, {0x460c, 0x20, 0, 0},
	{0x4837, 0x16, 0, 0}, {0x3824, 0x04, 0, 0}, {0x5001, 0x83, 0, 0},
	{0x3503, 0x00, 0, 0},
};

static struct reg_value mx6000_setting_30fps_QCIF_176_144[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x00, 0, 0}, {0x3809, 0xb0, 0, 0},
	{0x380a, 0x00, 0, 0}, {0x380b, 0x90, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x11, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_15fps_QCIF_176_144[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x00, 0, 0}, {0x3809, 0xb0, 0, 0},
	{0x380a, 0x00, 0, 0}, {0x380b, 0x90, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x22, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x21, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_30fps_XGA_1024_768[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x04, 0, 0}, {0x3809, 0x00, 0, 0},
	{0x380a, 0x03, 0, 0}, {0x380b, 0x00, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x20, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x01, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x21, 0, 0},
	{0x3036, 0x69, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_15fps_XGA_1024_768[] = {
	{0x3c07, 0x08, 0, 0}, {0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0},
	{0x3814, 0x31, 0, 0}, {0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9b, 0, 0}, {0x3808, 0x04, 0, 0}, {0x3809, 0x00, 0, 0},
	{0x380a, 0x03, 0, 0}, {0x380b, 0x00, 0, 0}, {0x380c, 0x07, 0, 0},
	{0x380d, 0x68, 0, 0}, {0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0},
	{0x3813, 0x06, 0, 0}, {0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x0b, 0, 0},
	{0x3a03, 0x88, 0, 0}, {0x3a14, 0x0b, 0, 0}, {0x3a15, 0x88, 0, 0},
	{0x4004, 0x02, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0},
	{0x460c, 0x20, 0, 0}, {0x4837, 0x22, 0, 0}, {0x3824, 0x01, 0, 0},
	{0x5001, 0xa3, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x21, 0, 0},
	{0x3036, 0x46, 0, 0}, {0x3037, 0x13, 0, 0},
};


static struct reg_value mx6000_setting_15fps_1080P_1920_1080[] = {
	{0x3c07, 0x07, 0, 0}, {0x3820, 0x40, 0, 0}, {0x3821, 0x06, 0, 0},
	{0x3814, 0x11, 0, 0}, {0x3815, 0x11, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0xee, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x05, 0, 0},
	{0x3807, 0xc3, 0, 0}, {0x3808, 0x07, 0, 0}, {0x3809, 0x80, 0, 0},
	{0x380a, 0x04, 0, 0}, {0x380b, 0x38, 0, 0}, {0x380c, 0x0b, 0, 0},
	{0x380d, 0x1c, 0, 0}, {0x380e, 0x07, 0, 0}, {0x380f, 0xb0, 0, 0},
	{0x3813, 0x04, 0, 0}, {0x3618, 0x04, 0, 0}, {0x3612, 0x2b, 0, 0},
	{0x3709, 0x12, 0, 0}, {0x370c, 0x00, 0, 0}, {0x3a02, 0x07, 0, 0},
	{0x3a03, 0xae, 0, 0}, {0x3a14, 0x07, 0, 0}, {0x3a15, 0xae, 0, 0},
	{0x4004, 0x06, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x02, 0, 0}, {0x4407, 0x0c, 0, 0}, {0x460b, 0x37, 0, 0},
	{0x460c, 0x20, 0, 0}, {0x4837, 0x2c, 0, 0}, {0x3824, 0x01, 0, 0},
	{0x5001, 0x83, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x21, 0, 0},
	{0x3036, 0x69, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct reg_value mx6000_setting_15fps_QSXGA_2592_1944[] = {
	{0x3c07, 0x07, 0, 0}, {0x3820, 0x40, 0, 0}, {0x3821, 0x06, 0, 0},
	{0x3814, 0x11, 0, 0}, {0x3815, 0x11, 0, 0}, {0x3800, 0x00, 0, 0},
	{0x3801, 0x00, 0, 0}, {0x3802, 0x00, 0, 0}, {0x3803, 0x00, 0, 0},
	{0x3804, 0x0a, 0, 0}, {0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0},
	{0x3807, 0x9f, 0, 0}, {0x3808, 0x0a, 0, 0}, {0x3809, 0x20, 0, 0},
	{0x380a, 0x07, 0, 0}, {0x380b, 0x98, 0, 0}, {0x380c, 0x0b, 0, 0},
	{0x380d, 0x1c, 0, 0}, {0x380e, 0x07, 0, 0}, {0x380f, 0xb0, 0, 0},
	{0x3813, 0x04, 0, 0}, {0x3618, 0x04, 0, 0}, {0x3612, 0x2b, 0, 0},
	{0x3709, 0x12, 0, 0}, {0x370c, 0x00, 0, 0}, {0x3a02, 0x07, 0, 0},
	{0x3a03, 0xae, 0, 0}, {0x3a14, 0x07, 0, 0}, {0x3a15, 0xae, 0, 0},
	{0x4004, 0x06, 0, 0}, {0x3002, 0x1c, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x4713, 0x02, 0, 0}, {0x4407, 0x0c, 0, 0}, {0x460b, 0x37, 0, 0},
	{0x460c, 0x20, 0, 0}, {0x4837, 0x2c, 0, 0}, {0x3824, 0x01, 0, 0},
	{0x5001, 0x83, 0, 0}, {0x3034, 0x1a, 0, 0}, {0x3035, 0x21, 0, 0},
	{0x3036, 0x69, 0, 0}, {0x3037, 0x13, 0, 0},
};

static struct mx6000_mode_info mx6000_mode_info_data[2][mx6000_mode_MAX + 1] = {
	{
		{mx6000_mode_VGA_640_480,      640,  480,
		mx6000_setting_15fps_VGA_640_480,
		ARRAY_SIZE(mx6000_setting_15fps_VGA_640_480)},
		{mx6000_mode_QVGA_320_240,     320,  240,
#if 0
		mx6000_setting_15fps_QVGA_320_240,
		ARRAY_SIZE(mx6000_setting_15fps_QVGA_320_240)},
#else
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
#endif
		{mx6000_mode_NTSC_720_480,     720,  480,
#if 0
		mx6000_setting_15fps_NTSC_720_480,
		ARRAY_SIZE(mx6000_setting_15fps_NTSC_720_480)},
#else
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
#endif
		{mx6000_mode_PAL_720_576,      720,  576,
		mx6000_setting_15fps_PAL_720_576,
		ARRAY_SIZE(mx6000_setting_15fps_PAL_720_576)},
		{mx6000_mode_720P_1280_720,   1280,  720,
		mx6000_setting_15fps_720P_1280_720,
		ARRAY_SIZE(mx6000_setting_15fps_720P_1280_720)},
		{mx6000_mode_1080P_1920_1080, 1920, 1080,
		mx6000_setting_15fps_1080P_1920_1080,
		ARRAY_SIZE(mx6000_setting_15fps_1080P_1920_1080)},
		{mx6000_mode_QSXGA_2592_1944, 2592, 1944,
		mx6000_setting_15fps_QSXGA_2592_1944,
		ARRAY_SIZE(mx6000_setting_15fps_QSXGA_2592_1944)},
		{mx6000_mode_QCIF_176_144,     176,  144,
		mx6000_setting_15fps_QCIF_176_144,
		ARRAY_SIZE(mx6000_setting_15fps_QCIF_176_144)},
		{mx6000_mode_XGA_1024_768,    1024,  768,
		mx6000_setting_15fps_XGA_1024_768,
		ARRAY_SIZE(mx6000_setting_15fps_XGA_1024_768)},
		{mx6000_mode_320p_480_320,     480, 320,
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
		{mx6000_mode_854p_480_854,     480, 854,
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
		{mx6000_mode_bt656_640_480,    640, 480,
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
	},
	{
#ifdef RES_960_1280
		{mx6000_mode_VGA_640_480,      960,  1280,
#endif
#ifdef RES_540_640
		{mx6000_mode_VGA_640_480,      540,  640,
#endif
#ifdef RES_480_640
		{mx6000_mode_VGA_640_480,      480,  640,
#endif
		mx6000_setting_30fps_VGA_640_480,
		ARRAY_SIZE(mx6000_setting_30fps_VGA_640_480)},
		{mx6000_mode_QVGA_320_240,     320,  240,
#if 0
		mx6000_setting_30fps_QVGA_320_240,
		ARRAY_SIZE(mx6000_setting_30fps_QVGA_320_240)},
#else
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
#endif
		{mx6000_mode_NTSC_720_480,     720,  480,
#if 0
		mx6000_setting_30fps_NTSC_720_480,
		ARRAY_SIZE(mx6000_setting_30fps_NTSC_720_480)},
#endif
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
		{mx6000_mode_PAL_720_576,      720,  576,
		mx6000_setting_30fps_PAL_720_576,
		ARRAY_SIZE(mx6000_setting_30fps_PAL_720_576)},
		{mx6000_mode_720P_1280_720,   1280,  720,
		mx6000_setting_30fps_720P_1280_720,
		ARRAY_SIZE(mx6000_setting_30fps_720P_1280_720)},
		{mx6000_mode_1080P_1920_1080, 0, 0, NULL, 0},
		{mx6000_mode_QSXGA_2592_1944, 0, 0, NULL, 0},
		{mx6000_mode_QCIF_176_144,     176,  144,
		mx6000_setting_30fps_QCIF_176_144,
		ARRAY_SIZE(mx6000_setting_30fps_QCIF_176_144)},
		{mx6000_mode_XGA_1024_768,    1024,  768,
		mx6000_setting_30fps_XGA_1024_768,
		ARRAY_SIZE(mx6000_setting_30fps_XGA_1024_768)},
		{mx6000_mode_320p_480_320,     480, 320,
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
		{mx6000_mode_854p_480_854,     480, 854,
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
		{mx6000_mode_bt656_640_480,    640, 480,
		mx6000_bt656_720_480,
		ARRAY_SIZE(mx6000_bt656_720_480)},
	},
};

static struct regulator *io_regulator;
static struct regulator *core_regulator;
static struct regulator *analog_regulator;

static int mx6000_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int mx6000_remove(struct i2c_client *client);

static s32 mx6000_read_reg(u16 reg, u16 *val);
static s32 mx6000_write_reg(u16 reg, u16 val);

/* mx6000 read and write reg */
static s32 mx6000_read_reg_val8(u16 reg, u8 *val);
static s32 mx6000_write_reg_val8(u16 reg, u8 val);

static const struct i2c_device_id mx6000_id[] = {
	{"mx6000", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, mx6000_id);

static struct i2c_driver mx6000_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "mx6000",
		  },
	.probe  = mx6000_probe,
	.remove = mx6000_remove,
	.id_table = mx6000_id,
};

static const struct mx6000_datafmt mx6000_colour_fmts[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	{MEDIA_BUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_JPEG},
	{MEDIA_BUS_FMT_ARGB8888_1X32, V4L2_COLORSPACE_JPEG},
	{MEDIA_BUS_FMT_RGB565_1X16, V4L2_COLORSPACE_JPEG},
};

static struct mx6000 *to_mx6000(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct mx6000, subdev);
}

/* Find a data format by a pixel code in an array */
static const struct mx6000_datafmt
			*mx6000_find_datafmt(u32 code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mx6000_colour_fmts); i++)
		if (mx6000_colour_fmts[i].code == code)
			return mx6000_colour_fmts + i;

	return NULL;
}

static inline void mx6000_power_down(int enable)
{
	gpio_set_value_cansleep(pwn_gpio, enable);

	msleep(2);
}

static inline void mx6000_reset(void)
{
	gpio_set_value_cansleep(rst_gpio, 0);
	gpio_set_value_cansleep(pwn_gpio, 0);
	msleep(10);
	gpio_set_value_cansleep(rst_gpio, 1);
	msleep(5);
}

static int mx6000_regulator_enable(struct device *dev)
{
	int ret = 0;

	io_regulator = devm_regulator_get(dev, "DOVDD");
	if (!IS_ERR(io_regulator)) {
		regulator_set_voltage(io_regulator,
				      MX6000_VOLTAGE_DIGITAL_IO,
				      MX6000_VOLTAGE_DIGITAL_IO);
		ret = regulator_enable(io_regulator);
		if (ret) {
			dev_err(dev, "set io voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set io voltage ok\n");
		}
	} else {
		io_regulator = NULL;
		dev_warn(dev, "cannot get io voltage\n");
	}

	core_regulator = devm_regulator_get(dev, "DVDD");
	if (!IS_ERR(core_regulator)) {
		regulator_set_voltage(core_regulator,
				      MX6000_VOLTAGE_DIGITAL_CORE,
				      MX6000_VOLTAGE_DIGITAL_CORE);
		ret = regulator_enable(core_regulator);
		if (ret) {
			dev_err(dev, "set core voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set core voltage ok\n");
		}
	} else {
		core_regulator = NULL;
		dev_warn(dev, "cannot get core voltage\n");
	}

	analog_regulator = devm_regulator_get(dev, "AVDD");
	if (!IS_ERR(analog_regulator)) {
		regulator_set_voltage(analog_regulator,
				      MX6000_VOLTAGE_ANALOG,
				      MX6000_VOLTAGE_ANALOG);
		ret = regulator_enable(analog_regulator);
		if (ret) {
			dev_err(dev, "set analog voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set analog voltage ok\n");
		}
	} else {
		analog_regulator = NULL;
		dev_warn(dev, "cannot get analog voltage\n");
	}

	return ret;
}

static s32 mx6000_write_reg_val8(u16 reg, u8 val)
{
	u8 au8Buf[3] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	if (i2c_master_send(mx6000_data.i2c_client, au8Buf, 3) < 0) {
		pr_err("%s:write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
		return -1;
	}

	return 0;
}

static s32 mx6000_read_reg_val8(u16 reg, u8 *val)
{
	u8 au8RegBuf[2] = {0};
	u8 u8RdVal = 0;

	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

	if (2 != i2c_master_send(mx6000_data.i2c_client, au8RegBuf, 2)) {
		pr_err("%s:write reg error:reg=%x\n",
				__func__, reg);
		return -1;
	}

	if (1 != i2c_master_recv(mx6000_data.i2c_client, &u8RdVal, 1)) {
		pr_err("%s:read reg error:reg=%x,val=%x\n",
				__func__, reg, u8RdVal);
		return -1;
	}

	pr_info("u8RdVal = 0x%x \n", u8RdVal);
	*val = u8RdVal;

	return u8RdVal;
}

static s32 mx6000_write_reg(u16 reg, u16 val)
{
	u8 au8Buf[4] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val >> 8;
	au8Buf[3] = val & 0xff;

	if (i2c_master_send(mx6000_data.i2c_client, au8Buf, 4) < 0) {
		pr_err("%s:write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
		return -1;
	}

	return 0;
}

static s32 mx6000_read_reg(u16 reg, u16 *val)
{
	u8 au8RegBuf[2] = {0};
	u8 u8RdVal[2] = {0};

	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

	if (2 != i2c_master_send(mx6000_data.i2c_client, au8RegBuf, 2)) {
		pr_err("%s:write reg error:reg=%x\n",
				__func__, reg);
		return -1;
	}

	if (2 != i2c_master_recv(mx6000_data.i2c_client, u8RdVal, 2)) {
		pr_err("%s:read reg error:reg=%x,val=%x\n",
				__func__, reg, u8RdVal);
		return -1;
	}

	pr_info("u8RdVal[0] = 0x%x , u8RdVal[1] = 0x%x \n", u8RdVal[0], u8RdVal[1]);
	*val = (((u16)u8RdVal[0]) << 8) | u8RdVal[1];

	return *val;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mx6000_get_register(struct v4l2_subdev *sd,
					struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 val;

	if (reg->reg & ~0xffff)
		return -EINVAL;

	reg->size = 1;

	ret = mx6000_read_reg(reg->reg, &val);
	if (!ret)
		reg->val = (__u64)val;

	return ret;
}

static int mx6000_set_register(struct v4l2_subdev *sd,
					const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->reg & ~0xffff || reg->val & ~0xff)
		return -EINVAL;

	return mx6000_write_reg(reg->reg, reg->val);
}
#endif

static void mx6000_soft_reset(void)
{
	/* sysclk from pad */
	mx6000_write_reg(0x3103, 0x11);

	/* software reset */
	mx6000_write_reg(0x3008, 0x82);

	/* delay at least 5ms */
	msleep(10);
}

/* set sensor driver capability
 * 0x302c[7:6] - strength
	00     - 1x
	01     - 2x
	10     - 3x
	11     - 4x
 */
static int mx6000_driver_capability(int strength)
{
	u16 temp = 0;

	if (strength > 4 || strength < 1) {
		pr_err("The valid driver capability of mx6000 is 1x~4x\n");
		return -EINVAL;
	}

	mx6000_read_reg(0x302c, &temp);

	temp &= ~0xc0;	/* clear [7:6] */
	temp |= ((strength - 1) << 6);	/* set [7:6] */

	mx6000_write_reg(0x302c, temp);

	return 0;
}

/* calculate sysclk */
static int mx6000_get_sysclk(void)
{
	int xvclk = mx6000_data.mclk / 10000;
	int sysclk;
	int temp1, temp2;
	int Multiplier, PreDiv, VCO, SysDiv, Pll_rdiv, Bit_div2x, sclk_rdiv;
	int sclk_rdiv_map[] = {1, 2, 4, 8};
	u16 regval = 0;

	temp1 = mx6000_read_reg(0x3034, &regval);
//	pr_info("%s: reg(0x3034) = %#x\n", __func__, temp1);
	temp2 = temp1 & 0x0f;
	if (temp2 == 8 || temp2 == 10) {
		Bit_div2x = temp2 / 2;
	} else {
		pr_err("mx6000: unsupported bit mode %d\n", temp2);
		return -1;
	}

	temp1 = mx6000_read_reg(0x3035, &regval);
//	pr_info("%s: reg(0x3035) = %#x\n", __func__, temp1);
	SysDiv = temp1 >> 4;
	if (SysDiv == 0)
		SysDiv = 16;

	temp1 = mx6000_read_reg(0x3036, &regval);
//	pr_info("%s: reg(0x3036) = %#x\n", __func__, temp1);
	Multiplier = temp1;
	temp1 = mx6000_read_reg(0x3037, &regval);
//	pr_info("%s: reg(0x3037) = %#x\n", __func__, temp1);
	PreDiv = temp1 & 0x0f;
	Pll_rdiv = ((temp1 >> 4) & 0x01) + 1;

	temp1 = mx6000_read_reg(0x3108, &regval);
//	pr_info("%s: reg(0x3108) = %#x\n", __func__, temp1);
	temp2 = temp1 & 0x03;

	sclk_rdiv = sclk_rdiv_map[temp2];
	VCO = xvclk * Multiplier / PreDiv;
	sysclk = VCO / SysDiv / Pll_rdiv * 2 / Bit_div2x / sclk_rdiv;
#if 0
	pr_info("%s: xvclk = %u, Multiplier = %d, Prediv = %d, VCO = %d\n",
		__func__, xvclk, Multiplier, PreDiv, VCO);
	pr_info("%s: SysDiv = %d, Pll_rdiv = %d, Bit_div2x = %d, sclk_rdiv = %d\n",
		__func__, SysDiv, Pll_rdiv, Bit_div2x, sclk_rdiv);

	pr_info("%s: sysclk = %d\n", __func__, sysclk);
#endif

	return sysclk;
}

/* read HTS from register settings */
static int mx6000_get_HTS(void)
{
	return 0;
}

/* read VTS from register settings */
static int mx6000_get_VTS(void)
{
	return 0;
}

/* write VTS to registers */
static int mx6000_set_VTS(int VTS)
{
	return 0;
}

/* read shutter, in number of line period */
static int mx6000_get_shutter(void)
{
	return 0;
}

/* write shutter, in number of line period */
static int mx6000_set_shutter(int shutter)
{
	return 0;
}

/* read gain, 16 = 1x */
static int mx6000_get_gain16(void)
{
	return 0;
}

/* write gain, 16 = 1x */
static int mx6000_set_gain16(int gain16)
{
	return 0;
}

/* get banding filter value */
static int mx6000_get_light_freq(void)
{
	return 0;
}

static void mx6000_set_bandingfilter(void)
{
	;
}

/* stable in high */
static int mx6000_set_AE_target(int target)
{
	return 0;
}

/* enable = 0 to turn off night mode
   enable = 1 to turn on night mode */
static int mx6000_set_night_mode(int enable)
{
	return 0;
}

/* enable = 0 to turn off AEC/AGC
   enable = 1 to turn on AEC/AGC */
static void mx6000_turn_on_AE_AG(int enable)
{
	;
}

/* download mx6000 settings to sensor through i2c */
static int mx6000_download_firmware(struct reg_value *pModeSetting, s32 ArySize)
{
	register u32 Delay_ms = 0;
	register u16 RegAddr = 0;
	register u16 Mask = 0;
	register u16 Val = 0;
	u16 RegVal = 0;
	int i, retval = 0;

	for (i = 0; i < ArySize; ++i, ++pModeSetting) {
		Delay_ms = pModeSetting->u32Delay_ms;
		RegAddr = pModeSetting->u16RegAddr;
		Val = pModeSetting->u16Val;
		Mask = pModeSetting->u16Mask;

		if (Mask) {
			retval = mx6000_read_reg(RegAddr, &RegVal);
			if (retval < 0)
				goto err;

			RegVal &= ~(u16)Mask;
			Val &= Mask;
			Val |= RegVal;
		}

		retval = mx6000_write_reg(RegAddr, Val);
		if (retval < 0)
			goto err;

		if (Delay_ms)
			udelay(Delay_ms);
	}
err:
	return retval;
}

static int mx6000_init_mode(void)
{
	struct reg_value *pModeSetting = NULL;
	int ArySize = 0, retval = 0;

#if 0
	mx6000_soft_reset();

	pModeSetting = mx6000_global_init_setting;
	ArySize = ARRAY_SIZE(mx6000_global_init_setting);
	retval = mx6000_download_firmware(pModeSetting, ArySize);
	if (retval < 0)
		goto err;
	pModeSetting = mx6000_init_setting_30fps_VGA;
	ArySize = ARRAY_SIZE(mx6000_init_setting_30fps_VGA);
#else

	pModeSetting = mx6000_setting_30fps_VGA_640_480;
	ArySize = ARRAY_SIZE(mx6000_setting_30fps_VGA_640_480);
	retval = mx6000_download_firmware(pModeSetting, ArySize);
	if (retval < 0)
		goto err;

#endif
	pr_info("%s: config bt656 success\n", __func__);
	/* change driver capability to 2x according to validation board.
	 * if the image is not stable, please increase the driver strength.
	 */
//	mx6000_driver_capability(2);
//	mx6000_set_bandingfilter();
//	mx6000_set_AE_target(AE_Target);
//	mx6000_set_night_mode(night_mode);

	/* turn off night mode */
	night_mode = 0;
#ifdef RES_960_1280
	mx6000_data.pix.width = 960;
	mx6000_data.pix.height = 1280;
#endif
#ifdef RES_540_640
	mx6000_data.pix.width = 540;
	mx6000_data.pix.height = 640;
#endif
#ifdef RES_480_640
	mx6000_data.pix.width = 480;
	mx6000_data.pix.height = 640;
#endif
err:
	return retval;
}

/* change to or back to subsampling mode set the mode directly
 * image size below 1280 * 960 is subsampling mode */
static int mx6000_change_mode_direct(enum mx6000_frame_rate frame_rate,
			    enum mx6000_mode mode)
{
	struct reg_value *pModeSetting = NULL;
	s32 ArySize = 0;
	int retval = 0;

	if (mode > mx6000_mode_MAX || mode < mx6000_mode_MIN) {
		pr_err("Wrong mx6000 mode detected!\n");
		return -1;
	}

	pr_info("%s: mode = %d, frame_rate = %d bp1\n", __func__, mode, frame_rate);
	pModeSetting = mx6000_mode_info_data[frame_rate][mode].init_data_ptr;
	ArySize =
		mx6000_mode_info_data[frame_rate][mode].init_data_size;

	mx6000_data.pix.width = mx6000_mode_info_data[frame_rate][mode].width;
	mx6000_data.pix.height = mx6000_mode_info_data[frame_rate][mode].height;

	pr_info("%s: width = %d, height = %d\n",
		__func__, mx6000_data.pix.width, mx6000_data.pix.height);

	if (mx6000_data.pix.width == 0 || mx6000_data.pix.height == 0)/* ||
	    pModeSetting == NULL || ArySize == 0)*/
		return -EINVAL;

	pr_info("%s: mode = %d, frame_rate = %d bp2\n", __func__, mode, frame_rate);
	/* set mx6000 to subsampling mode */
//	retval = mx6000_download_firmware(pModeSetting, ArySize);

	/* turn on AE AG for subsampling mode, in case the firmware didn't */
//	mx6000_turn_on_AE_AG(1);

	/* calculate banding filter */
//	mx6000_set_bandingfilter();

	/* set AE target */
//	mx6000_set_AE_target(AE_Target);

	/* update night mode setting */
//	mx6000_set_night_mode(night_mode);

	/* skip 9 vysnc: start capture at 10th vsync */
	if (mode == mx6000_mode_XGA_1024_768 && frame_rate == mx6000_30_fps) {
		pr_warning("mx6000: actual frame rate of XGA is 22.5fps\n");
		/* 1/22.5 * 9*/
		msleep(400);
		return retval;
	}

	if (frame_rate == mx6000_15_fps) {
		/* 1/15 * 9*/
	//	msleep(600);
	} else if (frame_rate == mx6000_30_fps) {
		/* 1/30 * 9*/
		//msleep(300);
	}

	return retval;
}

/* change to scaling mode go through exposure calucation
 * image size above 1280 * 960 is scaling mode */
static int mx6000_change_mode_exposure_calc(enum mx6000_frame_rate frame_rate,
			    enum mx6000_mode mode)
{
	int prev_shutter, prev_gain16, average;
	int cap_shutter, cap_gain16;
	int cap_sysclk, cap_HTS, cap_VTS;
	int light_freq, cap_bandfilt, cap_maxband;
	long cap_gain16_shutter;
	u16 temp;
	struct reg_value *pModeSetting = NULL;
	s32 ArySize = 0;
	int retval = 0;

	/* check if the input mode and frame rate is valid */
	pModeSetting =
		mx6000_mode_info_data[frame_rate][mode].init_data_ptr;
	ArySize =
		mx6000_mode_info_data[frame_rate][mode].init_data_size;

	mx6000_data.pix.width =
		mx6000_mode_info_data[frame_rate][mode].width;
	mx6000_data.pix.height =
		mx6000_mode_info_data[frame_rate][mode].height;

	if (mx6000_data.pix.width == 0 || mx6000_data.pix.height == 0 ||
		pModeSetting == NULL || ArySize == 0)
		return -EINVAL;

	/* read preview shutter */
	prev_shutter = mx6000_get_shutter();

	/* read preview gain */
	prev_gain16 = mx6000_get_gain16();

	/* get average */
	average = mx6000_read_reg(0x56a1, &temp);

	/* turn off night mode for capture */
//	mx6000_set_night_mode(0);

	/* turn off overlay */
	mx6000_write_reg(0x3022, 0x06);

	/* Write capture setting */
	retval = mx6000_download_firmware(pModeSetting, ArySize);
	if (retval < 0)
		goto err;

	/* turn off AE AG when capture image. */
	mx6000_turn_on_AE_AG(0);

	/* read capture VTS */
	cap_VTS = mx6000_get_VTS();
	cap_HTS = mx6000_get_HTS();
	cap_sysclk = mx6000_get_sysclk();

	/* calculate capture banding filter */
	light_freq = mx6000_get_light_freq();
	if (light_freq == 60) {
		/* 60Hz */
		cap_bandfilt = cap_sysclk * 100 / cap_HTS * 100 / 120;
	} else {
		/* 50Hz */
		cap_bandfilt = cap_sysclk * 100 / cap_HTS;
	}
	cap_maxband = (int)((cap_VTS - 4)/cap_bandfilt);
	/* calculate capture shutter/gain16 */
	if (average > AE_low && average < AE_high) {
		/* in stable range */
		cap_gain16_shutter =
			prev_gain16 * prev_shutter * cap_sysclk/prev_sysclk *
			prev_HTS/cap_HTS * AE_Target / average;
	} else {
		cap_gain16_shutter =
			prev_gain16 * prev_shutter * cap_sysclk/prev_sysclk *
			prev_HTS/cap_HTS;
	}

	/* gain to shutter */
	if (cap_gain16_shutter < (cap_bandfilt * 16)) {
		/* shutter < 1/100 */
		cap_shutter = cap_gain16_shutter/16;
		if (cap_shutter < 1)
			cap_shutter = 1;
		cap_gain16 = cap_gain16_shutter/cap_shutter;
		if (cap_gain16 < 16)
			cap_gain16 = 16;
	} else {
		if (cap_gain16_shutter > (cap_bandfilt*cap_maxband*16)) {
			/* exposure reach max */
			cap_shutter = cap_bandfilt*cap_maxband;
			cap_gain16 = cap_gain16_shutter / cap_shutter;
		} else {
			/* 1/100 < cap_shutter =< max, cap_shutter = n/100 */
			cap_shutter =
				((int)(cap_gain16_shutter/16/cap_bandfilt))
				* cap_bandfilt;
			cap_gain16 = cap_gain16_shutter / cap_shutter;
		}
	}

	/* write capture gain */
	mx6000_set_gain16(cap_gain16);

	/* write capture shutter */
	if (cap_shutter > (cap_VTS - 4)) {
		cap_VTS = cap_shutter + 4;
		mx6000_set_VTS(cap_VTS);
	}

	mx6000_set_shutter(cap_shutter);

	/* skip 2 vysnc: start capture at 3rd vsync
	 * frame rate of QSXGA and 1080P is 7.5fps: 1/7.5 * 2
	 */
	pr_warning("mx6000: the actual frame rate of %s is 7.5fps\n",
		mode == mx6000_mode_1080P_1920_1080 ? "1080P" : "QSXGA");
	msleep(267);
err:
	return retval;
}

static int mx6000_change_mode(enum mx6000_frame_rate frame_rate,
			    enum mx6000_mode mode)
{
	int retval = 0;

	if (mode > mx6000_mode_MAX || mode < mx6000_mode_MIN) {
		pr_err("Wrong mx6000 mode detected!\n");
		return -1;
	}

	if (mode == mx6000_mode_1080P_1920_1080 ||
			mode == mx6000_mode_QSXGA_2592_1944) {
		/* change to scaling mode go through exposure calucation
		 * image size above 1280 * 960 is scaling mode */
		retval = mx6000_change_mode_exposure_calc(frame_rate, mode);
		pr_info("change mx6000 mode exposure calc called!\n");
	} else {
		/* change back to subsampling modem download firmware directly
		 * image size below 1280 * 960 is subsampling mode */
		//retval = mx6000_change_mode_direct(frame_rate, mode);
		//pr_info("change mx6000 mode direct called!\n");
		pr_info("do not change mode\n");
	}

	return retval;
}

/*!
 * mx6000_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int mx6000_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mx6000 *sensor = to_mx6000(client);

	if (on)
		clk_enable(mx6000_data.sensor_clk);
	else
		clk_disable(mx6000_data.sensor_clk);

	sensor->on = on;

	return 0;
}

/*!
 * mx6000_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int mx6000_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mx6000 *sensor = to_mx6000(client);
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ov5460_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int mx6000_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mx6000 *sensor = to_mx6000(client);
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
	enum mx6000_frame_rate frame_rate;
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps > MAX_FPS) {
			timeperframe->denominator = MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < MIN_FPS) {
			timeperframe->denominator = MIN_FPS;
			timeperframe->numerator = 1;
		}

		/* Actual frame rate we use */
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps == 15)
			frame_rate = mx6000_15_fps;
		else if (tgt_fps == 30)
			frame_rate = mx6000_30_fps;
		else {
			pr_err(" The camera frame rate is not supported!\n");
			goto error;
		}

		ret = mx6000_change_mode(frame_rate,
				a->parm.capture.capturemode);
		if (ret < 0)
			goto error;

		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode = a->parm.capture.capturemode;

		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not " \
			"V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

error:
	return ret;
}

static int mx6000_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	const struct mx6000_datafmt *fmt = mx6000_find_datafmt(mf->code);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mx6000 *sensor = to_mx6000(client);

	if (format->pad)
		return -EINVAL;

	if (!fmt) {
		mf->code	= mx6000_colour_fmts[0].code;
		mf->colorspace	= mx6000_colour_fmts[0].colorspace;
	}

	mf->field	= V4L2_FIELD_NONE;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	sensor->fmt = fmt;

	return 0;
}

static int mx6000_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mx6000 *sensor = to_mx6000(client);
	const struct mx6000_datafmt *fmt = sensor->fmt;

	if (format->pad)
		return -EINVAL;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int mx6000_enum_code(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(mx6000_colour_fmts))
		return -EINVAL;

	code->code = mx6000_colour_fmts[code->index].code;
	return 0;
}

/*!
 * mx6000_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int mx6000_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index > mx6000_mode_MAX)
		return -EINVAL;

	fse->max_width =
			max(mx6000_mode_info_data[0][fse->index].width,
			    mx6000_mode_info_data[1][fse->index].width);
	fse->min_width = fse->max_width;
	fse->max_height =
			max(mx6000_mode_info_data[0][fse->index].height,
			    mx6000_mode_info_data[1][fse->index].height);
	fse->min_height = fse->max_height;
	return 0;
}

/*!
 * mx6000_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int mx6000_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	int i, j, count;

	if (fie->index < 0 || fie->index > mx6000_mode_MAX)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 ||
	    fie->code == 0) {
		pr_warning("Please assign pixel format, width and height.\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;

	count = 0;
	for (i = 0; i < ARRAY_SIZE(mx6000_mode_info_data); i++) {
		for (j = 0; j < (mx6000_mode_MAX + 1); j++) {
			if (fie->width == mx6000_mode_info_data[i][j].width
			 && fie->height == mx6000_mode_info_data[i][j].height
			 && mx6000_mode_info_data[i][j].init_data_ptr != NULL) {
				count++;
			}
			if (fie->index == (count - 1)) {
				fie->interval.denominator =
						mx6000_framerates[i];
				return 0;
			}
		}
	}

	return -EINVAL;
}

static int mx6000_set_clk_rate(void)
{
	u32 tgt_xclk;	/* target xclk */
	int ret;

	/* mclk */
	tgt_xclk = mx6000_data.mclk;
	tgt_xclk = min(tgt_xclk, (u32)MX6000_XCLK_MAX);
	tgt_xclk = max(tgt_xclk, (u32)MX6000_XCLK_MIN);
	mx6000_data.mclk = tgt_xclk;

	pr_debug("   Setting mclk to %d MHz\n", tgt_xclk / 1000000);
	ret = clk_set_rate(mx6000_data.sensor_clk, mx6000_data.mclk);
	if (ret < 0)
		pr_debug("set rate filed, rate=%d\n", mx6000_data.mclk);
	return ret;
}

//////////////////////////////////////////////////////////////////////////////////////
/*
static ssize_t ov7956_test_store(struct kobject *kobj, struct kobj_attribute *attr,
                                const char *buf, size_t n)
{
    u32 getdata[8];
    u16 regAddr;
    u8 data,data1;
    char cmd;
	int ret;
    const char *buftmp = buf;
    //struct ov_camera_module *cam_mod = g_cam_mod;
*/
    /**
     * W Addr(8Bit) regAddr(8Bit) data0(8Bit) data1(8Bit) data2(8Bit) data3(8Bit)
     * 		:data can be less than 4 byte
     * R regAddr(8Bit)
     * C gpio_name(poweron/powerhold/sleep/boot0/boot1) value(H/L)
     */
/*
	sscanf(buftmp, "%c ", &cmd);
	printk("------debug: get cmd = %c\n", cmd);
	switch (cmd) {
	case 'w':
		sscanf(buftmp, "%c %x %x ", &cmd, &getdata[0], &getdata[1]);
		regAddr = (u16)(getdata[0]);
		data = (u8)(getdata[1] & 0xff);
		printk("get value = %x\n", data);
		mx6000_write_reg(regAddr, data);

		//ov_camera_module_write_reg(cam_mod, regAddr,data);
		mx6000_read_reg(regAddr, &data1);
		//ret |= ov_camera_module_read_reg(cam_mod, regAddr, &data1);
		printk("successfully write +++++++++++++reg_add=0x%04x-----reg_value=%02x\n",getdata[0],data1);
		//printk("%x   %x\n", getdata[1], data);
		break;
	case 'r':
		sscanf(buftmp, "%c %x ", &cmd, &getdata[0]);
		printk("CMD : %c %x\n", cmd, getdata[0]);
		regAddr = (u16)(getdata[0]);
		mx6000_read_reg(regAddr, &data);
		//rk818_i2c_read(rk818, regAddr, 1, &data);

		//ret |= ov_camera_module_read_reg(cam_mod, regAddr, &data);
		printk("successfully read ++++++++++++reg_add=0x%04x-----reg_value=%02x\n",getdata[0],data);
		//printk("%x %x\n", getdata[0], data);
		break;
	default:
		printk("Unknown command\n");
		break;
	}
	return n;

}
*/
/*
static ssize_t ov7956_test_show(struct kobject *kobj, struct kobj_attribute *attr,
                               char *buf)
{
   char *s = buf;
    buf = "hello";
    return sprintf(s, "%s\n", buf);

}

static struct kobject *ov7956_kobj;
struct ov7956_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr,
			const char *buf, size_t n);
};
*/
/*
static struct ov7956_attribute ov7956_attrs[] = {
*/
	/*     node_name	permision		show_func	store_func */
/*	__ATTR(ov7956_test,	S_IRUGO | S_IWUSR,	ov7956_test_show,	ov7956_test_store),
};
*/
////////////////////////////////////////////////////////////////////////////////////
/*!
 * dev_init - V4L2 sensor init
 * @s: pointer to standard V4L2 device structure
 *
 */
static int init_device(void)
{
	u32 tgt_xclk;	/* target xclk */
	u32 tgt_fps;	/* target frames per secound */
	enum mx6000_frame_rate frame_rate;
	int ret;

	mx6000_data.on = true;

	/* mclk */
	tgt_xclk = mx6000_data.mclk;

	/* Default camera frame rate is set in probe */
	tgt_fps = mx6000_data.streamcap.timeperframe.denominator /
		  mx6000_data.streamcap.timeperframe.numerator;

	if (tgt_fps == 15)
		frame_rate = mx6000_15_fps;
	else if (tgt_fps == 30)
		frame_rate = mx6000_30_fps;
	else
		return -EINVAL; /* Only support 15fps or 30fps now. */

	ret = mx6000_init_mode();

	return ret;
}

static void depth_set_video_mode(void)
{
#ifdef RES_960_1280
	mx6000_write_reg_val8(0x0020 + 0x13, 0x19);
#endif
#ifdef RES_540_640
	//540 * 640
	mx6000_write_reg_val8(0x0020 + 0x13, 0x17);
#endif
#ifdef RES_480_640
	//480 * 640
	mx6000_write_reg_val8(0x0020 + 0x13, 0x15);
#endif
	mx6000_write_reg_val8(0x0020 + 0x15, 30);
	mx6000_write_reg_val8(0x0020 + 0x14, 0x04);
	mx6000_write_reg_val8(0x002d, 0x01);
}

static void ir_set_video_mode(void)
{
#ifdef RES_960_1280
	mx6000_write_reg_val8(0x0048 + 0x13, 0x19);
#endif
#ifdef RES_540_640
	//540 * 640
	mx6000_write_reg_val8(0x0048 + 0x13, 0x17);
#endif
#ifdef RES_480_640
	//480 * 640
	mx6000_write_reg_val8(0x0048 + 0x13, 0x15);
#endif
	mx6000_write_reg_val8(0x0048 + 0x15, 30);
	mx6000_write_reg_val8(0x0048 + 0x14, 0x04);
	mx6000_write_reg_val8(0x0055, 0x01);
}

static struct v4l2_subdev_video_ops mx6000_subdev_video_ops = {
	.g_parm = mx6000_g_parm,
	.s_parm = mx6000_s_parm,
};

static const struct v4l2_subdev_pad_ops mx6000_subdev_pad_ops = {
	.enum_frame_size       = mx6000_enum_framesizes,
	.enum_frame_interval   = mx6000_enum_frameintervals,
	.enum_mbus_code        = mx6000_enum_code,
	.set_fmt               = mx6000_set_fmt,
	.get_fmt               = mx6000_get_fmt,
};

static struct v4l2_subdev_core_ops mx6000_subdev_core_ops = {
	.s_power	= mx6000_s_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mx6000_get_register,
	.s_register	= mx6000_set_register,
#endif
};

static struct v4l2_subdev_ops mx6000_subdev_ops = {
	.core	= &mx6000_subdev_core_ops,
	.video	= &mx6000_subdev_video_ops,
	.pad	= &mx6000_subdev_pad_ops,
};

/*!
 * mx6000 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int mx6000_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct pinctrl *pinctrl;
	struct device *dev = &client->dev;
	int retval;
	int i;
	int ret;
	u8 chip_id_high, chip_id_low;
	u16 dvp_retval = 0x00;
	u16 mx6000_id = 0x00;
	u8 mx6000_reso = 0x00;
	u8 mx6000_mode = 0x00;

	/* mx6000 pinctrl */
	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "setup pinctrl failed\n");
		return PTR_ERR(pinctrl);
	}

	/* request power down pin */
	pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(pwn_gpio)) {
		dev_err(dev, "no sensor pwdn pin available\n");
		return -ENODEV;
	}
	retval = devm_gpio_request_one(dev, pwn_gpio, GPIOF_OUT_INIT_HIGH,
					"mx6000_pwdn");
	if (retval < 0)
		return retval;

	/* request reset pin */
	rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(rst_gpio)) {
		dev_err(dev, "no sensor reset pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, rst_gpio, GPIOF_OUT_INIT_HIGH,
					"mx6000_reset");
	if (retval < 0)
		return retval;

	/* Set initial values for the sensor struct. */
	memset(&mx6000_data, 0, sizeof(mx6000_data));
	mx6000_data.sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(mx6000_data.sensor_clk)) {
		dev_err(dev, "get mclk failed\n");
		return PTR_ERR(mx6000_data.sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk",
					&mx6000_data.mclk);
	if (retval) {
		dev_err(dev, "mclk frequency is invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &(mx6000_data.mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi_id",
					&(mx6000_data.csi));
	if (retval) {
		dev_err(dev, "csi_id invalid\n");
		return retval;
	}

	/* Set mclk rate before clk on */
	mx6000_set_clk_rate();

	clk_prepare_enable(mx6000_data.sensor_clk);

	mx6000_data.io_init = mx6000_reset;
	mx6000_data.i2c_client = client;
	mx6000_data.pix.pixelformat = V4L2_PIX_FMT_RGB565;
#ifdef RES_960_1280
	mx6000_data.pix.width = 960;
	mx6000_data.pix.height = 1280;
#endif
#ifdef RES_540_640
	mx6000_data.pix.width = 540;
	mx6000_data.pix.height = 640;
#endif
#ifdef RES_480_640
	mx6000_data.pix.width = 480;
	mx6000_data.pix.height = 640;
#endif
	mx6000_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
					   V4L2_CAP_TIMEPERFRAME;
	mx6000_data.streamcap.capturemode = 0;
	mx6000_data.streamcap.timeperframe.denominator = DEFAULT_FPS;
	mx6000_data.streamcap.timeperframe.numerator = 1;

	//mx6000_regulator_enable(&client->dev);

	mx6000_reset();

/*
	retval = mx6000_read_reg(MX6000_CHIP_ID_HIGH_BYTE, &chip_id_high);
	if (retval < 0 || chip_id_high != 0x56) {
		clk_disable_unprepare(mx6000_data.sensor_clk);
		pr_warning("camera mx6000 is not found\n");
		return -ENODEV;
	}
	retval = mx6000_read_reg(MX6000_CHIP_ID_LOW_BYTE, &chip_id_low);
	if (retval < 0 || chip_id_low != 0x40) {
		clk_disable_unprepare(mx6000_data.sensor_clk);
		pr_warning("camera mx6000 is not found\n");
		return -ENODEV;
	}

*/

	//pr_info("mx6000 i2c address 1: 0x%x\n", mx6000_data.i2c_client->addr);
	retval = init_device();
	if (retval < 0) {
		clk_disable_unprepare(mx6000_data.sensor_clk);
		pr_warning("camera mx6000 init failed\n");
		mx6000_power_down(1);
		return retval;
	}
#if 0
	// delay 1 second
	msleep(700);

	mx6000_read_reg(0x0018, &dvp_retval);
	pr_info("mx6000 dvp return value of 0x0018 is: 0x%x\n", dvp_retval);

	mx6000_read_reg(0x0004, &dvp_retval);
	pr_info("mx6000 dvp return value of 0x0004 is: 0x%x\n", dvp_retval);

	// config the mx6000 through 0x10
	mx6000_data.i2c_client->addr = 0x10;

	mx6000_read_reg(0x0000, &mx6000_id);
	pr_info("mx6000 id is: 0x%x\n", mx6000_id);

	//set the depth and ir output
	depth_set_video_mode();
	pr_info("mx6000 depth set is OK!\n");
	ir_set_video_mode();
	pr_info("mx6000 ir set is OK!\n");

	mx6000_read_reg_val8(0x2f, &mx6000_reso);
	pr_info("mx6000 ir reso reg is: 0x%x\n", mx6000_reso);
	mx6000_read_reg_val8(0x57, &mx6000_mode);
	pr_info("mx6000 depth reso reg is: 0x%x\n", mx6000_mode);

	mx6000_data.i2c_client->addr = 0x0e;
#endif
	/* create debug kobject */
	/*
        ov7956_kobj = kobject_create_and_add("ov7956", NULL);
        if (!ov7956_kobj) {
                ret = -ENOMEM;
                goto err_irq;
        }

        for (i = 0; i < ARRAY_SIZE(ov7956_attrs); i++) {
                ret = sysfs_create_file(ov7956_kobj, &ov7956_attrs[i].attr);
                if (ret != 0) {
                       // dev_err(ov7725->dev, "create index %d error\n", i);
                        goto err_irq;
                }
        }
	*/
	clk_disable(mx6000_data.sensor_clk);

	v4l2_i2c_subdev_init(&mx6000_data.subdev, client, &mx6000_subdev_ops);

	retval = v4l2_async_register_subdev(&mx6000_data.subdev);
	if (retval < 0)
		dev_err(&client->dev,
					"%s--Async register failed, ret=%d\n", __func__, retval);

	pr_info("camera mx6000, is found\n");
	err_irq:
	return retval;
}

/*!
 * mx6000 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int mx6000_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_async_unregister_subdev(sd);

	clk_unprepare(mx6000_data.sensor_clk);

	mx6000_power_down(1);

	if (analog_regulator)
		regulator_disable(analog_regulator);

	if (core_regulator)
		regulator_disable(core_regulator);

	if (io_regulator)
		regulator_disable(io_regulator);

	return 0;
}

module_i2c_driver(mx6000_i2c_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MX6000 Camera Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
