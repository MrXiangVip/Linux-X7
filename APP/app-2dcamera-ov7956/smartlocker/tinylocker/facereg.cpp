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
#include <poll.h>

#include "RSApiHandle.h"
#include "libyuv.h"

#include "bmp_registering.cpp"
#include "bmp_registered.cpp"
#include "bmp_registeredfail.cpp"
#include "bmp_aleadyindatabase.cpp"
#include "face_comm.h"
#include "function.h"
#include "sig_timer.h"

using namespace std;

extern int personId; 

#if CFG_ENABLE_APP_EXIT_FROM_TIMER
static void face_register_run_timer_cb(void)
{
	printf("app run Timeout, exit!\n");
	detectRun = false;
	
	static unsigned char Num = 0;
	if((++Num) > 1)
	{
		printf("App run timeout, exit(-1)!\n");		
		exit(-1);
	}
}
#endif

static void faceDetectProc()
{
	RSApiHandle rsHandle;
	int faceId=0, len=0, fd=0, ret=0; 
	int recogtime = 0;  
	unsigned char RegResp[4] = {0x41, 0x37, 0x02, 0xFF};  //'A' '7' 0x02 faceId
	imageInfo img = {
		faceBuf.buffer,
		CAM_WIDTH,
		CAM_HEIGHT,
		CAM_WIDTH * CAM_IMAGE_BPP,
	};

	if(!rsHandle.Init(FACE_REG)) {
		cerr << "Failed to init the ReadSense lib" << endl;
		return;
	}
	else
	{
		cout << "RS version: " << rsGetRSFaceSDKVersion() << endl;
	}
	
	personId = -1; //set personId to initial value

	cout << "faceDetect begin..." << endl;  //hannah test	
	while (detectRun) 
	{
		std::unique_lock<std::mutex> lck(mtx);
		// wait for buffer ready
		CV.wait(lck, []{ return faceBuf.filled; });

		// lock buffer to do face algorithm
		faceBuf.mutex.lock();
		ret = rsHandle.RegisterFace(img);
		faceBuf.filled = false;
		faceBuf.mutex.unlock();
		
		if (RS_RET_OK == ret) 
		{
			faceId = personId; 	
			cout << "face registered success!" << endl;					
			RegResp[3] = (unsigned char)faceId;
			SendMsgToRpMSG(RegResp, sizeof(RegResp));
			break;
		}		
		else if (RS_RET_ALREADY_IN_DATABASE == ret) 
		{
			// face recogized ok
			cout << "face registered before!" << endl;
			RegResp[3] = 0x00; //(unsigned char)faceId;
			SendMsgToRpMSG(RegResp, sizeof(RegResp));				
			break;
		}
		else if (RS_RET_NO_LIVENESS == ret) 
		{		
			cout << "liveness check failed!" << endl;
			recogtime += 2;
		}
		else if (RS_RET_NO_FACE == ret) 
		{
			cout << "no face detected!" << endl;
		}

		if (recogtime > 80)  
		{
			cout << "face registration failed!!!" << endl;
			RegResp[3] =0xFF;
			SendMsgToRpMSG(RegResp, sizeof(RegResp));
			break;
		}		
		//printf("=====recogtime<%d>==========\n", recogtime);
		recogtime++;
	}
}

static bool v4l2CaptureInit(int *pfd)
{
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_capability cap;
	const char *v4l2dev = "/dev/video0";
	int fd_v4l = 0, ret;

	if ((fd_v4l = open(v4l2dev, O_RDWR, 0)) < 0) {
		cerr << "unable to open " << v4l2dev << endl;
		return false;
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.capturemode = 0;
	parm.parm.capture.timeperframe.denominator = 15;
	parm.parm.capture.timeperframe.numerator = 1;
	ret = ioctl(fd_v4l, VIDIOC_S_PARM, &parm);
	if (ret < 0) {
		cerr << "VIDIOC_S_PARM failed:" << ret << endl;
		goto err;
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = camOutFmt;
	fmt.fmt.pix.width = CAM_WIDTH;
	fmt.fmt.pix.height = CAM_HEIGHT;
	if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0) {
		cerr << "set format failed:" << ret << endl;
		goto err;
	}

	*pfd = fd_v4l;
	return true;

err:
	close(fd_v4l);
	detectRun = false; //hannah added 181226
	return false;
}

static bool fbSinkInit(int *pfd, unsigned char **fbbase,
				struct fb_var_screeninfo *pvar)
{
	int fd_fb, fbsize, ret;
	const char *fb_dev = "/dev/fb0";
	struct fb_var_screeninfo var;

	if ((fd_fb = open(fb_dev, O_RDWR )) < 0) {
		cerr << "Unable to open frame buffer" << endl;
		return false;
	}

	if (ioctl(fd_fb, FBIOGET_VSCREENINFO, &var) < 0) {
		cerr << "FBIOPUT_VSCREENINFO failed" << endl;
		goto err;
	}

	// ping-pong buffer
	var.xres_virtual = var.xres;
	var.yres_virtual = 2 * var.yres;

	ret = ioctl(fd_fb, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0) {
		cerr << "FBIOPUT_VSCREENINFO failed:" << ret << endl;
		goto err;
	}

	// Map the device to memory
	fbsize = var.xres * var.yres_virtual * var.bits_per_pixel / 8;
	cout << "framebuffer info: (" << var.xres << "x" <<
		var.yres << "), bpp:" << var.bits_per_pixel << endl;

	*fbbase = (unsigned char *)mmap(0, fbsize,
					PROT_READ | PROT_WRITE,
					MAP_SHARED, fd_fb, 0);
	if (MAP_FAILED == *fbbase) {
		cerr << "Failed to map framebuffer device to memory" << endl;
		goto err;
	}

#if 0
	memcpy(*fbbase + fbsize/2 - var.xres * 100 * var.bits_per_pixel/8,
			(void *)registering_bmp,
			480 * 50 * 4);
	memcpy(*fbbase + fbsize - var.xres * 100 * var.bits_per_pixel/8,
			(void *)registering_bmp,
			480 * 50 * 4);
#else
	//memcpy(*fbbase, (void *)registering_bmp, 720 * 1280 * 4);
	memset(*fbbase+480*480*4 ,
	0x00,
	480*320*4);
#endif

	*pvar = var;

	return true;
err:
	close(fd_fb);
	return false;
}

static void fbSinkUninit(int fd, unsigned char *fbbase, int fbsize)
{
	memset(fbbase + 480 * 480 * 4,
	0x00,
	480*320*4);
	
    //printf("==========personId = %d\n", personId); 
	if (personId > 0)  //if registered success
	{
		// show the done image
	#if 0	
		memcpy(fbbase + fbsize/2 - 480 * 100 * 4,
				(void *)registered_bmp,
				480 * 50 * 4);
		memcpy(fbbase + fbsize - 480 * 100 * 4,
				(void *)registered_bmp,
				480 * 50 * 4);
	#else
		memcpy(fbbase + fbsize/2 - 480 * 200 * 4,
				(void *)registered_bmp,
				480*40*4);
		/*memcpy(fbbase + fbsize - 640 * 200  * 4,
				(void *)registered_bmp,
				720 * 108 * 4);*/		
    #endif		
	}
	else if(personId == 0)
	{
		memcpy(fbbase + fbsize/2 - 480 * 200 * 4,
				(void *)aleadyindatabase_bmp,
				480*40*4);
		/*memcpy(fbbase + fbsize - 640 * 200  * 4,
				(void *)aleadyindatabase_bmp,
				720 * 108 * 4);*/			
	}
	else
	{
		memcpy(fbbase + fbsize/2 - 480 * 200 * 4,
				(void *)registeredfail_bmp,
				480*40*4);
		/*memcpy(fbbase + fbsize - 640 * 200  * 4,
				(void *)registeredfail_bmp,
				720 * 108 * 4);*/			
	}
	
	if (fbbase)
		munmap(fbbase, fbsize);
	close(fd);
}

static void v4l2CaptureStop(int v4l2fd)
{
	enum v4l2_buf_type type;
	int i;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	// stream off first
	ioctl(v4l2fd, VIDIOC_STREAMOFF, &type);

	// unmap buffers
	for (i = 0; i < V4L2_BUF_NUM; i++)
		munmap(buffers[i].start, buffers[i].length);

	// close fd
	close(v4l2fd);
}

static bool v4l2CaptureStart(int v4l2fd)
{
	unsigned int i;
	struct v4l2_buffer buf;
	enum v4l2_buf_type type;
	struct v4l2_requestbuffers req;

	memset(&req, 0, sizeof (req));
	req.count = V4L2_BUF_NUM;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	// REQBUFS
	if (ioctl(v4l2fd, VIDIOC_REQBUFS, &req) < 0) {
		cerr << "VIDIOC_REQBUFS failed" << endl;
		return false;
	}

	// QUERYBUF
	for (i = 0; i < V4L2_BUF_NUM; i++) {
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(v4l2fd, VIDIOC_QUERYBUF, &buf) < 0) {
			cerr << "VIDIOC_QUERYBUF error" << endl;
			return false;
		}

		buffers[i].length = buf.length;
		buffers[i].offset = (size_t) buf.m.offset;
		buffers[i].start = (unsigned char*)mmap(NULL, buffers[i].length,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			v4l2fd, buffers[i].offset);
		memset(buffers[i].start, 0xFF, buffers[i].length);
	}

	// QBUF all buffers ready for streamon
	for (i = 0; i < V4L2_BUF_NUM; i++)
	{
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.length = buffers[i].length;
		buf.m.offset = buffers[i].offset;

		if (ioctl(v4l2fd, VIDIOC_QBUF, &buf) < 0) {
			cerr << "VIDIOC_QBUF error" << endl;
			return false;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(v4l2fd, VIDIOC_STREAMON, &type) < 0) {
		cerr << "VIDIOC_STREAMON error" << endl;
		return false;
	}

	return true;
}

static void blitToFB(unsigned char *fbbase,
                        int fbwidth, int fbheight,
                        unsigned char *v4lbase,
                        int camwidth, int camheight)
{
	int offsetx, y;
	unsigned int *fbpixel;
	unsigned int *campixel;

	offsetx =0;   //added 190214
	fbpixel = (unsigned int*)fbbase;  
	campixel = (unsigned int*)v4lbase;

	for (y = 0; y < camheight; y++) {
		memcpy((void*)(fbpixel+offsetx), (void*)campixel, 480 * 4);  
		//modified 190214 memcpy((void*)fbpixel, (void*)campixel, camwidth * 4);  
		campixel += 640;
		fbpixel += fbwidth;
	}

	memcpy(fbbase + 480 * 600 * 4,
	(void *)registering_bmp,
	480*40*4);
}

static void camPreviewProc()
{
	int v4l2fd, fbfd, xres, yres;
	unsigned char *fbBase, *blitSrc;
	struct v4l2_buffer buf;
	struct fb_var_screeninfo var;
	int fbIndex = 0, fbBufSize;

	// open and setup the video capture dev 
	if(!v4l2CaptureInit(&v4l2fd))
		return;

#ifdef LCD_DISPLAY_ENABLE
	// open and setup the framebuffer for preview 
	if(!fbSinkInit(&fbfd, &fbBase, &var)) {
		close(v4l2fd);
		return;
	} 

	fbBufSize = var.xres * var.yres * var.bits_per_pixel / 8;
#endif

	// stream on
	if(!v4l2CaptureStart(v4l2fd)) {
		close(v4l2fd);
#ifdef LCD_DISPLAY_ENABLE 	
		fbSinkUninit(fbfd, fbBase, fbBufSize * 2);
#endif
		return;  //hannah added 190115
	}

	cout << "camPreview begin..." << endl;
	while (previewRun) 
	{
		// DQBUF for ready buffer
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(v4l2fd, VIDIOC_DQBUF, &buf) < 0) 
		{
			cerr << "VIDIOC_DQBUF failed" << endl;
			break;
		}

		// CSC from YUYV to ARGB
		libyuv::YUY2ToARGB(buffers[buf.index].start, CAM_WIDTH * 2,
				argbBuffer, CAM_WIDTH * 4,
				CAM_WIDTH, CAM_HEIGHT);
		blitSrc = argbBuffer;

		if (!faceBuf.filled && faceBuf.mutex.try_lock()) 
		{
			// ARGB to BGRA8888
			libyuv::ARGBToBGRA(argbBuffer, CAM_WIDTH * 4,
					faceBuf.buffer, CAM_WIDTH * 4,
					CAM_WIDTH, CAM_HEIGHT);

			faceBuf.filled = true;
			faceBuf.mutex.unlock();

			// signal to face detect thread
			CV.notify_one();
		} else {
#ifdef LCD_DISPLAY_ENABLE			
			// directly blit to screen fb
			blitToFB(fbBase + fbIndex * fbBufSize,
					var.xres, var.yres,
					blitSrc,
					CAM_WIDTH, CAM_HEIGHT);
			var.yoffset = var.yres * fbIndex;
			ioctl(fbfd, FBIOPAN_DISPLAY, &var);
			fbIndex = !fbIndex;
#endif			
		}

		// QBUF return the used buffer back
		if (ioctl(v4l2fd, VIDIOC_QBUF, &buf) < 0) {
			cerr << "VIDIOC_QBUF failed" << endl;
			break;
		}
	}

	v4l2CaptureStop(v4l2fd);
#ifdef LCD_DISPLAY_ENABLE	
	fbSinkUninit(fbfd, fbBase, fbBufSize * 2);
#endif
}

int main(int argc, char **argv)
{
	faceBuf.filled = false;

	// create thread for face detection
	detectThread = new std::thread(faceDetectProc);
	// v4l2 preview thread 
	previewThread = new std::thread(camPreviewProc);
	
#if CFG_ENABLE_APP_EXIT_FROM_TIMER
	sig_timer_start(CFG_REG_APP_EXIT_TIMEOUT_MS, face_register_run_timer_cb);
#endif

	detectThread->join();
	// stop the preview first
	previewRun = false;
	previewThread->join();

	delete detectThread;
	delete previewThread;

	return 0;
}

