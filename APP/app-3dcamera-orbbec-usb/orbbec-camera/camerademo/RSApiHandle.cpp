#include <iostream>
#include "RSApiHandle.h"
#include "RSFaceSDK.h"
#include <sys/time.h>

//#define PER_TIME_CAL
#define LOG_USE_COLOR

#define DB_FILE_PATH "readsense.db"

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

#ifdef LIVENESS_DETECT_RGB_ENABLE
	if(m_PLivenessDetect != NULL)
	{
		rsUnInitLivenessDetect(&m_PLivenessDetect);
		m_PLivenessDetect = NULL;
	}
#endif //LIVENESS_DETECT_RGB_ENABLE

#ifdef LIVENESS_DETECT_DEPTH_ENABLE
	if(m_PLivenessDetect != NULL)
	{
		rsUnInitDepthLivenessDetect(&m_PLivenessDetect);
		m_PLivenessDetect = NULL;
	}

#endif

	if(m_Plicense)
	{
		rsUnInitLicenseManager(&m_Plicense);
		m_Plicense = NULL;
	}
}

bool RSApiHandle::Init()
{
	rsInitLicenseManager(&m_Plicense,
			"f91d6d9bae59f8393d4634db027d7224",
			"72aa62f44418f21e43ba9321525b6fadb7aea743");
	if(m_Plicense == NULL)
	{
		std::cerr << "License falied" << std::endl;
		return false;
	}
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
	rsRecognitionSetConfidence(m_PfaceRecognition, FACE_THRESHOLD_VALUE);
	rsInitFaceQuality(&m_PfaceQuality, m_Plicense);
	if (m_PfaceQuality == NULL)
	{
		std::cerr << "rsInitFaceQuality falied" << std::endl;
		return false;
	}
#ifdef LIVENESS_DETECT_RGB_ENABLE
	rsInitLivenessDetect(&m_PLivenessDetect,m_Plicense);
	if(m_PLivenessDetect == NULL)
	{
		return false;
	}
#endif//LIVENESS_DETECT_RGB_ENABLE

#ifdef LIVENESS_DETECT_DEPTH_ENABLE
	rsInitDepthLivenessDetect(&m_PLivenessDetect,m_Plicense);
	if(m_PLivenessDetect == NULL)
	{
		return false;
	}
#endif

	return true;
}

RS_RET_VALUE RSApiHandle::FaceDetect_Depth(imageInfo color_img, imageInfo depth_img)
{
	rs_face *pFaceArray = NULL;
	int faceCount = 0;
	int mapId = -1;
	int ret = RS_RET_NO_FACE;

#ifdef PER_TIME_CAL	
	uint32_t total_us = 0;
	gettimeofday(&tv_rs_start, NULL);
#endif
	//face detect
	int detectFlag = rsRunFaceDetect(m_pfaceDetect, color_img.data,
					PIX_FORMAT_BGRA8888,
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


	if (detectFlag == -1 || faceCount <= 0)
	{
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_FACE;
	}

	rs_face mFace = pFaceArray[0];
	for (int loop = 1; loop < faceCount; loop++)
	{
		if (pFaceArray[loop].rect.width > mFace.rect.width)
			mFace = pFaceArray[loop];
	}

	if (abs(mFace.pitch) > FACE_DETECT_RECGONIZE_ANGEL ||
			abs(mFace.roll) > FACE_DETECT_RECGONIZE_ANGEL ||
			abs(mFace.yaw) > FACE_DETECT_RECGONIZE_ANGEL) {
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_QUALITY;
	}

#ifdef LIVENESS_DETECT_DEPTH_ENABLE
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
	//Liveness detect
	int livenessState[3] = {0};
	int livenessFlag = rsRunDepthLivenessDetect(m_PLivenessDetect,
						depth_img.data,
						depth_img.width,
						depth_img.height,
						depth_img.stride,
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
		return RS_RET_NO_LIVENESS;
	}
#endif

#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
	rs_face_feature faceFeature;
        int regFlag = rsRecognitionGetFaceFeature(m_PfaceRecognition,
						color_img.data,
						PIX_FORMAT_BGRA8888,
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

	if (regFlag >= 0) {
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif
		// Compare with the faces in database
		mapId = rsRecognitionFaceIdentification(m_PfaceRecognition, &faceFeature);
#ifdef PER_TIME_CAL
				gettimeofday(&tv_rs_end, NULL);
				total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
				cout << "Recog Face RS: " << total_us << " usec" << endl;
#endif

		if (mapId > 0) {
#ifdef LOG_USE_COLOR
			cout << level_colors[0] << "recogition id: " << mapId << "\x1b[0m" << endl;
#else
			std::cout << "recogition id: " << mapId << std::endl;
#endif

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

RS_RET_VALUE RSApiHandle::FaceDetect(imageInfo img)
{
	rs_face *pFaceArray = NULL;
	int faceCount = 0;
	int mapId = -1;
	int ret;

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
		return RS_RET_NO_FACE;
	}

	rs_face mFace = pFaceArray[0];
	for (int loop = 1; loop < faceCount; loop++)
	{
		if (pFaceArray[loop].rect.width > mFace.rect.width)
			mFace = pFaceArray[loop];
	}
	if(abs(mFace.pitch) > FACE_DETECT_RECGONIZE_ANGEL ||
			abs(mFace.roll) > FACE_DETECT_RECGONIZE_ANGEL ||
			abs(mFace.yaw) > FACE_DETECT_RECGONIZE_ANGEL){
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_QUALITY;
	}

#ifdef LIVENESS_DETECT_RGB_ENABLE
#ifdef PER_TIME_CAL	
	gettimeofday(&tv_rs_start, NULL);
#endif

	//Liveness detect
	int livenessState[3] = { 0 };
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
		return RS_RET_NO_FACE;
	}
#endif //LIVENESS_DETECT_RGB_ENABLE

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

RS_RET_VALUE RSApiHandle::RegisterFace_Depth(imageInfo color_img, imageInfo depth_img)
{
	int faceCount = 0;
	int mapId = -1;
	int ret = -1, score = 0;
	rs_face *pFaceArray = NULL;

	//face detect
	int detectFlag = rsRunFaceDetect(m_pfaceDetect, color_img.data,
					PIX_FORMAT_BGRA8888,
					color_img.width,
					color_img.height,
					color_img.stride,
					RS_IMG_CLOCKWISE_ROTATE_0,
					&pFaceArray,
					&faceCount);


	if (detectFlag == -1 || faceCount <= 0 || faceCount > 1)
	{
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_FACE;
	}

	rs_face mFace = pFaceArray[0];
	for (int loop = 1; loop < faceCount; loop++) {
		if (pFaceArray[loop].rect.width > mFace.rect.width){
			mFace = pFaceArray[loop];
		}
	}

	//cout << "Face pitch: " << mFace.pitch << " roll: " << mFace.roll << " yaw: " << mFace.yaw << endl;
	if(abs(mFace.pitch) > FACE_DETECT_REGISTER_ANGEL ||
			abs(mFace.roll) > FACE_DETECT_REGISTER_ANGEL ||
			abs(mFace.yaw) > FACE_DETECT_REGISTER_ANGEL) {
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_QUALITY;
	}

#ifdef LIVENESS_DETECT_DEPTH_ENABLE
	//Liveness detect
	int livenessState[3] = {0};
	int livenessFlag = rsRunDepthLivenessDetect(m_PLivenessDetect,
						depth_img.data,
						depth_img.width,
						depth_img.height,
						depth_img.stride,
						&mFace,
						livenessState);

	// If a live man detected
	if(0 != livenessFlag || 1 != livenessState[0]) {
		// release tracking data
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_LIVENESS;
	}
#endif

	score = rsGetFaceQualityScore(m_PfaceQuality, color_img.data,
					PIX_FORMAT_BGRA8888,
					color_img.width,
					color_img.height,
					color_img.stride,
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
					color_img.data,
					PIX_FORMAT_BGRA8888,
					color_img.width,
					color_img.height,
					color_img.stride,
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
		return RS_RET_NO_FACE;
	}

	rs_face mFace = pFaceArray[0];
	for (int loop = 1; loop < faceCount; loop++) {
		if (pFaceArray[loop].rect.width > mFace.rect.width){
			mFace = pFaceArray[loop];
		}
	}

	//cout << "Face pitch: " << mFace.pitch << " roll: " << mFace.roll << " yaw: " << mFace.yaw << endl;
	if(abs(mFace.pitch) > FACE_DETECT_REGISTER_ANGEL ||
			abs(mFace.roll) > FACE_DETECT_REGISTER_ANGEL ||
			abs(mFace.yaw) > FACE_DETECT_REGISTER_ANGEL) {
		releaseFaceDetectResult(pFaceArray, faceCount);
		return RS_RET_NO_QUALITY;
	}

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

//hannah added for deleted specific face ID
RS_RET_VALUE RSApiHandle::DeleteFace(int faceId)
{
	int ret = -1;
	
	ret = rsRecognitionPersonDelete(m_PfaceRecognition, faceId);
	if(ret == 0)
		return RS_RET_OK;
	
	return RS_RET_UNKOWN_ERR;	
}