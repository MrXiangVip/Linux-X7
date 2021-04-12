/**************************************************************************
 * 	FileName:	 face_loop.cpp
 *	Description:	This file is used to ctrl projector ¡¢ gain¡¢Auto Exposure.
 *	Copyright(C):	2018-2020 Wave Group Inc.
 *	Version:		V 1.0
 *	Author:		tanqw
 *	Created:		2019-7-12
 *	Updated:		
 *					
**************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "projector_ctrl.h"


/* --- two byte register address I2C transfers ------------------------ begin */

static int32_t __i2c_2byte_addr_read_word(int32_t fd, uint16_t sub_addr, uint16_t *val)
{
	uint8_t addr[2] = { 0 };
	uint8_t data[2] = { 0 };

	addr[0] = sub_addr >> 8;
	addr[1] = sub_addr & 0xff;

	if (2 != write(fd, addr, 2)) {
		printf("I2C write sub address 0x%04X failed for read word!\n", sub_addr);
		return -1;
	}

	if (2 != read(fd, data, 2)) {
		printf("I2C read word from 0x%04X failed!\n", sub_addr);
		return -1;
	}

	*val = (data[0] << 8) | data[1];
	printf("reg: 0x%04X, val: 0x%04X!\n", sub_addr, *val);

	return 0;
}

static int32_t __i2c_2byte_addr_write_word(int32_t fd_i2c, uint16_t sub_addr, uint16_t val)
{
	uint8_t data[4] = { 0 };

	data[0] = sub_addr >> 8;
	data[1] = sub_addr & 0xff;
	data[2] = val >> 8;
	data[3] = val & 0xff;

	if (4 != write(fd_i2c, data, 4)) {
		printf("I2C write word 0x%04X to 0x%04X failed!\n", val, sub_addr);
		return -1;
	}

	return 0;
}

static int32_t __i2c_2byte_addr_read_byte(int32_t fd, uint16_t sub_addr, uint8_t *val)
{
	uint8_t addr[2] = { 0 };

	addr[0] = sub_addr >> 8;
	addr[1] = sub_addr & 0xff;

	if (2 != write(fd, addr, 2)) {
		printf("I2C write sub address 0x%04X failed for read byte!\n", sub_addr);
		return -1;
	}

	if (1 != read(fd, val, 1)) {
		printf("I2C read byte from 0x%04X failed!\n", sub_addr);
		return -1;
	}

	printf("reg: 0x%04X, val: 0x%02X!\n", sub_addr, *val);

	return 0;
}

static int32_t __i2c_2byte_addr_write_byte(int32_t fd, uint16_t sub_addr, uint8_t val)
{
	uint8_t data[3] = { 0 };

	data[0] = sub_addr >> 8;
	data[1] = sub_addr & 0xff;
	data[2] = val;

	if (write(fd, data, 3) != 3) {
		printf("I2C write byte 0x%02X to 0x%04X failed!\n", val, sub_addr);
		return -1;
	}

	return 0;
}

/* --- two byte register address I2C transfer --------------------------- end */

/* --- one byte register address I2C transfer ------------------------- begin */

static int32_t __i2c_1byte_addr_read_word(int32_t fd, uint8_t sub_addr, uint16_t *val)
{
	uint8_t data[2] = { 0 };

	if (1 != write(fd, &sub_addr, 1)) {
		printf("I2C write sub address 0x%02X failed for read word!\n", sub_addr);
		return -1;
	}

	if (2 != read(fd, data, 2)) {
		printf("I2C read word from 0x%02X failed!\n", sub_addr);
		return -1;
	}

	*val = (data[0] << 8) | data[1];
	printf("reg: 0x%02X, val: 0x%04X!\n", sub_addr, *val);

	return 0;
}

static int32_t __i2c_1byte_addr_write_word(int32_t fd_i2c, uint8_t sub_addr, uint16_t val)
{
	uint8_t data[3] = { 0 };

	data[0] = sub_addr;
	data[1] = val >> 8;
	data[2] = val & 0xff;

	if (3 != write(fd_i2c, data, 3)) {
		printf("I2C write word 0x%04X to 0x%02X failed!\n", val, sub_addr);
		return -1;
	}

	return 0;
}

static int32_t __i2c_1byte_addr_read_byte(int32_t fd, uint8_t sub_addr, uint8_t *val)
{
	if (1 != write(fd, &sub_addr, 1)) {
		printf("I2C write sub address 0x%02X failed for read byte!\n", sub_addr);
		return -1;
	}

	if (1 != read(fd, val, 1)) {
		printf("I2C read byte from 0x%02X failed!\n", sub_addr);
		return -1;
	}

	printf("reg: 0x%02X, val: 0x%02X!\n", sub_addr, *val);

	return 0;
}

static int32_t __i2c_1byte_addr_write_byte(int32_t fd, uint8_t sub_addr, uint8_t val)
{
	uint8_t data[2] = { 0 };

	data[0] = sub_addr;
	data[1] = val;

	if (write(fd, data, 2) != 2) {
		printf("I2C write byte 0x%02X to 0x%02X failed!\n", val, sub_addr);
		return -1;
	}

	return 0;
}

/* --- one byte register address I2C transfer --------------------------- end */


static projector_ctrl_reg_cfg_t projector_ctrl_himax_on[] = {
	{0x21, 0x01, 0},
};
static projector_ctrl_reg_cfg_t projector_ctrl_himax_off[] = {
	{0x21, 0x00, 0},
};

static gain_ctrl_reg_cfg_t gain_ctrl_himax[] = {
	{0x3509, 0x10, 0},
};

static int32_t projector_ctrl_load_settings(int32_t fd_i2c, projector_ctrl_reg_cfg_t *reg_cfg, int32_t size)
{
	int32_t i = 0,index;
	uint16_t reg_addr = 0;
	uint8_t reg_val = 0;
	uint8_t reg_rd = 0;
	uint32_t delay_ms = 0;
	int32_t retval = 0;

LOOP:
	index = 0;
	for (i = 0; i < size; ++i, ++reg_cfg) 
	{
		reg_addr = reg_cfg->reg_addr;
		reg_val = reg_cfg->val;
		delay_ms = reg_cfg->delay_ms;
		reg_rd = 0;

		printf("%s: WR I2C: 0x%x:0x%x (fd_i2c=0x%X)\n", __func__, reg_addr, reg_val, fd_i2c);
		retval = __i2c_1byte_addr_write_byte(fd_i2c, reg_addr, reg_val);
		if (retval < 0) 
		{
			printf("i2c write word failed!\n");
			if(++index > 5)
			{
				return -1;
			}
			usleep(10000);
			goto LOOP;
		}

		if (delay_ms)
			usleep(delay_ms * 1000);
	}
	
	return 0;
}

static int32_t gain_ctrl_load_settings(int32_t fd_i2c, gain_ctrl_reg_cfg_t *reg_cfg, int8_t id)
{
	int32_t i = 0,index;
	uint16_t reg_addr = 0;
	uint8_t reg_val = 0;
	uint8_t reg_rd = 0;
	uint32_t delay_ms = 0;
	int32_t retval = 0;

LOOP:
	index = 0;

	reg_addr = reg_cfg->reg_addr;
	reg_val = reg_cfg->val*id;
	delay_ms = reg_cfg->delay_ms;
	reg_rd = 0;

	printf("%s: WR I2C: 0x%x:0x%x (fd_i2c=0x%X)\n", __func__, reg_addr, reg_val, fd_i2c);
	retval = __i2c_2byte_addr_write_byte(fd_i2c, reg_addr, reg_val);
	if (retval < 0) 
	{
		printf("i2c write word failed!\n");
		if(++index > 5)
		{
			return -1;
		}
		usleep(10000);
		goto LOOP;
	}

	if (delay_ms)
		usleep(delay_ms * 1000);
	
	return 0;
}

int32_t projector_ctrl_init()
{
	int32_t fd_i2c = 0;
	int32_t ret = 0;

	//printf("==== himax_projector_init====\n");

	fd_i2c = open(CFG_CAMERA_I2C_DEV, O_RDWR);
	if (fd_i2c < 0) {
		printf("Open i2c device %s failed!\n", CFG_CAMERA_I2C_DEV);
		return -1;
	}

	ret = ioctl(fd_i2c, I2C_SLAVE, CFG_PROJECTOR_I2C_SLAVE_ADDR);
	if (ret < 0) {
		printf("i2c set addr failed: 0x%x: %d, Err: %d!\n", CFG_PROJECTOR_I2C_SLAVE_ADDR, ret, errno);
		goto err;
	}
	//printf("i2c_dev=\"%s\", fd_i2c=0x%X, slave_addr=0x%X\n", CFG_CAMERA_I2C_DEV, fd_i2c, CFG_PROJECTOR_I2C_SLAVE_ADDR);

	return fd_i2c;

err:
	if (fd_i2c)
		close(fd_i2c);

	return -1;
}

int32_t projector_ctrl_on(int32_t fd_i2c)
{
	printf("======projector_ctrl_himax_on=========\n");
	if (projector_ctrl_load_settings(fd_i2c, projector_ctrl_himax_on, 1))
		return -1;

	return 0;
}

int32_t projector_ctrl_off(int32_t fd_i2c)
{
	printf("======projector_ctrl_himax_off=========\n");
	if (projector_ctrl_load_settings(fd_i2c, projector_ctrl_himax_off, 1))
		return -1;

	return 0;
}


int32_t Gain_ctrl_init(uint8_t val)
{
	int32_t fd_i2c = 0;
	int32_t ret = 0;

	if(val<1|| val>8)
	{		
		printf("Warr:Gain_ctrl_init() val must between 1 and 8!\n");
		return -1;
	}

	fd_i2c = open(CFG_CAMERA_I2C_DEV, O_RDWR);
	if (fd_i2c < 0) {
		printf("Open i2c device %s failed!\n", CFG_CAMERA_I2C_DEV);
		return -1;
	}

	ret = ioctl(fd_i2c, I2C_SLAVE, CFG_GAIN_I2C_SLAVE_ADDR);
	if (ret < 0) {
		printf("i2c set addr failed: 0x%x: %d, Err: %d!\n", CFG_GAIN_I2C_SLAVE_ADDR, ret, errno);
		goto err;
	}
	printf("i2c_dev=\"%s\", fd_i2c=0x%X, slave_addr=0x%X\n", CFG_CAMERA_I2C_DEV, fd_i2c, CFG_GAIN_I2C_SLAVE_ADDR);

	if (gain_ctrl_load_settings(fd_i2c, gain_ctrl_himax, val))
		goto err;
	
	return 0;

err:
	if (fd_i2c)
		close(fd_i2c);

	return -1;
}

#if 0
int32_t main()
{
	int32_t i;	
	int32_t fd_i2c = 0;
	fd_i2c = projector_ctrl_init();
	if(fd_i2c <= 0)
	{
		return -1;
	}

	for (i = 0; i < 5; i++) 
	{
		printf("======projector_ctrl_himax_on=========\n");
		if (projector_ctrl_load_settings(fd_i2c, projector_ctrl_himax_on, 1))
			return -2;

		sleep(5);

		printf("======projector_ctrl_himax_off=========\n");
		if (projector_ctrl_load_settings(fd_i2c, projector_ctrl_himax_off, 1))
			return -2;

		sleep(5);
	}
	return 0;
}
#endif
