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

#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>

static struct timeval tv_rs_start;
static struct timeval tv_rs_end;
static uint32_t total_us;

void per_calc_start(void)
{
	gettimeofday(&tv_rs_start, NULL);
}

void per_calc_end(void)
{
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 +
	  			(tv_rs_end.tv_usec - tv_rs_start.tv_usec);
}

uint32_t per_calc_get_result(void)
{
	return total_us;
}
