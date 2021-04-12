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
#include <linux/input.h>
#include <math.h> 

#include "RSApiHandle.h"
#include "libyuv.h"
#include "orbbec_mipi_ctrl.h"
#include "hl_MsgQueue.h"  

#include "bmp_recognized.cpp"
#include "bmp_recognizefail.cpp"
#include "bmp_recognizing.cpp"
#include "bmp_livenessfail.cpp"
#include "custom.h"
#include "face_comm.h"
#include "function.h"
#include "sig_timer.h"
#include "per_calc.h"

using namespace std;

#define CFG_PER_TIME_CAL_OVERALL		1
#define DEV_INIT_TIME					0

struct faceBuffer
{ 
	unsigned char color_buf[FRAME_SIZE_INBYTE];
	std::mutex color_mutex;
	bool color_filled;
#ifdef LIVENESS_DETECT_SUPPORT
	unsigned short depth_buf[DEPTH_FRAME_SIZE_INBYTE];
	std::mutex depth_mutex;
	bool depth_filled;
#endif
};

static struct v4l2buffer buffers[V4L2_BUF_NUM];
static struct faceBuffer faceBuf;
static uint32_t v4l2_dev_num = 0;
#ifdef LCD_DISPLAY_ENABLE
#if defined(MIPI_LCD_ST7701S)
	static unsigned char rgbaBuffer[FRAME_SIZE_INBYTE * GRAY_TO_RGBA_BPP];
#elif defined(SPI_LCD_MIAOYING)
	static unsigned char rgb565scaleBuffer[SPI_LCD_SIZE_INBYTE];
#endif
#endif
static unsigned char grayBuffer[FRAME_SIZE_INBYTE];
static unsigned int camOutFmt = V4L2_PIX_FMT_RGB565;

static std::mutex mtx;
static std::condition_variable CV;
static std::thread *detectThread;
static std::thread *previewThread;

static bool detectRun = true;
static bool previewRun = true;

static int32_t cur_stream_type = STREAM_IR;
static int32_t i2c_fd;
static bool LcdOpenOK_flg = false;    // LCD 打开成功与否的标记

static uint32_t banner_address_1 = 0;
static uint32_t banner_address_2 = 0;
static std::mutex banner_mutex;


// 0 - register
// 1 - register done
static void face_recognize_update_banner(int32_t banner_type)
{
	if(!LcdOpenOK_flg)
	{
		return;
	}
		
#ifdef SPI_LCD_MIAOYING
	return;
#endif
	const unsigned char *bmp_data = NULL;

	switch (banner_type) 
	{
	case BANNER_RECOGNIZED:
		bmp_data = recognized_bmp;
		break;
	case BANNER_RECOGNIZING:
		bmp_data = recognizing_bmp;
		break;
	case BANNER_RECOGNIZE_FAIL:
		bmp_data = recognize_fail_bmp;
		break;
	case BANNER_LIVENESS_FAIL:
		bmp_data = liveness_fail_bmp;
		break;
	default:
		break;
	}
#ifdef LCD_DISPLAY_ENABLE	
	banner_mutex.lock();
	#ifdef LCD_DEFINITION_480P
		memcpy((void *)banner_address_1, (void *)bmp_data, 480 * 50 * 4);
		memcpy((void *)banner_address_2, (void *)bmp_data, 480 * 50 * 4);
	#endif
	#ifdef LCD_DEFINITION_720P
		memcpy((void *)banner_address_1, (void *)bmp_data, 720 * 108 * 4); 
		memcpy((void *)banner_address_2, (void *)bmp_data, 720 * 108 * 4);
	#endif
	banner_mutex.unlock();
#endif
}

#if CFG_ENABLE_APP_EXIT_FROM_TIMER
static void face_recognize_run_timer_cb(void)
{
	log_warn("app run timeout, exit!\n");
	detectRun = false;

	static unsigned char Num = 0;
	if((++Num) > 0)
	{
		usleep(200000);
		exit(-1);
	}
}
#endif

static void faceDetectProc(void)
{
	bool IsMouseOpen = false;	
	RSApiHandle rsHandle;
	int faceId = 0, len = 0, i=0; 
	int faceCount = 0;
	int recogtime = 0, distance = 0;  
	imageInfo color_img = {
		faceBuf.color_buf,
		CAM_WIDTH,
		CAM_HEIGHT,
		CAM_WIDTH * CAM_IMAGE_BPP,
	};
#ifdef LIVENESS_DETECT_SUPPORT
	imageInfo depth_img = {
		(unsigned char *)faceBuf.depth_buf,
		CAM_WIDTH,
		CAM_HEIGHT,
		CAM_WIDTH * CAM_DEPTH_IMAGE_BPP,
	};
#endif

	// enable the log by cout
	//cout.clear();
	
#if CFG_PER_TIME_CAL_OVERALL
	per_calc_start();
#endif
	for(i=0; i<10; i++)
	{	
		if(!rsHandle.Init(FACE_RECG)) 
		{
			cmdSysHardwareAbnormalReport(HW_BUG_ENCRYPTIC);
			log_warn("Failed to init the ReadSense lib\n");
			usleep(50000);
		}
		else
		{
			break;
		}
	
		if(i>= 9)			
			return;
	}
#if CFG_PER_TIME_CAL_OVERALL
	per_calc_end();
	log_set_color(1);
	log_info("RS_Init OK!used<%d us>\n", per_calc_get_result());
#endif	
	
#ifdef LCD_DISPLAY_ENABLE	
	if (banner_address_1 && banner_address_2)
		face_recognize_update_banner(BANNER_RECOGNIZING);		
#endif

	while (detectRun) 
	{
		std::unique_lock<std::mutex> lck(mtx);
		// wait for buffer ready
#ifdef LIVENESS_DETECT_SUPPORT
		CV.wait(lck, []{ return ((faceBuf.color_filled) && (faceBuf.depth_filled)); });
#else
		CV.wait(lck, []{ return (faceBuf.color_filled); });
#endif
		// lock buffer to do face algorithm
		faceBuf.color_mutex.lock();

#ifdef LIVENESS_DETECT_SUPPORT
		faceBuf.depth_mutex.lock();

#if CFG_PER_TIME_CAL_OVERALL
		per_calc_start();
#endif
		faceId = rsHandle.FaceDetect_Depth(color_img, depth_img, &IsMouseOpen, &distance, &faceCount);		
#if CFG_PER_TIME_CAL_OVERALL
		per_calc_end();
		log_set_color(1);
		log_info("RS_Detect OK! used:<%d us>\n", per_calc_get_result());
#endif

		faceBuf.depth_filled = 0;
		faceBuf.depth_mutex.unlock();
#else
		faceId = rsHandle.FaceDetect(color_img);
#endif
		faceBuf.color_filled = 0;
		faceBuf.color_mutex.unlock();
		
		if (faceId > 0)
		{
			// face recogized ok
			log_debug("face recogized: %d\n", faceId);
#ifdef LCD_DISPLAY_ENABLE
			face_recognize_update_banner(BANNER_RECOGNIZED);
#endif
			cmdFaceRecogizeRsp((uint16_t) faceId, IsMouseOpen, faceCount);
			break;
		} 
		else if (RS_RET_NO_FACE == faceId) 
		{
			usleep(40000); // skip 1 frame
		} 
		else if (RS_RET_NO_RECO == faceId)
		{
			// face recogized fail
			log_info("face OK but no record in DB!\n");
			recogtime+=2;
		} 
		else if (RS_RET_NO_LIVENESS == faceId) 
		{
			log_info("liveness check failed!\n");
#ifdef LCD_DISPLAY_ENABLE
			//face_recognize_update_banner(BANNER_LIVENESS_FAIL);
#endif
		}
		
		recogtime ++;
		if(recogtime > 12)
		{
			cmdFaceRecogizeRsp((uint16_t) 0x0000, 0, 0);
			
			log_info("face recogized fail!\n");
			recogtime = 0;

#ifdef LCD_DISPLAY_ENABLE			
			face_recognize_update_banner(BANNER_RECOGNIZE_FAIL);
#endif			
			break;
		}
	}
}

static bool v4l2CaptureInit(int *pfd)
{
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_frmsizeenum frmsize;
	char v4l2dev[100] = { 0 };
	int fd_v4l = 0, ret;

	sprintf(v4l2dev, "//dev//video%d", v4l2_dev_num);
	if ((fd_v4l = open(v4l2dev, O_RDWR, 0)) < 0) {
		log_error("Err:unable to open v4l2dev<%d>\n", v4l2dev);
		return false;
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.capturemode = 0;
	parm.parm.capture.timeperframe.denominator = 15;
	parm.parm.capture.timeperframe.numerator = 1;
	ret = ioctl(fd_v4l, VIDIOC_S_PARM, &parm);
	if (ret < 0) {
		log_error("VIDIOC_S_PARM failed:ret<%d>", ret);
		goto err;
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = camOutFmt;
	fmt.fmt.pix.width = CAM_WIDTH;
	fmt.fmt.pix.height = CAM_HEIGHT;
	if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0) {
		log_error("set format failed:ret<%d>\n", ret);
		goto err;
	}

	*pfd = fd_v4l;
	return true;

err:
	close(fd_v4l);
	detectRun = false;    
	return false;
}

static bool fbSinkInit(int *pfd, unsigned char **fbbase,
				struct fb_var_screeninfo *pvar)
{
	int fd_fb, fbsize, ret, i=0;
	const char *fb_dev = "/dev/fb0";
	struct fb_var_screeninfo var;

	do{
		if ((fd_fb = open(fb_dev, O_RDWR )) < 0) 
		{
			if(i>=5)
			{
				log_error("Unable to open frame buffer:i=%d\n", i);
				return false;
			}
			usleep(20);
		}
		else
		{
			break;
		}
	}while(i++);

	if (ioctl(fd_fb, FBIOGET_VSCREENINFO, &var) < 0) {
		log_error("FBIOPUT_VSCREENINFO failed\n");
		goto err;
	}

	// ping-pong buffer
	var.xres_virtual = var.xres;
	var.yres_virtual = 2 * var.yres;

	ret = ioctl(fd_fb, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0) {
		log_error("FBIOPUT_VSCREENINFO failed:ret<%d>\n", ret);
		goto err;
	}

	// Map the device to memory
	fbsize = var.xres * var.yres_virtual * var.bits_per_pixel / 8;
	log_debug("framebuffer info: (%d x %d), bpp:%d\n", var.xres,
		var.yres, var.bits_per_pixel);

	*fbbase = (unsigned char *)mmap(0, fbsize,
					PROT_READ | PROT_WRITE,
					MAP_SHARED, fd_fb, 0);
	if (MAP_FAILED == *fbbase) {
		log_error("Failed to map framebuffer device to memory\n");
		goto err;
	}

	/* Update ping-pong buffer */
	#ifdef LCD_DEFINITION_480P
		banner_address_1 = (uint32_t)(*fbbase + fbsize / 2 - var.xres * 100 * var.bits_per_pixel / 8);
		banner_address_2 = (uint32_t)(*fbbase + fbsize - var.xres * 100 * var.bits_per_pixel / 8);
	#endif
	#ifdef LCD_DEFINITION_720P
		banner_address_1 = (uint32_t)(*fbbase + fbsize / 2 - var.xres * 500 * var.bits_per_pixel / 8);
		banner_address_2 = (uint32_t)(*fbbase + fbsize - var.xres * 500 * var.bits_per_pixel / 8);	
	#endif

	*pvar = var;
	return true;

err:
	close(fd_fb);
	return false;
}

static void fbSinkUninit(int fd, unsigned char *fbbase, int fbsize)
{
	if(!LcdOpenOK_flg)
	{
		return;
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
		log_error("VIDIOC_REQBUFS failed\n");
		return false;
	}

	// QUERYBUF
	for (i = 0; i < V4L2_BUF_NUM; i++) {
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(v4l2fd, VIDIOC_QUERYBUF, &buf) < 0) {
			log_error("VIDIOC_QUERYBUF error\n");
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
	for (i = 0; i < V4L2_BUF_NUM; i++) {
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.length = buffers[i].length;
		buf.m.offset = buffers[i].offset;

		if (ioctl(v4l2fd, VIDIOC_QBUF, &buf) < 0) {
			log_error("VIDIOC_QBUF error\n");
			return false;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(v4l2fd, VIDIOC_STREAMON, &type) < 0) {
		log_error("VIDIOC_STREAMON error\n");
		return false;
	}

	return true;
}

static void blitToFB(unsigned char *fbbase,
			int fbwidth, int fbheight,
			unsigned char *v4lbase,
			int camwidth, int camheight)
{
	int offsetx, x, y;
#if defined(MIPI_LCD_ST7701S)
	unsigned int *fbpixel;
	unsigned int *campixel;

	offsetx = (camwidth - fbwidth) / 2;
	fbpixel = (unsigned int*)fbbase; // bpp=32
	campixel = (unsigned int*)(v4lbase + offsetx * CAM_IMAGE_BPP);

	for (y = 0; y < camheight; y++) {
		memcpy((void*)fbpixel, (void*)campixel, fbwidth * 4);
		campixel += camwidth;
		fbpixel += fbwidth;
	}
#elif defined(SPI_LCD_MIAOYING)
	unsigned short *fbpixel;
	unsigned short *campixel;

	offsetx = (camwidth - fbwidth) / 2;
	fbpixel = (unsigned short*)fbbase; // bpp=16
	campixel = (unsigned short*)(v4lbase + offsetx * CAM_IMAGE_BPP);

	for (y = 0; y < fbheight; y++) 
	{
		memcpy((void*)fbpixel, (void*)campixel, fbwidth * 2);
		campixel += camwidth;
		fbpixel += fbwidth;
	}
#endif
}

void IRToRGB565Scale(unsigned short *pInput, unsigned int nInputSize, unsigned char *pOutput)
{
#ifdef SPI_LCD_MIAOYING
	unsigned short *pInputTmp = pInput;
	unsigned char *pInputGray;
	for (unsigned int i = 0; i < nInputSize; i++)
	{
		*pInputGray = (unsigned char)((*pInputTmp >> 2) & 0xFF);
		*pOutput = ((*pInputGray << 3) & 0xE0) | ((*pInputGray >> 3) & 0x1F);
		*(pOutput + 1) = (*pInputGray & 0xF8) | ((*pInputGray >> 5) & 0x07);
		pOutput += 2;
		(SPI_LCD_WIDTH - 1 == i % SPI_LCD_WIDTH) ? (pInputTmp += (CAM_WIDTH + 2)) : (pInputTmp += 2);
	}
#endif
}

void IRToRGBA(unsigned short *pInput, unsigned int nInputSize, unsigned char *pOutput)
{
#ifdef MIPI_LCD_ST7701S
	unsigned short *pInputTmp = pInput;
	for (unsigned int i = 0; i < nInputSize; i++)
	{
		*pOutput = (unsigned char)((*pInputTmp >> 2) & 0xFF);
		*(pOutput + 1) = *pOutput;
		*(pOutput + 2) = *pOutput;
		*(pOutput + 3) = 0xFF;
		pOutput += 4;
		pInputTmp++;
	}
#endif
}

void IRToGRAY(unsigned short *pInput, unsigned int nInputSize, unsigned char *pOutput)
{
	unsigned short *pInputTmp = pInput;
	for (unsigned int i = 0; i < nInputSize; i++)
	{
		*pOutput = (unsigned char)((*pInputTmp >> 2) & 0xFF);
		pOutput++;
		pInputTmp++;
	}
}

static void camPreviewProc()
{
	int v4l2fd=0, fbfd=0, ret=0;
	unsigned char *fbBase, *blitSrc;
	struct v4l2_buffer buf;
	struct fb_var_screeninfo var;
	int fbIndex = 0, fbBufSize;
	
#if DEV_INIT_TIME
	per_calc_start();
#endif

	// open and setup the video capture dev 
	if(!v4l2CaptureInit(&v4l2fd))
	{
		cmdSysHardwareAbnormalReport(HW_BUG_CAMERA);
		return;
	}
#if DEV_INIT_TIME
		per_calc_end();
		log_set_color(1);
		log_info("v4l2CaptureInit over!used<%d>us\n", per_calc_get_result());
#endif	
	log_info("v4l2CaptureInit success\n");

#if DEV_INIT_TIME
	per_calc_start();
#endif
#ifdef LCD_DISPLAY_ENABLE
	// open and setup the framebuffer for preview 
	if(!fbSinkInit(&fbfd, &fbBase, &var)) 
	{	
		cmdSysHardwareAbnormalReport(HW_BUG_LCD);
		close(v4l2fd);
		//return;
		LcdOpenOK_flg = false;
	}
	else
	{
		LcdOpenOK_flg = true;
		log_info("fbSinkInit success\n");
		fbBufSize = var.xres * var.yres * var.bits_per_pixel / 8;
	}
#endif
#if DEV_INIT_TIME
	per_calc_end();
	log_set_color(1);
	log_info("fbSinkInit over!used<%d>us\n", per_calc_get_result());
#endif

#if DEV_INIT_TIME
		per_calc_start();
#endif
	// stream on
	if(!v4l2CaptureStart(v4l2fd)) 
	{
		close(v4l2fd);
#ifdef LCD_DISPLAY_ENABLE
		fbSinkUninit(fbfd, fbBase, fbBufSize * 2);
#endif
		return;
	}
	log_info("v4l2CaptureStart success\n");
#if DEV_INIT_TIME
	per_calc_end();
	log_set_color(1);
	log_info("v4l2CaptureStart used<%d>us\n", per_calc_get_result());
#endif

	while (previewRun) 
	{
		if(faceBuf.color_filled && faceBuf.depth_filled)
		{
			usleep(3000);
			continue;
		}
		
		// DQBUF for ready buffer
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(v4l2fd, VIDIOC_DQBUF, &buf) < 0) 
		{
			log_error("VIDIOC_DQBUF failed\n");
			break;
		}

		cur_stream_type = orbbec_parseFrame((uint16 *)buffers[buf.index].start);
		switch (cur_stream_type) 
		{
			case STREAM_IR:
			{
				log_debug("it is an ir image,len<%d>\n", buffers[buf.index].length);
				/*if (1 == ret) 
				{
					//cout << "it is a depth image, not an ir, break!" << endl;
					break;
				}
				else if (2 == ret) 
					cout << "it is an ir image" << endl;
				else 
					cout << "return number is: " << ret << endl;  */


				if (!faceBuf.color_filled && faceBuf.color_mutex.try_lock()) 
				{
/*#ifdef LIVENESS_DETECT_SUPPORT
					if (0 == orbbec_mipi_ctrl_switch_stream(i2c_fd, STREAM_DEPTH))
					{
						cur_stream_type = STREAM_DEPTH;
						printf("change frm_cap_status to depth success!\n");
					}
					else 
						cout << "change frm_cap_status to depth fail!" << endl;
#endif*/

					if(stSystemCfg.flag_faceBox)
					{
						getLastIrData(buffers[buf.index].start, buffers[buf.index].length);
						//myWriteData2File("pic_rcg_IR.raw", (unsigned char *)buffers[buf.index].start, buffers[buf.index].length);
					}

					// IR image to GRAY
					IRToGRAY((unsigned short *)buffers[buf.index].start, CAM_WIDTH * CAM_HEIGHT, grayBuffer);

					memcpy(faceBuf.color_buf,
							grayBuffer,
							sizeof(faceBuf.color_buf));

					faceBuf.color_filled = 1;
					faceBuf.color_mutex.unlock();
					CV.notify_one();

#ifdef LCD_DISPLAY_ENABLE
					// directly blit to screen fb					
					if(LcdOpenOK_flg)
					{
					#if defined(MIPI_LCD_ST7701S)
						// IR image to RGBA
						IRToRGBA((unsigned short *)buffers[buf.index].start, CAM_HEIGHT * CAM_WIDTH, rgbaBuffer);
						blitSrc = rgbaBuffer;
						//cout << "ir to rgba" << endl;

						blitToFB(fbBase + fbIndex * fbBufSize,
								var.xres, var.yres,
								blitSrc,
								CAM_WIDTH, CAM_HEIGHT);
						var.yoffset = var.yres * fbIndex;
						ioctl(fbfd, FBIOPAN_DISPLAY, &var);
						fbIndex = !fbIndex;
					#elif defined(SPI_LCD_MIAOYING)
						IRToRGB565Scale((unsigned short *)buffers[buf.index].start, var.xres * var.yres, rgb565scaleBuffer);
						blitSrc = rgb565scaleBuffer;
						//cout << "ir to rgb565 scale" << endl;

						blitToFB(fbBase + fbIndex * fbBufSize,
								var.xres, var.yres,
								blitSrc,
								SPI_LCD_WIDTH, SPI_LCD_HEIGHT);
							//cout << "blit to fb end" << endl;
					#endif
					}
#endif
				} 

				break;
			}

#ifdef LIVENESS_DETECT_SUPPORT
			case STREAM_DEPTH:
			{
				log_debug("it is an depth image\n");
				/*ret = orbbec_parseFrame((uint16 *)buffers[buf.index].start);
				if (1 == ret) 
					cout << "it is a depth image!" << endl;
				else if (2 == ret)
				{
					//cout << "it is an ir image, not a depth, break!" << endl;
					break;  //marked for hw reason, to be restored
				}
				else 
					cout << "return number is: " << ret << endl;*/

				if (!faceBuf.depth_filled && faceBuf.depth_mutex.try_lock())
				{
					/*if (0 == orbbec_mipi_ctrl_switch_stream(i2c_fd, STREAM_IR))
					{
						cur_stream_type = STREAM_IR;
						printf("change frm_cap_status to ir success\n");
					}
					else 
						cout << "change frm_cap_status to ir fail!" << endl;*/

					orbbec_mipi_ctrl_shift_to_depth((uint16_t *)buffers[buf.index].start, (uint16_t *)faceBuf.depth_buf, CAM_WIDTH, CAM_HEIGHT);
					faceBuf.depth_filled = true;
					faceBuf.depth_mutex.unlock();
					CV.notify_one();
				}

				break;
			}
#endif
		default:
			break;
		}

		// QBUF return the used buffer back
		if (ioctl(v4l2fd, VIDIOC_QBUF, &buf) < 0) 
		{
			log_error("VIDIOC_QBUF failed\n");
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
	faceBuf.color_filled = false;
#ifdef LIVENESS_DETECT_SUPPORT
	faceBuf.depth_filled = false;
#endif

	if(argc >= 2)
		sscanf(argv[1], "%d", &v4l2_dev_num);

	// Set the log level
	GetSysLogCfg(&stSysLogCfg);
	log_set_level(stSysLogCfg.log_level);
	log_set_simple(1);

	// get global local cfg from file
	GetSysLocalCfg(&stSystemCfg);

	i2c_fd = orbbec_mipi_ctrl_init(MIPI_I2C_DEV, MX6000_SLAVE_ADDR);
	if (i2c_fd < 0) 
	{
		cmdSysHardwareAbnormalReport(HW_BUG_CAMERA);
		log_error("Failed to open i2c device!\n");
		close(i2c_fd);
		return -1;
	}
	log_info("init orbbec_mipi success\n");
/*
	// init to IR stream
	if (orbbec_mipi_ctrl_switch_stream(i2c_fd, STREAM_IR)) {
		printf("Failed to set IR stream!\n");
		close(i2c_fd);
		return -1;
	}
	cout << "init orbbec_mipi to IR success" << endl;
*/
	// disable the log by cout
	//cout.setstate(ios_base::failbit);

	// create thread for face detection
	detectThread = new std::thread(faceDetectProc);
	// v4l2 preview thread 
	previewThread = new std::thread(camPreviewProc);
	
#if CFG_ENABLE_APP_EXIT_FROM_TIMER
	sig_timer_start(CFG_REC_APP_EXIT_TIMEOUT_MS, face_recognize_run_timer_cb);
#endif

	detectThread->join();
	delete detectThread;
	previewRun = false;
	previewThread->join();
	delete previewThread;

	return 0;
}

