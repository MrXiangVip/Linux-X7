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
#include "RSFaceSDK.h"

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
static int32_t projector_fd_i2c = 0;

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
static std::mutex mtx;
static std::condition_variable CV;
static bool LcdOpenOK_flg = false;    // LCD 打开成功与否的标记

extern int person_id;  


#if CFG_ENABLE_APP_EXIT_FROM_TIMER
static void face_register_run_timer_cb(void)
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

static void face_regisiter_detect_process_proc(unsigned char RegAddFaceType)
{
	static unsigned char RegAddFaceTypeOld = ADDFACE_NULL;
	static unsigned char num = 0;

	//防止发送状态太频繁,多条只发送一条
	if(RegAddFaceType == RegAddFaceTypeOld)
	{
		if(0 != ((num++) % 10))
		{
			return;
		}
	}
	else
	{
		RegAddFaceTypeOld = RegAddFaceType;
	}
	
	switch(RegAddFaceType)
	{
	case ADDFACE_NULL:
		cmdFaceRegistRsp((uint16_t) ID_REG_ADDFACE_NULL);
		break;
	case PITCH_UP:
		cmdFaceRegistRsp((uint16_t) ID_REG_PITCH_UP);
		break;
	case PITCH_DOWN:
		cmdFaceRegistRsp((uint16_t) ID_REG_PITCH_DOWN);
		break;
	case YAW_LEFT:
		cmdFaceRegistRsp((uint16_t) ID_REG_YAW_LEFT);
		break;
	case YAW_RIGHT:
		cmdFaceRegistRsp((uint16_t) ID_REG_YAW_RIGHT);
		break;
#if ROLL_ENABLE
	case ROLL_LEFT:
		cmdFaceRegistRsp((uint16_t) ID_REG_ROLL_LEFT);
		break;
	case ROLL_RIGHT:
		cmdFaceRegistRsp((uint16_t) ID_REG_ROLL_RIGHT);
		break;
#endif
	default:
		break;
	}

	/*每条消息发送完后，等待MCU端播放语音提示，后面可能要做成应答形式更方便，
	等待MCU播放完毕才开始运行人脸应用，暂时用固定的时间*/
	sleep(1);
}

static void face_register_detect_proc(void)
{
	static bool CreatPersonOK_Flag = true;	
	rs_face RsFace;
	unsigned char RegAddFaceType=ADDFACE_NULL;		

	int32_t ret_val = RET_NO_FACE;
	int nPersonID = 0,  faceId = 0, len = 0; 
	int recogtime = 0;  
	unsigned char RegResp[4] = {0x41, 0x37, 0x02, 0xFF};  //'A' '7' 0x02 faceId	
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

	if (face_alg_init(&facelib_param, FACE_REG)) 
	{
		cmdSysHardwareAbnormalReport(HW_BUG_ENCRYPTIC);
		log_error("Failed to init the face lib\n");
		return;
	}
	else
	{		
		log_info("RS version:%s ",rsGetRSFaceSDKVersion());
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

		// lock buffer to do face algorithm
		faceBuf.ir_mutex.lock();  

#if CFG_PER_TIME_CAL_OVERALL
		per_calc_start();
#endif

#if CFG_FACE_BIN_LIVENESS
		faceBuf.depth_mutex.lock();
		if (CreatPersonOK_Flag)
		{
			ret_val = face_alg_register(&ir_img, &depth_img, CREAT_PERSON,  &RsFace, ADDFACE_NULL);
		}
		else
		{
			ret_val = face_alg_register(&ir_img, &depth_img, PERSON_ADD_FACE,  &RsFace, RegAddFaceType);
		}
		faceBuf.depth_filled = 0;
		faceBuf.depth_mutex.unlock();
#else
		ret_val = face_alg_register(&ir_img);
#endif

		faceBuf.ir_filled = 0;
		faceBuf.ir_mutex.unlock();
#if CFG_PER_TIME_CAL_OVERALL
		per_calc_end();
		log_info("Face register: %d usec\n", per_calc_get_result());
#endif
		//log_set_color(1);
		
		log_info("@@@Face pitch<%f>  roll<%f> yaw<%f>  score<%f>\n",RsFace.pitch,RsFace.roll,RsFace.yaw,RsFace.score);
		//log_info("Face width* height<%d*%d>, top<%d>, left<%d> \n",RsFace.rect.width,RsFace.rect.height,RsFace.rect.top,RsFace.rect.left);
		//log_info("===recogtime<%d>==RegAddFaceType<%d>==ret_val<%d>====\n", recogtime, RegAddFaceType, ret_val);

		 if ((RET_OK == ret_val) || ((RegAddFaceType>=RegAddFaceTypeNUM) && (recogtime>20)) )
		{
			nPersonID = person_id; 
			log_info("face registered success!personId<%d>\n", nPersonID);					
			
#if CFG_ENABLE_PREVIEW_BANNER
			bmp_banner_set(BANNER_REGISTER_OK);
#endif			
			cmdFaceRegistRsp((uint16_t) nPersonID);
			break;
		}
		 else if ((RET_PERSON_ADD_FACE == ret_val) && (RegAddFaceType<RegAddFaceTypeNUM)) 
		 {
#if 0		
			  char buf[64] = {0};
			  memset(buf, 0, sizeof(buf));
			  snprintf(buf, sizeof(buf), "pic_reg_Depth_%d.raw", RegAddFaceType);
			  myWriteData2File(buf, faceBuf.depth_buf, 2*CFG_FACE_LIB_FRAME_WIDTH*CFG_FACE_LIB_FRAME_HEIGHT);
			  memset(buf, 0, sizeof(buf));
			  snprintf(buf, sizeof(buf), "pic_reg_Ir_%d.raw", RegAddFaceType);
			  myWriteData2File(buf, faceBuf.ir_buf, CFG_FACE_LIB_FRAME_WIDTH*CFG_FACE_LIB_FRAME_HEIGHT);
#endif
			 nPersonID = person_id; 
			 CreatPersonOK_Flag = false;
			 RegAddFaceType++;
			 log_info("For personID add a face!PersonID<%d>:RegAddFaceType<%d>\n", person_id, RegAddFaceType);
 
			 //send msg to M4
			 //提示用户做出某种人脸摆拍，开始录入
			 face_regisiter_detect_process_proc(RegAddFaceType);
			 recogtime = 0;
			 continue;
		}
		else if(RET_NO_ANGLE == ret_val)
		{
			//send msg to M4
			//根据RsFace的值，提示当前人脸角度不达标
			rs_face *pRsFace = &RsFace;
			log_info("\n\n\RET_NO_ANGLE   === RegAddFaceType<%d>\n" , RegAddFaceType);
			face_regisiter_detect_process_proc(RegAddFaceType);
		}
		else if (RET_NO_QUALITY  == ret_val)
		{
#if 0		
			static int index= 0;
			char buf[64] = {0};
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "pic_reg_Depth_%d.raw", index);
			myWriteData2File(buf, faceBuf.depth_buf, 2*CFG_FACE_LIB_FRAME_WIDTH*CFG_FACE_LIB_FRAME_HEIGHT);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "pic_reg_Ir_%d.raw", index);
			myWriteData2File(buf, faceBuf.ir_buf, CFG_FACE_LIB_FRAME_WIDTH*CFG_FACE_LIB_FRAME_HEIGHT);
			index++;
#endif
			//send msg to M4
			//根据RsFace的值，提示当前人脸质量不达标原因
			rs_face *pRsFace = &RsFace;
			log_info("\n\n\nRET_NO_QUALITY   === RegAddFaceType<%d>\n" , RegAddFaceType);
			face_regisiter_detect_process_proc(RegAddFaceType);
		}
		else if (RET_ALREADY_IN_DATABASE == ret_val) 
		{
#if CFG_ENABLE_PREVIEW_BANNER
			bmp_banner_set(BANNER_REGISTERED);
#endif			
			log_info("face already exists!\n");
			
			cmdFaceRegistRsp((uint16_t) 0xffff);				
			break;
		}
		else if (RET_NO_LIVENESS == ret_val) 
		{		
			log_info("liveness check failed!\n");				
			cmdFaceRegistRsp((uint16_t) 0xff01);
		}
		else if (RET_NO_FACE == ret_val) 
		{
			log_warn("No face detected!\n");
			cmdFaceRegistRsp((uint16_t) 0xff02);
		}
	
		recogtime ++;  
		//如果某个角度的人脸长期注册不上就跳过	
		if ((recogtime > 20) && (RegAddFaceType>0 && RegAddFaceType<RegAddFaceTypeNUM)) 
		{			
			log_debug("Warring: while personID[%d] added face<%d> failed!!!\n", person_id, RegAddFaceType);
			RegAddFaceType++;
			log_info("For personID add another face!PersonID<%d>:RegAddFaceType<%d>\n", person_id, RegAddFaceType);

			//send msg to M4
			//提示用户做出某种人脸摆拍，开始录入
			face_regisiter_detect_process_proc(RegAddFaceType);
			recogtime = 0;
			continue;
		}
		//注册正脸的超时处理
		if(recogtime > 35)
		{
#if CFG_ENABLE_PREVIEW_BANNER
			bmp_banner_set(BANNER_REGISTER_FAIL);	
#endif
			recogtime = 0;
			log_error("face register failed!\n");
		
			cmdFaceRegistRsp((uint16_t) 0x0000);

			if (RegAddFaceType > 0)
			{
				DeleteFaceID(nPersonID);
				log_info("face deleted: id<%d> \n", nPersonID);	
			}
			
			break;
		}
	}

	face_alg_deinit();
	detectRun = false;	
}

static void face_register_preview_proc(void)
{
	int32_t cur_stream_type = STREAM_IR;

#if CFG_ENABLE_PREVIEW
	int32_t fbfd = 0;
	uint8_t *fbBase = NULL;
	struct fb_var_screeninfo var;
	int32_t fbIndex = 0, fbBufSize = 0;
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
				BANNER_REGISTERING);
		//bmp_banner_update(fbIndex);
	#endif
	}
#endif

	// stream on
	if (cam_ctrl_start_stream(&gcam_ctrl)) 
	{
		log_error("cam ctrl start failed\n");
		cam_ctrl_deinit(&gcam_ctrl);
#if CFG_ENABLE_PREVIEW
		if(LcdOpenOK_flg)
		{
			fb_ctrl_fbSinkUninit(fbfd, fbBase, fbBufSize * 2);
		}
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
				log_debug("===it is ir stream.\n");
				cam_ctrl_deq_frame_data_ptr(&gcam_ctrl, &data_ptr, FRAME_TYPE_IR);

				if (!faceBuf.ir_filled && faceBuf.ir_mutex.try_lock()) 
				{
					uint8_t *tmp_buf_ptr = faceBuf.ir_buf;
#if CFG_FACE_BIN_LIVENESS
					cur_stream_type = STREAM_DEPTH;
#endif // CFG_FACE_BIN_LIVENESS

					cam_ctrl_cvt_to_gray(&gcam_ctrl, data_ptr, tmp_buf_ptr, CFG_FACE_LIB_FRAME_WIDTH, CFG_FACE_LIB_FRAME_HEIGHT);

					faceBuf.ir_filled = 1;
					faceBuf.ir_mutex.unlock();
					CV.notify_one();
				}

#if CFG_ENABLE_PREVIEW
				if(LcdOpenOK_flg)
				{

					uint32_t pre_width = gcam_ctrl.preview_width;
					uint32_t pre_height = gcam_ctrl.preview_height;
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
				#if CFG_ENABLE_PREVIEW_BANNER
					bmp_banner_update(fbIndex);
				#endif
				#if MIPI_LCD_ST7701S
					ioctl(fbfd, FBIOPAN_DISPLAY, &var);
					fbIndex = !fbIndex;
				#endif				
				}
#endif
			}
	  		break;
			
#if CFG_FACE_BIN_LIVENESS
			case STREAM_DEPTH:
			{
				log_debug("===it is depth stream.\n");
				cam_ctrl_deq_frame_data_ptr(&gcam_ctrl, &data_ptr, FRAME_TYPE_DEPTH);

				if (!faceBuf.depth_filled && faceBuf.depth_mutex.try_lock()) 
				{
					cur_stream_type = STREAM_IR;

					cam_ctrl_cvt_to_depth(&gcam_ctrl, (uint16_t *)data_ptr, (uint16_t *)faceBuf.depth_buf,
							CFG_FACE_LIB_FRAME_WIDTH, CFG_FACE_LIB_FRAME_HEIGHT);
					faceBuf.depth_filled = true;
					faceBuf.depth_mutex.unlock();
					CV.notify_one();
				}
			}
			break;
#endif
			default:
			break;
		}
		
		cam_ctrl_q_frame_data_ptr(&gcam_ctrl);
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

#if CFG_PER_TIME_CAL_INIT_OVERALL
	per_calc_start();
#endif

	// Set the log level
	stLogCfg stSysLogCfg;
	GetSysLogCfg(&stSysLogCfg);
	log_set_level(stSysLogCfg.log_level);
	log_set_simple(1);

	//system function cfg
	faceBuf.ir_filled = false;
#if CFG_FACE_BIN_LIVENESS
	faceBuf.depth_filled = false;
#endif

	//system function cfg
	memset(&facelib_param, 0, sizeof(face_alg_param_t));
	facelib_param.quality_flag = 1;
	facelib_param.face_landarks_flag = 1;
	facelib_param.living_detect_flag = 1;

	// Init camera
	cam_init_data.v4l2_dev = v4l2_dev;
	cam_init_data.fmt = CFG_V4L2_PIX_FMT;
	cam_init_data.width = CFG_CAM_WIDTH;
	cam_init_data.height = CFG_CAM_HEIGHT;
	ret = cam_ctrl_init(&cam_init_data, &gcam_ctrl);
	if (ret) 
	{		
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
	Gain_ctrl_init(2);
#endif

	ret = cam_ctrl_config_ir(&gcam_ctrl, CFG_IR_FRAME_WIDTH, CFG_IR_FRAME_HEIGHT, CFG_IR_FPS);
	if (ret) {
		goto err_rtn;
	}

	ret = cam_ctrl_config_depth(&gcam_ctrl, CFG_DEPTH_FRAME_WIDTH, CFG_DEPTH_FRAME_HEIGHT, CFG_DEPTH_FPS);
	if (ret) {
		goto err_rtn;
	}

	if (RET_OK != cam_ctrl_enable_ir_and_depth(&gcam_ctrl)) {
		log_debug("Failed to set IR and Depth stream!\n");
		goto err_rtn;
	}

#if CFG_PER_TIME_CAL_INIT_OVERALL
	per_calc_end();
	log_info("Init proc: %d usec\n", per_calc_get_result());
#endif

	// create thread for face detection
	detectThread = new std::thread(face_register_detect_proc);
	// v4l2 preview thread 
	previewThread = new std::thread(face_register_preview_proc);
	
#if CFG_ENABLE_APP_EXIT_FROM_TIMER
	sig_timer_start(CFG_REG_APP_EXIT_TIMEOUT_MS, face_register_run_timer_cb);
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

