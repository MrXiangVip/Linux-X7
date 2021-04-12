/**************************************************************************
 * 	FileName:	 face_loop.cpp
 *	Description:	This file is used to define the common function interface.
 *	Copyright(C):	2018-2020 Wave Group Inc.
 *	Version:		V 1.0
 *	Author:		tanqw
 *	Created:		2019-7-12
 *	Updated:		
 *					
**************************************************************************/

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
#include <condition_variable>
#include <mutex>
#include <thread>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include<sys/stat.h>
#include <arpa/inet.h>

#include "RSApiHandle.h"
#include "function.h"
#include "util_crc32.h"


/****************************************************************************************
函数名称：myWriteData2File
函数功能：写数据到文件
入口参数：pData   --数据
		iDataLen--数据长度
出口参数：无
返回值：
****************************************************************************************/
void myWriteData2File(const char * pFileName, unsigned char *pData, int iDataLen)
{
	FILE *fp = fopen(pFileName, "wb");
	if (fp != NULL)
	{
		fwrite(pData, 1, iDataLen, fp);
		fclose(fp);
	}
	else
	{
		printf("fwrite() error!");
	}
}

int SendMsgToRpMSG(unsigned char *MsgBuf, unsigned char MsgLen)
{
	int fd, ret; 

	fd = open(RPMSG_DEV, O_RDWR);
	if(fd < 0)
	{
		printf("open ttyRPMSG device failed\n");
		return -1;
	}
	else
	{
		ret = write(fd, (unsigned char *)MsgBuf, MsgLen);
		if (ret < 0)  
		{
			printf("write ttyRPMSG device failed!\n");	
			return -1;
		}
		
		close(fd);
	}

	return 0;
}

int SaveSysCfgToLocalCfgFile(pstSysCfg pstSystemCfg)
{
	FILE   *pFile = NULL;
	char	szBuffer[4096] = {0};
	int iBufferSize=0;				

	memset(szBuffer, 0, sizeof(szBuffer));
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_reg_threshold=%d\n", pstSystemCfg->face_reg_threshold_val);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_recg_threshold=%d\n", pstSystemCfg->face_recg_threshold_val);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_reg_angle=%4.1f\n", pstSystemCfg->face_detect_reg_angle);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_recg_angle=%4.1f\n", pstSystemCfg->face_detect_recg_angle);
	iBufferSize += sprintf(szBuffer + iBufferSize, "auto_udisk_upgrade=%d\n", pstSystemCfg->auto_udisk_upgrade);

	pFile = fopen("./sys.cfg", "w");
	if (pFile)
	{
		fwrite(szBuffer, iBufferSize, 1, pFile);	
		
		/* 将文件同步,及时写入磁盘,不然，系统重启动，导致文件内容丢失 */
		fflush(pFile);		/* 刷新内存缓冲，将内容写入内核缓冲 */
		fsync(fileno(pFile));	/* 将内核缓冲写入磁盘 */
	}
	else
	{
		return -1;
	}

	if (NULL != pFile)
	{
		fclose(pFile);
		pFile = NULL;
	}

	return 0;
}

int GetSysLocalCfg(pstSysCfg pstSystemCfg)
{
	FILE   *pFile = NULL;
	char   szMsg[256] = {0};
	unsigned short len = 0;
	
	if (access("./sys.cfg", F_OK) == 0)  
	{
		pFile = fopen("./sys.cfg", "r");
		if (pFile == NULL)
		{
			printf("Err, can't fopen<./sys.cfg>\n");
			return -1;
		}
		memset(szMsg, 0, sizeof(szMsg));

		while (fgets(szMsg, sizeof(szMsg), pFile) != NULL)
		{
			if (strstr(szMsg, "face_reg_threshold=") != NULL) 
			{
				len = strlen("face_reg_threshold=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->face_reg_threshold_val));//skip "face_threshold="
				if((pstSystemCfg->face_reg_threshold_val > 80) || (pstSystemCfg->face_reg_threshold_val <50))
				{
					pstSystemCfg->face_reg_threshold_val =  FACE_REG_THRESHOLD_VALUE;
				}
			}
			if (strstr(szMsg, "face_recg_threshold=") != NULL) 
			{
				len = strlen("face_recg_threshold=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->face_recg_threshold_val));//skip "face_threshold="
				if((pstSystemCfg->face_recg_threshold_val > 80) || (pstSystemCfg->face_recg_threshold_val <40))
				{
					pstSystemCfg->face_recg_threshold_val =  FACE_RECG_THRESHOLD_VALUE;
				}
			}
			else if (strstr(szMsg, "face_reg_angle=") != NULL) 
			{
				len = strlen("face_reg_angle=");
				sscanf(&szMsg[len], "%f", &(pstSystemCfg->face_detect_reg_angle));//skip "face_reg_angle="
				if((pstSystemCfg->face_detect_reg_angle > 40.0) || (pstSystemCfg->face_detect_reg_angle <5.0))
				{
					pstSystemCfg->face_detect_reg_angle =  FACE_DETECT_REGISTER_ANGEL;
				}
			}
			else if (strstr(szMsg, "face_recg_angle=") != NULL) 
			{
				len = strlen("face_recg_angle=");
				sscanf(&szMsg[len], "%f", &(pstSystemCfg->face_detect_recg_angle));//skip "face_threshold="
				if((pstSystemCfg->face_detect_recg_angle > 40.0) || (pstSystemCfg->face_detect_recg_angle <5.0))
				{
					pstSystemCfg->face_detect_recg_angle =  FACE_DETECT_RECGONIZE_ANGEL;
				}
			}
			else if (strstr(szMsg, "auto_udisk_upgrade=") != NULL) 
			{
				len = strlen("auto_udisk_upgrade=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->auto_udisk_upgrade));
			}

			memset(szMsg, 0, sizeof(szMsg));
		}
	}
	else
	{
		/*如果不存在配置文件，那么用系统默认的值，并生成配置文件*/
		pstSystemCfg->face_reg_threshold_val =	FACE_REG_THRESHOLD_VALUE;
		pstSystemCfg->face_recg_threshold_val = FACE_RECG_THRESHOLD_VALUE;
		pstSystemCfg->face_detect_reg_angle =  FACE_DETECT_REGISTER_ANGEL;
		pstSystemCfg->face_detect_recg_angle =	FACE_DETECT_RECGONIZE_ANGEL;
		pstSystemCfg->auto_udisk_upgrade = 0;
		printf("Warring, not exit </opt/smartlocker/sys.cfg>.\n");

		SaveSysCfgToLocalCfgFile(pstSystemCfg);
	}

	printf("face_reg_threshold<%d>,face_recg_threshold<%d>,face_reg_angle<%4.1f>,face_recg_angle<%4.1f>,auto_udisk_upgrade<%d>\n",   \
		pstSystemCfg->face_reg_threshold_val,pstSystemCfg->face_recg_threshold_val, pstSystemCfg->face_detect_reg_angle, \
		pstSystemCfg->face_detect_recg_angle,pstSystemCfg->auto_udisk_upgrade);

	if (NULL != pFile)
	{
		fclose(pFile);
		pFile = NULL;
	}
	
	return 0;
}

/*
describe:
	校验升级包文件头中的CRC32，如果校验通过，并去掉该64自己的文件头后，恢复正常的压缩包，加上-bak
return: 
	0:success  -1:failed
*/
int CheckImageFileCRC32(const char * SrcFilePath, const char * DstFilePath,  uint32_t *crc32_val)
{
	const char *filename = NULL;
	int  i=0, val = 0;
	struct stat filestat;
	image_header_t hdr;
	unsigned long checksum;
	int dst_fd = -1,src_fd = -1;
	int val1 = 0, val2 = 0;
	ssize_t result = 0;
	size_t size = 0, written = 0, readsize = 0;
	unsigned char buffer[4096] = {0};

	if (SrcFilePath == NULL)
	{
		return -1;
	}
	filename = SrcFilePath;
	printf("filename<%s>\n", filename);
	
	/* get some info about the file we want to copy */
	src_fd = open (filename,O_RDONLY);
	if (fstat (src_fd,&filestat) < 0)
	{
		printf ("Err: While trying to get the file status of %s failed!\n",filename);
		return -1;
	}
	
	/* read the update file header from filename */
	memset(&hdr, 0, sizeof(image_header_t));
	val = read (src_fd, &hdr, sizeof(image_header_t));
	if (val <= 0)
	{
		return -1;
	}
	printf("ih_name<%s>, ih_size<%ld> ,st_size<%ld>\n", hdr.ih_name, (size_t)ntohl(hdr.ih_size), (size_t)filestat.st_size);

	/*映射升级文件到内存*/
	void * start = mmap(NULL, filestat.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
	if (start == MAP_FAILED)
	{
		return -1;
	}
	
	/* check the data CRC */
	checksum = ntohl(hdr.ih_dcrc);	
	*crc32_val = util_crc32(0, (unsigned char const *)(start + sizeof(hdr)), ntohl(hdr.ih_size));
	if (*crc32_val != checksum) 
	{
		printf("Warring: Image %s bad data checksum, CrcVal<0x%lx>, checksum<0x%lx>\n", filename, *crc32_val, checksum);
		return -1;
	}
	//printf("Info: Image %s OK data checksum, CrcVal<0x%lx>, checksum<0x%lx>\n", filename, *crc32_val, checksum);

	/*restore the updatefile to Raw data file,clear the 64Byte head of file*/
	dst_fd= open(DstFilePath, O_SYNC|O_RDWR|O_CREAT);
	if (dst_fd <= 0)
	{
		printf("open() after.bin error.\n");
		return -1;
	}

	size = ntohl(hdr.ih_size);
	i = sizeof(buffer);
	written = 0;
	readsize = 0;

	while (size)
	{
		if (size < sizeof(buffer)) i = size;

		/* read from filename */
		val = read (src_fd, buffer, i); 		
		if (val <= 0)
		{
			printf("safe_read() datalen<i=%d> error.\n", i);
			return -1;
		}
		readsize += i;

		/* write to another file */
		result = write (dst_fd,buffer,i);
		if (i != result)
		{
			if (result < 0)
			{
				printf ("Err: While writing data to 0x%.8lx-0x%.8lx on %s: %m\n",
						written,written + i, hdr.ih_name);
				return -1;
			}
			printf ("Err: Short write count returned while writing to x%.8lx-0x%.8lx on %s: %ld/%lu bytes written to flash\n",
					written,written + i,hdr.ih_name,written + result,(size_t)ntohl(hdr.ih_size));
			return -1;
		}
		else
		{
			printf("Written %ld Bytes to flash, written<%ld>, readsize<%ld>\n", result, written+i, readsize);
		}				

		written += i;
		size -= i;
	}

	char cmd[128] = {0};
	snprintf(cmd, sizeof(cmd), "chmod 777 %s", DstFilePath);
	system(cmd);
	
	return 0;
}

int SetOv7956ModuleLed(BOOL value)
{
	char buf[64] = {0};
	system("echo 78 > /sys/class/gpio/export");	
	system("echo \"out\" > /sys/class/gpio/gpio78/direction");
	snprintf(buf, sizeof(buf), "echo %d > /sys/class/gpio/gpio78/value", value);
	system(buf);
	system("echo 78 > /sys/class/gpio/unexport");
}

