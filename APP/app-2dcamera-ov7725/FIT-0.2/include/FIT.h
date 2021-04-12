/*
 * Copyright (c) 2021, wavewisdom-bj. All rights reserved.
 *
 * @file     FIT.cpp
 * @brief    FIT　
 * @date     2021年01月20日
 */

#ifndef FIT_H
#define FIT_H

#ifdef FIT_API_EXPORT
#define FIT_API_PUBLIC __attribute__((visibility("default")))
#else
#define FIT_API_PUBLIC
#endif

#include "Definitions.h"

typedef struct SRect
{
  int x;
  int y;
  int width;
  int height;
} SRect;

class FIT_API_PUBLIC FIT
{
public:
  FIT(void);
  ~FIT(void);

  // 初始化
  // 返回值：0表示成功，其他表示缺少配置文件
  int Initial(INPARA char *configFilePath);

  // 算法红外图像转可见光图像
  // 返回值：0表示成功，-1表示未初始化，-4表示图像为空
  int InfraredTranformColor(INPARA byte *imageBuffer,                                                    //图像RGB数据
                            INPARA int nWidth, INPARA int nHeight, INPARA int nChannels,                 //图像尺寸
                            OUTPUT byte **colorImageBuffer,                                              //输出彩色图RGB数据
                            OUTPUT int &colorWidth, OUTPUT int &colorHeight, OUTPUT int &colorChannels); //输出彩色图尺寸

  // 释放资源
  void Release();

  // 注册，结合ClearRegisteredFace使用
  // 返回值：0表示注册成功，-1表示未初始化，-2表示人脸检测失败，-4表示图像为空，-5表示重复姿态，-8标识活体检测不通过
  int Register(INPARA byte *imageBuffer,              //图像RGB数据
               INPARA int nWidth, INPARA int nHeight, //图像尺寸
               INPARA SRect *RoiRect,                 //输入图像处理区域，若为NULL 则处理全图  否则处理ROI处理区域
               OUTPUT int &lightValue,                //若检测到人脸，则返回人脸区域亮度值，否则返回RoiRect 处理区域亮度值
               OUTPUT EFaceState &faceState,          //图像中人脸的状态，见EFaceState定义
               OUTPUT int &numRegistered,             //已注册的人脸张数
               OUTPUT SRect *faceRect = NULL,         //人脸位置
               bool open_antiface = true);           // 设置为 true 开启活体检测，默认关闭

  // 注册，结合ClearRegisteredFace使用
  // 返回值：0表示注册成功，-1表示未初始化，-2表示人脸检测失败，-3表示重复注册，-4表示图像为空，-5表示重复姿态，-8标识活体检测不通过
  int RegisterEx(INPARA byte *imageBuffer,              //图像RGB数据
                 INPARA int nWidth, INPARA int nHeight, //图像尺寸
                 INPARA SRect *RoiRect,                 //输入图像处理区域，若为NULL 则处理全图  否则处理ROI处理区域
                 OUTPUT int &lightValue,                //若检测到人脸，则返回人脸区域亮度值，否则返回RoiRect 处理区域亮度值
                 OUTPUT EFaceState &faceState,          //图像中人脸的状态，见EFaceState定义
                 OUTPUT int &numRegistered,             //已注册的人脸张数
                 INARRAY SFaceFt **features,            //参考人脸库特征数据
                 INPARA int nPeopleNum,                 //人数n
                 OUTPUT float &score,                   //相似度得分（满分100）
                 OUTPUT int &peopleID,                  //重复注册者ID号
                 OUTPUT SRect *faceRect = NULL,         //人脸位置
                 bool open_antiface = true);           // 设置为 true 开启活体检测，默认关闭

  // 清空已注册的人脸--重新注册一个新的用户时必须先调用本接口
  void ClearRegisteredFace();

  // 用于注册完成后，获取当前注册的人脸特征
  // 返回值：非0表示指针地址，0表示特征尚未注册
  SFaceFt *GetFeature(bool bAutoClearFace = false); //默认每次获取完之后自动清空注册的人脸

  // 载入特征文件
  // 返回值：0表示成功，-1表示打开文件失败
  int LoadFeature(INPARA char *filePath, OUTPUT SFaceFt **feature);

  // 保存特征文件
  // 返回值：0表示成功，-1表示创建文件失败，-2表示feature为空
  int SaveFeature(INPARA char *filePath, INPARA SFaceFt *feature);

  // 释放人脸特征数据空间
  void ReleaseFeature(IO SFaceFt **feature);

  // 1:1 识别
  // 返回值：0表示识别通过，-1表示未初始化，-2表示人脸检测失败，-3表示识别不通过，-8标识活体检测不通过
  int Recognize1_1(INPARA byte *imageBuffer,              //图像RGB数据
                   INPARA int nWidth, INPARA int nHeight, //图像尺寸
                   INPARA SRect *RoiRect,                 //输入图像处理区域，若为NULL 则处理全图  否则处理ROI处理区域
                   IO SFaceFt *feature,                   //参考人脸特征数据
                   OUTPUT int &lightValue,                //若检测到人脸，则返回人脸区域亮度值，否则返回RoiRect 处理区域亮度值
                   OUTPUT EFaceState &faceState,          //图像中人脸的状态，见EFaceState定义
                   OUTPUT float &score,                   //相似度得分（满分100）
                   OUTPUT SRect *faceRect = NULL,         //人脸位置
                   bool open_antiface = true);           // 设置为 true 开启活体检测，默认关闭

  //1:n 识别
  //返回值：0~n-1表示识别通过者的ID，-1表示未初始化，-2表示人脸检测失败，-3表示识别不通过，-8标识活体检测不通过
  int Recognize1_n(INPARA byte *imageBuffer,              //图像RGB数据
                   INPARA int nWidth, INPARA int nHeight, //图像尺寸
                   INPARA SRect *RoiRect,                 //输入图像处理区域，若为NULL 则处理全图  否则处理ROI处理区域
                   INARRAY SFaceFt **features,            //参考人脸库特征数据
                   INPARA int nPeopleNum,                 //人数n
                   OUTPUT int &lightValue,                //若检测到人脸，则返回人脸区域亮度值，否则返回RoiRect 处理区域亮度值
                   OUTPUT EFaceState &faceState,          //图像中人脸的状态，见EFaceState定义
                   OUTPUT float &score,                   //相似度得分（满分100）
                   OUTPUT SRect *faceRect,                //人脸位置
                   bool open_antiface = true);           // 设置为 true 开启活体检测，默认关闭

  //提取特征
  //返回值：0表示成功，-1表示未初始化，-2表示人脸检测失败，-8标识活体检测不通过
  int ExtractFeature(INPARA byte *imageBuffer,              //图像RGB数据
                     INPARA int nWidth, INPARA int nHeight, //图像尺寸
                     INPARA SRect *RoiRect,                 //输入图像处理区域，若为NULL 则处理全图  否则处理ROI处理区域
                     OUTPUT int &lightValue,                //若检测到人脸，则返回人脸区域亮度值，否则返回RoiRect 处理区域亮度值
                     OUTPUT EFaceState &faceState,          //图像中人脸的状态，见EFaceState定义
                     OUTPUT SFaceFt **feature,              //输出特征
                     OUTPUT SRect *faceRect = NULL,         //人脸位置
                     bool open_antiface = true);           // 设置为 true 开启活体检测，默认关闭

  //1:n 识别
  //返回值：0~n-1表示识别通过者的ID，-1表示未初始化，-3表示识别不通过
  int Recognize1_n(SFaceFt *feature,           //输入人脸特征
                   INARRAY SFaceFt **features, //参考人脸库特征数据
                   INPARA int nPeopleNum,      //人数n
                   OUTPUT float &score);       //相似度得分（满分100）

  // 获取版本号
  void GetVersion(OUTPUT char *version); //version: 长度预分配24，实际使用19，如：1.0.0.0-2013.08.26

  // 设置判决阈值
  void SetThreshold(INPARA float threshold);

  // 设置及获取参数
  // recognizeThreshold -- 识别阈值，50~150, 默认 85
  // posFilterThreshold -- 注册相邻姿态过滤阈值，0.8~1.0, 默认 0.93， 值越大越宽松
  // faceMinSize -- 人脸检测最小窗口， 0~width， 默认150
  // faceMaxSize -- 人脸检测最大窗口， 0~width， 默认400
  // faceMinLight -- 人脸区域最低亮度， 0~255， 默认80
  // faceMaxLight -- 人脸区域最高亮度， 0~255， 默认180
  // regSkipFace -- 注册启动时跳过的人脸张数， 0~20， 默认10
  // antifaceThreshold -- 活体检测阈值，0.0~1.0，默认 0.5
  bool SetPara(INPARA char *name, INPARA float val);
  bool GetPara(INPARA char *name, OUTPUT float &val);

  // 将BGR888格式的数据保存为jpg格式的文件，目前只支持保存".jpg"后缀名的文件
  // filename ---- 保存的路径
  // bgrbuffer --- 传入图像的BGR888字节流数据，格式为[B, G, R, B, G, R, ... , B, G, R]
  // width -----   传入图像的宽
  // height -----  传入图像的高
  // channels ---- 传入图像的通道，因为原始数据为BGR888的数据，因此通道只能为3，由于图片是黑白照片，内部会转换为单通道灰度图进行保存
  // quality ----- 保存的图像质量设置[0-100],数字越小，图像质量越低，保存的图片越小
  int WriteJPG(INPARA char *filename,
               INPARA byte *bgrBuffer,
               INPARA int width, 
               INPARA int height, 
               INPARA int channels,
               INPARA int quality
              );

private:
  //算法模块
  void *m_pFitCv;

public:
  int m_threadID;
};

#endif
