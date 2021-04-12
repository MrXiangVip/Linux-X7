#include <iostream>
#include "RSApiHandle.h"
#include "RSFaceSDK.h"
#include <sys/time.h>
#include <math.h>
#include "function.h"
#include <string.h>

//#define PER_TIME_CAL
#define LOG_USE_COLOR

#define DB_FILE_PATH "readsense.db"
#define LICENSE_FILE_PATH "f91d6d9b_arm_linux_chip_license_content.lic"

#ifdef PER_TIME_CAL
struct timeval tv_rs_start;
struct timeval tv_rs_end;
#endif

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
	  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif

int personId = -1;  //hannah move out for id transfer

static double perf_get_timestamp()
{
	struct timeval  tv;
	struct timezone tz;
	gettimeofday(&tv,&tz);

	double timemillsec = tv.tv_sec *1000 + tv.tv_usec/1000;
	return timemillsec;
}

bool RSApiHandle::GetInitCfgInfo(std::string filePath, std::string& initInfo)
{
	std::ifstream filestream(filePath.c_str());
	if (!filestream.is_open())
	{
		std::cerr << "open license failed" << std::endl;
		return false;
	}
	std::stringstream streambuf;
	streambuf << filestream.rdbuf();
	initInfo = streambuf.str();
	return true;
}

RSApiHandle::RSApiHandle()
{
	m_Plicense = NULL;
	m_pfaceDetect = NULL;
	m_PfaceRecognition = NULL;
	m_PfaceQuality = NULL;
#ifdef LIVENESS_DETECT_DEPTH_ENABLE
	m_PLivenessDetect = NULL;
#endif
}

RSApiHandle::~RSApiHandle()
{
	if(m_pfaceDetect != NULL)
	{
		rsUnInitFaceDetect(&m_pfaceDetect);
		m_pfaceDetect = NULL;
	}

	if(m_PfaceRecognition != NULL)
	{
		rsUnInitFaceRecognition(&m_PfaceRecognition);
		m_PfaceRecognition = NULL;
	}

	if(m_PfaceQuality != NULL)
	{
		rsUnInitFaceQuality(&m_PfaceQuality);
		m_PfaceQuality = NULL;
	}

#ifdef LIVENESS_DETECT_DEPTH_ENABLE
	#if MODE_3D_LIVENESS
		if(m_PLivenessDetect != NULL)
		{
			rsUnInitDepthLivenessDetect(&m_PLivenessDetect);
			m_PLivenessDetect = NULL;
		}
	#endif
	#if MODE_MUTI_LIVENESS
		if(m_PLivenessDetect != NULL)
		{
			rsUnInitMutiLivenessDetect(&m_PLivenessDetect);
			m_PLivenessDetect = NULL;
		}	
	#endif
#endif

	if(m_Plicense)
	{
		rsUnInitLicenseManagerV2(&m_Plicense);
		m_Plicense = NULL;
	}
}

bool RSApiHandle::Init(int AppType)
{
	string licenseContent;
	int ret = -10;
#ifdef PER_TIME_CAL	
		uint32_t total_us = 0;
		gettimeofday(&tv_rs_start, NULL);
#endif	
	if(false == GetInitCfgInfo(LICENSE_FILE_PATH, licenseContent))
	{
		std::cerr << "License file missing" << std::endl;
		return false;
	}
	std::cout << "cfg info pass" << std::endl;
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "GetInitCfgInfo: " << total_us << " usec" << endl;
#endif
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif	
/*	rsInitLicenseManagerV2(&m_Plicense,
			"f91d6d9bae59f8393d4634db027d7224",
			"72aa62f44418f21e43ba9321525b6fadb7aea743"); */
	ret = rsInitLicenseManagerV2(&m_Plicense, licenseContent.c_str());
	if(m_Plicense == NULL)
	{
		std::cerr << "License falied" << std::endl;
		return false;
	}	
	log_debug("license manager v2 pass!ret<%d>\n", ret);
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "rsInitLicenseManagerV2: " << total_us << " usec" << endl;
#endif
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif		
	rsInitFaceDetect(&m_pfaceDetect, m_Plicense);
	if (m_pfaceDetect == NULL)
	{
		std::cerr << "rsInitFaceDetect falied" << std::endl;
		return false;
	}
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "rsInitFaceDetect: " << total_us << " usec" << endl;
#endif
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
	rsInitFaceRecognition(&m_PfaceRecognition, m_Plicense, DB_FILE_PATH);
	if (m_PfaceRecognition == NULL)
	{
		std::cerr << "rsInitFaceRecognition falied" << std::endl;
		return false;
	}
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "rsInitFaceRecognition: " << total_us << " usec" << endl;
#endif
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif	
	if(AppType = FACE_REG)
	{
		rsRecognitionSetConfidence(m_PfaceRecognition, stSystemCfg.face_reg_threshold_val);
	}
	else if(AppType = FACE_RECG)
	{
		rsRecognitionSetConfidence(m_PfaceRecognition, stSystemCfg.face_recg_threshold_val);
	}
	else
	{
		rsRecognitionSetConfidence(m_PfaceRecognition, FACE_THRESHOLD_VALUE);		
	}
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "rsRecognitionSetConfidence: " << total_us << " usec" << endl;
#endif
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif		
	rsInitFaceQuality(&m_PfaceQuality, m_Plicense);
	if (m_PfaceQuality == NULL)
	{
		std::cerr << "rsInitFaceQuality falied" << std::endl;
		return false;
	}
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "rsInitFaceQuality: " << total_us << " usec" << endl;
#endif
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif

#ifdef LIVENESS_DETECT_DEPTH_ENABLE
	#if MODE_3D_LIVENESS
		rsInitDepthLivenessDetect(&m_PLivenessDetect,m_Plicense);
		if(m_PLivenessDetect == NULL)
		{
			std::cerr << "rsInitDepthLivenessDetect falied" << std::endl;
			return false;
		}
	#endif	
	#if MODE_MUTI_LIVENESS
		rsInitMutiLivenessDetect(&m_PLivenessDetect, m_Plicense);
		if (m_PLivenessDetect == NULL) {
			std::cerr << "rsInitDepthLivenessDetect falied" << std::endl;
			return false;
		}
	#endif
	
#endif
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "rsInitMutiLivenessDetect: " << total_us << " usec" << endl;
#endif

	log_debug("ReadSense SDK init successfully !\n");
	return true;
}

//for draw face box on LCD
rs_rect box_coordinate;
uint8_t Ir_buf[FRAME_SIZE_INBYTE*2];
int face_alg_get_last_detected_box(rs_rect * box)
{
	if((box_coordinate.width == 0) || (box_coordinate.height == 0))
	{
		box->left = (CAM_WIDTH - 220)/2;
		box->top = (CAM_HEIGHT - 220)/2;
		box->width = 220;
		box->height = 220;
	}
	else
	{
		box->left = box_coordinate.left;
		box->top = box_coordinate.top;
		box->width = box_coordinate.width;
		box->height = box_coordinate.height;
	}
	
	return 0;
}
int face_alg_set_last_detected_box(rs_rect * box)
{
	box_coordinate.left = box->left;
	box_coordinate.top = box->top;
	box_coordinate.width = box->width;
	box_coordinate.height = box->height;
	
	return 0;
}
int face_alg_set_NULL_detected_box()
{
	memset(&box_coordinate, 0, sizeof(rs_rect));
	return 0;
}

static int img_num = 0;
RS_RET_VALUE RSApiHandle::FaceDetect_Depth(imageInfo color_img, imageInfo depth_img, bool *pIsMouseOpen,  int *distance, int *pFaceNum)
{
	rs_face *pFaceArray = NULL;
	int faceCount = 0;
	int mapId = -1;
	int ret = RS_RET_NO_FACE;

	log_debug("Face Detect Depth in\n");
/*
	// save 2 gray images use bgra
	if(img_num != 2)
	{
		Mat bgraimg, rgbimg;
		bgraimg.create(color_img.height, color_img.width, CV_8UC4);
		memcpy(bgraimg.data, color_img.data, (color_img.height)*(color_img.stride));
		cvtColor(bgraimg, rgbimg, COLOR_BGRA2GRAY);
		imwrite("./images/bgra2gray" + to_string(img_num) +".jpg", rgbimg);
		std::cout << "detect bgra to gray image " << img_num << " saved" << std::endl;
		++img_num;
	}
*/
/*
	//save 2 gray images
	if(img_num != 2)
	{
		Mat grayimg;
		grayimg.create(color_img.height, color_img.width, CV_8UC1);
		memcpy(grayimg.data, color_img.data, (color_img.height)*(color_img.stride));
		imwrite("./images/grayimg" + to_string(img_num) +".jpg", grayimg);
		std::cout << "detect gray image " << img_num << " saved" << std::endl;
		++img_num;
	}
*/
#ifdef PER_TIME_CAL	
	uint32_t total_us = 0;
	gettimeofday(&tv_rs_start, NULL);
#endif

	//face detect
	int detectFlag = rsRunFaceDetect(m_pfaceDetect, color_img.data,
					PIX_FORMAT_GRAY,  	//PIX_FORMAT_BGRA8888,
					color_img.width,
					color_img.height,
					color_img.stride,
					RS_IMG_CLOCKWISE_ROTATE_0,
					&pFaceArray,
					&faceCount);
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "Face Det RS: " << total_us << " usec" << endl;
#endif
	*pFaceNum = faceCount;
	if (detectFlag == -1 || faceCount <= 0)
	{
		releaseFaceDetectResult(pFaceArray, faceCount);
		cout << "no face detected!" << endl;
		return RS_RET_NO_FACE;
	}

	log_debug("Detect pass!\n");

	rs_face mFace = pFaceArray[0];
	if(faceCount > 1)
	{
		for (int loop = 1; loop < faceCount; loop++)
		{
			if (pFaceArray[loop].rect.width > mFace.rect.width)
				mFace = pFaceArray[loop];
		}
	}
	
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
	/*draw back face box on lcd*/
	if(stSystemCfg.flag_faceBox)
	{
		face_alg_set_last_detected_box(&mFace.rect);
		DrawRawImgOnScreen((uint16_t *)Ir_buf, CAM_WIDTH, CAM_HEIGHT, true);
	}

	/*发送人脸坐标给外部MCU*/
	if(stSystemCfg.flag_facePosition_Rpt)
	{
		cmdFacePositionRpt(&mFace.rect);
	}
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "my logical: " << total_us << " usec" << endl;
#endif

#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
	if (abs(mFace.pitch) > stSystemCfg.face_recg_angle ||
			abs(mFace.roll) > stSystemCfg.face_recg_angle ||
			abs(mFace.yaw) > stSystemCfg.face_recg_angle) {
		releaseFaceDetectResult(pFaceArray, faceCount);
		cout << "===face quality fail!!!" << endl;
		return RS_RET_NO_QUALITY;
	}

	log_debug("Quality pass!\n");
	
	// open mouse detect
	*pIsMouseOpen = rsIsMouseOpen(color_img.data,
						PIX_FORMAT_GRAY,
						color_img.width,
						color_img.height,
						color_img.stride,
						&mFace);
	log_debug("IsMouseOpen=%d\n", *pIsMouseOpen);
#ifdef PER_TIME_CAL
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "rsIsMouseOpen: " << total_us << " usec" << endl;
#endif
#ifdef PER_TIME_CAL	
		gettimeofday(&tv_rs_start, NULL);
#endif

#ifdef LIVENESS_DETECT_DEPTH_ENABLE
	log_debug("stride =<%d>\n", depth_img.stride);
	*distance =  rsRunDepthFaceDistance(m_PLivenessDetect, 
	                    (const  unsigned short *)depth_img.data,
						depth_img.width,
						depth_img.height,
						depth_img.width * 2,
						&mFace);
	log_debug("distance = %d\n", *distance); 
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "rsRunDepthFaceDistance: " << total_us << " usec" << endl;
#endif

	#ifdef PER_TIME_CAL	
		gettimeofday(&tv_rs_start, NULL);
	#endif
	//Liveness detect
	int livenessState[3] = {0};

	#if MODE_3D_LIVENESS
		int livenessFlag = rsRunDepthLivenessDetect(m_PLivenessDetect,
							(unsigned short *)depth_img.data,
							depth_img.width,
							depth_img.height,
							depth_img.stride,
							&mFace,
							livenessState);
	#endif
	#if MODE_MUTI_LIVENESS
		int livenessFlag = rsRunMutiLivenessDetect(m_PLivenessDetect,
								color_img.data,
								(unsigned short *)depth_img.data,
								PIX_FORMAT_GRAY,
								color_img.width,
								color_img.height,
								color_img.stride,
								&mFace,
								livenessState);
	#endif
	#ifdef PER_TIME_CAL
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "Liveness Det RS: " << total_us << " usec" << endl;
	#endif

	//to be restored with se chip
	// If a live man detected
	if(0 != livenessFlag || 1 != livenessState[0]) {
		// release tracking data
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_LIVENESS;
	}
	log_debug("Liveness detect pass!\n");

#endif  //LIVENESS_DETECT_DEPTH_ENABLE

#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
	rs_face_feature faceFeature;
	int regFlag = rsRecognitionGetFaceFeature(m_PfaceRecognition,
					color_img.data,
					PIX_FORMAT_GRAY,  //PIX_FORMAT_BGRA8888,
					color_img.width,
					color_img.height,
					color_img.stride,
					&mFace,
					&faceFeature);
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "Get Face Feature RS: " << total_us << " usec" << endl;
#endif

	if (regFlag >= 0) 
	{
		log_debug("Feature pass!\n");
#ifdef PER_TIME_CAL	
		gettimeofday(&tv_rs_start, NULL);
#endif
		// Compare with the faces in database
		mapId = rsRecognitionFaceIdentification(m_PfaceRecognition, &faceFeature);
		log_debug("Identification pass!\n");
#ifdef PER_TIME_CAL
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "Recog Face RS: " << total_us << " usec" << endl;
#endif

#ifdef PER_TIME_CAL	
		gettimeofday(&tv_rs_start, NULL);
#endif
#ifdef CHECK_FACE_VERIFICATION
		float conf = 0;
		conf = rsRecognitionGetConfidence(m_PfaceRecognition);
		log_debug("Confidence:%f\n", conf);
#endif //CHECK_FACE_VERIFICATION
#ifdef PER_TIME_CAL
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "rsGetConfidence: " << total_us << " usec" << endl;
#endif

		if (mapId > 0) 
		{
		#ifdef LOG_USE_COLOR
			cout << level_colors[0] << "recogition id: " << mapId << "\x1b[0m" << endl;
		#else
			std::cout << "recogition id: " << mapId << std::endl;
		#endif
			ret = mapId;
		} 
		else 
		{
			ret = RS_RET_NO_RECO;
		}
	}
	else 
	{
		ret = RS_RET_NO_FEATURE;
	}

	releaseFaceDetectResult(pFaceArray, faceCount);

	return (RS_RET_VALUE)ret;
}

RS_RET_VALUE RSApiHandle::FaceDetect(imageInfo img)
{
	rs_face *pFaceArray = NULL;
	int faceCount = 0;
	int mapId = -1;
	int ret;

	cout << "Face Detect in" << endl;
#ifdef PER_TIME_CAL	
	uint32_t total_us = 0;
	gettimeofday(&tv_rs_start, NULL);
#endif
	//face detect
	int detectFlag = rsRunFaceDetect(m_pfaceDetect, img.data,
					PIX_FORMAT_GRAY,  //PIX_FORMAT_BGRA8888, hannah modified
					img.width,
					img.height,
					img.stride,
					RS_IMG_CLOCKWISE_ROTATE_0,
					&pFaceArray,
					&faceCount);
#ifdef PER_TIME_CAL
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "Face Det RS: " << total_us << " usec" << endl;
#endif

	if (detectFlag == -1 || faceCount <= 0)
	{
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_FACE;
	}

	cout << "run face detect pass!" << endl;
	
	rs_face mFace = pFaceArray[0];
	for (int loop = 1; loop < faceCount; loop++)
	{
		if (pFaceArray[loop].rect.width > mFace.rect.width)
			mFace = pFaceArray[loop];
	}
	if(abs(mFace.pitch) > stSystemCfg.face_recg_angle ||
			abs(mFace.roll) > stSystemCfg.face_recg_angle ||
			abs(mFace.yaw) > stSystemCfg.face_recg_angle){
		releaseFaceDetectResult(pFaceArray, faceCount);
		cout << "===face quality fail!!!" << endl;
		return RS_RET_NO_QUALITY;
	}

	cout << "face quality select pass!" << endl;

#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
	rs_face_feature faceFeature;
	int regFlag = rsRecognitionGetFaceFeature(m_PfaceRecognition,
					img.data,
					PIX_FORMAT_GRAY,  //PIX_FORMAT_BGRA8888,
					img.width,
					img.height,
					img.stride,
					&mFace,
					&faceFeature);
	cout << "get feature pass!" << endl;
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "Get Face Feature RS: " << total_us << " usec" << endl;
#endif

	if (regFlag >= 0) {
#ifdef PER_TIME_CAL	
		gettimeofday(&tv_rs_start, NULL);
#endif
	// Compare with the faces in database
		mapId = rsRecognitionFaceIdentification(m_PfaceRecognition, &faceFeature);
#ifdef PER_TIME_CAL
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "Fece Ident RS: " << total_us << " usec" << endl;
#endif

		if (mapId > 0) {
			std::cout << "recogition id" << mapId << std::endl;

			ret = mapId;
		} else {
			ret = RS_RET_NO_RECO;
		}
	} else {
		ret = RS_RET_NO_FEATURE;
	}

	releaseFaceDetectResult(pFaceArray, faceCount);

	return (RS_RET_VALUE)ret;
}

RS_RET_VALUE RSApiHandle::RegisterFace(imageInfo img)
{
	int faceCount = 0;
	int mapId = -1;
	int ret = -1, score = -10;
	rs_face *pFaceArray = NULL;

	cout << "===>RegisterFace in" << endl;
/*
	// save 2 gray images use bgra
	if(img_num != 2)
	{
		Mat bgraimg, rgbimg;
		bgraimg.create(img.height, img.width, CV_8UC4);
		memcpy(bgraimg.data, img.data, (img.height)*(img.stride));
		cvtColor(bgraimg, rgbimg, COLOR_BGRA2GRAY);
		imwrite("./images/bgra2gray" + to_string(img_num) +".jpg", rgbimg);
		std::cout << "register bgra to gray image " << img_num << " saved" << std::endl;
		++img_num;
	}
*/
/*
	//save 2 gray images
	if(img_num != 2)
	{
		Mat grayimg;
		grayimg.create(img.height, img.width, CV_8UC1);
		memcpy(grayimg.data, img.data, (img.height)*(img.stride));
		imwrite("./images/reg-gray-shift2" + to_string(img_num) +".jpg", grayimg);
		std::cout << "register gray image " << img_num << " saved" << std::endl;
		++img_num;
	}
*/
	//face detect
	int detectFlag = rsRunFaceDetect(m_pfaceDetect, img.data,					
					PIX_FORMAT_GRAY,  //PIX_FORMAT_BGRA8888,
					img.width,
					img.height,
					img.stride,
					RS_IMG_CLOCKWISE_ROTATE_0,
					&pFaceArray,
					&faceCount);

	if (detectFlag == -1 || faceCount <= 0 || faceCount > 1)
	{
		releaseFaceDetectResult(pFaceArray, faceCount);
		cout << "no face detected!" << endl;
		return RS_RET_NO_FACE;
	}
/*
	//save 20 gray images
	if(img_num != 20)
	{
		imwrite("./images/reg-score" + to_string(img_num) +".jpg", img);
		std::cout << "register gray image " << img_num << " saved" << std::endl;
		++img_num;
	}
*/
	cout << "run face detect pass!" << endl;

	rs_face mFace = pFaceArray[0];
	for (int loop = 1; loop < faceCount; loop++) {
		if (pFaceArray[loop].rect.width > mFace.rect.width){
			mFace = pFaceArray[loop];
		}
	}

	//cout << "Face pitch: " << mFace.pitch << " roll: " << mFace.roll << " yaw: " << mFace.yaw << endl;
	if(abs(mFace.pitch) > stSystemCfg.face_reg_angle ||
			abs(mFace.roll) > stSystemCfg.face_reg_angle ||
			abs(mFace.yaw) > stSystemCfg.face_reg_angle) {
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_QUALITY;
	}

	cout << "face quality select pass!" << endl;

	score = rsGetFaceQualityScore(m_PfaceQuality, img.data,
					PIX_FORMAT_GRAY,    //PIX_FORMAT_BGRA8888,
					img.width,
					img.height,
					img.stride,
					&mFace);

	if (score < 5)
	{
		cout << "face quality is low:" << score << endl;
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_FACE;
	}
	cout << "face quality is:" << score << endl;

	rs_face_feature faceFeature;
	ret = rsRecognitionGetFaceFeature(m_PfaceRecognition,
				img.data,
				PIX_FORMAT_GRAY,  //PIX_FORMAT_BGRA8888,
				img.width,
				img.height,
				img.stride,
				&mFace,
				&faceFeature);
	if (ret < 0) {
		cout << "no face features can be got" << endl;
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_FACE;
	}

	// release detect result first
	releaseFaceDetectResult(pFaceArray, faceCount);

	// Compare with the faces in database
	mapId = rsRecognitionFaceIdentification(m_PfaceRecognition, &faceFeature);

	if (mapId < 0) {
		// no such faces in the database, add it
		personId = rsRecognitionPersonCreate(
				m_PfaceRecognition, &faceFeature);
		if (personId > 0) {
#ifdef LOG_USE_COLOR
			cout << level_colors[1] << "face is added to the database! id:" << personId << "\x1b[0m" << endl;
#else
			cout << "face is added to the database! id:" <<
							personId << endl;
#endif
			return RS_RET_OK;
		}
	} else {
		//face is there
		personId = 0;  //hannah added for existed faceID
#ifdef LOG_USE_COLOR
		cout << level_colors[2] << "face is already in the database" << "\x1b[0m" << endl;
#else
		cout << "face is already in the database" << endl;
#endif
		return RS_RET_ALREADY_IN_DATABASE;
	}

	return RS_RET_UNKOWN_ERR;
}

RS_RET_VALUE RSApiHandle::RegisterFace_Depth(imageInfo color_img, imageInfo depth_img, int flag, rs_face *pRsFace, int RegAddFaceType, int *distance )
{
	int faceCount = 0;
	int mapId = -1;
	int ret = -1, score = 0;
	rs_face *pFaceArray = NULL;

	log_debug("===>RegisterFace_Depth in\n");
	
	//face detect
	int detectFlag = rsRunFaceDetect(m_pfaceDetect, color_img.data,
					PIX_FORMAT_GRAY,  //PIX_FORMAT_BGRA8888,
					color_img.width,
					color_img.height,
					color_img.stride,
					RS_IMG_CLOCKWISE_ROTATE_0,
					&pFaceArray,
					&faceCount);

	if (detectFlag == -1 || faceCount <= 0 || faceCount > 1)
	{
		releaseFaceDetectResult(pFaceArray, faceCount);
		cout << "no face detected!" << endl;
		return RS_RET_NO_FACE;
	}

	*pRsFace = pFaceArray[0];
	
	if(faceCount > 1)
	{	
		for (int loop = 1; loop < faceCount; loop++) 
		{
			if (pFaceArray[loop].rect.width > pRsFace->rect.width)
			{
				*pRsFace = pFaceArray[loop];
			}
		}
	}
	
	/*draw back face box on lcd*/
	if(stSystemCfg.flag_faceBox)
	{
		face_alg_set_last_detected_box(&pRsFace->rect);
		DrawRawImgOnScreen((uint16_t *)Ir_buf, CAM_WIDTH, CAM_HEIGHT, true);
	}	
	
	/*发送人脸坐标给外部MCU*/
	if(stSystemCfg.flag_facePosition_Rpt)
	{
		cmdFacePositionRpt(&pRsFace->rect);
	}

	//cout << "Face pitch: " << pRsFace->pitch << " roll: " << pRsFace->roll << " yaw: " << pRsFace->yaw << endl;
	if(flag == CREAT_PERSON)
	{
		if(abs(pRsFace->pitch) > stSystemCfg.face_reg_angle ||
				abs(pRsFace->roll) > stSystemCfg.face_reg_angle ||
				abs(pRsFace->yaw) > stSystemCfg.face_reg_angle) 
		{
			releaseFaceDetectResult(pFaceArray, faceCount);
			cout << "RS_RET_NO_ANGLE: "<< "Face pitch: " << pRsFace->pitch << " roll: " << pRsFace->roll << " yaw: " << pRsFace->yaw << endl;
			return RS_RET_NO_ANGLE;
		}
	}
	else
	{		
		switch(RegAddFaceType)
		{
		case PITCH_UP://抬头
			if(pRsFace->pitch > ADDFACE_REG_DETECT_MAX_ANGEL || pRsFace->pitch < ADDFACE_REG_DETECT_MIN_ANGEL ||
					abs(pRsFace->roll) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
					abs(pRsFace->yaw) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
			{
				releaseFaceDetectResult(pFaceArray, faceCount);
				cout << "RS_RET_NO_ANGLE: "<< "Face pitch: " << pRsFace->pitch << endl;
				return RS_RET_NO_ANGLE;
			}
			break;
			
		case PITCH_DOWN://低头
			if(pRsFace->pitch > -ADDFACE_REG_DETECT_MIN_ANGEL || pRsFace->pitch < -ADDFACE_REG_DETECT_MAX_ANGEL ||
					abs(pRsFace->roll) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
					abs(pRsFace->yaw) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
			{
				releaseFaceDetectResult(pFaceArray, faceCount);
				cout << "RS_RET_NO_ANGLE: "<< "Face pitch: " << pRsFace->pitch << endl;
				return RS_RET_NO_ANGLE;
			}
		break;
		
		case YAW_LEFT://脸偏左
			if(pRsFace->yaw> ADDFACE_REG_DETECT_MAX_ANGEL || pRsFace->yaw< ADDFACE_REG_DETECT_MIN_ANGEL ||
					abs(pRsFace->roll) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
					abs(pRsFace->pitch) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
			{
				releaseFaceDetectResult(pFaceArray, faceCount);
				cout << "RS_RET_NO_ANGLE: "<< "Face yaw: " << pRsFace->yaw << endl;
				return RS_RET_NO_ANGLE;
			}
		break;
		
		case YAW_RIGHT://脸偏右
			if(pRsFace->yaw> -ADDFACE_REG_DETECT_MIN_ANGEL || pRsFace->yaw< -ADDFACE_REG_DETECT_MAX_ANGEL ||
					abs(pRsFace->roll) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
					abs(pRsFace->pitch) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
			{
				releaseFaceDetectResult(pFaceArray, faceCount);
				cout << "RS_RET_NO_ANGLE: "<< "Face yaw: " << pRsFace->yaw << endl;
				return RS_RET_NO_ANGLE;
			}
		break;
#if ROLL_ENABLE		
		case ROLL_LEFT://头偏左
			if(pRsFace->roll> -ADDFACE_REG_DETECT_MIN_ANGEL || pRsFace->roll < -ADDFACE_REG_DETECT_MAX_ANGEL ||
					abs(pRsFace->pitch) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
					abs(pRsFace->yaw) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
			{
				releaseFaceDetectResult(pFaceArray, faceCount);
				cout << "RS_RET_NO_ANGLE: "<< "Face roll: " << pRsFace->roll << endl;
				return RS_RET_NO_ANGLE;
			}
		break;
		
		case ROLL_RIGHT://头偏右
			if(pRsFace->roll > ADDFACE_REG_DETECT_MAX_ANGEL || pRsFace->roll < ADDFACE_REG_DETECT_MIN_ANGEL ||
					abs(pRsFace->pitch) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
					abs(pRsFace->yaw) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
			{
				releaseFaceDetectResult(pFaceArray, faceCount);
				cout << "RS_RET_NO_ANGLE: "<< "Face roll: " << pRsFace->roll << endl;
				return RS_RET_NO_ANGLE;
			}
		break;
#endif
		default:
			break;
		}
	}

#ifdef LIVENESS_DETECT_DEPTH_ENABLE
	#ifdef PER_TIME_CAL	
		uint32_t total_us = 0;
		gettimeofday(&tv_rs_start, NULL);
	#endif
	//std::cout << "stride " << depth_img.stride << std::endl;
	*distance =	rsRunDepthFaceDistance(m_PLivenessDetect, 
						(const	unsigned short *)depth_img.data,
						depth_img.width,
						depth_img.height,
						depth_img.width * 2,
						pRsFace);
	std::cout << "distance = " << *distance << std::endl; 
	#ifdef PER_TIME_CAL
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "Liveness distance time: " << total_us << " usec" << endl;
	#endif

	//Liveness detect
	int livenessState[3] = {0};
	#if MODE_3D_LIVENESS
			int livenessFlag = rsRunDepthLivenessDetect(m_PLivenessDetect,
								(unsigned short *)depth_img.data,
								depth_img.width,
								depth_img.height,
								depth_img.stride,
								pRsFace,
								livenessState);
	#endif
	#if MODE_MUTI_LIVENESS
			int livenessFlag = rsRunMutiLivenessDetect(m_PLivenessDetect,
									color_img.data,
									(unsigned short *)depth_img.data,
									PIX_FORMAT_GRAY,
									color_img.width,
									color_img.height,
									color_img.stride,
									pRsFace,
									livenessState);
	#endif

	//to be restored with se chip
	// If a live man detected
	if(0 != livenessFlag || 1 != livenessState[0]) {
		// release tracking data
		releaseFaceDetectResult(pFaceArray, faceCount);
		cout << "<<<Info:RS_RET_NO_LIVENESS."<<"livenessFlag="<<livenessFlag<<"livenessState:"<<livenessState[0]<<livenessState[1]<<livenessState[2]<< endl;
		return RS_RET_NO_LIVENESS;
	}
#endif

	score = rsGetFaceQualityScore(m_PfaceQuality, color_img.data,
					PIX_FORMAT_GRAY,  //PIX_FORMAT_BGRA8888,
					color_img.width,
					color_img.height,
					color_img.stride,
					pRsFace);
	if (score < 5)
	{
		cout << "face quality is low:" << score << endl;
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_QUALITY;
	}
	cout << "face quality is:" << score << endl;

	rs_face_feature faceFeature;
	ret = rsRecognitionGetFaceFeature(m_PfaceRecognition,
					color_img.data,
					PIX_FORMAT_GRAY,  //PIX_FORMAT_BGRA8888,
					color_img.width,
					color_img.height,
					color_img.stride,
					pRsFace,
					&faceFeature);
	if (ret < 0) 
	{
		cout << "no face features can be got" << endl;
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_FEATURE;
	}

	// release detect result first
	releaseFaceDetectResult(pFaceArray, faceCount);
	
	if(flag == CREAT_PERSON)
	{
		// Compare with the faces in database
		mapId = rsRecognitionFaceIdentification(m_PfaceRecognition, &faceFeature);
		if (mapId < 0) 
		{
			// no such faces in the database, add it
			personId = rsRecognitionPersonCreate(
					m_PfaceRecognition, &faceFeature);
			if (personId > 0) 
			{
			#ifdef LOG_USE_COLOR
				cout << level_colors[1] << "PersonID is added to the database! id:" << personId << "\x1b[0m" << endl;
			#else
				cout << "PersonID is added to the database! id:" << personId << endl;
			#endif
				face_reg_SaveImgToLocalFileByPersonID(color_img, depth_img, personId);
				if(stSystemCfg.flag_muxAngle_reg)
				{
					return RS_RET_PERSON_ADD_FACE;
				}
				else
				{
					return RS_RET_OK;
				}
			}
		}		
		else 
		{
			//face is there
			personId = 0; 
		#ifdef LOG_USE_COLOR
			cout << level_colors[2] << "face is already in the database" << "\x1b[0m" << endl;
		#else
			cout << "face is already in the database" << endl;
		#endif
			return RS_RET_ALREADY_IN_DATABASE;
		}
	} 
	else
	{
		/*为PersonID添加多个人脸FaceID*/
		static int num = 0;
		printf("=====For personID<%d> add a face<%d>\n", personId, num+1);
		rsRecognitionPersonAddFace(m_PfaceRecognition, &faceFeature, personId);
		if(++num >= RegAddFaceTypeNUM)
			return RS_RET_OK;
		else
			return RS_RET_PERSON_ADD_FACE;
	}

	return RS_RET_UNKOWN_ERR;
}

//added for deleted specific face ID
RS_RET_VALUE RSApiHandle::DeleteFace(int PersonId)
{
	int ret = -1;
	
	ret = rsRecognitionPersonDelete(m_PfaceRecognition, PersonId);
	if(ret == 0)
	{
		face_reg_DelImgFromLocalFileByPersonID(PersonId);
		return RS_RET_OK;
	}
	
	return RS_RET_UNKOWN_ERR;	
}

/*获取人脸数据库中已保存的人数*/
RS_RET_VALUE RSApiHandle::GetAlbumSizeFromDataBase(unsigned short *faceSize)
{
	int ret = -1;
	
	*faceSize = rsRecognitionGetAlbumSize(m_PfaceRecognition);
	if(faceSize >= 0)
		return RS_RET_OK;
	
	return RS_RET_UNKOWN_ERR;	
}

int getLastIrData(uint8_t *pIrData, uint32_t data_len)
{
	memset(Ir_buf, 0, sizeof(Ir_buf));
	memcpy(Ir_buf, pIrData, data_len);
	return 0;
}

