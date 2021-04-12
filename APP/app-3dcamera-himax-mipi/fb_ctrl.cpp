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

#include <stdint.h>
#include <linux/fb.h>
#include <stdio.h>
#include <libsimplelog.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>

void fb_ctrl_blitToFB(uint8_t *fb_base, int32_t fb_width, int32_t fb_height,
								uint8_t *preview_buf, int32_t preview_bpp, int32_t cam_width, int32_t cam_height)
{
	int32_t offsetx, y;
	uint8_t *fbpixel, *campixel;
	int32_t tmp_width, tmp_height;

	tmp_width = (fb_width > cam_width) ? cam_width : fb_width;
	tmp_height = (fb_height > cam_height) ? cam_height : fb_height;

	fbpixel = (uint8_t *)fb_base; 
	campixel = (uint8_t *)preview_buf;

	for (y = 0; y < tmp_height; y++) 
	{
		if(fb_width > cam_width)
		{
			offsetx = (fb_width - cam_width)* preview_bpp / 2;
			memcpy((void *)fbpixel+offsetx, (void *)campixel, tmp_width * preview_bpp);			
		}
		else
		{
			offsetx = (cam_width - fb_width)* preview_bpp / 2;
			memcpy((void *)fbpixel, (void *)campixel+offsetx, tmp_width * preview_bpp);
		}	
		campixel += cam_width * preview_bpp;
		fbpixel += fb_width * preview_bpp;
	}
}

int32_t fb_ctrl_fbSinkInit(int32_t *pfd, uint8_t **fbbase,
				struct fb_var_screeninfo *pvar)
{
	int32_t fd_fb, fbsize, ret;
	const char *fb_dev = "/dev/fb0";
	struct fb_var_screeninfo var;

	if ((fd_fb = open(fb_dev, O_RDWR)) < 0) {
		log_error("Unable to open frame buffer\n");
		return -1;
	}

	if (ioctl(fd_fb, FBIOGET_VSCREENINFO, &var) < 0) {
		log_error("FBIOPUT_VSCREENINFO failed");
		goto err;
	}

	// ping-pong buffer
	var.xres_virtual = var.xres;
	var.yres_virtual = 2 * var.yres;

	ret = ioctl(fd_fb, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0) {
		log_error("FBIOPUT_VSCREENINFO failed:\n");
		goto err;
	}

	// Map the device to memory
	fbsize = var.xres * var.yres_virtual * var.bits_per_pixel / 8;

	*fbbase = (uint8_t *)mmap(0, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb, 0);
	if (MAP_FAILED == *fbbase) {
		log_error("Failed to map framebuffer device to memory\n");
		goto err;
	}
	log_info("framebuffer info: %dx%d, bpp:%d, address:0x%08x\n", var.xres, var.yres, var.bits_per_pixel, *fbbase);

	*pfd = fd_fb;
	*pvar = var;

	return 0;
err:
	close(fd_fb);
	return -1;
}

void fb_ctrl_fbSinkUninit(int32_t fd, uint8_t *fbbase, int32_t fbsize)
{
	if (fbbase)
		munmap(fbbase, fbsize);
	close(fd);
}

