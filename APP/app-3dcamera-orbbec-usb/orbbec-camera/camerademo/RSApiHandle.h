#ifndef RS_API_HANDLE_H
#define RS_API_HANDLE_H

#include <string>
#include "RSCommon.h"
#include "RSFaceSDK.h"

#define LIVENESS_DETECT_SUPPORT  //hannah marked
//#define LIVENESS_DETECT_RGB_ENABLE
#define LIVENESS_DETECT_DEPTH_ENABLE

#define FACE_THRESHOLD_VALUE 63
#define FACE_DETECT_RECGONIZE_ANGEL	(25.0)
#define FACE_DETECT_REGISTER_ANGEL	(10.0)  //hannah modified 190408 from 15-->10 

using namespace std;

typedef struct imageinfo
{
	const unsigned char *data;
	int width;
	int height;
	int stride;

} imageInfo;

enum RS_RET_VALUE
{
	RS_RET_ALREADY_IN_DATABASE = -7,
	RS_RET_NO_LIVENESS = -6,
	RS_RET_NO_QUALITY = -5,
	RS_RET_NO_FACE = -4,
	RS_RET_NO_FEATURE = -3,
	RS_RET_NO_RECO = -2,
	RS_RET_UNKOWN_ERR = -1,
	RS_RET_OK = 0,
};

class RSApiHandle
{
public:
	RSApiHandle();
	~RSApiHandle();
	bool Init();
	RS_RET_VALUE FaceDetect(imageInfo img);
	RS_RET_VALUE FaceDetect_Depth(imageInfo color_img, imageInfo depth_img);
	RS_RET_VALUE RegisterFace(imageInfo img);
	RS_RET_VALUE RegisterFace_Depth(imageInfo color_img, imageInfo depth_img);
	RS_RET_VALUE DeleteFace(int faceId);
		
protected:
	RSHandle m_Plicense;
	RSHandle m_PfaceRecognition;
	RSHandle m_pfaceDetect;
	RSHandle m_PfaceQuality;
#if defined(LIVENESS_DETECT_ENABLE) || defined(LIVENESS_DETECT_DEPTH_ENABLE)
	RSHandle m_PLivenessDetect;
#endif//LIVENESS_DETECT_ENABLE
};

#endif // RS_API_HANDLE_H
