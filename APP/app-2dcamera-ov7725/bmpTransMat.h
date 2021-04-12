/*
 * Copyright (c) 2019, wavewisdom-bj. All rights reserved.
 */

#ifndef _BMP_TRANS_MAT_H_
#define _BMP_TRANS_MAT_H_

typedef enum _WV_IMG_TYPE{ WV_GRAY = 0, WV_BGR  }WV_IMG_TYPE;

typedef	struct _WV_Image {
  int width;
  int height;
  int nWidthStep;
  int nChannel;
  int nType;
  unsigned char* pData;
} WV_Image;

bool saveBmp(const char *bmpName, void* imgBuf);

bool readBmp(const char *bmpName, void** imgBuf);

int BmpConvertImage(const void* pImgIn, WV_Image** Image);

int freeImage(WV_Image* Img);

int ImageConvertBmp(const  WV_Image Image, void**imgBuffer);

#endif  // _BMP_TRANS_MAT_H_
