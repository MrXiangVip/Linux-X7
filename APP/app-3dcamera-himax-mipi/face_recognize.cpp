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

#include "libyuv.h"
#include "libsimplelog.h"
#include "cam_ctrl.h"
#include "projector_ctrl.h"
#include "fb_ctrl.h"
#include "face_alg.h"
#include "per_calc.h"
#include "bmp_banner.h"
#include "config.h"
#include "sig_timer.h"
#include "function.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <linux/input.h>
#include <math.h> 

using namespace std;

struct faceBuffer {
	uint8_t ir_buf[CFG_FACE_LIB_IR_IMAGE_SIZE];
	uint8_t ir_filled;	
	std::mutex ir_mutex;
#if CFG_FACE_BIN_LIVENESS
	uint8_t depth_buf[CFG_FACE_LIB_DEPTH_IMAGE_SIZE];
	uint8_t depth_filled;
	std::mutex depth_mutex;
#endif
};

static cam_ctrl_t gcam_ctrl;
static int32_t projector_fd_i2c;

#if CFG_ENABLE_PREVIEW
static uint8_t preview_buffer32[CFG_IR_FRAME_WIDTH * CFG_IR_FRAME_HEIGHT * 4];
	#if (CFG_PREVIEW_LCD_BPP == 16)
static uint8_t preview_buffer16[CFG_IR_FRAME_WIDTH * CFG_IR_FRAME_HEIGHT * 2];
	#endif
#endif

#if CFG_FACE_BIN_LIVENESS
static uint8_t g_depth_frm_buf[CFG_DEPTH_FRAME_SIZE];
#endif //CFG_FACE_BIN_LIVENESS

static struct faceBuffer faceBuf;
static face_alg_param_t facelib_param;

static bool detectRun = true;
static bool previewRun = true;
static int32_t fbIndex = 0;
static std::mutex mtx;
static std::condition_variable CV;
static bool LcdOpenOK_flg = false;    // LCD 打开成功与否的标记


#if CFG_ENABLE_APP_EXIT_FROM_TIMER
static void face_recognize_run_timer_cb(void)
{
	log_info("app run timeout, exit!\n");
	detectRun = false;

	static unsigned char Num = 0;
	if(++Num > 1)
	{
		log_error("App run timeout, exit(-1)!\n");		
		exit(-1);
	}
}
#endif

static void face_recognize_detect_proc(void)
{
	bool IsMouseOpen = false;	
	int faceCount = 0, distance = 0;
	int32_t faceId = 0, len = 0; 
	int recogtime = 0;  
	face_image_info_t ir_img = {
		faceBuf.ir_buf,
		CFG_FACE_LIB_FRAME_WIDTH,
		CFG_FACE_LIB_FRAME_HEIGHT,
		(CFG_FACE_LIB_FRAME_WIDTH * CFG_FACE_LIB_PIX_FMT_BPP) >> 3,
	};
	
#if CFG_FACE_BIN_LIVENESS
	face_image_info_t depth_img = {
		faceBuf.depth_buf,
		CFG_FACE_LIB_FRAME_WIDTH,
		CFG_FACE_LIB_FRAME_HEIGHT,
		(CFG_FACE_LIB_FRAME_WIDTH * CFG_FACE_LIB_DEPTH_PIX_FMT_BPP) >> 3,
	};
#endif

	if (face_alg_init(&facelib_param, FACE_RECG)) 
	{
		cmdSysHardwareAbnormalReport(HW_BUG_ENCRYPTIC);
		log_error("Failed to init the face lib\n");
		return;
	}

	while (detectRun) 
	{
		std::unique_lock<std::mutex> lck(mtx);
		// wait for buffer ready
#ifdef CFG_FACE_BIN_LIVENESS
		CV.wait(lck, []{ return ((faceBuf.ir_filled) && (faceBuf.depth_filled)); });
#else
		CV.wait(lck, []{ return (faceBuf.ir_filled); });
#endif
		
#if CFG_PROJECTOR_CTRL
		//close projector
		//projector_ctrl_off(projector_fd_i2c);
#endif

		// lock buffer to do face algorithm
		faceBuf.ir_mutex.lock();  

#if CFG_PER_TIME_CAL_OVERALL
		per_calc_start();
#endif

#if CFG_FACE_BIN_LIVENESS
		faceBuf.depth_mutex.lock();
		faceId = face_alg_detect(&ir_img, &depth_img, &IsMouseOpen, &distance, &faceCount);
		if(faceId<0)
		{
			faceBuf.depth_filled = 0;		
			faceBuf.depth_mutex.unlock();
		}
#else
		faceId = face_alg_detect(&ir_img);
#endif
#if 0
		if(faceId >0)
		{
			myWriteData2File("pic_recg_Depth.raw", faceBuf.depth_buf, 2*CFG_FACE_LIB_FRAME_WIDTH*CFG_FACE_LIB_FRAME_HEIGHT);
			myWriteData2File("pic_recg_Ir.raw", faceBuf.ir_buf, CFG_FACE_LIB_FRAME_WIDTH*CFG_FACE_LIB_FRAME_HEIGHT);
		}
#endif
		if(faceId<0)
		{
			faceBuf.ir_filled = 0;
			faceBuf.ir_mutex.unlock();
		}

#if CFG_PER_TIME_CAL_OVERALL
		per_calc_end();
		log_info("Face recognize: %d usec\n", per_calc_get_result());
#endif

		if(faceId <= 0)
		{
#if CFG_PROJECTOR_CTRL
			//close projector
			//projector_ctrl_off(projector_fd_i2c);
#endif
		}

		if (faceId > 0) 
		{
#if CFG_ENABLE_PREVIEW_BANNER
			bmp_banner_set(BANNER_RECOGNIZED);
			bmp_banner_update(fbIndex);
#endif			
			log_warn("face recognized: %d\n", faceId);

			cmdFaceRecogizeRsp((uint16_t) faceId, IsMouseOpen, faceCount);
			break;
		}
		if (RET_NO_FACE == faceId)
		{
			usleep(40000);
			log_warn("No face detected!\n");
		}
		if(RET_NO_LIVENESS == faceId)
		{
			log_info("liveness check failed!\n");
			recogtime+=2;
		}
		if(RET_NO_RECO  == faceId)
		{
			log_error("face OK but no record in DB!\n");
			recogtime+=2;
		}

		recogtime ++; 
		if(recogtime > 10)
		{
#if CFG_ENABLE_PREVIEW_BANNER
			bmp_banner_set(BANNER_RECOGNIZE_FAIL);
			bmp_banner_update(fbIndex);
#endif
			cmdFaceRecogizeRsp((uint16_t) 0x0000, 0, 0);			
			log_warn("------face recogized fail!\n");
			recogtime = 0;

			break;
		}
	}

	face_alg_deinit();
	detectRun = false;	
}
	
static void face_recognize_preview_proc(void)
{
	int32_t cur_stream_type = STREAM_IR;

#if CFG_ENABLE_PREVIEW
	int32_t fbfd = 0;
	uint8_t *fbBase = NULL;
	struct fb_var_screeninfo var;
	int32_t fbBufSize = 0;
	uint32_t lcd_bpp = 0;

	if (fb_ctrl_fbSinkInit(&fbfd, &fbBase, &var)) 
	{
		cmdSysHardwareAbnormalReport(HW_BUG_LCD);
		log_error("fbSink init failed\n");
		//return;
		LcdOpenOK_flg = false;
	}
	else
	{
		LcdOpenOK_flg = true;
	}

	if(LcdOpenOK_flg)
	{
		lcd_bpp = var.bits_per_pixel / 8;
		fbBufSize = var.xres * var.yres * lcd_bpp;
	#if CFG_ENABLE_PREVIEW_BANNER
		bmp_banner_init(
				(uint32_t)(fbBase + fbBufSize - var.xres * 100 * var.bits_per_pixel / 8),
				(uint32_t)(fbBase + 2 * fbBufSize - var.xres * 100 * var.bits_per_pixel / 8),
				BANNER_RECOGNIZING);
		bmp_banner_update(fbIndex);
	#endif
	}
#endif

	// stream on
	if (cam_ctrl_start_stream(&gcam_ctrl)) 
	{
		cmdSysHardwareAbnormalReport(HW_BUG_CAMERA);
		log_error("cam ctrl start failed\n");
		cam_ctrl_deinit(&gcam_ctrl);
#if CFG_ENABLE_PREVIEW
		if(LcdOpenOK_flg)
			fb_ctrl_fbSinkUninit(fbfd, fbBase, fbBufSize * 2);
#endif
		return;
	}

	while (previewRun) 
	{
		uint8_t *data_ptr = NULL;

		switch (cur_stream_type) 
		{
			case STREAM_IR:
			{
				if (!faceBuf.ir_filled && faceBuf.ir_mutex.try_lock()) 
				{
					log_debug("===it is ir stream.\n");
					cam_ctrl_deq_frame_data_ptr(&gcam_ctrl, &data_ptr, FRAME_TYPE_IR);
					
					uint8_t *tmp_buf_ptr = faceBuf.ir_buf;
#if CFG_FACE_BIN_LIVENESS
					cur_stream_type = STREAM_DEPTH;
#endif 

					cam_ctrl_cvt_to_gray(&gcam_ctrl, data_ptr, tmp_buf_ptr, CFG_FACE_LIB_FRAME_WIDTH, CFG_FACE_LIB_FRAME_HEIGHT);

					faceBuf.ir_filled = 1;
					faceBuf.ir_mutex.unlock();
					CV.notify_one();
					
#if CFG_ENABLE_PREVIEW
					if(LcdOpenOK_flg)
					{
						uint32_t pre_width = gcam_ctrl.preview_width;
						uint32_t pre_height = gcam_ctrl.preview_height;
						//log_debug("pre_width<%d>, pre_height<%d>, var.xres<%d>, var.yres<%d>\n", pre_width, pre_height, var.xres, var.yres);
						cam_ctrl_cvt_to_argb(&gcam_ctrl, gcam_ctrl.preview_buf_ptr, preview_buffer32,
								pre_width, pre_height);
						// directly blit to screen fb
					#if (CFG_PREVIEW_LCD_BPP == 32)
						fb_ctrl_blitToFB(fbBase + fbIndex * fbBufSize, var.xres, var.yres,
								preview_buffer32, CFG_PREVIEW_LCD_BPP >> 3, pre_width, pre_height);
					#elif (CFG_PREVIEW_LCD_BPP == 16)
						libyuv::ARGBToRGB565(preview_buffer32, pre_width * 4, preview_buffer16, pre_width * 2, pre_width, pre_height);
						fb_ctrl_blitToFB(fbBase + fbIndex * fbBufSize, var.xres, var.yres,
								preview_buffer16, CFG_PREVIEW_LCD_BPP >> 3, pre_width, pre_height);
					#endif
					#if MIPI_LCD_ST7701S
						var.yoffset = var.yres * fbIndex;
						ioctl(fbfd, FBIOPAN_DISPLAY, &var);
						fbIndex = !fbIndex;
					#endif
					}
#endif
					cam_ctrl_q_frame_data_ptr(&gcam_ctrl);
				}
			}
	  		break;
			
#if CFG_FACE_BIN_LIVENESS
			case STREAM_DEPTH:
			{
				if (!faceBuf.depth_filled && faceBuf.depth_mutex.try_lock()) 
				{
					log_debug("===it is depth stream.\n");
					cam_ctrl_deq_frame_data_ptr(&gcam_ctrl, &data_ptr, FRAME_TYPE_DEPTH);
					cur_stream_type = STREAM_IR;

					cam_ctrl_cvt_to_depth(&gcam_ctrl, (uint16_t *)data_ptr, (uint16_t *)faceBuf.depth_buf,
							CFG_FACE_LIB_FRAME_WIDTH, CFG_FACE_LIB_FRAME_HEIGHT);
					faceBuf.depth_filled = true;
					faceBuf.depth_mutex.unlock();
					CV.notify_one();
					
					cam_ctrl_q_frame_data_ptr(&gcam_ctrl);
				}
			}
			break;
#endif
			default:			
			break;
		}
		
		usleep(5000);
	}

#if CFG_PROJECTOR_CTRL
	//close projector
	projector_ctrl_off(projector_fd_i2c);
#endif
	cam_ctrl_stop_stream(&gcam_ctrl);
	cam_ctrl_deinit(&gcam_ctrl);

#if CFG_ENABLE_PREVIEW
	if(LcdOpenOK_flg)
	{
		fb_ctrl_fbSinkUninit(fbfd, fbBase, fbBufSize * 2);
	}
#endif

	return;
}

int main(int argc, char **argv)
{
	std::thread *detectThread;
	std::thread *previewThread;
	int32_t *pdet_thr_ret = NULL, *ppre_thr_ret = NULL;
	cam_ctrl_init_t cam_init_data;
	int32_t ret = 0;
	char v4l2_dev[30] = "/dev/video0";

	// Set the log level
	stLogCfg stSysLogCfg;
	GetSysLogCfg(&stSysLogCfg);
	log_set_level(stSysLogCfg.log_level);
	log_set_simple(1);

	faceBuf.ir_filled = false;
#if CFG_FACE_BIN_LIVENESS
	faceBuf.depth_filled = false;
#endif

	//system function cfg
	memset(&facelib_param, 0, sizeof(face_alg_param_t));
	facelib_param.quality_flag = 0;
	facelib_param.face_landarks_flag = 1;
	facelib_param.living_detect_flag = 1;

	// Init camera
	cam_init_data.v4l2_dev = v4l2_dev;
	cam_init_data.fmt = CFG_V4L2_PIX_FMT;
	cam_init_data.width = CFG_CAM_WIDTH;
	cam_init_data.height = CFG_CAM_HEIGHT;
	ret = cam_ctrl_init(&cam_init_data, &gcam_ctrl);
	if (ret) {
		cmdSysHardwareAbnormalReport(HW_BUG_CAMERA);
		goto err_rtn;
	}
	
#if CFG_PROJECTOR_CTRL
	projector_fd_i2c = projector_ctrl_init();
	if(projector_fd_i2c <= 0)
	{
		return -1;
	}
	//open projector
	projector_ctrl_on(projector_fd_i2c);
	//图像亮度
	//Gain_ctrl_init(4);
#endif

	ret = cam_ctrl_config_ir(&gcam_ctrl, CFG_IR_FRAME_WIDTH, CFG_IR_FRAME_HEIGHT, CFG_IR_FPS);
	if (ret) {
		goto err_rtn;
	}

	ret = cam_ctrl_config_depth(&gcam_ctrl, CFG_DEPTH_FRAME_WIDTH, CFG_DEPTH_FRAME_HEIGHT, CFG_DEPTH_FPS);
	if (ret) {
		goto err_rtn;
	}

	// v4l2 preview thread 
	previewThread = new std::thread(face_recognize_preview_proc);
	// create thread for face detection
	detectThread = new std::thread(face_recognize_detect_proc);
	
#if CFG_ENABLE_APP_EXIT_FROM_TIMER
	sig_timer_start(CFG_RCG_APP_EXIT_TIMEOUT_MS, face_recognize_run_timer_cb);
#endif

	detectThread->join();
	delete detectThread;
	previewRun = false;
	previewThread->join();
	delete previewThread;

	return 0;
err_rtn:
	cam_ctrl_deinit(&gcam_ctrl);

	return -1;
}

