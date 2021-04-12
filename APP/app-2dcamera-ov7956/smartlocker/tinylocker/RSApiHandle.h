#ifndef RS_API_HANDLE_H
#define RS_API_HANDLE_H

#include <string>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include "RSCommon.h"
#include "RSFaceSDK.h"

#define CHECK_FACE_VERIFICATION
//#define OPENCV_SAVE_FILE_ENABLE
//#define SAVE_IMAGE_TO_FILE_ENABLE
//#define LIVENESS_DETECT_ENABLE

#define FACE_REG_THRESHOLD_VALUE 			70
#define FACE_RECG_THRESHOLD_VALUE 		50
#define FACE_THRESHOLD_VALUE 				60

#define FACE_DETECT_RECGONIZE_ANGEL	(25.0)
#define FACE_DETECT_REGISTER_ANGEL	(25.0)   //10

using namespace std;

typedef struct imageinfo
{
	const unsigned char *data;
	int width;
	int height;
	int stride;

} imageInfo;

enum FACE_APP_TYPE
{
	FACE_LOOP = 0,
	FACE_REG,
	FACE_RECG
};

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
	bool GetInitCfgInfo(string filePath, string& initInfo);
	bool Init(int AppType);
	RS_RET_VALUE FaceDetect(imageInfo img);
	RS_RET_VALUE RegisterFace(imageInfo img);
	RS_RET_VALUE DeleteFace(int faceId);  //hannah added 0326	
		
protected:
	RSHandle m_Plicense;
	RSHandle m_PfaceRecognition;
	RSHandle m_pfaceDetect;
	RSHandle m_PfaceQuality;
#ifdef LIVENESS_DETECT_ENABLE
	RSHandle m_PLivenessDetect;
#endif//LIVENESS_DETECT_ENABLE

};

#endif // RS_API_HANDLE_H
