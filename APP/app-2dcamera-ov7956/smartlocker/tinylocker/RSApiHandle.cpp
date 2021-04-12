#include <iostream>
#include "RSApiHandle.h"
#include "RSFaceSDK.h"
#include <sys/time.h>
#include <math.h>
#include "function.h"

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
static stSysCfg stSystemCfg;

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
#ifdef LIVENESS_DETECT_ENABLE
	m_PLivenessDetect = NULL;
#endif //LIVENESS_DETECT_ENABLE
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

#ifdef LIVENESS_DETECT_ENABLE
	if(m_PLivenessDetect != NULL)
	{
		rsUnInitLivenessDetect(&m_PLivenessDetect);
		m_PLivenessDetect = NULL;
	}
#endif //LIVENESS_DETECT_ENABLE

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

	GetSysLocalCfg(&stSystemCfg);
	
	if(false == GetInitCfgInfo(LICENSE_FILE_PATH, licenseContent))
	{
		std::cerr << "License file missing" << std::endl;
		return false;
	}
	std::cout << "cfg info pass" << std::endl;
	ret = rsInitLicenseManagerV2(&m_Plicense, licenseContent.c_str());
	std::cout << "ret value: " << ret << std::endl;
	if(m_Plicense == NULL)
	{
		std::cerr << "License falied" << std::endl;
		return false;
	}	
	std::cout << "license manager v2 pass" << std::endl;
	rsInitFaceDetect(&m_pfaceDetect, m_Plicense);
	if (m_pfaceDetect == NULL)
	{
		std::cerr << "rsInitFaceDetect falied" << std::endl;
		return false;
	}
	rsInitFaceRecognition(&m_PfaceRecognition, m_Plicense, DB_FILE_PATH);
	if (m_PfaceRecognition == NULL)
	{
		std::cerr << "rsInitFaceRecognition falied" << std::endl;
		return false;
	}
	
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
	
	rsInitFaceQuality(&m_PfaceQuality, m_Plicense);
	if (m_PfaceQuality == NULL)
	{
		std::cerr << "rsInitFaceQuality falied" << std::endl;
		return false;
	}
#ifdef LIVENESS_DETECT_ENABLE
	rsInitLivenessDetect(&m_PLivenessDetect,m_Plicense);
	//cout << "6--rsInitLivenessDetect done!" << endl;	
	if(m_PLivenessDetect == NULL)
	{
		std::cerr << "rsInitDepthLivenessDetect falied" << std::endl;
		return false;
	}
#endif//LIVENESS_DETECT_ENABLE

	std::cout << "ReadSense SDK init successfully !" << std::endl;
	return true;
}

static int img_num = 0;
RS_RET_VALUE RSApiHandle::FaceDetect(imageInfo img)
{
	rs_face *pFaceArray = NULL;
	int faceCount = 0;
	int mapId = -1;
	int ret = RS_RET_NO_FACE;
#ifdef CHECK_FACE_VERIFICATION
	float conf = 0;
#endif //CHECK_FACE_VERIFICATION

#ifdef PER_TIME_CAL	
	uint32_t total_us = 0;
	gettimeofday(&tv_rs_start, NULL);
#endif
	//face detect
	int detectFlag = rsRunFaceDetect(m_pfaceDetect, img.data,
					PIX_FORMAT_BGRA8888,
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
		cout << "no face when doing detect!" << endl;
		return RS_RET_NO_FACE;
	}

	cout << "run face detect pass!" << endl;

	rs_face mFace = pFaceArray[0];
	for (int loop = 1; loop < faceCount; loop++)
	{
		if (pFaceArray[loop].rect.width > mFace.rect.width)
			mFace = pFaceArray[loop];
	}

	if (abs(mFace.pitch) > stSystemCfg.face_detect_recg_angle ||
			abs(mFace.roll) > stSystemCfg.face_detect_recg_angle ||
			abs(mFace.yaw) > stSystemCfg.face_detect_recg_angle) {
		releaseFaceDetectResult(pFaceArray, faceCount);
		cout << "===face quality fail!!!" << endl;
		return RS_RET_NO_QUALITY;
	}

	cout << "face quality select pass!" << endl;
#ifdef LIVENESS_DETECT_ENABLE
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
	//Liveness detect
	int livenessState[3] = {0};
	int livenessFlag = rsRunLivenessDetect(m_PLivenessDetect,
						img.data,
						PIX_FORMAT_BGRA8888,
						img.width,
						img.height,
						img.stride,
						&mFace,
						livenessState);
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "Liveness Det RS: " << total_us << " usec" << endl;
#endif
	// If a live man detected
	if(0 != livenessFlag || 1 != livenessState[0]) {
		// release tracking data
		releaseFaceDetectResult(pFaceArray, faceCount);
		std::cout << "no live man" << std::endl;
		return RS_RET_NO_FACE;
	}
	cout << "face liveness detect pass!" << endl;

#endif  //LIVENESS_DETECT_DEPTH_ENABLE

#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
	rs_face_feature faceFeature;
        int regFlag = rsRecognitionGetFaceFeature(m_PfaceRecognition,
						img.data,
						PIX_FORMAT_BGRA8888,
						img.width,
						img.height,
						img.stride,
						&mFace,
						&faceFeature);
#ifdef PER_TIME_CAL
	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	cout << "Get Face Feature RS: " << total_us << " usec" << endl;
#endif

	if (regFlag >= 0) {
		cout << "get feature pass!" << endl;
#ifdef PER_TIME_CAL	
		gettimeofday(&tv_rs_start, NULL);
#endif
		// Compare with the faces in database
		mapId = rsRecognitionFaceIdentification(m_PfaceRecognition, &faceFeature);
		cout << "face identification pass!" << endl;
#ifdef PER_TIME_CAL
		gettimeofday(&tv_rs_end, NULL);
		total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
		cout << "Recog Face RS: " << total_us << " usec" << endl;
#endif

#ifdef CHECK_FACE_VERIFICATION
		conf = rsRecognitionGetConfidence(m_PfaceRecognition);
		std::cout << "confidence " << conf << std::endl;
#endif //CHECK_FACE_VERIFICATION

		if (mapId > 0) {
			std::cout << "recogition id" << mapId << std::endl;

#ifdef SAVE_IMAGE_TO_FILE_ENABLE
			/* save img to file for debug */
			int filedesc = open("recog-image", O_WRONLY | O_APPEND);
 
			if (filedesc < 0) {
				cout << "can not open recog-image file!! " << endl;
			}
 
			if (write(filedesc, img.data, (img.width)*(img.stride)) != (img.width)*(img.stride)) {
				cout << "error write to recognition image file!! " << endl;
			}
#endif //SAVE_IMAGE_TO_FILE_ENABLE

#ifdef OPENCV_SAVE_FILE_ENABLE
			Mat bgraimg, rgbimg;
			bgraimg.create(img.height, img.width, CV_8UC4);
			memcpy(bgraimg.data, img.data, (img.height)*(img.stride));
			cvtColor(bgraimg, rgbimg, COLOR_BGRA2GRAY);
			imwrite("./images/recognition.jpg", rgbimg);
#endif //OPENCV_SAVE_FILE_ENABLE

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
	int ret = -1, score = 0;
	rs_face *pFaceArray = NULL;

	//face detect
	int detectFlag = rsRunFaceDetect(m_pfaceDetect, img.data,
					PIX_FORMAT_BGRA8888,
					img.width,
					img.height,
					img.stride,
					RS_IMG_CLOCKWISE_ROTATE_0,
					&pFaceArray,
					&faceCount);

	if (detectFlag == -1 || faceCount <= 0 || faceCount > 1)
	{
		releaseFaceDetectResult(pFaceArray, faceCount);
		cout << "no face when doing detect!" << endl;
		return RS_RET_NO_FACE;
	}
	cout << "run face detect pass!" << endl;

	rs_face mFace = pFaceArray[0];
	for (int loop = 1; loop < faceCount; loop++) {
	        if (pFaceArray[loop].rect.width > mFace.rect.width){
	                mFace = pFaceArray[loop];
	        }
	}

	//cout << "Face pitch: " << mFace.pitch << " roll: " << mFace.roll << " yaw: " << mFace.yaw << endl;
	if(abs(mFace.pitch) > stSystemCfg.face_detect_reg_angle ||
			abs(mFace.roll) > stSystemCfg.face_detect_reg_angle ||
			abs(mFace.yaw) > stSystemCfg.face_detect_reg_angle) {
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_QUALITY;
	}

	cout << "face quality select pass!" << endl;

	score = rsGetFaceQualityScore(m_PfaceQuality, img.data,
					PIX_FORMAT_BGRA8888,
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
					PIX_FORMAT_BGRA8888,
					img.width,
					img.height,
					img.stride,
					pFaceArray,
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
			cout << "face is added to the database! id:" <<
							personId << endl;
#ifdef SAVE_IMAGE_TO_FILE_ENABLE
			/* save img to file for debug */
			int filedesc = open("database-image", O_WRONLY | O_APPEND);
 
			if (filedesc < 0) {
				cout << "can not open datbase-image file!! " << endl;
			}
 
			if (write(filedesc, img.data, (img.width)*(img.stride)) != (img.width)*(img.stride)) {
				cout << "error write to database image file!! " << endl;
			}
#endif //SAVE_IMAGE_TO_FILE_ENABLE

#ifdef OPENCV_SAVE_FILE_ENABLE
			Mat bgraimg, rgbimg;
			bgraimg.create(img.height, img.width, CV_8UC4);
			memcpy(bgraimg.data, img.data, (img.height)*(img.stride));
			cvtColor(bgraimg, rgbimg, COLOR_BGRA2GRAY);
			imwrite("./images/database.jpg", rgbimg);
#endif //OPENCV_SAVE_FILE_ENABLE

			return RS_RET_OK;
		}
	} 
	else 
	{
		//face is there
		personId = 0;  //hannah added
		cout << "face is already in the database" << endl;
		return RS_RET_ALREADY_IN_DATABASE; 
	}

	return RS_RET_UNKOWN_ERR;
}
//hannah added for deleted specific face ID
RS_RET_VALUE RSApiHandle::DeleteFace(int faceId)
{
	int ret = -1;
	
	ret = rsRecognitionPersonDelete(m_PfaceRecognition, faceId);
	if(ret == 0)
		return RS_RET_OK;
	
	return RS_RET_UNKOWN_ERR;	
}

bool GetPersonID(int * pPersonId)
{
	pPersonId = &personId;
	if(*pPersonId < 0)
	{
		return false;
	}

	return true;
}
