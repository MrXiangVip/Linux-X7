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

#define QH430MR_VOLTAGE_ANALOG               2800000
#define QH430MR_VOLTAGE_DIGITAL_CORE         1500000
#define QH430MR_VOLTAGE_DIGITAL_IO           1800000

#define MIN_FPS 15
#define MAX_FPS 30
#define DEFAULT_FPS 30

#define QH430MR_XCLK_MIN 6000000
#define QH430MR_XCLK_MAX 24000000

#define QH430MR_CAM_WIDTH	1280
#define QH430MR_CAM_HEIGHT  800

enum qh430mr_mode {
	qh430mr_mode_MIN = 0,
	qh430mr_mode_1280_800 = 0,
	qh430mr_mode_MAX = 1
};

enum qh430mr_frame_rate {
	qh430mr_15_fps,
	qh430mr_30_fps
};

struct qh430mr_datafmt {
	u32	code;
	enum v4l2_colorspace		colorspace;
};

struct reg_value {
	u16 u16RegAddr;
	u16 u16Val;
	u16 u16Mask;
	u32 u32Delay_ms;
};

struct qh430mr_mode_info {
	enum qh430mr_mode mode;
	u32 width;
	u32 height;
	struct reg_value *init_data_ptr;
	u32 init_data_size;
};

struct qh430mr {
	struct v4l2_subdev		subdev;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	const struct qh430mr_datafmt	*fmt;
	struct v4l2_captureparm streamcap;
	bool on;

	u32 mclk;
	u8 mclk_source;
	struct clk *sensor_clk;
	int csi;
};

/*!
 * Maintains the information on the current state of the sesor.
 */
static struct qh430mr qh430mr_data;
static int brg_rst_gpio, rst_gpio;

static struct reg_value qh430mr_setting_30fps_1280_800[] = {
	{0xC005, 0x49, 0, 0},
	{0xC073, 0x65, 0, 0},
	{0xC092, 0x42, 0, 0},
	{0xC093, 0x64, 0, 0},
	{0xC0B9, 0x1,  0, 0},
	{0xC0A3, 0x64, 0, 0},
	{0xC0AA, 0x6,  0, 0},
	{0xC0BF, 0x3,  0, 0},
	{0xC0A0, 0x1,  0, 0},
	{0xC0BF, 0x3,  0, 0},
	{0xC0A0, 0x3,  0, 0},
	{0xC0BF, 0x3,  0, 0},
	{0xc004, 0x0,  0, 0},
	{0xc0bf, 0x1,  0, 0},
	{0xc003, 0x0,  0, 0},
	{0xc0b6, 0x1,  0, 0},
	{0xc07F, 0x1,  0, 0},
	{0xc0bf, 0x1,  0, 0},
	{0xc249, 0x0,  0, 0},
	{0xc24A, 0x12, 0, 0},
	{0xc24B, 0x12, 0, 0},
	{0xc24C, 0x12, 0, 0},
	{0xC0BF, 0x3,  0, 0},
};

static struct reg_value bridge_tc358746axbg_setting_30fps_1280_800[] = {
	{0x0002, 0x0001, 0, 1000},
	{0x0002, 0x0000, 0, 0},
	{0x0016, 0x3063, 0, 0},
	{0x0018, 0x0403, 0, 1000},
	{0x0018, 0x0413, 0, 0},
	{0x0020, 0x0011, 0, 0},
	{0x0060, 0x800a, 0, 0},
	{0x0006, 0x00c8, 0, 0},
	{0x0008, 0x0001, 0, 0},
	{0x0004, 0x8174, 0, 0},
};

static struct qh430mr_mode_info qh430mr_mode_info_data[2][qh430mr_mode_MAX + 1] = {
	{
		{
			qh430mr_mode_1280_800,
			QH430MR_CAM_WIDTH,
			QH430MR_CAM_HEIGHT,
			qh430mr_setting_30fps_1280_800,
			ARRAY_SIZE(qh430mr_setting_30fps_1280_800)
		},
	},
	{
		{
			qh430mr_mode_1280_800,
			QH430MR_CAM_WIDTH,
			QH430MR_CAM_HEIGHT,
			qh430mr_setting_30fps_1280_800,
			ARRAY_SIZE(qh430mr_setting_30fps_1280_800)
		},
	},
};

static struct regulator *io_regulator;
static struct regulator *core_regulator;
static struct regulator *analog_regulator;

static int qh430mr_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int qh430mr_remove(struct i2c_client *client);

static s32 qh430mr_read_reg16(u16 reg, u16 *val);
static s32 qh430mr_read_reg8(u16 reg, u8 *val);
static s32 qh430mr_write_reg16(u16 reg, u16 val);
static s32 qh430mr_write_reg8(u16 reg, u8 val);

static const struct i2c_device_id qh430mr_id[] = {
	{"qh430mr", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, qh430mr_id);

static struct i2c_driver qh430mr_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "qh430mr",
		  },
	.probe  = qh430mr_probe,
	.remove = qh430mr_remove,
	.id_table = qh430mr_id,
};

static const struct qh430mr_datafmt qh430mr_colour_fmts[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	{MEDIA_BUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_JPEG},
	{MEDIA_BUS_FMT_ARGB8888_1X32, V4L2_COLORSPACE_JPEG},
	{MEDIA_BUS_FMT_RGB565_1X16, V4L2_COLORSPACE_JPEG},
};

static struct qh430mr *to_qh430mr(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct qh430mr, subdev);
}

/* Find a data format by a pixel code in an array */
static const struct qh430mr_datafmt
			*qh430mr_find_datafmt(u32 code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(qh430mr_colour_fmts); i++)
		if (qh430mr_colour_fmts[i].code == code)
			return qh430mr_colour_fmts + i;

	return NULL;
}

static inline void qh430mr_reset(void)
{
	gpio_set_value_cansleep(rst_gpio, 0);
	gpio_set_value_cansleep(brg_rst_gpio, 0);
	msleep(10);
	gpio_set_value_cansleep(brg_rst_gpio, 1);
	gpio_set_value_cansleep(rst_gpio, 1);
	msleep(5);
}

static int qh430mr_regulator_enable(struct device *dev)
{
	int ret = 0;

	io_regulator = devm_regulator_get(dev, "DOVDD");
	if (!IS_ERR(io_regulator)) {
		regulator_set_voltage(io_regulator,
				      QH430MR_VOLTAGE_DIGITAL_IO,
				      QH430MR_VOLTAGE_DIGITAL_IO);
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
				      QH430MR_VOLTAGE_DIGITAL_CORE,
				      QH430MR_VOLTAGE_DIGITAL_CORE);
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
				      QH430MR_VOLTAGE_ANALOG,
				      QH430MR_VOLTAGE_ANALOG);
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

static s32 qh430mr_write_reg8(u16 reg, u8 val)
{
	u8 au8Buf[4] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	if (i2c_master_send(qh430mr_data.i2c_client, au8Buf, 4) < 0) {
		pr_err("%s:write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
		return -1;
	}

	return 0;
}

static s32 qh430mr_read_reg8(u16 reg, u8 *val)
{
	u8 au8RegBuf[2] = {0};
	u8 u8RdVal = 0;

	au8RegBuf[0] = reg;
	au8RegBuf[1] = reg & 0xff;

	if (2 != i2c_master_send(qh430mr_data.i2c_client, au8RegBuf, 2)) {
		pr_err("%s:write reg error:reg=%x\n",
				__func__, reg);
		return -1;
	}

	if (2 != i2c_master_recv(qh430mr_data.i2c_client, &u8RdVal, 1)) {
		pr_err("%s:read reg error:reg=%x\n",
				__func__, reg);
		return -1;
	}

	pr_info("u8RdVal = 0x%x\n", u8RdVal);
	*val = u8RdVal;

	return *val;
}

static s32 qh430mr_write_reg16(u16 reg, u16 val)
{
	u8 au8Buf[4] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val >> 8;
	au8Buf[3] = val & 0xff;

	if (i2c_master_send(qh430mr_data.i2c_client, au8Buf, 4) < 0) {
		pr_err("%s:write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
		return -1;
	}

	return 0;
}

static s32 qh430mr_read_reg16(u16 reg, u16 *val)
{
	u8 au8RegBuf[2] = {0};
	u8 u8RdVal[2] = {0};

	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

	if (2 != i2c_master_send(qh430mr_data.i2c_client, au8RegBuf, 2)) {
		pr_err("%s:write reg error:reg=%x\n",
				__func__, reg);
		return -1;
	}

	if (2 != i2c_master_recv(qh430mr_data.i2c_client, u8RdVal, 2)) {
		pr_err("%s:read reg error:reg=%x\n",
				__func__, reg);
		return -1;
	}

	pr_info("u8RdVal[0] = 0x%x , u8RdVal[1] = 0x%x \n", u8RdVal[0], u8RdVal[1]);
	*val = (((u16)u8RdVal[0]) << 8) | u8RdVal[1];

	return *val;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int qh430mr_get_register(struct v4l2_subdev *sd,
					struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 val;

	if (reg->reg & ~0xffff)
		return -EINVAL;

	reg->size = 1;

	ret = qh430mr_read_reg(reg->reg, &val);
	if (!ret)
		reg->val = (__u64)val;

	return ret;
}

static int qh430mr_set_register(struct v4l2_subdev *sd,
					const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->reg & ~0xffff || reg->val & ~0xff)
		return -EINVAL;

	return qh430mr_write_reg(reg->reg, reg->val);
}
#endif

/* download qh430mr settings to sensor through i2c */
static int qh430mr_download_firmware(struct reg_value *pModeSetting, s32 ArySize)
{
	register u32 delay_ms = 0;
	register u16 reg_addr = 0;
	register u16 mask = 0;
	register u8 val = 0;
	u8 reg_val = 0;
	int i, retval = 0;

	for (i = 0; i < ArySize; ++i, ++pModeSetting) {
		delay_ms = pModeSetting->u32Delay_ms;
		reg_addr = pModeSetting->u16RegAddr;
		val = pModeSetting->u16Val & 0xff;
		mask = pModeSetting->u16Mask;

		if (mask) {
			retval = qh430mr_read_reg8(reg_addr, &reg_val);
			if (retval < 0)
				goto err;

			reg_val &= ~(u16)mask;
			val &= mask;
			val |= reg_val;
		}

		retval = qh430mr_write_reg8(reg_addr, val);
		if (retval < 0)
			goto err;

		if (delay_ms)
			udelay(delay_ms);
	}
err:
	return retval;
}

static int qh430mr_download_bridge_firmware(struct reg_value *pModeSetting, s32 ArySize)
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
			retval = qh430mr_read_reg16(RegAddr, &RegVal);
			if (retval < 0)
				goto err;

			RegVal &= ~(u16)Mask;
			Val &= Mask;
			Val |= RegVal;
		}

		retval = qh430mr_write_reg16(RegAddr, Val);
		if (retval < 0)
			goto err;

		if (Delay_ms)
			udelay(Delay_ms);
	}
err:
	return retval;
}

static int qh430mr_init_bridge(void)
{
	struct reg_value *pModeSetting = NULL;
	s32 ArySize = 0, retval = 0;

	pModeSetting = bridge_tc358746axbg_setting_30fps_1280_800;
	ArySize = ARRAY_SIZE(bridge_tc358746axbg_setting_30fps_1280_800);
	retval = qh430mr_download_bridge_firmware(pModeSetting, ArySize);

	if (retval < 0)
		goto err;

	pr_info("%s: config bridge success\n", __func__);
	return 0;
err:
	return -1;
}

static s32 qh430mr_camera_test(void)
{
	u8 reg_rd = 0;
	s32 retval = 0;

	retval = qh430mr_read_reg8(0xC000, &reg_rd);
    if (retval < 0) {
		pr_err("read failed!\n");
		return retval;
	}
	if (0x30 == reg_rd) {
		pr_info("Read 0xC000 match!\n");
	} else {
		pr_err("Read 0xC000 mismatch: 0x%x!\n", reg_rd);
	}

	retval = qh430mr_write_reg8(0xC0C0, 0x5a);
	if (retval < 0) {
		pr_err("write 0xC0C0=0x5a failed!\n");
		return retval;
	}
	retval = qh430mr_write_reg8(0xC0BF, 0x03); // force update
	if (retval < 0) {
		pr_err("write 0xC0BF=0x03 failed!\n");
		return retval;
	}
	retval = qh430mr_read_reg8(0xC0C0, &reg_rd);
	if (retval < 0) {
		pr_err("read failed!\n");
		return retval;
	}
	if (reg_rd == 0x5a) {
		pr_info("0xC0C0==0x5a, matched\n");
	} else {
		pr_info("0xC0C0==0x%x, not 0x5a, mismatched\n", reg_rd);
	}
	return 0;
}

static int qh430mr_init_mode(void)
{
	int retval = 0;
	struct reg_value *pModeSetting = NULL;
	int ArySize = 0;

	pModeSetting = qh430mr_setting_30fps_1280_800;
	ArySize = ARRAY_SIZE(qh430mr_setting_30fps_1280_800);
	retval = qh430mr_download_firmware(pModeSetting, ArySize);
	if (retval < 0)
		goto err;

	pr_info("%s: config bt656 success\n", __func__);
	/* change driver capability to 2x according to validation board.
	 * if the image is not stable, please increase the driver strength.
	 */
	qh430mr_data.pix.width = QH430MR_CAM_WIDTH;
	qh430mr_data.pix.height = QH430MR_CAM_HEIGHT;
err:
	return retval;
}

/*!
 * qh430mr_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int qh430mr_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct qh430mr *sensor = to_qh430mr(client);

	if (on)
		clk_enable(qh430mr_data.sensor_clk);
	else
		clk_disable(qh430mr_data.sensor_clk);

	sensor->on = on;

	return 0;
}

/*!
 * qh430mr_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int qh430mr_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct qh430mr *sensor = to_qh430mr(client);
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
static int qh430mr_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct qh430mr *sensor = to_qh430mr(client);
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
	enum qh430mr_frame_rate frame_rate;
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
			frame_rate = qh430mr_15_fps;
		else if (tgt_fps == 30)
			frame_rate = qh430mr_30_fps;
		else {
			pr_err(" The camera frame rate is not supported!\n");
			goto error;
		}

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

static int qh430mr_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	const struct qh430mr_datafmt *fmt = qh430mr_find_datafmt(mf->code);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct qh430mr *sensor = to_qh430mr(client);

	if (format->pad)
		return -EINVAL;

	if (!fmt) {
		mf->code	= qh430mr_colour_fmts[0].code;
		mf->colorspace	= qh430mr_colour_fmts[0].colorspace;
	}

	mf->field	= V4L2_FIELD_NONE;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	sensor->fmt = fmt;

	return 0;
}

static int qh430mr_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct qh430mr *sensor = to_qh430mr(client);
	const struct qh430mr_datafmt *fmt = sensor->fmt;

	if (format->pad)
		return -EINVAL;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int qh430mr_enum_code(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(qh430mr_colour_fmts))
		return -EINVAL;

	code->code = qh430mr_colour_fmts[code->index].code;
	return 0;
}

/*!
 * qh430mr_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int qh430mr_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index > qh430mr_mode_MAX)
		return -EINVAL;

	fse->max_width = 
			max(qh430mr_mode_info_data[0][fse->index].width,
			    qh430mr_mode_info_data[1][fse->index].width);
	fse->min_width = fse->max_width;
	fse->max_height =
			max(qh430mr_mode_info_data[0][fse->index].height,
			    qh430mr_mode_info_data[1][fse->index].height);
	fse->min_height = fse->max_height;
	return 0;
}

/*!
 * qh430mr_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int qh430mr_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index < 0 || fie->index > qh430mr_mode_MAX)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 ||
	    fie->code == 0) {
		pr_warning("Please assign pixel format, width and height.\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = 30;

	return -EINVAL;
}

static int qh430mr_set_clk_rate(void)
{
	u32 tgt_xclk;	/* target xclk */
	int ret;

	/* mclk */
	tgt_xclk = qh430mr_data.mclk;
	tgt_xclk = min(tgt_xclk, (u32)QH430MR_XCLK_MAX);
	tgt_xclk = max(tgt_xclk, (u32)QH430MR_XCLK_MIN);
	qh430mr_data.mclk = tgt_xclk;

	pr_debug("   Setting mclk to %d MHz\n", tgt_xclk / 1000000);
	ret = clk_set_rate(qh430mr_data.sensor_clk, qh430mr_data.mclk);
	if (ret < 0)
		pr_debug("set rate filed, rate=%d\n", qh430mr_data.mclk);
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////
/*!
 * dev_init - V4L2 sensor init
 * @s: pointer to standard V4L2 device structure
 *
 */
static int init_device(void)
{
	int ret;

	qh430mr_data.on = true;

	ret = qh430mr_init_bridge();

	//ret = qh430mr_init_mode();

	return ret;
}

static struct v4l2_subdev_video_ops qh430mr_subdev_video_ops = {
	.g_parm = qh430mr_g_parm,
	.s_parm = qh430mr_s_parm,
};

static const struct v4l2_subdev_pad_ops qh430mr_subdev_pad_ops = {
	.enum_frame_size       = qh430mr_enum_framesizes,
	.enum_frame_interval   = qh430mr_enum_frameintervals,
	.enum_mbus_code        = qh430mr_enum_code,
	.set_fmt               = qh430mr_set_fmt,
	.get_fmt               = qh430mr_get_fmt,
};

static struct v4l2_subdev_core_ops qh430mr_subdev_core_ops = {
	.s_power	= qh430mr_s_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= qh430mr_get_register,
	.s_register	= qh430mr_set_register,
#endif
};

static struct v4l2_subdev_ops qh430mr_subdev_ops = {
	.core	= &qh430mr_subdev_core_ops,
	.video	= &qh430mr_subdev_video_ops,
	.pad	= &qh430mr_subdev_pad_ops,
};

/*!
 * qh430mr I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int qh430mr_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct pinctrl *pinctrl;
	struct device *dev = &client->dev;
	int retval;

	/* qh430mr pinctrl */
	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "setup pinctrl failed\n");
		return PTR_ERR(pinctrl);
	}

	/* request bridge reset pin */
	brg_rst_gpio = of_get_named_gpio(dev->of_node, "bridge-rst-gpios", 0);
	if (!gpio_is_valid(brg_rst_gpio)) {
		dev_err(dev, "no sensor bridge reset pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, brg_rst_gpio, GPIOF_OUT_INIT_HIGH,
					"qh430mr_bridge_reset");
	if (retval < 0)
		return retval;

	/* request reset pin */
	rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(rst_gpio)) {
		dev_err(dev, "no sensor reset pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, rst_gpio, GPIOF_OUT_INIT_HIGH,
					"qh430mr_reset");
	if (retval < 0)
		return retval;

	/* Set initial values for the sensor struct. */
	memset(&qh430mr_data, 0, sizeof(qh430mr_data));
	qh430mr_data.sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(qh430mr_data.sensor_clk)) {
		dev_err(dev, "get mclk failed\n");
		return PTR_ERR(qh430mr_data.sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk",
					&qh430mr_data.mclk);
	if (retval) {
		dev_err(dev, "mclk frequency is invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &(qh430mr_data.mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi_id",
					&(qh430mr_data.csi));
	if (retval) {
		dev_err(dev, "csi_id invalid\n");
		return retval;
	}

	/* Set mclk rate before clk on */
	qh430mr_set_clk_rate();

	clk_prepare_enable(qh430mr_data.sensor_clk);

	qh430mr_data.i2c_client = client;
	qh430mr_data.pix.pixelformat = V4L2_PIX_FMT_RGB565;
	qh430mr_data.pix.width = QH430MR_CAM_WIDTH;
	qh430mr_data.pix.height = QH430MR_CAM_HEIGHT;
	qh430mr_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
					   V4L2_CAP_TIMEPERFRAME;
	qh430mr_data.streamcap.capturemode = 0;
	qh430mr_data.streamcap.timeperframe.denominator = 30;
	qh430mr_data.streamcap.timeperframe.numerator = 1;

	//pr_info("i2c address: 0x%x\n", qh430mr_data.i2c_client->addr);
	//qh430mr_regulator_enable(&client->dev);

	qh430mr_reset();

	retval = init_device();
	if (retval < 0) {
		clk_disable_unprepare(qh430mr_data.sensor_clk);
		pr_warning("camera qh430mr init failed\n");
		return retval;
	}

	clk_disable(qh430mr_data.sensor_clk);

	v4l2_i2c_subdev_init(&qh430mr_data.subdev, client, &qh430mr_subdev_ops);

	retval = v4l2_async_register_subdev(&qh430mr_data.subdev);
	if (retval < 0)
		dev_err(&client->dev,
					"%s--Async register failed, ret=%d\n", __func__, retval);

	pr_info("camera qh430mr, is found\n");

	return retval;
}

/*!
 * qh430mr I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int qh430mr_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_async_unregister_subdev(sd);

	clk_unprepare(qh430mr_data.sensor_clk);

	if (analog_regulator)
		regulator_disable(analog_regulator);

	if (core_regulator)
		regulator_disable(core_regulator);

	if (io_regulator)
		regulator_disable(io_regulator);

	return 0;
}

module_i2c_driver(qh430mr_i2c_driver);

MODULE_AUTHOR("NXP Semiconductor, Inc.");
MODULE_DESCRIPTION("QH430MR Camera Bridge Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
