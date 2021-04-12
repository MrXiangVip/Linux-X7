/*
 * Copyright (C) 2011-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 *
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/mipi_dsi.h>
#include <linux/mxcfb.h>
#include <linux/backlight.h>
#include <video/mipi_display.h>

#include "mipi_dsi.h"

#define ST7701S_TWO_DATA_LANE				(0x2)
#define ST7701S_MAX_DPHY_CLK				(800)
#define ST7701S_CMD_GETHXID					(0xF4)
#define ST7701S_CMD_GETHXID_LEN				(0x4)
#define ST7701S_ID							(0x84)
#define ST7701S_ID_MASK						(0xFF)


#define CHECK_RETCODE(ret)					\
do {								\
	if (ret < 0) {						\
		dev_err(&mipi_dsi->pdev->dev,			\
			"%s ERR: ret:%d, line:%d.\n",		\
			__func__, ret, __LINE__);		\
		return ret;					\
	}							\
} while (0)

static void parse_variadic(int n, u8 *buf, ...)
{
	int i = 0;
	va_list args;

	if (unlikely(!n)) return;

	va_start(args, buf);

	for (i = 0; i < n; i++)
		buf[i + 1] = (u8)va_arg(args, int);

	va_end(args);
}

#define TC358763_DCS_write_1A_nP(n, addr, ...) {		\
	int err;						\
								\
	buf[0] = addr;						\
	parse_variadic(n, buf, ##__VA_ARGS__);			\
								\
	if (n >= 2)						\
		err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi,		\
			MIPI_DSI_DCS_LONG_WRITE, (u32*)buf, n + 1);	\
	else if (n == 1)					\
		err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi,	\
			MIPI_DSI_DCS_SHORT_WRITE_PARAM, (u32*)buf, 0);	\
	else if (n == 0)					\
	{							\
		buf[1] = 0;					\
		err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi,	\
			MIPI_DSI_DCS_SHORT_WRITE, (u32*)buf, 0);	\
	}							\
	CHECK_RETCODE(err);					\
}

#define TC358763_DCS_write_1A_0P(addr)		\
	TC358763_DCS_write_1A_nP(0, addr)

#define TC358763_DCS_write_1A_1P(addr, ...)	\
	TC358763_DCS_write_1A_nP(1, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_2P(addr, ...)	\
	TC358763_DCS_write_1A_nP(2, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_3P(addr, ...)	\
	TC358763_DCS_write_1A_nP(3, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_4P(addr, ...)	\
	TC358763_DCS_write_1A_nP(4, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_5P(addr, ...)	\
	TC358763_DCS_write_1A_nP(5, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_6P(addr, ...)	\
	TC358763_DCS_write_1A_nP(6, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_7P(addr, ...)	\
	TC358763_DCS_write_1A_nP(7, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_11P(addr, ...)	\
	TC358763_DCS_write_1A_nP(11, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_12P(addr, ...)	\
	TC358763_DCS_write_1A_nP(12, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_13P(addr, ...)	\
	TC358763_DCS_write_1A_nP(13, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_14P(addr, ...)	\
	TC358763_DCS_write_1A_nP(14, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_16P(addr, ...)	\
	TC358763_DCS_write_1A_nP(16, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_19P(addr, ...)	\
	TC358763_DCS_write_1A_nP(19, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_34P(addr, ...)	\
	TC358763_DCS_write_1A_nP(34, addr, __VA_ARGS__)

#define TC358763_DCS_write_1A_127P(addr, ...)	\
	TC358763_DCS_write_1A_nP(127, addr, __VA_ARGS__)

#define ACTIVE_HIGH_NAME	"TRUULY-WVGA-SYNC-HIGH"
#define ACTIVE_LOW_NAME		"TRUULY-WVGA-SYNC-LOW"

static struct fb_videomode truly_lcd_modedb[] = {
	{
		ACTIVE_HIGH_NAME, 50, 480, 800, 41042,
		40, 60,
		3, 3,
		8, 4,
		0x0,
		FB_VMODE_NONINTERLACED,
		0,
	}, {
		ACTIVE_LOW_NAME, 50, 480, 800, 41042,
		40, 60,
		3, 3,
		8, 4,
		FB_SYNC_OE_LOW_ACT,
		FB_VMODE_NONINTERLACED,
		0,
	},
};

static struct mipi_lcd_config lcd_config = {
	.virtual_ch		= 0x0,
	.data_lane_num  = ST7701S_TWO_DATA_LANE,
	.max_phy_clk    = ST7701S_MAX_DPHY_CLK,
	.dpi_fmt		= MIPI_RGB888,
};
void mipid_st7701s_get_lcd_videomode(struct fb_videomode **mode, int *size,
		struct mipi_lcd_config **data)
{
	*mode = &truly_lcd_modedb[0];
	*size = ARRAY_SIZE(truly_lcd_modedb);
	*data = &lcd_config;
}

int mipid_st7701s_lcd_setup(struct mipi_dsi_info *mipi_dsi)
{
	u8 buf[DSI_CMD_BUF_MAXSIZE];

	dev_dbg(&mipi_dsi->pdev->dev, "MIPI DSI LCD ST7710s setup.\n");

	TC358763_DCS_write_1A_5P(0xFF, 0x77, 0x01, 0x00, 0x00, 0x10);
	TC358763_DCS_write_1A_2P(0xC0, 0x64, 0x00);
	TC358763_DCS_write_1A_2P(0xC1, 0x09, 0x02);
	TC358763_DCS_write_1A_2P(0xC2, 0x37, 0x08);
	TC358763_DCS_write_1A_16P(0xB0, 0x00, 0x11, 0x19, 0x0C, 0x10, 0x06, 0x07, 0x0A, 0x09, 0x22, 0x04, 0x10, 0x0E, 0x28, 0x30, 0x1C);
	TC358763_DCS_write_1A_16P(0xB1, 0x00, 0x12, 0x19, 0x0D, 0x10, 0x04, 0x06, 0x07, 0x08, 0x23, 0x04, 0x12, 0x11, 0x28, 0x30, 0x1C);
	TC358763_DCS_write_1A_5P(0xFF, 0x77, 0x01, 0x00, 0x00, 0x11);

	TC358763_DCS_write_1A_1P(0xB0, 0x4D);
	TC358763_DCS_write_1A_1P(0xB1, 0x4A);
	TC358763_DCS_write_1A_1P(0xB2, 0x00);
	TC358763_DCS_write_1A_1P(0xB3, 0x80);
	TC358763_DCS_write_1A_1P(0xB5, 0x40);
	TC358763_DCS_write_1A_1P(0xB7, 0x8A);
	TC358763_DCS_write_1A_1P(0xB8, 0x21);
	TC358763_DCS_write_1A_1P(0xC1, 0x78);
	TC358763_DCS_write_1A_1P(0xC2, 0x78);
	TC358763_DCS_write_1A_1P(0xD0, 0x88);
	//mdelay(20);

	TC358763_DCS_write_1A_3P(0xE0, 0x00, 0x00, 0x02);
	TC358763_DCS_write_1A_11P(0xE1, 0x01, 0xA0, 0x03, 0xA0, 0x02, 0xA0, 0x04, 0xA0, 0x00, 0x44, 0x44);
	TC358763_DCS_write_1A_12P(0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

	TC358763_DCS_write_1A_4P(0xE3, 0x00, 0x00, 0x33, 0x33);
	TC358763_DCS_write_1A_2P(0xE4, 0x44, 0x44);
	TC358763_DCS_write_1A_16P(0xE5, 0x01, 0x26, 0xA0, 0xA0, 0x03, 0x28, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0);
	TC358763_DCS_write_1A_4P(0xE6, 0x00, 0x00, 0x33, 0x33);
	TC358763_DCS_write_1A_2P(0xE7, 0x44, 0x44);
	TC358763_DCS_write_1A_16P(0xE8, 0x02, 0x26, 0xA0, 0xA0, 0x04, 0x28, 0xA0, 0xA0, 0x06, 0x2A, 0xA0, 0xA0, 0x08, 0x2C, 0xA0, 0xA0);
	TC358763_DCS_write_1A_7P(0xEB, 0x00, 0x00, 0xE4, 0xE4, 0x44, 0x00, 0x40);
	TC358763_DCS_write_1A_16P(0xED, 0xFF, 0xF7, 0x65, 0x4F, 0x0B, 0xA1, 0xCF, 0xFF, 0xFF, 0xFC, 0x1A, 0xB0, 0xF4, 0x56, 0x7F, 0xFF);
	TC358763_DCS_write_1A_5P(0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);


	TC358763_DCS_write_1A_5P(0xFF, 0x77, 0x01, 0x00, 0x00, 0x12);
	TC358763_DCS_write_1A_1P(0xd1, 0x81);
	TC358763_DCS_write_1A_1P(0x0C, 0xFF);
	TC358763_DCS_write_1A_5P(0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);

	TC358763_DCS_write_1A_5P(0xFF, 0x77, 0x01, 0x00, 0x00, 0x12);
	TC358763_DCS_write_1A_1P(0xd1, 0x00);

	TC358763_DCS_write_1A_0P(0x11);
	mdelay(40);
	TC358763_DCS_write_1A_0P(0x29);
	//mdelay(20);

	return 0;
}

static int mipid_bl_update_status(struct backlight_device *bl)
{
	return 0;
}

static int mipid_bl_get_brightness(struct backlight_device *bl)
{
	return 0;
}

static int mipi_bl_check_fb(struct backlight_device *bl, struct fb_info *fbi)
{
	return 0;
}

static const struct backlight_ops mipid_lcd_bl_ops = {
	.update_status = mipid_bl_update_status,
	.get_brightness = mipid_bl_get_brightness,
	.check_fb = mipi_bl_check_fb,
};
