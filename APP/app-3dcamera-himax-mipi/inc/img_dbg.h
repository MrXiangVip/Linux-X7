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
#ifndef __IMG_DBG_H__
#define __IMG_DBG_H__

int32_t img_dbg_save_img(const uint8_t *buf, uint32_t len,  uint32_t width,
		uint32_t height, const char *prefix,
		int32_t img_index, int32_t verbose);
#endif
