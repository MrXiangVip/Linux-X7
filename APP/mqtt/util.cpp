#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "util.h"
#include "log.h"

#include <ctype.h>

void freePointer(char **p) {
	if (*p != NULL) {
		free(*p);
		*p = NULL;
	}
}

int mysplit(char *src, char *first, char *last, char *separator) {
#if 0
	first[0] = 'a';
	last[0] = 'b';
	first[1] = '\0';
	last[1] = '\0';
#endif
	char *second=(char*)strstr(src, separator); 
	int first_len = strlen(src) - strlen(second);
	strncpy(first, src, first_len);
	first[first_len] = '\0';
	strncpy(last, "b", 1);
	last[1] = '\0';
	int last_len = strlen(second) - strlen(separator);
	if (last_len > 0) {
		strcpy(last, second + strlen(separator));
	} else {
		last[0] = '\0';
	}
	return 1;
}

int splitToTwo(char *src, char **first, char **last, char *separator) {
	if (src == NULL || strlen(src) == 0) //如果传入的地址为空或长度为0，直接终止 
	{
		printf("---- is NULL\n");
		return -1;
	}
	// printf("split\n\n");

	char *second=(char*)strstr(src, separator); 

	int first_len = strlen(src) - strlen(second);
	*first = (char*)malloc(first_len);
	// memset(first, '\0', BUF_LEN);
	// memset(last, '\0', BUF_LEN);
	strncpy(*first, src, first_len);
	// printf("src is %s\n", src);
	// printf("first is %s\n", *first);

	int last_len = strlen(second) - strlen(separator);
	if (last_len > 0) {
		*last = (char*)malloc(last_len);
		strcpy(*last, second + strlen(separator));
		// printf("last is %s\n", *last); 
	}

	return 0;
}

/*
// C prototype : void HexToStr(char *pszDest, byte *pbSrc, int nLen)
// parameter(s): [OUT] pszDest - 存放目标字符串
//  [IN] pbSrc - 输入16进制数的起始地址
//  [IN] nLen - 16进制数的字节数
// return value:
// remarks : 将16进制数转化为字符串
*/
void HexToStr(char *pszDest, char *pbSrc, int nLen)
{
	char    ddl, ddh;
	int i;
	for (i = 0; i < nLen; i++)
	{
		ddh = 48 + pbSrc[i] / 16;
		ddl = 48 + pbSrc[i] % 16;
		if (ddh > 57) ddh = ddh + 7;
		if (ddl > 57) ddl = ddl + 7;
		pszDest[i * 2] = ddh;
		pszDest[i * 2 + 1] = ddl;
	}

	pszDest[nLen * 2] = '\0';

}

/*
// C prototype : void StrToHex(byte *pbDest, char *pszSrc, int nLen)
// parameter(s): [OUT] pbDest - 输出缓冲区
//  [IN] pszSrc - 字符串
//  [IN] nLen - 16进制数的字节数(字符串的长度/2)
// return value:
// remarks : 将字符串转化为16进制数
 */
void StrToHex(char *pbDest, char *pszSrc, int nLen)
{
	int i;
	char h1, h2;
	unsigned char s1, s2;
	for (i = 0; i < nLen; i++)
	{
		h1 = pszSrc[2 * i];
		h2 = pszSrc[2 * i + 1];

		s1 = toupper(h1) - 0x30;
		if (s1 > 9)
			s1 -= 7;

		s2 = toupper(h2) - 0x30;
		if (s2 > 9)
			s2 -= 7;

		pbDest[i] = s1 * 16 + s2;
	}
}


void setTime(int year,int month,int day,int hour,int min,int sec)
{
	struct tm tptr;
	struct timeval tv;

	tptr.tm_year = year - 1900;
	tptr.tm_mon = month - 1;
	tptr.tm_mday = day;
	tptr.tm_hour = hour;
	tptr.tm_min = min;
	tptr.tm_sec = sec;

	tv.tv_sec = mktime(&tptr);
	tv.tv_usec = 0;
	settimeofday(&tv, NULL);
}

void setTimestamp(int sec)
{
	struct timeval tv;
	tv.tv_sec = sec;
	tv.tv_usec = 0;
	int ret = settimeofday(&tv, NULL);
	if (ret == 0) {
		log_info("set time of day success\n");
	} else {
		log_info("set time of day fail ret %d\n", ret);
	}
}

int StrGetUInt32( char* i_pSrc )
{
    int u32Rtn = 0;
    for ( char i=0; i<4; i++ )
    {
        int u32Temp = 0;
        memcpy(&u32Temp, i_pSrc, 1);
        u32Rtn += u32Temp << (4-1-i)*8;
        i_pSrc++;
    }
    return u32Rtn;
}
