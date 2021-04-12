#ifndef _UTIL_H_
#define _UTIL_H_

#include "mqttutil.h"

#ifdef __cplusplus
extern "C" {
#endif
void freePointer(char **p); 
// int mysplit(char *src, char *first, char *last, char *separator);
int splitToTwo(char *src, char **first, char **last, char *separator);
void HexToStr(char *pszDest, char *pbSrc, int nLen);
void StrToHex(char *pbDest, char *pszSrc, int nLen);
void setTime(int year,int month,int day,int hour,int min,int sec);
void setTimestamp(int sec);
int StrGetUInt32(char* i_pSrc );
#ifdef __cplusplus
}
#endif

#endif
