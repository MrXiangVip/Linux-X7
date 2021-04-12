/**************************************************************************
 * 	FileName:	 face_comm.h
 *	Description:	define the common function interface and constant of face app
 *	Copyright(C):	2018-2020 Wave Group Inc.
 *	Version:		V 1.0
 *	Author:		tanqw
 *	Created:		2019-8-30
 *	Updated:		
 *					
**************************************************************************/
 
#ifndef __FACE_COMM_H__
#define __FACE_COMM_H__

#include "custom.h"
#include "FIT.h"
#include "pfmgr.h"

#define UART7_DEV		"/dev/ttyLP3"
#define UART6_DEV		"/dev/ttyLP2"
#define UART5_DEV		"/dev/ttyLP1"
#define RPMSG_DEV		"/dev/ttyRPMSG30"
#define MAIN_WORK_PATH(file_name)		"/opt/smartlocker/"#file_name

typedef unsigned char		uint8_t;      
typedef unsigned short 	uint16_t;  
typedef unsigned int		uint32_t; 
typedef unsigned long long  uint64_t;
typedef int				int32_t;     
typedef bool 				BOOL;

#define YES			1
#define NO			0

//#define PER_TIME_CAL
#ifdef PER_TIME_CAL
#include <sys/time.h>
#endif

//LCD type
///#define MIPI_LCD_ST7701S
#define SPI_LCD_ST7789V


#if 0
#define CAM_WIDTH 640
#define CAM_HEIGHT 480
#else
#define CAM_WIDTH 320
#define CAM_HEIGHT 240
#endif


#define CAM_IMAGE_BPP 1 // GRAY
#define CAM_DEPTH_IMAGE_BPP 2 // GRAY

#ifdef LCD_DISPLAY_ENABLE
#define GRAY_TO_RGBA_BPP 4 // RGBA
#define GRAY_TO_RGB565_BPP 2 // RGB565
#define SPI_LCD_WIDTH 240 // SPI LCD WIDTH
#define SPI_LCD_HEIGHT 320 // SPI LCD HEIGHT
#define SPI_LCD_SIZE_INBYTE (SPI_LCD_WIDTH * SPI_LCD_HEIGHT * GRAY_TO_RGB565_BPP)
#endif

#define V4L2_BUF_NUM 4
#define FRAME_SIZE_INBYTE (CAM_WIDTH * CAM_HEIGHT * CAM_IMAGE_BPP)
#define DEPTH_FRAME_SIZE_INBYTE (CAM_WIDTH * CAM_HEIGHT * CAM_DEPTH_IMAGE_BPP)

#define SAMPLE_READ_WAIT_TIMEOUT 2000 //2000ms

#define MIPI_I2C_DEV	"/dev/i2c-0"

// Misc settings
#define CFG_ENABLE_APP_EXIT_FROM_TIMER			1
#if CFG_ENABLE_APP_EXIT_FROM_TIMER
	#define CFG_REC_APP_EXIT_TIMEOUT_MS			10000	
	#define CFG_REG_APP_EXIT_TIMEOUT_MS 			90000
#endif


#define FACE_RECG_THRESHOLD_VALUE 		80  // 识别阈值
#define FACE_REG_POS_FILTER_THRESHOLD	(0.93) //注册相邻姿态过滤阈值
#define FACE_DETECT_MIN_SIZE				150   //人脸检测最小窗口， 0~width， 默认150
#define FACE_DETECT_MAX_SIZE 				400
#define FACE_DETECT_MIN_LIGHT 				80
#define FACE_DETECT_MAX_LIGHT 			180
#define FACE_REG_SKIP_NUM					2
#define FACE_ANTIFACE_THRESHOLD 			(0.5)
#define FACE_REG_TIMEOUT 			30    //单张人脸注册超时时间
#define FACE_RECG_TIMEOUT 			5	// 人脸识别超时时间

struct v4l2buffer
{
	unsigned char *start;
	size_t offset;
	unsigned int length;
};

enum {
	BANNER_REGISTERING,
	BANNER_REGISTER_OK,
	BANNER_REGISTERED,
	BANNER_RECOGNIZING,
	BANNER_RECOGNIZED,
	BANNER_RECOGNIZE_FAIL,
	BANNER_LIVENESS_FAIL,
	BANNER_REGISTER_FAIL  
};

 
#endif // __FACE_COMM_H__


