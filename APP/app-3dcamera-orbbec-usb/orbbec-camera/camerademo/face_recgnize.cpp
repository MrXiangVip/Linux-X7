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
#include <OpenNI.h>
#include "OniSampleUtilities.h"  //should mark in yocto build

#include "RSApiHandle.h"
#include "libyuv.h"

#include "bmp_registering.cpp"
#include "bmp_registered.cpp"
#include "bmp_registerok.cpp"
#include "bmp_registerfail.cpp"
#include "bmp_recognized.cpp"
#include "bmp_recognizefail.cpp"
#include "bmp_recognizing.cpp"
#include "bmp_livenessfail.cpp"

//#define PER_TIME_CAL

#ifdef PER_TIME_CAL
#include <sys/time.h>
#endif

#define VIU_OUTFMT_YUV
//#define LCD_DISPLAY_ENABLE  //hannah added

#define CAM_WIDTH 640
#define CAM_HEIGHT 480
#define CAM_IMAGE_BPP 4 // BGRA8888
#define CAM_DEPTH_WIDTH 640
#define CAM_DEPTH_HEIGHT 400
#define CAM_DEPTH_IMAGE_BPP 2
#define V4L2_BUF_NUM 8
#define FRAME_SIZE_INBYTE (CAM_DEPTH_WIDTH*CAM_DEPTH_HEIGHT*CAM_IMAGE_BPP)
#define DEPTH_FRAME_SIZE_INBYTE (CAM_DEPTH_WIDTH*CAM_DEPTH_HEIGHT*CAM_DEPTH_IMAGE_BPP)

// Cut macros
// For both top and bottom
#define COLOR_TO_DEPTH_CUT_HEIGHT	((CAM_HEIGHT - CAM_DEPTH_HEIGHT) >> 1)

#define SAMPLE_READ_WAIT_TIMEOUT 2000 //2000ms

#define KEY_EVENT_DEV_NAME		"/dev/input/event0"

using namespace std;
using namespace openni;

static bool detectRun = true;
static bool previewRun = true;
static bool livenessRun = true;
extern int personId; //for face register

struct faceBuffer
{
	unsigned char color_buf[FRAME_SIZE_INBYTE];
	std::mutex color_mutex;
	bool color_filled;
#ifdef LIVENESS_DETECT_SUPPORT
	unsigned char depth_buf[DEPTH_FRAME_SIZE_INBYTE];
	std::mutex depth_mutex;
	bool depth_filled;
#endif
};

struct v4l2buffer
{
	unsigned char *start;
	size_t offset;
	unsigned int length;
};

enum {
	FACE_RECOGNIZE,
	FACE_REGISTER
};

#ifdef LIVENESS_DETECT_SUPPORT
enum {
	LIVENESS_INIT,
	LIVENESS_START,
	LIVENESS_CAPTURE,
	LIVENESS_STOP,
	LIVENESS_IDLE,
	LIVENESS_SHUTDOWN
};
#endif

enum {
	PREVIEW_INIT,
	PREVIEW_CAPTURE,
	PREVIEW_STOP,
	PREVIEW_IDLE,
	PREVIEW_SHUTDOWN
};

enum {
	FACE_DETECT_INIT,
	FACE_DETECT_RECOGNIZE_INIT,
	FACE_DETECT_RECOGNIZE,
	FACE_DETECT_REGISTER_INIT,
	FACE_DETECT_REGISTER,
	FACE_DETECT_STOP,
	FACE_DETECT_IDLE,
	FACE_DETECT_SHUTDOWN
};

enum {
	BANNER_REGISTERING,
	BANNER_REGISTER_OK,
	BANNER_REGISTERED,
	BANNER_RECOGNIZING,
	BANNER_RECOGNIZED,
	BANNER_RECOGNIZE_FAIL,
	BANNER_LIVENESS_FAIL,
	BANNER_REGISTER_FAIL  //hannah added
};

static struct v4l2buffer buffers[V4L2_BUF_NUM];
static struct faceBuffer faceBuf;
static uint32_t v4l2_dev_num = 0;
#ifdef VIU_OUTFMT_YUV
static unsigned char argbBuffer[FRAME_SIZE_INBYTE];
static unsigned int camOutFmt = V4L2_PIX_FMT_YUYV;
#else
static unsigned int camOutFmt = V4L2_PIX_FMT_ARGB32;
#endif
static std::mutex mtx;
static std::condition_variable CV;
static std::thread *detectThread;
static std::thread *previewThread;
static std::thread *livenessThread;

static Device cam_device;

static VideoStream dep_stream;
static VideoFrameRef dep_frame;

static int32_t face_op_status = FACE_RECOGNIZE;
static std::mutex face_op_mutex;
#ifdef LIVENESS_DETECT_SUPPORT
static int32_t liveness_op_status = LIVENESS_IDLE;
static std::mutex liveness_op_mutex;
#endif
static int32_t preview_op_status = PREVIEW_IDLE;
static std::mutex preview_op_mutex;
static int32_t face_det_op_status = FACE_DETECT_IDLE;
static std::mutex face_det_op_mutex;

static bool app_run_flag = true;  // false--no effect
static std::mutex app_run_flag_mutex;

#ifdef PER_TIME_CAL
struct timeval tv_rs_start;
struct timeval tv_rs_end;
#endif

static uint32_t banner_address_1 = 0;
static uint32_t banner_address_2 = 0;
static std::mutex banner_mutex;

// 0 - register
// 1 - register done
//static void face_recognize_update_banner(int32_t banner_type)
static int32_t face_recognize_update_banner(int32_t banner_type)
{
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
	case BANNER_REGISTER_FAIL: //hannah added
		bmp_data = registerfail_bmp;
		break;
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
	//memcpy((void *)banner_address_1, (void *)bmp_data, 480 * 50 * 4);
	//memcpy((void *)banner_address_2, (void *)bmp_data, 480 * 50 * 4);
	memcpy((void *)banner_address_1, (void *)bmp_data, 720 * 108 * 4); 
	memcpy((void *)banner_address_2, (void *)bmp_data, 720 * 108 * 4);
	banner_mutex.unlock();
#endif
	
	return 0;
}

#ifdef LIVENESS_DETECT_SUPPORT
static int32_t face_recognize_init_camera(Device *pcam, VideoStream *pvstream, int32_t *img_width, int32_t *img_height, SensorType type)
{
	Status rc;

	if (pcam->getSensorInfo(type) != NULL) {
		rc = pvstream->create(*pcam, type);
		if (rc == STATUS_OK) {
			*img_width = pvstream->getVideoMode().getResolutionX();
			*img_height = pvstream->getVideoMode().getResolutionY();
		} else {
			cerr << "Couldn't create stream: " << type << endl << OpenNI::getExtendedError() << endl;
			return -2;
		}

		rc = pcam->setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
		if (rc != STATUS_OK) {
			cerr << "Couldn't enable D2C: " << type << endl << OpenNI::getExtendedError() << endl;
		}
		return 0;
	} else {
		cerr << "Dont support stream: " << type << endl;
		return -3;
	}
}

static inline int32_t face_recognize_start_camera(VideoStream *pvstream)
{
	Status rc;

	rc = pvstream->start();
	if (rc != STATUS_OK) {
		cerr << "Couldn't start the stream!" << endl << OpenNI::getExtendedError() << endl;
		return -1;
	}
	return 0;
}

static inline int32_t face_recognize_stop_camera(VideoStream *pvstream)
{
	pvstream->stop();
	return 0;
}

static inline int32_t face_recognize_shutdown_camera(VideoStream *pvstream)
{
	pvstream->stop();
	pvstream->destroy();
	return 0;
}

static int32_t face_recognize_dummy_grab(VideoStream *pvstream)
{
	VideoFrameRef dummy_frame;
	Status rc;

	rc = pvstream->readFrame(&dummy_frame);
	if (rc != STATUS_OK) {
		cerr << "Read failed!" << endl << OpenNI::getExtendedError() << endl;
		return -1;
	}
	return 0;
}
#endif

static inline void face_recognize_change_face_detect_status(int32_t status)
{
	face_det_op_mutex.lock();
	face_det_op_status = status;
	face_det_op_mutex.unlock();
}

static inline int32_t face_recognize_get_face_detect_status(void)
{
	int32_t tmp_face_det_status = 0;

	face_det_op_mutex.lock();
	tmp_face_det_status = face_det_op_status;
	face_det_op_mutex.unlock();

	return tmp_face_det_status;
}

static inline void face_recognize_change_preview_status(int32_t status)
{
	preview_op_mutex.lock();
	preview_op_status = status;
	preview_op_mutex.unlock();
}

static inline int32_t face_recognize_get_preview_status(void)
{
	int32_t tmp_preview_status = 0;

	preview_op_mutex.lock();
	tmp_preview_status = preview_op_status;
	preview_op_mutex.unlock();

	return tmp_preview_status;
}

#ifdef LIVENESS_DETECT_SUPPORT
static inline void face_recognize_change_liveness_detect_status(int32_t status)
{
	liveness_op_mutex.lock();
	liveness_op_status = status;
	liveness_op_mutex.unlock();
}

static inline int32_t face_recognize_get_liveness_detect_status(void)
{
	int32_t tmp_liveness_status = 0;

	liveness_op_mutex.lock();
	tmp_liveness_status = liveness_op_status;
	liveness_op_mutex.unlock();

	return tmp_liveness_status;
}

#ifdef PER_TIME_CAL
static int32_t face_recognize_start_timer(int32_t ms)
{
	struct itimerval timer;
	time_t secs, usecs;
	secs = ms / 1000;
	usecs = ms % 1000 * 1000;

	timer.it_interval.tv_sec = secs;
	timer.it_interval.tv_usec = usecs;
	timer.it_value.tv_sec = secs;
	timer.it_value.tv_usec = usecs;

	setitimer(ITIMER_VIRTUAL, &timer, NULL);

	return 0;
}

static void face_recognize_stop_timer(void)
{
	face_recognize_start_timer(0);
}

static void face_recognize_signal_handler(int32_t sig_num)
{
	switch (sig_num) {
	case SIGALRM:
		break;
	case SIGVTALRM:
	//case SIGINT:
		cout << "Recognize timeout. Exiting." << endl;
		face_recognize_stop_timer();
		app_run_flag = false;
		break;
	default:
		break;
	}
}
#endif

static void livenessDetectProc(void)
{
	Status rc;
	int32_t img_size = 0;
//	bool livenessRun = true;  //hannah move out
	VideoStream *pdep_stream = &dep_stream;

	while (livenessRun) {
		int32_t tmp_liveness_status = 0;

		liveness_op_mutex.lock();
		tmp_liveness_status = liveness_op_status;
		liveness_op_mutex.unlock();

		switch (tmp_liveness_status) {
		case LIVENESS_INIT:
			{
				int32_t img_width = 0, img_height = 0;
				if (face_recognize_init_camera(&cam_device, pdep_stream, &img_width,
					&img_height, SENSOR_DEPTH)) {
					cerr << "Depth Camera init failed!" << endl;
					continue;
				}
				img_size = img_width * img_height * 2;
				face_recognize_change_liveness_detect_status(LIVENESS_START);
			}
			break;
		case LIVENESS_START:
			{
				if (face_recognize_start_camera(pdep_stream)) {
					cerr << "Depth Camera start failed!" << endl;
					face_recognize_change_liveness_detect_status(LIVENESS_INIT);
					continue;
				}
				face_recognize_change_liveness_detect_status(LIVENESS_CAPTURE);
			}
			break; //hannah added
		case LIVENESS_CAPTURE:
			{
				int32_t readyStream = -1;
				PixelFormat fmt;

				rc = OpenNI::waitForAnyStream(&pdep_stream, 1, &readyStream, SAMPLE_READ_WAIT_TIMEOUT);
				if (rc != STATUS_OK) {
					cerr << "Wait failed! (timeout is " << SAMPLE_READ_WAIT_TIMEOUT <<  "ms) " << endl
							<< OpenNI::getExtendedError() << endl;
					continue;
				}

				if ((1 == faceBuf.color_filled) && (0 == faceBuf.depth_filled) && faceBuf.depth_mutex.try_lock()) {
					rc = pdep_stream->readFrame(&dep_frame);

					if (rc != STATUS_OK) {
						cerr << "Read failed!" << endl << OpenNI::getExtendedError() << endl;
						faceBuf.depth_mutex.unlock();
						continue;
					}

					fmt = pdep_stream->getVideoMode().getPixelFormat();
					if (fmt != PIXEL_FORMAT_DEPTH_1_MM && fmt != PIXEL_FORMAT_DEPTH_100_UM) {
						cerr << "Unexpected frame format" << endl;
						faceBuf.depth_mutex.unlock();
						continue;
					}

					if (dep_frame.isValid()) {
						memcpy(faceBuf.depth_buf, dep_frame.getData(), img_size);
						faceBuf.depth_filled = 1;
						faceBuf.depth_mutex.unlock();
						CV.notify_one();
					}
				} else {
					face_recognize_dummy_grab(pdep_stream);
				}
			}
			break;
		case LIVENESS_STOP:
			face_recognize_stop_camera(&dep_stream);
			{
				// wait for start again
				std::unique_lock<std::mutex> lck(mtx);
				CV.wait(lck, []{ return (LIVENESS_STOP != face_recognize_get_liveness_detect_status()); });
			}
			break;
		case LIVENESS_IDLE:
			break;
		case LIVENESS_SHUTDOWN:
			face_recognize_shutdown_camera(&dep_stream);
			livenessRun = false;
			break;
		default:
			break;
		}
	}
}
#endif

static void faceDetectProc(void)
{
	RSApiHandle rsHandle;
	int faceId, len, fd, ret; 
	int recogtime = 0;  
	const char *rpmsgdev = "/dev/ttyRPMSG30";  	
	unsigned char RecgResp[4] = {0x41, 0x37, 0x01, 0x00};  //'A', '7', 0x01, faceId
	unsigned char RegResp[4] = {0x41, 0x37, 0x02, 0xFF};  //'A' '7' 0x02 faceId	
	imageInfo color_img = {
		faceBuf.color_buf,
		CAM_DEPTH_WIDTH,
		CAM_DEPTH_HEIGHT,
		CAM_DEPTH_WIDTH * CAM_IMAGE_BPP,
	};
#ifdef LIVENESS_DETECT_SUPPORT
	imageInfo depth_img = {
		faceBuf.depth_buf,
		CAM_DEPTH_WIDTH,
		CAM_DEPTH_HEIGHT,
		CAM_DEPTH_WIDTH * CAM_DEPTH_IMAGE_BPP,
	};
#endif
//	bool detectRun = true;  //hannah move out
	
	while (detectRun) {
		int32_t tmp_face_det_status = 0;

		face_det_op_mutex.lock();
		tmp_face_det_status = face_det_op_status;
		face_det_op_mutex.unlock();

//		cout << "tmp_face_det_status: " << tmp_face_det_status << endl;
		switch (tmp_face_det_status) {
		case FACE_DETECT_INIT:
			if(!rsHandle.Init()) {
				cerr << "Failed to init the ReadSense lib" << endl;
				return;
			}
		
//			cout << "FACE_DETECT_INIT done!" << endl;
			face_recognize_change_face_detect_status(FACE_DETECT_RECOGNIZE_INIT);
			break;
		case FACE_DETECT_RECOGNIZE_INIT:
			face_recognize_change_face_detect_status(FACE_DETECT_RECOGNIZE);
			if (banner_address_1 && banner_address_2)
				face_recognize_update_banner(BANNER_RECOGNIZING);		
			cout << "FACE_DETECT_RECOGNIZE_INIT done!" << endl;
			break;
		case FACE_DETECT_RECOGNIZE:
			{
				std::unique_lock<std::mutex> lck(mtx);
				// wait for buffer ready
#ifdef LIVENESS_DETECT_SUPPORT
				CV.wait(lck, []{ return ((faceBuf.color_filled) && (faceBuf.depth_filled)); });
#else
				CV.wait(lck, []{ return (faceBuf.color_filled); });
#endif
				// lock buffer to do face algorithm
#ifdef PER_TIME_CAL
				uint32_t total_us = 0;
				gettimeofday(&tv_rs_start, NULL);
#endif
				faceBuf.color_mutex.lock();
#ifdef LIVENESS_DETECT_SUPPORT
				faceBuf.depth_mutex.lock();
				face_recognize_change_liveness_detect_status(LIVENESS_STOP);
				faceId = rsHandle.FaceDetect_Depth(color_img, depth_img);
				face_recognize_change_liveness_detect_status(LIVENESS_START);
				faceBuf.depth_filled = 0;
				faceBuf.depth_mutex.unlock();
				// Wakeup liveness thread
				CV.notify_one();
#else
				faceId = rsHandle.FaceDetect(color_img);
#endif
//				cout << "RECOGNIZE--faceId: " << faceId << endl;
				faceBuf.color_filled = 0;
				faceBuf.color_mutex.unlock();
#ifdef PER_TIME_CAL
				gettimeofday(&tv_rs_end, NULL);
				total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
				cout << "RS: " << total_us << " usec" << endl;
#endif

				if (RS_RET_NO_FACE == faceId) {
					usleep(40000); // skip 1 frame
				} else if (RS_RET_NO_LIVENESS == faceId) {
					cout << "liveness check failed!" << endl;
					//face_recognize_update_banner(BANNER_LIVENESS_FAIL);
				} else if (RS_RET_NO_RECO == faceId) {
					//cout << "face recogized fail" << endl;
					cout << "face OK but no record in DB!" << endl;
					//face_recognize_update_banner(BANNER_RECOGNIZE_FAIL);
				} else if (faceId > 0) { //face recogized ok				
					cout << "face recogized success!" << endl;
					face_recognize_update_banner(BANNER_RECOGNIZED);
					//detectRun = previewRun = livenessRun = false;  //hannah added
					face_recognize_change_preview_status(PREVIEW_SHUTDOWN);
					face_recognize_change_face_detect_status(FACE_DETECT_SHUTDOWN);
				#ifdef LIVENESS_DETECT_SUPPORT
					face_recognize_change_liveness_detect_status(LIVENESS_SHUTDOWN);
				#endif
					RecgResp[3] = (unsigned char) faceId;
					len = sizeof(RecgResp);
					fd = open(rpmsgdev, O_RDWR);
					if(fd < 0)
					{
						printf("open /dev/ttyRPMSG30 failed\n");
					}
					else
					{
						ret = write(fd, (unsigned char *)RecgResp, len);
						if (ret < 0)  //190108
							printf("write /dev/ttyRPMSG30 failed!\n");	
						ret = close(fd);
						if (ret < 0)  //190108
							printf("close /dev/ttyRPMSG30 failed!\n");
					}
					break;
				} else {
					//usleep(60000); // skip one frame
				}
				
				recogtime ++; //hannah added
				if(recogtime > 18)
				{
					cout << "face recogized fail!" << endl;
					system("echo -ne \"A7\x01\xFF\" > /dev/ttyRPMSG30"); 	
					face_recognize_update_banner(BANNER_RECOGNIZE_FAIL);
					//detectRun = previewRun = livenessRun = false;  //hannah added
					face_recognize_change_preview_status(PREVIEW_SHUTDOWN);
					face_recognize_change_face_detect_status(FACE_DETECT_SHUTDOWN);
				#ifdef LIVENESS_DETECT_SUPPORT
					face_recognize_change_liveness_detect_status(LIVENESS_SHUTDOWN);
				#endif					
					break;
				}
				
			}  //end of case FACE_DETECT_RECOGNIZE {	
			break;
		case FACE_DETECT_REGISTER_INIT:
			face_recognize_change_face_detect_status(FACE_DETECT_REGISTER);
			if (banner_address_1 && banner_address_2)   //190325
				face_recognize_update_banner(BANNER_REGISTERING);			
			cout << "FACE_DETECT_REGISTER_INIT done!" << endl;			
#ifdef PER_TIME_CAL
			face_recognize_stop_timer();
#endif
			break;
		case FACE_DETECT_REGISTER:
			{
				RS_RET_VALUE ret;

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
				face_recognize_change_liveness_detect_status(LIVENESS_STOP);
				ret = rsHandle.RegisterFace_Depth(color_img, depth_img);
				face_recognize_change_liveness_detect_status(LIVENESS_START);
				faceBuf.depth_filled = 0;
				faceBuf.depth_mutex.unlock();
				// Wakeup liveness thread
				CV.notify_one();
#else
				ret = rsHandle.RegisterFace(color_img);
#endif
				faceBuf.color_filled = 0;
				faceBuf.color_mutex.unlock();

				if (RS_RET_OK == ret) {
					faceId = personId;   //hannah added
					cout << "face registered success!" << endl;
					face_recognize_update_banner(BANNER_REGISTER_OK);
					//std::this_thread::sleep_for(std::chrono::milliseconds(1000));  //deleted by NXP
					face_recognize_change_preview_status(PREVIEW_SHUTDOWN);
					face_recognize_change_face_detect_status(FACE_DETECT_SHUTDOWN);
				#ifdef LIVENESS_DETECT_SUPPORT
					face_recognize_change_liveness_detect_status(LIVENESS_SHUTDOWN);
				#endif
					RegResp[3] = (unsigned char)faceId;
					len = sizeof(RegResp);
					fd = open(rpmsgdev, O_RDWR);  //open RPmsg virtual tty device		
					if(fd > 0)
					{
						write(fd, (unsigned char *)RegResp, len);
						close(fd);  //close fd after RW operation
					}
					else
						printf("open /dev/ttyRPMSG30 failed!\n");	

					break;
				} else if (RS_RET_ALREADY_IN_DATABASE == ret) {
//					cout << "face already exists in database!" << endl;
					face_recognize_update_banner(BANNER_REGISTERED);
					face_recognize_change_preview_status(PREVIEW_SHUTDOWN);
					face_recognize_change_face_detect_status(FACE_DETECT_SHUTDOWN);
				#ifdef LIVENESS_DETECT_SUPPORT
					face_recognize_change_liveness_detect_status(LIVENESS_SHUTDOWN);
				#endif						
					//system("echo -ne \"A7\x02\x00\" > /dev/ttyRPMSG30"); //result in "sh: syntax error: unterminated quoted string"
					RegResp[3] = 0x00; //(unsigned char)faceId;
					len = sizeof(RegResp);
					fd = open(rpmsgdev, O_RDWR);  //open RPmsg virtual tty device		
					if(fd > 0)
					{
						write(fd, (unsigned char *)RegResp, len);
						close(fd);  //close fd after RW operation
					}
					else
						printf("open /dev/ttyRPMSG30 failed!\n");					
					break;
				}
				else if (RS_RET_NO_LIVENESS == ret) {
					cout << "liveness check failed!" << endl;
				}
				
				recogtime ++;  //hannah added
				if(recogtime > 25)
				{
					cout << "face registered fail" << endl;
					face_recognize_update_banner(BANNER_REGISTER_FAIL);
					face_recognize_change_preview_status(PREVIEW_SHUTDOWN);
					face_recognize_change_face_detect_status(FACE_DETECT_SHUTDOWN);
				#ifdef LIVENESS_DETECT_SUPPORT
					face_recognize_change_liveness_detect_status(LIVENESS_SHUTDOWN);
				#endif					
					system("echo -ne \"A7\x02\xFF\" > /dev/ttyRPMSG30");					
					break;
				}
			}
			break;
		case FACE_DETECT_IDLE:
			break;
		case FACE_DETECT_SHUTDOWN:
#ifdef PER_TIME_CAL		
			face_recognize_stop_timer();
#endif			
			detectRun = false;
			break;
		default:
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
	struct v4l2_capability cap;
	char v4l2dev[100] = { 0 };
	int fd_v4l = 0, ret;

	sprintf(v4l2dev, "/dev/video%d", v4l2_dev_num);  
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
	detectRun = livenessRun = false;    //hannah added
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
	//banner_address_1 = (uint32_t)(*fbbase + fbsize / 2 - var.xres * 100 * var.bits_per_pixel / 8);
	//banner_address_2 = (uint32_t)(*fbbase + fbsize - var.xres * 100 * var.bits_per_pixel / 8);
	banner_address_1 = (uint32_t)(*fbbase + fbsize / 2 - var.xres * 500 * var.bits_per_pixel / 8);
	banner_address_2 = (uint32_t)(*fbbase + fbsize - var.xres * 500 * var.bits_per_pixel / 8);	

	*pvar = var;

	return true;
err:
	close(fd_fb);
	return false;
}

static void fbSinkUninit(int fd, unsigned char *fbbase, int fbsize)
{
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
	for (i = 0; i < V4L2_BUF_NUM; i++) {
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
	int offsetx, x, y;
	unsigned int *fbpixel;
	unsigned int *campixel;

	offsetx = (fbwidth - camwidth) / 2;  //(camwidth - fbwidth) / 2;  
	fbpixel = (unsigned int*)fbbase; 
	campixel = (unsigned int*)v4lbase;   //(v4lbase + offsetx * CAM_IMAGE_BPP);

	for (y = 0; y < camheight; y++) {
		//memcpy((void*)fbpixel, (void*)campixel, fbwidth * 4);
		memcpy((void*)(fbpixel+offsetx), (void*)campixel, camwidth * 4);
		campixel += camwidth;
		fbpixel += fbwidth;
	}
}

static void camPreviewProc(void)
{
	int v4l2fd = 0, fbfd, xres, yres;
	unsigned char *fbBase = NULL, *blitSrc = NULL;
	struct v4l2_buffer buf;
	struct fb_var_screeninfo var;
	int fbIndex = 0, fbBufSize = 0;
//	int32_t previewRun = true;

	while (previewRun) {
		int32_t tmp_preview_status = 0;

		preview_op_mutex.lock();
		tmp_preview_status = preview_op_status;
		preview_op_mutex.unlock();

		switch (tmp_preview_status) {
		case PREVIEW_INIT:
			// open and setup the video capture dev 
			if(!v4l2CaptureInit(&v4l2fd))
				return;
			//cout << "v4l2CaptureInit OK!" << endl;
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
			//cout << "v4l2CaptureStart OK!" << endl;
			//face_recognize_update_banner(BANNER_RECOGNIZING); //hannah deleted in190325
			face_recognize_change_preview_status(PREVIEW_CAPTURE);
			break;
		case PREVIEW_CAPTURE:
			{
				// DQBUF for ready buffer
				memset(&buf, 0, sizeof (buf));
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				if (ioctl(v4l2fd, VIDIOC_DQBUF, &buf) < 0) {
					cerr << "VIDIOC_DQBUF failed" << endl;
					break;
				}
				//cout << "1" << endl;
#ifdef VIU_OUTFMT_YUV
				// CSC from YUYV to ARGB
				// Trim to depth size
				libyuv::YUY2ToARGB(buffers[buf.index].start + COLOR_TO_DEPTH_CUT_HEIGHT * CAM_WIDTH * 2, CAM_WIDTH * 2,
				argbBuffer, CAM_WIDTH * 4,
				CAM_DEPTH_WIDTH, CAM_DEPTH_HEIGHT);
				blitSrc = argbBuffer;
#else
				blitSrc = buffers[buf.index].start;
#endif
				//cout << "2" << endl;
				if (!faceBuf.color_filled && faceBuf.color_mutex.try_lock()) {

#ifdef VIU_OUTFMT_YUV
					// ARGB to BGRA8888
					libyuv::ARGBToBGRA(argbBuffer, CAM_DEPTH_WIDTH * 4,
							faceBuf.color_buf, CAM_DEPTH_WIDTH * 4,
							CAM_DEPTH_WIDTH, CAM_DEPTH_HEIGHT);
#else
					memcpy(faceBuf.color_buf,
							buffers[buf.index].start,
							sizeof(faceBuf.color_buf));
#endif
				//cout << "3" << endl;
					faceBuf.color_filled = 1;
					faceBuf.color_mutex.unlock();
					CV.notify_one();
				} else {
#ifdef LCD_DISPLAY_ENABLE
					// directly blit to screen fb
					blitToFB(fbBase + fbIndex * fbBufSize,
							var.xres, var.yres,
							blitSrc,
							CAM_DEPTH_WIDTH, CAM_DEPTH_HEIGHT);
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
			break;
		case PREVIEW_STOP:
			v4l2CaptureStop(v4l2fd);
			fbSinkUninit(fbfd, fbBase, fbBufSize * 2);
			face_recognize_change_preview_status(PREVIEW_IDLE);
			break;
		case PREVIEW_IDLE:
			break;
		case PREVIEW_SHUTDOWN:
			v4l2CaptureStop(v4l2fd);
#ifdef LCD_DISPLAY_ENABLE
			fbSinkUninit(fbfd, fbBase, fbBufSize * 2);
#endif			
			face_recognize_change_preview_status(PREVIEW_IDLE);
			previewRun = false;
			break;
		default:
			break;
		}
	}
}

int main(int argc, char **argv)
{
	faceBuf.color_filled = false;
#ifdef LIVENESS_DETECT_SUPPORT
	faceBuf.depth_filled = false;
#endif
	int32_t l_ret = -1;
	int32_t key_fd = 0;
	Status rc = STATUS_ERROR;
	struct input_event key_event;

	if(argc >= 2)
		sscanf(argv[1], "%d", &v4l2_dev_num);

#ifdef KEY_SUPPORT  //hannah deleted
	// KEY_POWER
	memset(&key_event, 0, sizeof(struct input_event));
	key_fd = open(KEY_EVENT_DEV_NAME, O_RDONLY);
	if(key_fd < 0) {
		cout << "Error open key device: " << KEY_EVENT_DEV_NAME << endl;
		return -1;
	}
#endif

#ifdef LIVENESS_DETECT_SUPPORT
	rc = OpenNI::initialize();
	if (rc != STATUS_OK) {
		cerr << "Initialize failed " << OpenNI::getExtendedError() << endl;
		return -1;
	}

	rc = cam_device.open(ANY_DEVICE);
	if (rc != STATUS_OK) {
		cerr << "Couldn't open device" << OpenNI::getExtendedError();
		return -2;
	}
#endif

	//signal(SIGINT, face_recognize_signal_handler);

	// create thread for face detection
	detectThread = new std::thread(faceDetectProc);
	// v4l2 preview thread 
	previewThread = new std::thread(camPreviewProc);
	//
#ifdef LIVENESS_DETECT_SUPPORT
	livenessThread = new std::thread(livenessDetectProc);
#endif

	face_recognize_change_face_detect_status(FACE_DETECT_INIT);
#ifdef LIVENESS_DETECT_SUPPORT
	face_recognize_change_liveness_detect_status(LIVENESS_INIT);
#endif
	face_recognize_change_preview_status(PREVIEW_INIT);

#ifdef KEY_SUPPORT	//hannah deleted
	while (app_run_flag) {
		//l_ret = lseek(key_fd, 0, SEEK_SET);
		l_ret = read(key_fd, &key_event, sizeof(key_event));

		if (l_ret) {
			if (key_event.code == KEY_POWER && key_event.type == EV_KEY) {
				switch (key_event.value) {
				case 0:
					cout << "key " << key_event.code << " released" << endl;
					switch (face_op_status) {
					case FACE_RECOGNIZE:
						cout << "Switch to face register!" << endl;
#ifdef LIVENESS_DETECT_SUPPORT
						//face_recognize_change_liveness_detect_status(LIVENESS_STOP);
#endif
						face_recognize_change_face_detect_status(FACE_DETECT_REGISTER_INIT);
						face_op_status = FACE_REGISTER;
						break;
					case FACE_REGISTER:
						cout << "Switch to face recognize!" << endl;
						face_recognize_change_face_detect_status(FACE_DETECT_RECOGNIZE_INIT);
#ifdef LIVENESS_DETECT_SUPPORT
						//face_recognize_change_liveness_detect_status(LIVENESS_START);
#endif
						face_op_status = FACE_RECOGNIZE;
						break;
					default:
						break;
					}
					break;
				case 1:
					break;
				default:
					cout << "Other key event: " << key_event.value << endl;
					break;
				}
			}
		}
	}

	close(key_fd);

	// stop the preview first
	face_recognize_change_preview_status(PREVIEW_SHUTDOWN);
	face_recognize_change_face_detect_status(FACE_DETECT_SHUTDOWN);
#ifdef LIVENESS_DETECT_SUPPORT
	face_recognize_change_liveness_detect_status(LIVENESS_SHUTDOWN);
#endif

#endif //end of KEY_SUPPORT

	detectThread->join();
	delete detectThread;
	previewThread->join();
	delete previewThread;
#ifdef LIVENESS_DETECT_SUPPORT
	livenessThread->join();
	delete livenessThread;
#endif

	cam_device.close();
	OpenNI::shutdown();

	return 0;
}

