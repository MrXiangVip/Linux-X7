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

#ifndef __BMP_BANNER_H__
#define __BMP_BANNER_H__

#include <stdint.h>
#include "config.h"

enum {
	BANNER_REGISTERING = 0,
	BANNER_REGISTER_OK,
	BANNER_REGISTERED,
	BANNER_REGISTER_FAIL,
	BANNER_RECOGNIZING,
	BANNER_RECOGNIZED,
	BANNER_RECOGNIZE_FAIL,
	BANNER_LIVENESS_FAIL
};

int32_t bmp_banner_init(uint32_t ping_address, uint32_t pong_address, int32_t type);
void bmp_banner_update(int32_t fb_index);
void bmp_banner_set(int32_t type);

#endif
