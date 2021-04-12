/*
 * Copyright (c) 2019, wavewisdom-bj. All rights reserved.
 */

#include "bmpTransMat.h"

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

typedef unsigned short int WORD;
typedef unsigned int DWORD;
typedef int LONG;
typedef unsigned char BYTE;

/*
 * 位图文件头
 */
#pragma pack(1)       // 将结构体中成员按n字节对齐
typedef struct tagBITMAPFILEHEADER {
  WORD bfType;        // 文件类型，必须为BM
  DWORD bfSize;       // 指定文件大小，以字节为单位（3-6字节，低位在前）
  WORD bfReserved1;   // 文件保留字，必须为0
  WORD bfReserved2;   // 文件保留字，必须为0
  DWORD bfOffBits;    // 从文件头到实际位图数据的偏移字节数（11-14字节，低位在前）
} BITMAPFILEHEADER;

/*
 * 位图信息头
 */
typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;             // 本结构所占用字节数，为40。注意：实际操作中则有44，这是字节补齐的原因
  LONG biWidth;             // 位图的宽度，以像素为单位
  LONG biHeight;            // 位图的高度，以像素为单位
  WORD biPlanes;            // 目标设备的级别，必须为1
  WORD biBitCount;          // 每个像素所需的位数，1（双色），4(16色），8(256色）16(高彩色)，24（真彩色）或32之一
  DWORD biCompression;      // 位图压缩类型，0（不压缩），1(BI_RLE8压缩类型）或2(BI_RLE4压缩类型）之一
  DWORD biSizeImage;        // 位图的大小(其中包含了为了补齐行数是4的倍数而添加的空字节)，以字节为单位
  LONG biXPelsPerMeter;     // 位图水平分辨率，每米像素数
  LONG biYPelsPerMeter;     // 位图垂直分辨率，每米像素数
  DWORD biClrUsed;          // 位图实际使用的颜色表中的颜色数，若该值为0,则使用颜色数为2的biBitCount次方
  DWORD biClrImportant;     // 位图显示过程中重要的颜色数，若该值为0,则所有的颜色都重要
} BITMAPINFOHEADER;
#pragma pack()              // 取消自定义字节方式

typedef struct tagRGBQUAD {
  BYTE rgbBlue;             // 蓝色的亮度（0-255)
  BYTE rgbGreen;            // 绿色的亮度（0-255)
  BYTE rgbRed;              // 红色的亮度（0-255)
  BYTE rgbReserved;         // 保留，必须为0
} RGBQUAD;

bool saveBmp(const char *bmpName, void* imgBuf) {
  FILE* fp = NULL;
  BITMAPFILEHEADER fileHead;
  BITMAPINFOHEADER head;
  int bmpWidth = 0;
  int bmpHeight = 0;
  int biBitCount = 0;
  int offset = 0;
  int lineByte = (bmpWidth * biBitCount / 8 + 3) / 4 * 4;
  if (!imgBuf) {
    return 0;
  }
  fp = fopen(bmpName, "wb");
  if (fp == 0) {
    return 0;
  }
  memcpy(&fileHead, imgBuf, sizeof(BITMAPFILEHEADER));
  fwrite(&fileHead, sizeof(BITMAPFILEHEADER), 1, fp);
  memcpy(&head, (BYTE*)imgBuf + sizeof(BITMAPFILEHEADER),
    sizeof(BITMAPINFOHEADER));
  fwrite(&head, sizeof(BITMAPINFOHEADER), 1, fp);
  offset = sizeof(BITMAPINFOHEADER)+sizeof(BITMAPFILEHEADER);
  biBitCount = head.biBitCount;
  if (biBitCount == 8) {
    fwrite((BYTE*)imgBuf + offset, sizeof(RGBQUAD), 256, fp);
    offset += sizeof(RGBQUAD)* 256;
  }
  bmpHeight = head.biHeight;
  bmpWidth = head.biWidth;
  biBitCount = head.biBitCount;
  lineByte = (bmpWidth * biBitCount / 8 + 3) / 4 * 4;
  fwrite((BYTE*)imgBuf + offset, bmpHeight*lineByte, 1, fp);
 
 /* 将文件同步,及时写入磁盘,不然，系统重启动，导致文件内容丢失 */
 fflush(fp);	 /* 刷新内存缓冲，将内容写入内核缓冲 */
 fsync(fileno(fp));  /* 将内核缓冲写入磁盘 */
  fclose(fp);
  return 1;
}

bool readBmp(const char *bmpName, void** imgBuf) {
  BITMAPFILEHEADER fileHead;
  BITMAPINFOHEADER head;
  int bmpWidth = 0;
  int bmpHeight = 0;
  int biBitCount = 0;
  int lineByte = 0;
  int offset = 0;
  int ret = 0;
  FILE *fp = fopen(bmpName, "rb");
  if (fp == 0) {
    return 0;
  }
  ret = fread(&fileHead, sizeof(BITMAPFILEHEADER), 1, fp);
  if (ret <= 0) {
    return 1;
  }
  ret = fread(&head, sizeof(BITMAPINFOHEADER), 1, fp);
  if (ret <= 0) {
    return 1;
  }
  bmpWidth = head.biWidth;
  bmpHeight = head.biHeight;
  biBitCount = head.biBitCount;
  lineByte = (bmpWidth * biBitCount / 8 + 3) / 4 * 4;
  offset = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
  BYTE* pBmpBuf = (BYTE*)malloc(sizeof(BYTE) * (offset + sizeof(RGBQUAD) *
    256 + bmpHeight*lineByte));
  memcpy(pBmpBuf, &fileHead, sizeof(BITMAPFILEHEADER));
  memcpy(pBmpBuf + sizeof(BITMAPFILEHEADER), &head, sizeof(BITMAPINFOHEADER));
  if (biBitCount == 8) {
    ret = fread(pBmpBuf + offset, sizeof(RGBQUAD), 256, fp);
    if (ret <= 0) {
      return 1;
    }
    offset += sizeof(RGBQUAD) * 256;
  }
  ret = fread(pBmpBuf + offset, 1, lineByte * bmpHeight, fp);
  if (ret <= 0) {
    return 1;
  }
  *imgBuf = pBmpBuf;
  fclose(fp);
  return 1;
}

int BmpConvertImage(const void* pImgIn, WV_Image** Image) {
  unsigned char *imgBuf = (unsigned char*)pImgIn;
  BITMAPINFOHEADER head;
  memcpy(&head, imgBuf + sizeof(BITMAPFILEHEADER), sizeof(BITMAPINFOHEADER));
  // unsigned char *pBmpBuf = NULL;   // 读入图像数据的指针
  int bmpWidth = 0;                // 图像的宽
  int bmpHeight = 0;               // 图像的高
  int biBitCount = 0;              // 图像类型，每像素位数
  bmpHeight = head.biHeight;
  bmpWidth = head.biWidth;
  biBitCount = head.biBitCount;
  int lineByte = (bmpWidth * biBitCount / 8 + 3) / 4 * 4;
  int lineByte2 = lineByte;
  if (lineByte / bmpWidth >= 4) {
    lineByte = (bmpWidth * 24 / 8 + 3) / 4 * 4;
  }
  int offset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  if (biBitCount == 8) {
    offset += sizeof(RGBQUAD)* 256;
  }
  WV_Image* pParamIn = NULL;
  pParamIn = (WV_Image*)malloc(sizeof(WV_Image)* 1);
  if (pParamIn == NULL) {
    return -1;
  }
  memset(pParamIn, 0, sizeof(WV_Image));
  printf("---- pData lineByte is %d bmpHeight is %d size is %d\n", lineByte, bmpHeight, sizeof(unsigned char) * lineByte * bmpHeight);
  pParamIn->pData = (unsigned char*)malloc(sizeof(unsigned char) * lineByte *
    bmpHeight);
  if (pParamIn->pData == NULL) {
    return -1;
  }
  unsigned char* pData = imgBuf + offset;
  if (biBitCount == 8) {
    pParamIn->nType = WV_GRAY;
  } else if (biBitCount == 24 || biBitCount == 32) {
    pParamIn->nType = WV_BGR;
  } else {
    return -2;
  }
  pParamIn->height = bmpHeight;
  pParamIn->width = bmpWidth;
  pParamIn->nChannel = lineByte /pParamIn->width;
  pParamIn->nWidthStep = pParamIn->width*pParamIn->nChannel;
  if (biBitCount == 24 || biBitCount == 8) {
    unsigned char* pData1 = pParamIn->pData + (bmpHeight - 1) *
      pParamIn->nWidthStep;
    unsigned char* pData2 = pData;
    int channel = biBitCount / 8;
    for (int i = 0; i < bmpHeight; i++) {
      for (int j = 0; j < bmpWidth; j++) {
        for (int k = 0; k < channel; k++) {
          pData1[j * channel+k] = pData2[j*channel + k];
        }
      }
      pData1 -= pParamIn->nWidthStep;
      pData2 += lineByte;
    }
    pData1 = NULL;
    pData2 = NULL;
  } else if (biBitCount == 32) {
    unsigned char* pData1 = pParamIn->pData + (bmpHeight - 1) *
      pParamIn->nWidthStep;
    unsigned char* pData2 = pData;
    int channel = biBitCount / 8;
    for (int i = 0; i < bmpHeight; i++) {
      for (int j = 0; j < bmpWidth; j++) {
        pData1[j * 3] = pData2[j*channel];
        pData1[j * 3 + 1] = pData2[j*channel + 1];
        pData1[j * 3 + 2] = pData2[j*channel + 2];
      }
      pData1 -= pParamIn->nWidthStep;
      pData2 += lineByte2;
    }
    pData1 = NULL;
    pData2 = NULL;
  } else {
    return -1;
  }
  imgBuf = NULL;
  pData = NULL;
  *Image = pParamIn;
  return 0;
}

int ImageConvertBmp(const  WV_Image Image, void**imgBuffer) {
  if (Image.pData == NULL  || Image.height <=0 || Image.width <= 0
    || Image.nWidthStep/Image.nChannel != Image.width) {
    return -2;
  }
  int bmpWidth = 0;        // 图像的宽
  int bmpHeight = 0;       // 图像的高
  int biBitCount = 0;      // 图像类型，每像素位数
  bmpHeight = Image.height;
  bmpWidth = Image.width;
  biBitCount = Image.nWidthStep / Image.width * 8;
  int lineByte = (bmpWidth * biBitCount / 8 + 3) / 4 * 4;
  // int lineByte2 = lineByte;
  if (lineByte / bmpWidth >= 4) {
    lineByte = (bmpWidth * 24 / 8 + 3) / 4 * 4;
  }
  unsigned char* imgBuf = NULL;
  int imagesize = 0;
  int offset = 0;
  if (Image.nChannel==3) {
    biBitCount = 24;
    imagesize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
      lineByte * bmpHeight;
    offset = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
  }
  if (Image.nChannel == 1) {
    biBitCount = 8;
    imagesize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
      sizeof(RGBQUAD)* 256 + lineByte*bmpHeight;
    offset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
      sizeof(RGBQUAD)* 256;
  }
  imgBuf = (unsigned char*)malloc(sizeof(unsigned char)*imagesize);
  if (imgBuf == NULL) {
    return -1;
  }
  memset(imgBuf, 0, imagesize);
  BITMAPFILEHEADER fileHead;
  memset(&fileHead, 0, sizeof(BITMAPFILEHEADER));
  fileHead.bfOffBits = offset;
  fileHead.bfSize = imagesize;
  fileHead.bfType = 19778;
  memcpy(imgBuf, &fileHead, sizeof(BITMAPFILEHEADER));
  BITMAPINFOHEADER head;
  memset(&head, 0, sizeof(BITMAPINFOHEADER));
  head.biSize = 40;
  head.biHeight = bmpHeight;
  head.biWidth = bmpWidth;
  head.biBitCount = biBitCount;
  head.biPlanes = 1;
  memcpy(imgBuf + sizeof(BITMAPFILEHEADER), &head, sizeof(BITMAPINFOHEADER));
  if (Image.nChannel == 1) {
    RGBQUAD temp[256];
    memset(temp, 0, sizeof(RGBQUAD)* 256);
    for (int i = 0; i < 256;i++) {
      temp[i].rgbRed = i;
      temp[i].rgbGreen = i;
      temp[i].rgbBlue = i;
    }
    memcpy(imgBuf + sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER),
      &temp, sizeof(RGBQUAD)* 256);
  }
  unsigned char* pData = imgBuf + offset;
  if (biBitCount == 24 || biBitCount == 8) {
    unsigned char* pData1 = Image.pData + (bmpHeight - 1)*Image.nWidthStep;
    unsigned char* pData2 = pData;
    int channel = biBitCount / 8;
    for (int i = 0; i < bmpHeight; i++) {
      for (int j = 0; j < bmpWidth; j++) {
        for (int k = 0; k < channel; k++) {
          pData2[j * channel + k] = pData1[j*channel + k];
        }
      }
      pData1 -= Image.nWidthStep;
      pData2 += lineByte;
    }
    pData1 = NULL;
    pData2 = NULL;
  } else {
    return -1;
  }
  pData = NULL;
  *imgBuffer = (void*)imgBuf;
  return 0;
}

int freeImage(WV_Image* Img) {
  if (Img == NULL) {
    return 0;
  }
  if (Img->pData != NULL) {
    free(Img->pData);
    Img->pData = NULL;
  }
  if (Img != NULL) {
    free(Img);
    Img = NULL;
  }
  return 0;
}
