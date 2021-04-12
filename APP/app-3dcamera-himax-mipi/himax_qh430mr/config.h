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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <linux/videodev2.h>
#include "RSCommon.h"


//LCD type 	只能选一
#define MIPI_LCD_ST7701S						1
#define SPI_LCD_MIAOYING						0
#if (MIPI_LCD_ST7701S + SPI_LCD_MIAOYING > 1)
	#error "MIPI_LCD_ST7701S and SPI_LCD_MIAOYING can't be defined at the same time!"
#endif

#define CHECK_FACE_VERIFICATION				1
#define CFG_PROJECTOR_CTRL					0

#define CFG_CAM_WIDTH							1280
#define CFG_CAM_HEIGHT							800

// Camera parameters
#define CFG_CAMERA_CROP_OUTPUT					0
#define CFG_CAMERA_SCALE_OUTPUT					0

#define CFG_IR_CAM_WIDTH						CFG_CAM_WIDTH
#define CFG_IR_CAM_HEIGHT						CFG_CAM_HEIGHT
#define CFG_DEPTH_CAM_WIDTH						CFG_CAM_WIDTH
#define CFG_DEPTH_CAM_HEIGHT					CFG_CAM_HEIGHT

/* Roate needed */
#if MIPI_LCD_ST7701S
#define CFG_DEPTH_FRAME_WIDTH 					640
#define CFG_DEPTH_FRAME_HEIGHT 					400
#endif
#if SPI_LCD_MIAOYING
#define CFG_DEPTH_FRAME_WIDTH 					320
#define CFG_DEPTH_FRAME_HEIGHT 					200
#endif
#define CFG_DEPTH_FRAME_BPP						16
#define CFG_DEPTH_FRAME_SIZE					(CFG_DEPTH_FRAME_WIDTH * CFG_DEPTH_FRAME_HEIGHT * CFG_DEPTH_FRAME_BPP / 8)
#define CFG_DEPTH_FRAME_ACTUAL_WIDTH			214
#define CFG_DEPTH_FRAME_ACTUAL_HEIGHT			134
#define CFG_DEPTH_ACTUAL_FRAME_SIZE				(CFG_DEPTH_FRAME_ACTUAL_WIDTH * CFG_DEPTH_FRAME_ACTUAL_HEIGHT * CFG_DEPTH_FRAME_BPP / 8)

#define CFG_ENABLE_DEPTH_CLKWISE_ROTATE			1

#if CFG_ENABLE_DEPTH_CLKWISE_ROTATE

	#define CFG_DEPTH_CLKWISE_ROTATE_90			0
	#define CFG_DEPTH_CLKWISE_ROTATE_180		0
	#define CFG_DEPTH_CLKWISE_ROTATE_270		1
	#if ((CFG_DEPTH_CLKWISE_ROTATE_90 + CFG_DEPTH_CLKWISE_ROTATE_180 + CFG_DEPTH_CLKWISE_ROTATE_270) > 1)
		#error "Only one of depth rotate mode can be defined!"
	#endif
#endif

/* Roate needed */
#if MIPI_LCD_ST7701S
#define CFG_IR_FRAME_WIDTH						(640)
#define CFG_IR_FRAME_HEIGHT 					(400)
#endif
#if SPI_LCD_MIAOYING
#define CFG_IR_FRAME_WIDTH						(320)
#define CFG_IR_FRAME_HEIGHT 					(200)
#endif
#define CFG_IR_FRAME_BPP						8	
#define CFG_IR_FRAME_SIZE						(CFG_IR_FRAME_WIDTH * CFG_IR_FRAME_HEIGHT * CFG_IR_FRAME_BPP / 8)

#define CFG_ENABLE_IR_CLKWISE_ROTATE			1

#if CFG_ENABLE_IR_CLKWISE_ROTATE
	#define CFG_IR_CLKWISE_ROTATE_90			0
	#define CFG_IR_CLKWISE_ROTATE_180			0
	#define CFG_IR_CLKWISE_ROTATE_270			1
	#if ((CFG_IR_CLKWISE_ROTATE_90 + CFG_IR_CLKWISE_ROTATE_180 + CFG_IR_CLKWISE_ROTATE_270) > 1)
		#error "Only one of ir rotate mode can be defined!"
	#endif
#endif

#define CFG_IR_FPS								30
#define CFG_DEPTH_FPS							30

#define CFG_READSENSE_DB_PATH				"./readsense.db"

// Liveness related
#define CFG_FACE_BIN_LIVENESS					(1)		// 双目摄像头，奥比和himax都是双面摄像头
/*2d 活体检测/3d 活体检测/多模态活体检测  三选一*/
#define MODE_2D_LIVENESS						0		// 2d 活体检测
#define MODE_3D_LIVENESS						0		// 3D合体检测 
#define MODE_MUTI_LIVENESS					1		//多模态活体检测
#if ((MODE_2D_LIVENESS + MODE_3D_LIVENESS + MODE_MUTI_LIVENESS) > 1)
	#error "MODE_2D_LIVENESS and MODE_3D_LIVENESS, MODE_MUTI_LIVENESS can't be defined at the same time!"
#endif

// Face library related
//#define CFG_FACE_LIB_FRAME_WIDTH				((CFG_DEPTH_FRAME_WIDTH  - 2)* 4)
#define CFG_FACE_LIB_FRAME_WIDTH				(CFG_DEPTH_FRAME_HEIGHT)
#define CFG_FACE_LIB_FRAME_HEIGHT				(CFG_DEPTH_FRAME_WIDTH)

#define CFG_FACE_LIB_LM_VER					2   //阅面算法库版本号
#define CFG_FACE_LIB_LICENSE_PATH				"2891c3af_arm_linux_chip_license_content.lic"

// PIX_FORMAT_GRAY, PIX_FORMAT_BGRA8888 and PIX_FORMAT_BGR888 are from RS header.
//#define CFG_FACE_LIB_PIX_FMT					0 //PIX_FORMAT_GRAY
#define CFG_FACE_LIB_PIX_FMT					PIX_FORMAT_GRAY

#if (CFG_FACE_LIB_PIX_FMT == 3) //PIX_FORMAT_BGRA8888
	#define CFG_FACE_LIB_PIX_FMT_BPP			32
#elif (CFG_FACE_LIB_PIX_FMT == 1) //PIX_FORMAT_BGR888
	#define CFG_FACE_LIB_PIX_FMT_BPP			24
#elif (CFG_FACE_LIB_PIX_FMT == 0) //PIX_FORMAT_GRAY
	#define CFG_FACE_LIB_PIX_FMT_BPP			8
#else
	#define CFG_FACE_LIB_PIX_FMT_BPP			16
#endif

#define CFG_FACE_LIB_DEPTH_PIX_FMT_BPP			16

#define CFG_FACE_LIB_IR_IMAGE_SIZE	\
		(CFG_FACE_LIB_FRAME_WIDTH * CFG_FACE_LIB_FRAME_HEIGHT * CFG_FACE_LIB_PIX_FMT_BPP / 8)
#define CFG_FACE_LIB_DEPTH_IMAGE_SIZE	\
		(CFG_FACE_LIB_FRAME_WIDTH * CFG_FACE_LIB_FRAME_HEIGHT * CFG_FACE_LIB_DEPTH_PIX_FMT_BPP / 8)

#define FACE_REG_THRESHOLD_VALUE 			70
#define FACE_RECG_THRESHOLD_VALUE 		50
#define FACE_THRESHOLD_VALUE 				60

#define CFG_FACE_QUALITY_THRESHHOLD		5

#define FACE_DETECT_RECGONIZE_ANGEL		(25.0)
#define FACE_DETECT_REGISTER_ANGEL		(12.0)   //10
/*注册过程中添加人脸时的角度参数*/
#define ADDFACE_REG_DETECT_MAX_ANGEL  	(35.0)
#define ADDFACE_REG_DETECT_MIN_ANGEL  	(13.0)
#define ADDFACE_REG_OFFSET_ANGEL			8

// Preview related
#define CFG_ENABLE_PREVIEW						1
#if MIPI_LCD_ST7701S
#define CFG_PREVIEW_LCD_BPP						32
#endif
#if SPI_LCD_MIAOYING
#define CFG_PREVIEW_LCD_BPP						16
#endif

#if (CFG_ENABLE_PREVIEW == 1) && (CFG_PREVIEW_LCD_BPP == 32)
	// Currently, only 32bits banner supported
	#define CFG_ENABLE_PREVIEW_BANNER			1
	#if CFG_ENABLE_PREVIEW_BANNER
		#define CFG_BANNER_WIDTH				480
		#define CFG_BANNER_HEIGHT				50
		#define CFG_BANNER_BPP					32
	#endif
#endif

#define CFG_ENABLE_CAM_LIBUVC					0
#if CFG_ENABLE_CAM_LIBUVC
	#define CFG_DEVICE_VENDOR_ID				0x1080
	#define CFG_DEVICE_PRODUCT_ID				0x1063
#endif

enum FACE_APP_TYPE
{
	FACE_LOOP = 0,
	FACE_REG,
	FACE_RECG
};

// Camera configs
#define CFG_ENABLE_CAM_I2C						0
#if CFG_ENABLE_CAM_I2C
	#define CFG_CAM_I2C_DEV						"/dev/i2c-0"
	#define CFG_CAM_I2C_SLAVE_ADDR				0x38
	#define CFG_CAM_ID							0x6010
#endif
#define CFG_ORBBEC_CAM_CONFIG_PATH				"/etc/config1.ini"
#define CFG_HIMAX_CAM_CONFIG_PATH				"/etc/reg.bin"

// Frame buffer related
#define CFG_RINGBUFFER_SIZE						(8 * 0x100000) // 8M

#define CFG_ENABLE_V4L2_SUPPORT					1
#if CFG_ENABLE_V4L2_SUPPORT
	#define CFG_V4L2_PIX_FMT					V4L2_PIX_FMT_RGB565	
	#define CFG_V4L2_BUF_NUM					4
#endif

/*log switch*/
#define CFG_PER_TIME_CAL_INIT_OVERALL				1
#define CFG_PER_TIME_CAL_OVERALL					1
#define CFG_PER_TIME_CAL_EVERYSTEP				0
#define CFG_PER_TIME_CAL_FRAME_PROCESS			0
#if ((CFG_PER_TIME_CAL_OVERALL + CFG_PER_TIME_CAL_EVERYSTEP + CFG_PER_TIME_CAL_FRAME_PROCESS) > 1)
	#error "CFG_PER_TIME_CAL_OVERALL and CFG_PER_TIME_CAL_EVERYSTEP, CFG_PER_TIME_CAL_FRAME_PROCESS can't be defined at the same time!"
#endif

#define CFG_CAM_USE_HX_PP_LIB					0

#define CFG_CAM_BRIDGE_IF_SUPPORT				0
#if CFG_CAM_BRIDGE_IF_SUPPORT
	#define CFG_CAM_BRIDGE_I2C_DEV				"/dev/i2c-1"
	#define CFG_CAM_BRIDGE_I2C_SLAVE_ADDR		0x0E
#endif

// Misc settings
#define CFG_ENABLE_APP_EXIT_FROM_TIMER			1
#if CFG_ENABLE_APP_EXIT_FROM_TIMER
	#define CFG_REG_APP_EXIT_TIMEOUT_MS			180000
	#define CFG_RCG_APP_EXIT_TIMEOUT_MS 			10000
#endif

#define CFG_ENABLE_IMG_DBG						1
#if CFG_ENABLE_IMG_DBG
	#define CFG_IMG_DBG_PREVIEW_NIR				0
	#define CFG_IMG_DBG_PREVIEW_DEPTH			0
	#define CFG_IMG_DBG_RECOGNIZE_NIR			0
	#define CFG_IMG_DBG_RECOGNIZE_DEPTH			0
#endif

#endif
