/*
 * Copyright (c) 2019, wavewisdom-bj. All rights reserved.
 */

#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#ifndef  WINLIB
#include "LinuxCppDef.h"
#endif

enum EFaceState {
  FaceSTD,            // 0  标准合格人脸
  FaceNone,           // 1  人脸检测不到
  FaceLeftSide,       // 2  人脸偏左
  FaceRightSide,      // 3  人脸偏右
  FaceUpSide,         // 4  人脸偏上
  FaceDownSide,       // 5  人脸偏下
  FaceTooNear,        // 6  人脸偏近
  FaceTooFar,         // 7  人脸偏远
  FaceNoEye,          // 8  眼睛定位不到
  FaceSame,           // 9  重复脸（用于注册筛选）
  FaceAbnormal,       // 10 人脸规则验证失败，有饰物、遮挡
  FaceMulti,          // 11 人脸规则验证失败，有饰物、遮挡
  FaceTooMuchLight,   // 12 人脸过曝 
  FaceTooLittleLight  // 13 人脸过暗
};

struct SFaceFt {
  short nFaceCount;   //人脸数
  short nVersion;     //人脸特征版本序号
  int nLocalFtLen;    //本地特征数据总长度
  byte *pbLocalFt;    //本地特征数据
  int nTimeUpdate;    //特征更新时间
  SFaceFt(void) {
    nVersion = 5;
    nFaceCount = 0;   //人脸数
    nLocalFtLen = 0;  //本地特征数据总长度
    pbLocalFt = 0;    //本地特征数据
    nTimeUpdate = 20000101;
  }
};

#define INPARA const 
#define IO 
#define OUTPUT
#define INARRAY

#endif