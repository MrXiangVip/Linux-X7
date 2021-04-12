#ifndef RS_API_HANDLE_H
#define RS_API_HANDLE_H

#include <string>
#include <fstream>
#include <sstream>
#include "RSCommon.h"
#include "RSFaceSDK.h"

#define CHECK_FACE_VERIFICATION
#define LIVENESS_DETECT_SUPPORT
//#define SAVE_IMAGE_TO_FILE_ENABLE
#define LIVENESS_DETECT_DEPTH_ENABLE
/*2d 活体检测/3d 活体检测/多模态活体检测  三选一*/
#define MODE_2D_LIVENESS						0		// 2d 活体检测
#define MODE_3D_LIVENESS						0		// 3D合体检测 
#define MODE_MUTI_LIVENESS					1		//多模态活体检测
#if ((MODE_2D_LIVENESS + MODE_3D_LIVENESS + MODE_MUTI_LIVENESS) > 1)
	#error "MODE_2D_LIVENESS and MODE_3D_LIVENESS, MODE_MUTI_LIVENESS can't be defined at the same time!"
#endif

#define FACE_REG_THRESHOLD_VALUE 			70
#define FACE_RECG_THRESHOLD_VALUE 		50
#define FACE_THRESHOLD_VALUE 				60

#define FACE_DETECT_RECGONIZE_ANGEL	(25.0)
#define FACE_DETECT_REGISTER_ANGEL	(12.0)   //10

#define ADDFACE_REG_DETECT_MAX_ANGEL  (35.0)
#define ADDFACE_REG_DETECT_MIN_ANGEL  (13.0)
#define ADDFACE_REG_OFFSET_ANGEL		7

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

enum FACE_REG_TYPE
{
	CREAT_PERSON = 0,
	PERSON_ADD_FACE,
};

#define ROLL_ENABLE						0		//注册时是否需要注册Roll 角度
#define RegAddFaceTypeNUM				4
enum REG_ADD_FACE_TYPE
{
	ADDFACE_NULL = 0,
	PITCH_UP,
	PITCH_DOWN,
	YAW_LEFT,
	YAW_RIGHT,
#if ROLL_ENABLE
	ROLL_LEFT,
	ROLL_RIGHT,
#endif
};

enum RS_RET_VALUE
{
	RS_RET_UNKOWN_ERR = -9,
	RS_RET_ALREADY_IN_DATABASE = -8,		
	RS_RET_NO_ANGLE = -7,
	RS_RET_NO_LIVENESS = -6,
	RS_RET_NO_QUALITY = -5,
	RS_RET_NO_FACE = -4,
	RS_RET_NO_FEATURE = -3,
	RS_RET_NO_RECO = -2,
	RS_RET_FAIL = -1,
	RS_RET_OK = 0,	
	RS_RET_PERSON_ADD_FACE = 1,
};

class RSApiHandle
{
public:
	RSApiHandle();
	~RSApiHandle();
    bool GetInitCfgInfo(string filePath, string& initInfo);
	bool Init(int AppType);
	RS_RET_VALUE FaceDetect(imageInfo img);
	RS_RET_VALUE FaceDetect_Depth(imageInfo color_img, imageInfo depth_img, bool *pIsMouseOpen, int  *distance, int *pFaceNum);
	RS_RET_VALUE RegisterFace(imageInfo img);
	RS_RET_VALUE RegisterFace_Depth(imageInfo color_img, imageInfo depth_img, int flag, rs_face *pRsFace,int RegAddFaceType,int *distance);
	RS_RET_VALUE DeleteFace(int faceId);
	RS_RET_VALUE GetAlbumSizeFromDataBase(unsigned short *faceSize);
	
protected:
	RSHandle m_Plicense;
	RSHandle m_PfaceRecognition;
	RSHandle m_pfaceDetect;
	RSHandle m_PfaceQuality;
#if defined(LIVENESS_DETECT_ENABLE) || defined(LIVENESS_DETECT_DEPTH_ENABLE)
	RSHandle m_PLivenessDetect;
#endif//LIVENESS_DETECT_ENABLE
};

int face_alg_get_last_detected_box(rs_rect * box);
int face_alg_set_NULL_detected_box();
int getLastIrData(uint8_t *pIrData, uint32_t data_len);

#endif // RS_API_HANDLE_H
