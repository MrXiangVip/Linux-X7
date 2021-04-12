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
#include "libyuv.h"
// #include "libyuv/convert_argb.h"
#include "hl_MsgQueue.h"  

#include "bmp_registering.cpp"
#include "bmp_registered.cpp"
#include "bmp_registerok.cpp"
#include "bmp_registerfail.cpp"
#include "bmp_livenessfail.cpp"
#include "custom.h"
#include "face_comm.h"
#include "function.h"

#include "FIT.h"
#include <string>
#include <vector>

#include "FitApiHandle.h"
#include "pfmgr.h"
using namespace std;

#define FRAME_SIZE_INBYTE (CAM_WIDTH * CAM_HEIGHT)
#define GRAY_TO_RGBA_BPP 4
#define CFG_PER_TIME_CAL_OVERALL		1
//#define DEBUG_TIME	1

struct faceBuffer
{ 
	unsigned char color_buf[FRAME_SIZE_INBYTE * 3];
	std::mutex color_mutex;
	bool color_filled;
};
static struct faceBuffer faceBuf;
static struct v4l2buffer buffers[V4L2_BUF_NUM];
static uint32_t v4l2_dev_num = 0;

static unsigned char grayBuffer[FRAME_SIZE_INBYTE];
static unsigned int camOutFmt = V4L2_PIX_FMT_RGB565;

static std::mutex mtx;
static std::condition_variable CV;
static std::thread *detectThread;
static std::thread *previewThread;

static bool detectRun = true;
static bool previewRun = true;
static int32_t i2c_fd;
static bool LcdOpenOK_flg = false;    // LCD  打开成功与否的标记

static uint32_t banner_address_1 = 0;
static uint32_t banner_address_2 = 0;
static std::mutex banner_mutex;
void* imgbuf = NULL;


#if CFG_ENABLE_APP_EXIT_FROM_TIMER
static void face_recgnize_run_timer_cb(void)
{
	printf("app run Timeout, exit!\n");
	detectRun = false;

	// 发送关机指令给MCU
	cmdCloseFaceBoardReq();

	static unsigned char Num = 0;
	if((++Num) > 0)
	{
		usleep(100000);
		exit(-1);
	}
}
#endif

static void faceDetectProc(void)
{
	int recogtime = 0;
	uUID uu_id;
	
    OpenLcdBackgroud();
	int ret = FitlibInit();
	if (ret) {
		cout << "FitlibInit() failed!" << endl;
		return;
	}

	while (detectRun) 
	{
		std::unique_lock<std::mutex> lck(mtx);
		CV.wait(lck, []{ return (faceBuf.color_filled); });
		// lock buffer to do face algorithm
		faceBuf.color_mutex.lock();  

#if CFG_PER_TIME_CAL_OVERALL
		per_calc_start();
#endif
		// enable the log by cout
		cout.clear();
		/* 返回值:1 ~ n-1 表示识别通过的ID, -1表示未初始化, -2 表示人脸检测失败,-3表示识别不通过,-8表示活体检测不通过*/
		ret = recognizeFace(faceBuf.color_buf, &uu_id, &g_person_id);
		faceBuf.color_filled = false;
		faceBuf.color_mutex.unlock();	
#if CFG_PER_TIME_CAL_OVERALL
		per_calc_end();
		log_set_color(1);
		log_info("Fit_Detect used:<%d us>\n", per_calc_get_result());
#endif
		
		if (ret >=0) 
		{ 
			log_info("recg uuid: H<0x%08x>, L<0x%08x>, UID %s.\n", uu_id.tUID.H, uu_id.tUID.L, uu_id.UID);
			log_info("Face Detect OK! PersonID<%d>\n", g_person_id);
			cmdOpenDoorReq(uu_id);
			memcpy(g_uu_id.UID, uu_id.UID, UUID_LEN);
			break;
		}
		else if (ret == -1)
		{
            log_error("FIT lib has not been init ok.\n");
			break;
		}
		else{
            log_info("--Face Detect FAILED -!\n");

		}
		 recogtime ++;
		 log_debug("recgonize %d times\n", recogtime);
		 // 超时退出
		 if(recogtime > 30)
		 {
		 	log_info("===Recogize failed.\n");
			/*失败则直接关机, 如果成功则不能关机,还需要后续上传记录等处理*/
			cmdCloseFaceBoardReq();
			break;
		 }		 
	}
	
	CloseLcdBackgroud();
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
	fmt.fmt.pix.width = CAM_WIDTH * 2;
	fmt.fmt.pix.height = CAM_HEIGHT;
	cout << "width * height " << CAM_WIDTH * 2 << " * " << CAM_HEIGHT << endl;
	if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0) 
	{
		cerr << "set format failed:" << ret << endl;
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
	if (ioctl(v4l2fd, VIDIOC_REQBUFS, &req) < 0)   // 申请缓冲区,内存映射
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
#elif defined(SPI_LCD_ST7789V)
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

static void AddPowerLogo(void* pSrc,int nSrcWidth,int nSrcHeight,int nSrcBits,void*pLogo,int nLogoWidth,int nLogoHeight,int nLogoBits,int x,int y)
{
#ifdef MIPI_LCD_ST7701S
	unsigned char *pInputTmp = (unsigned char *)pSrc+y*nSrcWidth*4;
	int32_t i = 0;
	myWriteData2File("logo.raw", (unsigned char*)pLogo, 480*80*4);	  
	myWriteData2File("rgba_1.raw", (unsigned char*)pSrc, 480*640*4);	
	
	for (i = 0; i< nLogoHeight; ++i) 
	{
		pInputTmp += x*4;
		memcpy(pInputTmp, pLogo, nLogoWidth*4);		
		pInputTmp += (nSrcWidth-x)*4;
	}
	
	myWriteData2File("rgba_2.raw", (unsigned char*)pSrc, 480*640*4);	
#endif
}

static void camPreviewProc()
{
	int v4l2fd = 0, fbfd, xres, yres;
	unsigned char *fbBase = NULL, *blitSrc = NULL;
	struct v4l2_buffer buf;
	struct fb_var_screeninfo var;
	int fbIndex = 0, fbBufSize = 0;
	unsigned char LocalBuffer[FRAME_SIZE_INBYTE * 4]={0}; //Datalen(viu) = 2*picDataLen(yuv422) = FRAME_SIZE_INBYTE * 4
	uint8 rgble_data[CAM_WIDTH*CAM_HEIGHT*2]={0};
	unsigned char rgb_data[CAM_WIDTH*CAM_HEIGHT*3]={0};
	unsigned char rgbr_data[CAM_HEIGHT*CAM_WIDTH*3]={0};

	// open and setup the video capture dev 
	if(!v4l2CaptureInit(&v4l2fd))
	{
		cout << "Err: v4l2CaptureInit failed!" << endl;
		return;
	}
	cout << "v4l2 capture init success!" << endl;
#ifdef LCD_DISPLAY_ENABLE
	unsigned char rgba_data_qvga[CAM_WIDTH*CAM_HEIGHT*4]={0};
	unsigned char rgba_data_vga[CAM_WIDTH*4*CAM_HEIGHT*4]={0};
	unsigned char rgbar_data[CAM_HEIGHT*CAM_WIDTH*4]={0};
	unsigned char rgb565_data[CAM_HEIGHT*CAM_WIDTH*2]={0};
	// open and setup the framebuffer for preview 
	if(!fbSinkInit(&fbfd, &fbBase, &var)) 
	{
		cout << "Err: fbSinkInit failed!" << endl;
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
		cout << "v4l2CaptureStart error" << endl;
		close(v4l2fd);
#ifdef LCD_DISPLAY_ENABLE
		fbSinkUninit(fbfd, fbBase, fbBufSize * 2);
#endif
		return;
	}
	cout << "v4l2CaptureStart success" << endl;

	int frame = 0;
	while (previewRun) 
	{
		if(stSystemCfg.flag_auto_recg && faceBuf.color_filled)
		{
			usleep(3000);
			continue;
		}

		// DQBUF for ready buffer
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(v4l2fd, VIDIOC_DQBUF, &buf) < 0) //获取有已经采集到数据的buf 出列
		{
			cerr << "VIDIOC_DQBUF failed" << endl;
			break;
		}
		else
		{
			log_debug("Frame[%d]\n", ++frame);
		}

#if DEBUG_TIME
		struct timeval tv_rs_start;
		struct timeval tv_rs_end;

		uint32_t total_us = 0;
		gettimeofday(&tv_rs_start, NULL);
#endif

		//copy v4l2  memory of mmaping to memory of user space
		memset(LocalBuffer, 0, sizeof(LocalBuffer));
		memcpy(LocalBuffer, buffers[buf.index].start, FRAME_SIZE_INBYTE * 4);
#if DEBUG_TIME
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "memorycopy: " << total_us << " usec" << endl;
#endif
		// delete zero which added by viu
		int k =0, j = 0;
		for(j=0; j<CAM_WIDTH*CAM_HEIGHT * 4; j+=32, k+=16)	// 266f/30sec
		{
			LocalBuffer[k] = LocalBuffer[j];
			LocalBuffer[k+1] = LocalBuffer[j+2];
			LocalBuffer[k+2] = LocalBuffer[j+4];
			LocalBuffer[k+3] = LocalBuffer[j+6];
			LocalBuffer[k+4] = LocalBuffer[j+8];
			LocalBuffer[k+5] = LocalBuffer[j+10];
			LocalBuffer[k+6] = LocalBuffer[j+12];
			LocalBuffer[k+7] = LocalBuffer[j+14];
			LocalBuffer[k+8] = LocalBuffer[j+16];
			LocalBuffer[k+9] = LocalBuffer[j+18];
			LocalBuffer[k+10] = LocalBuffer[j+20];
			LocalBuffer[k+11] = LocalBuffer[j+22];
			LocalBuffer[k+12] = LocalBuffer[j+24];
			LocalBuffer[k+13] = LocalBuffer[j+26];
			LocalBuffer[k+14] = LocalBuffer[j+28];
			LocalBuffer[k+15] = LocalBuffer[j+30];
		}
		//myWriteData2File("test_rgb565.raw", LocalBuffer, 240*320*2);
#if DEBUG_TIME
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "zerocut: " << total_us << " usec" << endl;
#endif

		//Rgb565 big endpoint to litte endpoint
		Rgb565BE2LE((uint16_t*)LocalBuffer, (uint16_t*)rgble_data, CAM_WIDTH * CAM_HEIGHT);
#if DEBUG_TIME
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "be2le: " << total_us << " usec" << endl;
#endif
		RGB565ToRGB((unsigned short *)rgble_data, CAM_WIDTH * CAM_HEIGHT, rgb_data);
		//Rotate 90��
		RotateRGB(rgb_data, rgbr_data, CAM_WIDTH, CAM_HEIGHT, 1);
		memcpy(faceBuf.color_buf,
				rgbr_data,
				sizeof(faceBuf.color_buf));
		
		faceBuf.color_filled = 1;
		faceBuf.color_mutex.unlock();
		// signal to face detect thread
		CV.notify_one();

#if DEBUG_TIME
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "rotate rgb: " << total_us << " usec" << endl;
#endif
#ifdef LCD_DISPLAY_ENABLE
		if(LcdOpenOK_flg)
		{
	#ifdef MIPI_LCD_ST7701S
			RGB565ToRGBA((unsigned short *)rgble_data, CAM_WIDTH * CAM_HEIGHT, rgba_data_qvga);
			libyuv::ARGBRotate(rgba_data_qvga, 1280, rgbar_data, 960, 320, 240, libyuv::kRotate90); 
			ScaleRGB(rgba_data_vga, 480, 640, 32, rgbar_data, 240, 320, 32);
			blitSrc = rgba_data_vga;
			blitToFB(fbBase + fbIndex * fbBufSize,
					var.xres, var.yres,
					blitSrc,
					CAM_HEIGHT*2, CAM_WIDTH*2);
	#elif defined(SPI_LCD_ST7789V)
		#if 1
			// 转换rgb图像为伪彩色
			unsigned char *bgrColor_data=NULL;
			int heigth = 320, width = 240,	channel = 3;
			int &colorWidth = width,  colorHeight = heigth,  colorChannels =channel;
			//myWriteData2File("test_rgb888.raw", rgbr_data, 240*320*3);	
			g_face.InfraredTranformColor((byte *)rgbr_data, width, heigth, channel, 	\
								(byte **)&bgrColor_data, colorWidth, colorHeight, colorChannels);
			//myWriteData2File("test_rgbColor.raw", bgrColor_data, 240*320*3);	
			RGB888ToRGB565((void *) bgrColor_data,CAM_HEIGHT, CAM_WIDTH, (void *) rgb565_data);
			if(NULL != bgrColor_data)
			{
				free(bgrColor_data);
				bgrColor_data = NULL;
			}
		#else
			RGB888ToRGB565((void *) rgbr_data,CAM_HEIGHT, CAM_WIDTH, (void *) rgb565_data);
			//myWriteData2File("test_rgb565.raw", rgb565_data, 240*320*2);	
		#endif
			blitSrc = rgb565_data;
			blitToFB(fbBase + fbIndex * fbBufSize,
					var.xres, var.yres,
					blitSrc,
					CAM_HEIGHT, CAM_WIDTH);
	#endif
		}
#endif
		// QBUF return the used buffer back
		if (ioctl(v4l2fd, VIDIOC_QBUF, &buf) < 0) //重新返回buf 入队
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

	if(argc >= 2)
		sscanf(argv[1], "%d", &v4l2_dev_num);

	if(!p_person_face_mgr.loadPersonFaceMgr())
	{
		printf("ERR: loadPersonFaceMgr Failed!\n");
#if 0
		sleep(1);
		/* 关机 */
		cmdCloseFaceBoardReq();
		return -1;
#endif
	}
	log_debug("persondb size: %d\n", p_person_face_mgr.m_person_faces.size());

	// Set the log level
	GetSysLogCfg(&stSysLogCfg);
	log_set_level(stSysLogCfg.log_level);
	log_set_simple(1);
	
	// get global local cfg from file
	GetSysLocalCfg(&stSystemCfg);

	// create thread for face detection
	detectThread = new std::thread(faceDetectProc);
	// v4l2 preview thread 
	previewThread = new std::thread(camPreviewProc);

#if CFG_ENABLE_APP_EXIT_FROM_TIMER
	sig_timer_start(20000, face_recgnize_run_timer_cb);
#endif
	
	/* 下面两个join()不能调换位置,因为主线程等待人脸探测线程的结果后就可以
	stop preview线程, 最后主线程等待preview 线程退出,防止主线程先退出*/
	detectThread->join();
	// stop the preview first
	previewRun = false;
	previewThread->join();

	delete detectThread;
	delete previewThread;

	if(imgbuf != NULL)
	{
		free(imgbuf);
		imgbuf = NULL;
	}
	return 0;
}

