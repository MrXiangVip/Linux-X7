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
#include "custom.h"

#define VIU_OUTFMT_YUV

#define CAM_WIDTH 640
#define CAM_HEIGHT 480
#define CAM_IMAGE_BPP 4 // BGRA8888
#define V4L2_BUF_NUM 8
#define FRAME_SIZE_INBYTE (CAM_WIDTH*CAM_HEIGHT*CAM_IMAGE_BPP)

// Misc settings
#define CFG_ENABLE_APP_EXIT_FROM_TIMER			1
#if CFG_ENABLE_APP_EXIT_FROM_TIMER
	#define CFG_REC_APP_EXIT_TIMEOUT_MS			8000	
	#define CFG_REG_APP_EXIT_TIMEOUT_MS 			12000
#endif

struct faceBuffer
{
	unsigned char buffer[FRAME_SIZE_INBYTE];
	std::mutex mutex;
	bool filled;
};

struct v4l2buffer
{
	unsigned char *start;
	size_t offset;
	unsigned int length;
};

static struct v4l2buffer buffers[V4L2_BUF_NUM];
static struct faceBuffer faceBuf;

static unsigned char argbBuffer[FRAME_SIZE_INBYTE];
static unsigned int camOutFmt = V4L2_PIX_FMT_YUYV;

static std::mutex mtx;
static std::condition_variable CV;
static std::thread *detectThread;
static std::thread *previewThread;

static bool detectRun = true;
static bool previewRun = true;

 
#endif // __FACE_COMM_H__


