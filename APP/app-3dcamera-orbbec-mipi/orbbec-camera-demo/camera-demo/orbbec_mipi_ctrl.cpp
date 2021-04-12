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

#include "orbbec_mipi_ctrl.h"
#include "OrbbecCAPI.h"

#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#define ORBBEC_CONFIG_PATH					"/etc/config1.ini"

#define MX6000_I2C_ID_ADDR					0x0000
#define MX6000_I2C_STREAM_MODE_ADDR				0x0018

#define MX6000_I2C_DEPTH_BASE					0x0020
#define MX6000_I2C_IR_BASE					0x0048

#define MX6000_I2C_RES_OFFSET					0x0013
#define MX6000_I2C_FPS_OFFSET					0x0015
#define MX6000_I2C_FMT_OFFSET					0x0014
#define MX6000_I2C_LOAD_OFFSET					0x000d

static int32_t __i2c_read_2byte(int32_t fd, uint16_t sub_addr, uint16_t *data_byte)
{
	uint8_t addr[2] = { 0 };
	uint16_t temp;

	addr[0] = sub_addr >> 8;
	addr[1] = sub_addr & 0xff;
	if (2 != write(fd, addr, 2)) {
		printf("Write sub address %d failed!\n", sub_addr);
		return -1;
	}

	if (2 != read(fd, data_byte, 2)) {
		printf("Read byte failed!\n");
		return -1;
	}

	//exchange high 8 bits and low 8 bits
	temp = (*data_byte) & 0xff00;
	temp = temp >> 8;
	*data_byte = (*data_byte) << 8;
	*data_byte = *data_byte | temp;
	//printf("reg: 0x%x, val: 0x%x!\n", sub_addr, *data_byte);
	return 0;
}

static int32_t __i2c_read_1byte(int32_t fd, uint16_t sub_addr, uint8_t *data_byte)
{
	uint8_t addr[2] = { 0 };

	addr[0] = sub_addr >> 8;
	addr[1] = sub_addr & 0xff;
	if (2 != write(fd, addr, 2)) {
		printf("Write sub address %d failed!\n", sub_addr);
		return -1;		
	}

	if (1 != read(fd, data_byte, 1)) {
		printf("Read byte failed!\n");
		return -1;
	}
	//printf("reg: 0x%x, val: 0x%x!\n", sub_addr, *data_byte);
	return 0;
}

static int32_t __i2c_write_1byte(int32_t fd, uint16_t sub_addr, uint8_t val)
{
	uint8_t data[3] = { 0 };

	data[0] = sub_addr >> 8;
	data[1] = sub_addr & 0xff;
	data[2] = val;
	if (write(fd, data, 3) != 3) {
		printf("I2C write failed!\n");
		return -1;
	} else
		return 0;
}

static int32_t mx6000_depth_set_resolution(int32_t fd, int32_t val)
{
	if (__i2c_write_1byte(fd, MX6000_I2C_DEPTH_BASE + MX6000_I2C_RES_OFFSET, val))
		return -1;
	else
		return MX6000_DEP_RES_SUCCESS;
}

static int32_t mx6000_depth_set_framerate(int32_t fd, uint32_t val)
{
	if (__i2c_write_1byte(fd, MX6000_I2C_DEPTH_BASE + MX6000_I2C_FPS_OFFSET, val))
		return -1;
	else
		return MX6000_DEP_FRATE_SUCCESS;
}

static int32_t mx6000_depth_set_pixel_format(int32_t fd, int32_t fmt)
{
	if (__i2c_write_1byte(fd, MX6000_I2C_DEPTH_BASE + MX6000_I2C_FMT_OFFSET, fmt))
		return -1;
	else
		return MX6000_DEP_FORMAT_SUCCESS;
}

static int32_t mx6000_depth_load(int32_t fd)
{
	if (__i2c_write_1byte(fd, MX6000_I2C_DEPTH_BASE + MX6000_I2C_LOAD_OFFSET,
				DEPTH_LOAD_PARAMETERS))
		return -1;
	else
		return MX6000_DEP_LOAD_SUCCESS; 
}

static int32_t mx6000_ir_set_resolution(int32_t fd, int32_t val)
{
	if (__i2c_write_1byte(fd, MX6000_I2C_IR_BASE + MX6000_I2C_RES_OFFSET, val))
		return -1;
	else
		return MX6000_IR_RES_SUCCESS;
}

static int32_t mx6000_ir_set_framerate(int32_t fd, uint32_t val)
{
	if (__i2c_write_1byte(fd, MX6000_I2C_IR_BASE + MX6000_I2C_FPS_OFFSET, val))
		return -1;
	else
		return MX6000_IR_FRATE_SUCCESS;
}

static int32_t mx6000_ir_set_pixel_format(int32_t fd, int32_t fmt)
{
	if (__i2c_write_1byte(fd, MX6000_I2C_IR_BASE + MX6000_I2C_FMT_OFFSET, fmt))
		return -1;
	else
		return MX6000_IR_FORMAT_SUCCESS;
}

static int32_t mx6000_ir_load(int32_t fd)
{
	if (__i2c_write_1byte(fd, MX6000_I2C_IR_BASE + MX6000_I2C_LOAD_OFFSET, IR_LOAD_PARAMETERS))
		return -1;
	else
		return MX6000_IR_LOAD_SUCCESS; 
}

int32_t orbbec_mipi_ctrl_depth_set_video_mode(int32_t i2c_fd, uint32_t out_width, uint32_t out_height)
{
	int32_t ret = 0, val = 0;

	if (out_width == 480 && out_height == 640) {
		val = DEPTH_STREAM_RES_480X640;
	} else if (out_width == 540 && out_height == 1280) {
		val = DEPTH_STREAM_RES_540X640;
	}else if (out_width == 960 && out_height == 1280) {
		val = DEPTH_STREAM_RES_960X1280;
	} else {
		val = DEPTH_STREAM_RES_960X1280;
	}

	// Set resolution:
	// 0x15 - 480*640
	// 0x17 - 540*640
	// 0x19 - 960 *1280
	// Write 0x15 or 0x19 to 16bit address (0x0020 + 0x13)
	ret = mx6000_depth_set_resolution(i2c_fd, val);
	if (ret != MX6000_DEP_RES_SUCCESS) {
		printf("mx6000_depth_set_resolution val = 0x%x failed\n",val);
		goto err_rtn;
	}

	// Set fps, write 30 to (0x0020 + 0x15)
	ret = mx6000_depth_set_framerate(i2c_fd, 30);
    if (ret != MX6000_DEP_FRATE_SUCCESS) {
        printf("mx6000_depth_set_framerate val = 0x%x failed\n",30);
		goto err_rtn;
    }

	// Set RAW data format:
	// 0x04 - raw12
	// 0x14 - raw10
	// Write to (0x0020 + 0x14)
	ret = mx6000_depth_set_pixel_format(i2c_fd, DEPTH_PIXEL_FORMAT_RAW12);
	if (ret != MX6000_DEP_FORMAT_SUCCESS) {
		printf("mx6000_depth_set_pixel_format val = 0x%x failed\n", val);
		goto err_rtn;
	}

	// Enable registers by writing 0x01 to 0x002D
	ret = mx6000_depth_load(i2c_fd);
	if (ret != MX6000_DEP_LOAD_SUCCESS) {
		printf("mx6000_depth_load failed\n");
		goto err_rtn;
	}

	return 0;
err_rtn:
	return -1;
}

int32_t orbbec_mipi_ctrl_ir_set_video_mode(int32_t i2c_fd, uint32_t out_width, uint32_t out_height)
{
	int32_t ret = 0, val = 0;

	if (out_width == 480 && out_height == 640) {
		val = IR_STREAM_RES_480X640;
	} else if(out_width == 540 && out_height == 640) {
		val = IR_STREAM_RES_540X640;
	} else if(out_width == 960 && out_height == 1280) {
		val = IR_STREAM_RES_960X1280;
	} else {
		val = IR_STREAM_RES_960X1280;
	}

	// Set resolution:
	// 0x15: 480*640
	// 0x17: 540*640
	// 0x19: 960 *1280
	// Write to 16bits address (0x0048 + 0x13)
	ret = mx6000_ir_set_resolution(i2c_fd, val);
	if (ret != MX6000_IR_RES_SUCCESS) {
		printf("mx6000_ir_set_resolution val = 0x%x failed\n", val);
		goto err_rtn;
	}

	// Set fps, write 30 to (0x0048 + 0x15)
	ret = mx6000_ir_set_framerate(i2c_fd, val);
	if (ret != MX6000_IR_RES_SUCCESS) {
		printf("mx6000_ir_set_resolution val = 0x%x failed\n", val);
		goto err_rtn;
	}

	// Set raw data format
	// 0x04 - raw12
	// 0x14 - raw10
	ret = mx6000_ir_set_pixel_format(i2c_fd, IR_PIXEL_FORMAT_RAW10);
	if (ret != MX6000_IR_FORMAT_SUCCESS) {
		printf("mx6000_ir_set_pixel_format val = 0x%x failed\n", val);
		goto err_rtn;
	}

	// Load register settings, write 0x01 to 0x0055
	ret = mx6000_ir_load(i2c_fd);
	if (ret != MX6000_IR_LOAD_SUCCESS) {
		printf("mx6000_ir_load failed!\n");
		goto err_rtn;
	}

	return 0;
err_rtn:
	return -1;
}

int32_t orbbec_mipi_ctrl_switch_stream(int32_t i2c_fd, int32_t strm_mode)
{
	int32_t w_ret_val = 0;
	if (strm_mode == STREAM_DEPTH) {
		// switch to depth
		w_ret_val = __i2c_write_1byte(i2c_fd, MX6000_I2C_STREAM_MODE_ADDR, STREAM_MODE_DEPTH);
		return w_ret_val;
	} else {
		// switch to ir
		w_ret_val = __i2c_write_1byte(i2c_fd, MX6000_I2C_STREAM_MODE_ADDR, STREAM_MODE_IR);
		return w_ret_val;
	}
}

int32_t orbbec_mipi_ctrl_init(const char *i2c_dev, uint8_t device_addr)
{
	int32_t fd = 0;
	uint16_t val = 0;
	int32_t ret = 0;

	fd = open(i2c_dev, O_RDWR);
	if (fd < 0) {
		printf("Open i2c device %s failed!\n", i2c_dev);
		return -1;
	}

	ret = ioctl(fd, I2C_SLAVE, device_addr);
	if (ret < 0) {
		printf("i2c set addr failed: 0x%x: %d, Err: %d!\n", device_addr, ret, errno);
		return -1;
	}

	__i2c_read_2byte(fd, MX6000_I2C_ID_ADDR, &val);
	if (0x6010 != val) {
		printf("mx6000 id is %d ?\n", val);
		return -1;
	}
	printf("mx6000 ros<%d*%d>.\n", MX6000_CAM_WIDTH, MX6000_CAM_HEIGHT);

	//ioctl(fd, I2C_TIMEOUT, 1);
	//ioctl(fd, I2C_RETRIES, 1);

	if (orbbec_Init(ORBBEC_CONFIG_PATH)) {
		printf("Init orbbec failed!\n");
		return -1;
	}

	if (orbbec_RefreshWorldConversionCache(MX6000_CAM_WIDTH, MX6000_CAM_HEIGHT, 1)) {
		printf("orbbec RefreshWorldConversionCache failed!\n");
		return -1;
	}

	return fd;
}

