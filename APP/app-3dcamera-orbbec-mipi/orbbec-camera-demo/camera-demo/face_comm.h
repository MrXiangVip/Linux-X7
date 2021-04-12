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

#include "RSApiHandle.h" 
#include "orbbec_mipi_ctrl.h"
#include "custom.h"

#define UART5_DEV		"/dev/ttyLP1"
#define RPMSG_DEV		"/dev/ttyRPMSG30"
#define MAIN_WORK_PATH(file_name)		"/opt/smartlocker/"#file_name

#define uint8_t       unsigned char
#define uint16_t      unsigned short
#define uint32_t      unsigned int
#define uint64_t      unsigned long long
#define size_t        unsigned long
#define int32_t       int
#define BOOL	unsigned char

#define YES			1
#define NO			0

//#define PER_TIME_CAL
#ifdef PER_TIME_CAL
#include <sys/time.h>
#endif

//LCD type
#define MIPI_LCD_ST7701S
//#define SPI_LCD_MIAOYING

#define CAM_WIDTH MX6000_CAM_WIDTH
#define CAM_HEIGHT MX6000_CAM_HEIGHT
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


