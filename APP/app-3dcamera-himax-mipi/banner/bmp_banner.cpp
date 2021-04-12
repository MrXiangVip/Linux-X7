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

#include "bmp_banner.h"
#include "config.h"
#include "string.h"
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

#if CFG_ENABLE_PREVIEW_BANNER
#include "bmp_livenessfail.cpp"
#include "bmp_recognized.cpp"
#include "bmp_recognizefail.cpp"
#include "bmp_recognizing.cpp"
#include "bmp_registered.cpp"
#include "bmp_registering.cpp"
#include "bmp_registerok.cpp"
#include "bmp_registerfail.cpp"

static uint32_t banner_address_ping = 0;
static uint32_t banner_address_pong = 0;
static pthread_mutex_t banner_set_mutex;
static pthread_mutex_t banner_update_mutex;
static int32_t gbanner_type = 0;

int32_t bmp_banner_init(uint32_t ping_address, uint32_t pong_address, int32_t type)
{
	/* Update ping-pong buffer */
	if (ping_address && pong_address) {
		banner_address_ping = ping_address;
		banner_address_pong = pong_address;

		pthread_mutex_init(&banner_set_mutex, NULL);
		pthread_mutex_init(&banner_update_mutex, NULL);

		gbanner_type = type;
	} else {
		return -1;
	}
	return 0;
}

void bmp_banner_update(int32_t fb_index)
{
	const uint8_t *bmp_data = NULL;
	uint32_t banner_addr = 0;

	pthread_mutex_lock(&banner_set_mutex);
	switch (gbanner_type) {
	case BANNER_REGISTERING:
		bmp_data = registering_bmp;
		break;
	case BANNER_REGISTERED:
		bmp_data = registered_bmp;
		break;
	case BANNER_REGISTER_OK:
		bmp_data = registerok_bmp;
		break;
	case BANNER_REGISTER_FAIL:
		bmp_data = registerfail_bmp;
		break;
	case BANNER_RECOGNIZED:
		bmp_data = recognized_bmp;
		break;
	case BANNER_RECOGNIZING:
		bmp_data = recognizing_bmp;
		break;
	case BANNER_RECOGNIZE_FAIL:
		bmp_data = recognize_fail_bmp;
		break;
	case BANNER_LIVENESS_FAIL:
		bmp_data = liveness_fail_bmp;
		break;
	default:
		break;
	}
	pthread_mutex_unlock(&banner_set_mutex);

	banner_addr = (fb_index) ? banner_address_pong : banner_address_ping;

	pthread_mutex_lock(&banner_update_mutex);
	memcpy((void *)banner_address_ping, (void *)bmp_data, CFG_BANNER_WIDTH * CFG_BANNER_HEIGHT * CFG_BANNER_BPP / 8);
	memcpy((void *)banner_address_pong, (void *)bmp_data, CFG_BANNER_WIDTH * CFG_BANNER_HEIGHT * CFG_BANNER_BPP / 8);
	pthread_mutex_unlock(&banner_update_mutex);
}

void bmp_banner_set(int32_t type)
{
	pthread_mutex_lock(&banner_set_mutex);
	gbanner_type = type;
	pthread_mutex_unlock(&banner_set_mutex);
}
#else
int32_t bmp_banner_init(uint32_t ping_address, uint32_t pong_address, int32_t type) {
	return 0;
}

void bmp_banner_update(int32_t fb_index) {
}

void bmp_banner_set(int32_t type) {
}
#endif
