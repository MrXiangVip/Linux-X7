/*
 * Copyright (c) 2019, wavewisdom-bj. All rights reserved.
 */

#ifndef LINUX_CPP_DEF_H
#define LINUX_CPP_DEF_H

#define DEBUG_OUT_INFO 1

typedef long LONG;
typedef unsigned char       BYTE;
typedef unsigned char       byte;
typedef unsigned char  *LPBYTE;     //far
typedef unsigned int        UINT;
typedef unsigned long       DWORD;
typedef unsigned short      WORD;
typedef WORD             *LPWORD;
typedef unsigned int        UINT;
typedef unsigned long  ULONG;

typedef struct tagPOINT {
  LONG  x;
  LONG  y;
} POINT, *PPOINT,  *NPPOINT,  *LPPOINT;

#define MAX_PATH 256

#define TRUE 1
#define FALSE 0

#define FAR                 far
#define NEAR                near

typedef int INT_PTR, *PINT_PTR;
typedef unsigned int UINT_PTR, *PUINT_PTR;

typedef long LONG_PTR, *PLONG_PTR;
typedef unsigned long ULONG_PTR, *PULONG_PTR;

typedef unsigned short UHALF_PTR, *PUHALF_PTR;
typedef short HALF_PTR, *PHALF_PTR;

typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LPARAM;
typedef LONG_PTR            LRESULT;

//typedef char *va_list;
typedef bool BOOL;
typedef unsigned char BYTE;
typedef unsigned int UINT;
typedef long LONG;
typedef unsigned short int WORD;
typedef char TCHAR;

typedef const TCHAR * LPCTSTR;
typedef TCHAR * LPTSTR;

typedef DWORD COLORREF;

#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(rgb)  ((BYTE)(rgb))
#define GetGValue(rgb)  ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb)  ((BYTE)((rgb) >> 16))
#define BI_RGB              0
#define _MAX_PATH 255

typedef struct tagRGBQUAD {
  BYTE    rgbBlue;
  BYTE    rgbGreen;
  BYTE    rgbRed;
  BYTE    rgbReserved;
} RGBQUAD,*LPRGBQUAD;



typedef struct tagBITMAPINFOHEADER {
  DWORD  biSize;
  LONG   biWidth;
  LONG   biHeight;
  WORD   biPlanes;
  WORD   biBitCount;
  DWORD  biCompression;
  DWORD  biSizeImage;
  LONG   biXPelsPerMeter;
  LONG   biYPelsPerMeter;
  DWORD  biClrUsed;
  DWORD  biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct _tagBITMAPFILEHEADER {
  WORD    bfType;
  DWORD   bfSize;
  WORD    bfReserved1;
  WORD    bfReserved2;
  DWORD   bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagSIZE {
  LONG        cx;
  LONG        cy;
} SIZE, *PSIZE, *LPSIZE;

typedef struct tagRECT {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
} RECT, *PRECT,  *NPRECT,  *LPRECT;

typedef const RECT * LPCRECT;

typedef struct sRECT {
  LONG    left;
  LONG    top;
  LONG    right;
  LONG    bottom;
  POINT CenterPoint() {
    POINT p;
    p.x = (left+right) / 2;
    p.y = (top+bottom) / 2;
    return p;
  }
} SRECT, CRect;

#endif  // LINUX_CPP_DEF_H
