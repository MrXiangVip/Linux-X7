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

#ifndef __V4L2_PROJECTOR_CTRL_H__
#define __V4L2_PROJECTOR_CTRL_H__

#include "config.h"

#define CFG_CAMERA_I2C_DEV 				"/dev/i2c-0"
#define CFG_PROJECTOR_I2C_SLAVE_ADDR 	0x4F
#define CFG_GAIN_I2C_SLAVE_ADDR 			0x60

typedef struct projector_ctrl_reg_cfg {
	uint8_t reg_addr;
	uint8_t val;
	uint32_t delay_ms;
} projector_ctrl_reg_cfg_t;

typedef struct gain_ctrl_reg_cfg {
	uint16_t reg_addr;
	uint8_t val;
	uint32_t delay_ms;
} gain_ctrl_reg_cfg_t;

int32_t projector_ctrl_init();
int32_t projector_ctrl_on(int32_t fd_i2c);
int32_t projector_ctrl_off(int32_t fd_i2c);

int32_t Gain_ctrl_init(uint8_t val);


#endif
