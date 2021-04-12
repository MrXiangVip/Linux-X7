/*
 * Copyright 2018 NXP Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <linux/input.h>
#include <math.h>  

#include "bmpTransMat.h"  // API for load `.bmp` image files
#include "FIT.h"
#include <string>
#include <vector>

using namespace std;

// 定义算法实例
FIT face;

int getVersion() {
  /** 初始化模型 */
  int ret = face.Initial("");  // 一个实例只需执行一次，返回 0 表示初始化成功，模型初始化后其他接口可正常调用
  if (0 != ret) {
    printf("ERROR: model loading failed %d\n", ret);
    return 1;
  }
  char version[24];
  face.GetVersion(version);
  printf("FIT version is %s\n", version);
  return 0;
}

int registerFeature(char **argv) {
  std::vector<std::string> image_paths;
  image_paths.push_back(std::string(argv[1]));
  int i = 0;
  int ret;

  // 定义人脸特征向量长度，一个人脸的特征向量为 256 个浮点数，字节长度为 256*sizeof(float) 字节，即 2048 个字节
  const int featurelen = 256;
  // 定义两个特征向量长度的空间
  float features[featurelen * 1];

  void* imgbuf = NULL;
  printf("---------- src %s\n", image_paths[i].c_str());
  // 读取一张 bmp 图片，图片需为 BGR 三通道
  if (!readBmp(image_paths[i].c_str(), &imgbuf)) {
    printf("ERROR: cannot read image %s\n", image_paths[i].c_str());
    return -1;
  }
  printf("---------- read done\n");
  WV_Image* image = NULL;
  ret = BmpConvertImage(imgbuf, &image);
  if (ret != 0) {
    printf("ERRPR: cannot analyze picture format to bmp\n");
    return -1;
  }
  printf("LOADED: %s\n", image_paths[i].c_str());

    // 提取图像中最大人脸的人脸特征向量
    int light = 0;
    EFaceState  faceState;
    SFaceFt *feature = NULL;  // 存放人脸特征向量
    SRect faceRect;

    // 调用 FIT ExtractFeature 接口检测和提取图片中最大人脸特征向量：如果检测到合法的人脸，内部将对其进行特征提取，并返回 0，否则返回非 0 值，表示提取失败
    // image->pData 图像像素按 BGR 格式存放的字节流
    // image->width 图像宽度
    // image->height 图像高度
    // light 输出的人脸区域亮度值
    // faceState 输出的人脸状态信息
    // feature 输出的人脸特征
    // faceRect 输出的人脸框信息
    ret = face.ExtractFeature((byte*)image->pData,
      image->width, image->height, NULL, light,
      faceState, &feature, &faceRect);
    if (0 != ret) {
      printf("ERROR: feature extraction failed  %d\n", ret);
      free(imgbuf);
      imgbuf = NULL;
      freeImage(image);
      image = NULL;
	  return 1;
    }

    // 打印检测到的人脸信息，light　表示人脸区域亮度值，rect 标识人脸框位置信息
    printf("DETECTED INFO: light -> %d   "
           "rect(x, y, height, width) -> (%d, %d, %d, %d)\n\n",
      light, faceRect.x, faceRect.y, faceRect.height, faceRect.width);

    // 收集人脸特征向量
    memcpy(features + i * featurelen, (float*)feature->pbLocalFt,
      sizeof(float) * featurelen);

	ret = face.SaveFeature("feature.f", feature);

    // 释放 SFaceFt 内存空间
    face.ReleaseFeature(&feature);

    free(imgbuf);
    imgbuf = NULL;
    freeImage(image);
    image = NULL;
}

int main(int argc, char ** argv) {
#if 0
	struct timeval tv_rs_start;
	struct timeval tv_rs_end;

	uint32_t total_us = 0;
	gettimeofday(&tv_rs_start, NULL);
	printf("start \n");

	int j = 0;
	unsigned char YUV422Buffer[640 * 480];
	memset(YUV422Buffer, 0, sizeof(YUV422Buffer));
	for(j=0; j<640*480-1; j++)
	{
		YUV422Buffer[j] = YUV422Buffer[j+1];
	}

	gettimeofday(&tv_rs_end, NULL);
	total_us = (tv_rs_end.tv_sec - tv_rs_start.tv_sec) * 1000000 + (tv_rs_end.tv_usec - tv_rs_start.tv_usec);
	printf("total us is %d usec\n", total_us);
#endif

	getVersion();
	registerFeature(argv);

	return 0;
}
