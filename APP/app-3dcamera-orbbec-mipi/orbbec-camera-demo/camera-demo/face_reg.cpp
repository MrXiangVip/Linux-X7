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

#include "bmp_registering.cpp"
#include "bmp_registered.cpp"
#include "bmp_registerok.cpp"
#include "bmp_registerfail.cpp"
#include "bmp_livenessfail.cpp"
#include "custom.h"
#include "face_comm.h"
#include "function.h"
#include "sig_timer.h"
#include "per_calc.h"

using namespace std;

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

extern int personId; 

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

	switch (banner_type) {
	case BANNER_REGISTERING:
		bmp_data = registering_bmp;
		break;
	case BANNER_REGISTERED:
		bmp_data = registered_bmp;
		break;
	case BANNER_REGISTER_OK:
		bmp_data = registerok_bmp;
		break;
	case BANNER_REGISTER_FAIL: 
		bmp_data = registerfail_bmp;
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
static void face_register_run_timer_cb(void)
{
	printf("app run Timeout, exit!\n");
	detectRun = false;

	static unsigned char Num = 0;
	if((++Num) > 0)
	{
		usleep(200000);
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

static void faceDetectProc(void)
{
	bool CreatPersonOK_Flag = true;	
	rs_face RsFace;
	unsigned char RegAddFaceType=ADDFACE_NULL;		

	RSApiHandle rsHandle;
	int nPersonID = 0, len = 0, ret = 0; 
	int recogtime = 0, distance = 0;  
	unsigned char RegResp[4] = {0x41, 0x37, 0x02, 0xFF};  //'A' '7' 0x02 faceId	
	imageInfo color_img = {
		faceBuf.color_buf,
		CAM_WIDTH,
		CAM_HEIGHT,
		CAM_WIDTH * CAM_IMAGE_BPP,
	};
#ifdef LIVENESS_DETECT_SUPPORT
	imageInfo depth_img = {
		(unsigned char*)faceBuf.depth_buf,
		CAM_WIDTH,
		CAM_HEIGHT,
		CAM_WIDTH * CAM_DEPTH_IMAGE_BPP,
	};
#endif

	// enable the log by cout
	cout.clear();

	log_info("before rshandle init...\n");

	if(!rsHandle.Init(FACE_REG)) 
	{	
		cmdSysHardwareAbnormalReport(HW_BUG_ENCRYPTIC);
		cerr << "Failed to init the ReadSense lib" << endl;
		return;
	}
	else
	{
		log_debug("RS version: %s\n", rsGetRSFaceSDKVersion());
	}

	// disable the log by cout
//	cout.setstate(ios_base::failbit);
	
#ifdef LCD_DISPLAY_ENABLE	
	if (banner_address_1 && banner_address_2)   
		face_recognize_update_banner(BANNER_REGISTERING);
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

		// enable the log by cout
		cout.clear();
		
#ifdef LIVENESS_DETECT_SUPPORT  
		faceBuf.depth_mutex.lock();
		//printf("==enter rsHandle.RegisterFace_Depth...\n");
		
		log_info("************RegAddFaceType<%d>************\n", RegAddFaceType);				
		if (CreatPersonOK_Flag)
		{
			ret = rsHandle.RegisterFace_Depth(color_img, depth_img, CREAT_PERSON,  &RsFace, ADDFACE_NULL, &distance);  
		}
		else
		{
			ret = rsHandle.RegisterFace_Depth(color_img, depth_img, PERSON_ADD_FACE,  &RsFace, RegAddFaceType, &distance);  
		}
		
		faceBuf.depth_filled = 0;
		faceBuf.depth_mutex.unlock();
#else
		ret = rsHandle.RegisterFace(color_img);
#endif

		faceBuf.color_filled = 0;
		faceBuf.color_mutex.unlock();
		log_info("Face pitch<%f>,roll<%f>,yaw<%f>,score<%f>\n",RsFace.pitch, RsFace.roll, RsFace.yaw, RsFace.score);
		//cout << "Face width: " << RsFace.rect.width<< " height: " << RsFace.rect.height<< " top: " << RsFace.rect.top<< " left: " << RsFace.rect.left<< endl;

		if(distance > 700)
		{
			cmdFaceRegistRsp((uint16_t) ID_REG_DISTANSE_FAR);
			sleep(1);
		}
		else if(distance < 300)
		{
			cmdFaceRegistRsp((uint16_t) ID_REG_DISTANSE_NEAR);
			sleep(1);
		}
		
		if ((RS_RET_OK == ret) || ((RegAddFaceType>=RegAddFaceTypeNUM) && (recogtime > 12)) )
		{
			nPersonID = personId; 
			log_info("face registered success!personId<%d>\n", personId);					
			
#ifdef LCD_DISPLAY_ENABLE
			face_recognize_update_banner(BANNER_REGISTER_OK);
#endif
			cmdFaceRegistRsp((uint16_t) nPersonID);
			break;
		}
		else if ((RS_RET_PERSON_ADD_FACE == ret) && (RegAddFaceType<RegAddFaceTypeNUM)) 
		{
#if 0		
			char buf[64] = {0};
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "pic_reg_Depth_%d.raw", RegAddFaceType);
			myWriteData2File(buf, (unsigned char *)faceBuf.depth_buf, 2*CAM_WIDTH * CAM_HEIGHT);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "pic_reg_Ir_%d.raw", RegAddFaceType);
			myWriteData2File(buf, faceBuf.color_buf, CAM_WIDTH * CAM_HEIGHT);
#endif
			nPersonID = personId; 
			CreatPersonOK_Flag = false;
			RegAddFaceType++;
			log_info("For personID<%d> add a face!:RegAddFaceType<%d>", personId, RegAddFaceType);

			//send msg to M4
			//提示用户做出某种人脸摆拍，开始录入
			face_regisiter_detect_process_proc(RegAddFaceType);
			recogtime = 0;
			continue;
		}
		else if(RS_RET_NO_ANGLE == ret)
		{
			//send msg to M4
			//根据RsFace的值，提示当前人脸不达标原因
			rs_face *pRsFace = &RsFace;
			log_info("\n\n\RS_RET_NO_ANGLE   === RegAddFaceType<%d>\n" , RegAddFaceType);
			face_regisiter_detect_process_proc(RegAddFaceType);
		}
		else if (RS_RET_NO_QUALITY  == ret)
		{
#if 0		
			static int index= 0;
			char buf[64] = {0};
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "pic_reg_Depth_%d.raw", index);
			myWriteData2File(buf, (unsigned char *)faceBuf.depth_buf, 2*CAM_WIDTH * CAM_HEIGHT);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "pic_reg_Ir_%d.raw", index);
			myWriteData2File(buf, faceBuf.color_buf, CAM_WIDTH * CAM_HEIGHT);
			index++;
#endif
			//send msg to M4
			//根据RsFace的值，提示当前人脸不达标原因
			rs_face *pRsFace = &RsFace;
			log_info("\n\n\nRS_RET_NO_QUALITY   === RegAddFaceType<%d>\n" , RegAddFaceType);
			face_regisiter_detect_process_proc(RegAddFaceType);
		}
		else if (RS_RET_ALREADY_IN_DATABASE == ret) 
		{
			// face recogized ok
			cout << "face registered before!" << endl;
#ifdef LCD_DISPLAY_ENABLE			
			face_recognize_update_banner(BANNER_REGISTERED);
#endif
			cmdFaceRegistRsp((uint16_t) 0xffff);				
			break;
		}
		else if (RS_RET_NO_LIVENESS == ret) 
		{		
			log_info("liveness check failed!\n");
			cmdFaceRegistRsp((uint16_t) 0xff01);
		}
		else if (RS_RET_NO_FACE == ret) 
		{
			log_info("no face detected!\n");			
			cmdFaceRegistRsp((uint16_t) 0xff02);
		}
	
		recogtime ++;  
		//如果某个角度的人脸长期注册不上就跳过	
		if ((recogtime > 12) && (RegAddFaceType>0 && RegAddFaceType<RegAddFaceTypeNUM)) 
		{
			log_info("Warring: For personID[%d] added face<%d> failed!!!\n", personId,RegAddFaceType);
			RegAddFaceType++;
			log_info("For personID[%d] add another face<%d>!\n", personId , RegAddFaceType);

			//send msg to M4
			//提示用户做出某种人脸摆拍，开始录入
			face_regisiter_detect_process_proc(RegAddFaceType);
			recogtime = 0;
			continue;
		}
		if(recogtime > 25)
		{
#ifdef LCD_DISPLAY_ENABLE			
			face_recognize_update_banner(BANNER_REGISTER_FAIL);	
#endif		
			log_info("face registered fail\n");	
			cmdFaceRegistRsp((uint16_t) 0x0000);

			if (RegAddFaceType > 0)
			{
				rsHandle.DeleteFace(nPersonID);
				log_info("face deleted: id<%d>\n", nPersonID);	
			}
			
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

	//打开视频设备文件
	sprintf(v4l2dev, "//dev//video%d", v4l2_dev_num);
	if ((fd_v4l = open(v4l2dev, O_RDWR, 0)) < 0) 
	{
		cerr << "Err: unable to open " << v4l2dev << endl;
		return false;
	}

	//设置帧速率
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.capturemode = 0;
	parm.parm.capture.timeperframe.denominator = 15;    //设置采集帧率
	parm.parm.capture.timeperframe.numerator = 1;
	ret = ioctl(fd_v4l, VIDIOC_S_PARM, &parm);
	if (ret < 0) 
	{
		cerr << "VIDIOC_S_PARM failed:" << ret << endl;
		goto err;
	}

	//设置帧格式
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = camOutFmt;
	fmt.fmt.pix.width = CAM_WIDTH;
	fmt.fmt.pix.height = CAM_HEIGHT;
	if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0) 
	{
		cerr << "set format failed:" << ret << endl;
		goto err;
	}

	*pfd = fd_v4l;
	return true;

err:
	close(fd_v4l);
	detectRun = false;    //hannah added
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
	if (ioctl(v4l2fd, VIDIOC_REQBUFS, &req) < 0)   //申请帧缓冲，内存映射
	{
		cerr << "VIDIOC_REQBUFS failed" << endl;
		return false;
	}

	// QUERYBUF
	for (i = 0; i < V4L2_BUF_NUM; i++) 
	{
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(v4l2fd, VIDIOC_QUERYBUF, &buf) < 0) 
		{
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

		if (ioctl(v4l2fd, VIDIOC_QBUF, &buf) < 0) //入队
		{
			cerr << "VIDIOC_QBUF error" << endl;
			return false;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(v4l2fd, VIDIOC_STREAMON, &type) < 0) //开始采集流
	{
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
	int offsetx, x, y;
#if defined(MIPI_LCD_ST7701S)
	unsigned int *fbpixel;
	unsigned int *campixel;

	offsetx = (camwidth - fbwidth) / 2;
	fbpixel = (unsigned int*)fbbase; // bpp=32
	campixel = (unsigned int*)(v4lbase + offsetx * CAM_IMAGE_BPP);

	for (y = 0; y < camheight; y++) 
	{
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
	int v4l2fd = 0, fbfd, xres, yres;
	unsigned char *fbBase = NULL, *blitSrc = NULL;
	struct v4l2_buffer buf;
	struct fb_var_screeninfo var;
	int fbIndex = 0, fbBufSize = 0;

	cout << "camPreviewProc in" << endl;
	// open and setup the video capture dev 
	if(!v4l2CaptureInit(&v4l2fd))
	{
		cmdSysHardwareAbnormalReport(HW_BUG_CAMERA);
		return;
	}
	cout << "v4l2 capture init success!" << endl;
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
		cout << "fbSinkInit success" << endl;
		fbBufSize = var.xres * var.yres * var.bits_per_pixel / 8;
	}
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
	cout << "v4l2CaptureStart success" << endl;

	cout << "before preview run..." << endl;
	while (previewRun) 
	{
		// DQBUF for ready buffer
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(v4l2fd, VIDIOC_DQBUF, &buf) < 0) //获取有已经采集到数据的buf出队
		{
			cerr << "VIDIOC_DQBUF failed" << endl;
			break;
		}

		cur_stream_type = orbbec_parseFrame((uint16 *)buffers[buf.index].start);		

		switch (cur_stream_type) 
		{
			case STREAM_IR:
			{
				//cout << "it is an ir image,len<%d>" << buffers[buf.index].length<< endl;
				//myWriteData2File("IrStream.raw", buffers[buf.index].start, CAM_WIDTH * CAM_HEIGHT);
				/*if (STREAM_DEPTH == ret) 
				{
					//cout << "STREAM_IR:it is a depth image!" << endl;
					break;
				}
				else if (2 == ret) cout << "STREAM_IR:it is an ir image" << endl;
				else cout << "STREAM_IR:return number is: " << ret << endl;  */
		
				if (!faceBuf.color_filled && faceBuf.color_mutex.try_lock()) //faceBuf没有填充,并且没有锁定
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
					}

					// IR image to GRAY
					IRToGRAY((unsigned short *)buffers[buf.index].start, CAM_WIDTH * CAM_HEIGHT, grayBuffer);
					log_debug("convert from IR to GRAY successfully !\n");

					memcpy(faceBuf.color_buf,
							grayBuffer,
							sizeof(faceBuf.color_buf));
					faceBuf.color_filled = 1;
					faceBuf.color_mutex.unlock();

					// signal to face detect thread
					CV.notify_one();
				} 
				else 
				{
#ifdef LCD_DISPLAY_ENABLE
					if(LcdOpenOK_flg)
					{

						// directly blit to screen fb
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
							cout << "blit to fb end" << endl;
					#endif
					}
#endif
				}
				break;
			}

#ifdef LIVENESS_DETECT_SUPPORT
			case STREAM_DEPTH:
			{
				//cout << "it is an depth image,len:" << buffers[buf.index].length << endl;
				//myWriteData2File("DepthStream.raw", buffers[buf.index].start, buffers[buf.index].length);
				/*if (STREAM_DEPTH == ret) 
				{
					cout << "STREAM_DEPTH:it is a depth image!" <<">>>length="<<buffers[buf.index].length<<",offset="<<buffers[buf.index].offset<< endl;
					myWriteData2File("DepthStream.data", buffers[buf.index].start, buffers[buf.index].length);
				}
				else if (STREAM_IR == ret)
				{
					//cout << "STREAM_DEPTH:it is an ir image, not a depth, break!" << endl;
					break;  //marked for hw reason, to be restored
				}
				else cout << "STREAM_DEPTH:return number is: " << ret << endl;*/
		
				if (!faceBuf.depth_filled && faceBuf.depth_mutex.try_lock())
				{
					/*if (0 == orbbec_mipi_ctrl_switch_stream(i2c_fd, STREAM_IR))
					{
						cur_stream_type = STREAM_IR;
						printf("change frm_cap_status to ir success\n");
					}
					else cout << "change frm_cap_status to ir fail!" << endl;*/

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
		if (ioctl(v4l2fd, VIDIOC_QBUF, &buf) < 0) //重新返回buf入队
		{
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
		printf("Failed to open i2c device!\n");
		close(i2c_fd);
		return -1;
	}
	cout << "init orbbec mipi success" << endl;
/*
	if (orbbec_mipi_ctrl_switch_stream(i2c_fd, STREAM_IR)) {
		printf("Failed to set IR stream!\n");
		close(i2c_fd);
		return -1;
	}
	cout << "init orbbec_mipi to IR success" << endl;
*/

	// create thread for face detection
	detectThread = new std::thread(faceDetectProc);
	// v4l2 preview thread 
	previewThread = new std::thread(camPreviewProc);

#if CFG_ENABLE_APP_EXIT_FROM_TIMER
	sig_timer_start(CFG_REG_APP_EXIT_TIMEOUT_MS, face_register_run_timer_cb);
#endif
	
	/*下面两个join()不能调换位置，因为主线程等待人脸探测线程的结果后就
	可以stop preview线程，最后主线程等待preview线程退出，防止主线程先退出*/
	detectThread->join();
	// stop the preview first
	previewRun = false;
	previewThread->join();

	delete detectThread;
	delete previewThread;

	return 0;
}

