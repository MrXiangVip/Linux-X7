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

#include "libsimplelog.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int32_t img_dbg_save_img(const uint8_t *buf, uint32_t len, uint32_t width,
		uint32_t height, const char *prefix,
		int32_t img_index, int32_t verbose)
{
	char str_file_name[100] = { 0 };
	FILE *fp = NULL;

	sprintf(str_file_name, "%s_%dx%d_%d.raw", prefix, width, height, img_index);
	if (verbose)
		log_info("Image filename: %s\n", str_file_name);
	fp = fopen(str_file_name, "w+");
	fwrite(buf, len, 1, fp);
	fclose(fp);
	return 0;
}
