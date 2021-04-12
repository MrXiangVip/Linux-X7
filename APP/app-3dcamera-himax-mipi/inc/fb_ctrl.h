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

#ifndef __FB_CTRL_H__
#define __FB_CTRL_H__

#include <stdint.h>

void fb_ctrl_blitToFB(uint8_t *fb_base, int32_t fb_width, int32_t fb_height,
					uint8_t *preview_buf, int32_t preview_bpp, int32_t cam_width, int32_t cam_height);
int32_t fb_ctrl_fbSinkInit(int32_t *pfd, uint8_t **fbbase,
				struct fb_var_screeninfo *pvar);

void fb_ctrl_fbSinkUninit(int32_t fd, uint8_t *fbbase, int32_t fbsize);

#endif
