#ifndef FIT_API_HANDLE_H
#define FIT_API_HANDLE_H

#include <string>
#include <fstream>
#include <sstream>

#include "FIT.h"
#include "bmpTransMat.h"  // API for load `.bmp` image files
#include "pfmgr.h"

extern FIT g_face;

/* �㷨����ֵ��
0��ʾע��ɹ���
-1��ʾδ��ʼ����
-2��ʾ�������ʧ�ܻ�δ��⵽������
-3��ʾ�����ؼ��㶨λʧ�ܣ�
-4��ʾͼ��Ϊ�գ�
-5��ʾ��ȡ����ʧ�ܣ�
-8��ʶ�����ⲻͨ����
-10��ʾROI���������ֵ���ڻ�����趨��ֵ��
-11��ʾ��⵽����������������������ֵ���ڻ�����趨��ֵ����ͨ��SetPara("faceMinLight", val)��SetPara("faceMaxLight", val)������С������ֵ�����������ֵ����
-20��ʾ��⵽�������������
-21��ʾ��⵽������ƫ��
-22��ʾ��⵽������ƫ�ϣ�
-23��ʾ��⵽������ƫ�ң�
-24��ʾ��⵽������ƫ�£�
-25��ʾ��⵽�����������С
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
