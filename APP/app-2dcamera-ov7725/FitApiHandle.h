#ifndef FIT_API_HANDLE_H
#define FIT_API_HANDLE_H

#include <string>
#include <fstream>
#include <sstream>

#include "FIT.h"
#include "bmpTransMat.h"  // API for load `.bmp` image files
#include "pfmgr.h"

extern FIT g_face;

/* 算法返回值：
0表示注册成功，
-1表示未初始化，
-2表示人脸检测失败或未检测到人脸，
-3表示人脸关键点定位失败，
-4表示图像为空，
-5表示提取特征失败，
-8标识活体检测不通过，
-10表示ROI区域的亮度值高于或低于设定阈值，
-11表示检测到的人脸，但人脸区域亮度值高于或低于设定阈值（可通过SetPara("faceMinLight", val)和SetPara("faceMaxLight", val)调整最小亮度阈值和最大亮度阈值），
-20表示检测到的人脸面积过大，
-21表示检测到的人脸偏左，
-22表示检测到的人脸偏上，
-23表示检测到的人脸偏右，
-24表示检测到的人脸偏下，
-25表示检测到的人脸区域过小
*/
typedef enum
{
	RS_RET_SUCCESS=0,
	RS_RET_NO_INITED=-1,
	RS_RET_FACECHK_FAILED=-2,
	RS_RET_FACEPOS_FAILED=-3,
	RS_RET_IMAGE_NULL=-4,
	RS_RET_NO_FEATURE=-5,
	RS_RET_NO_LIVENESS=-8,
	RS_RET_ROILIGHT_NoOK=-10,	
	RS_RET_FACELIGHT_NoOK=-11,
	RS_RET_FACESIZE_BIG=-20,
	RS_RET_FACE_LEFT=-21,
	RS_RET_FACE_UP=-22,
	RS_RET_FACE_RIGHT=-23,
	RS_RET_FACE_DOWN=-24,
	RS_RET_FACESIZE_SMALL=-25,
}RegStatusType;

int FitlibInit();
void GetFitVersion(char * pstrVer);
float FaceVerify( float* feature1,  float* feature2);
int recognizeFace(const byte *imgBuf , uUID * pUU_ID,  int *pPersonID);
int registerFace(const byte *imgBuf);
void saveFace();


void RGB565ToRGB(unsigned short *pInput, unsigned int nInputSize, unsigned char *pRgb);
void RGB565ToRGBA(unsigned short *pInput, unsigned int nInputSize, unsigned char *pRgba);
void RotateRGB(const unsigned char* src,unsigned char* result,int width,int height,int mode);
void Rgb565BE2LE(const uint16_t* src_rgb, uint16_t* dst_rgb, int width);
void ScaleRGB(void* pDest, int nDestWidth, int nDestHeight, int nDestBits, void* pSrc, int nSrcWidth, int nSrcHeight, int nSrcBits);
int RGB888ToRGB565(void * psrc, int w, int h, void * pdst);
#endif // FIT_API_HANDLE_H
