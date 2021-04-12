/**************************************************************************
 * 	FileName:	 function.cpp
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
#include<sys/time.h>
#include<vector>
#include <sys/types.h>
#include<dirent.h>

#include "config.h"

#include "function.h"
#include "util_crc32.h"
#include "sys_upgrade.h"
#include "hl_MsgQueue.h"  

using namespace std;

stSysCfg stSystemCfg;				// 记录系统配置信息
stLogCfg stSysLogCfg;				// 记录系统日志配置信息
InitSyncInfo	stInitSyncInfo;		// 记录初始化同步信息( 电量,MCU版本等 )
FIT face;							
int g_person_id = 1;  // 全局personID ,自动管理,注册时,如果被占, 自动增加
uUID g_uu_id;  // 记录当前应用的uuid
bool flg_sendMcuMsg = false;

int ttyMCU_fd=-1;
int ttyRpMsg_fd=-1;
int ttyUart7_fd=-1;

/****************************************************************************************
函数名称:myWriteData2File
函数功能:写数据到文件
入口参数:
出口参数:
返回值:
****************************************************************************************/
int myWriteData2File(const char * pFileName, unsigned char *pData, int iDataLen)
{
	FILE *fp = fopen(pFileName, "wb");
	if (fp != NULL)
	{
		fwrite(pData, 1, iDataLen, fp);

		/*将文件同步,及时写入磁盘,不然系统重启动,导致文件内容丢失*/
		fflush(fp);		/* 刷新内存缓冲,将内容写入内核缓冲 */
		fsync(fileno(fp));	/* 将内核缓冲写入磁盘 */
		
		fclose(fp);
	}
	else
	{
		log_error("fwrite() error!");
		return -1;
	}
	return 0;
}

/****************************************************************************************
函数名称: myReadDataFromFile
函数功能: 从本地文件读取数据
入口参数:
出口参数:
返回值:
****************************************************************************************/
int myReadDataFromFile(const char * pFileName, unsigned char *pData, int iDataLen)
{
	FILE *fp = fopen(pFileName, "rb");
	if (fp != NULL)
	{
		fread(pData, 1, iDataLen, fp);
		fclose(fp);
	}
	else
	{
		log_error("fwrite() error!");
		return -1;
	}
	return 0;
}

/*
 * 通过uart 发送消息给wifi模块
 * */
int SendMsgToWifiMod(unsigned char *MsgBuf, unsigned char MsgLen)
{
	int ret; 
	log_info("Send_Wifi_AT:<%s>\n", MsgBuf);

	if(stSysLogCfg.log_level < LOG_INFO)	
	{
		int i = 0;
		printf("\n===send wifiAtCmd<len:%d>:", MsgLen);	
		for(i; i<MsgLen; i++)	
		{		
			printf("0x%02x   ", MsgBuf[i]);	
		}	
		printf("\n");
	}
	/*
	by tanqw 20200722
	如果后期出现串口数据丢失,需要重发的情况,可以在此增加一个全局UartMsgList,
	把需要发送的消息都先存放在这个List的节点中,并把发送标记置最大重发次数Num,
	然后在face_loop中另外开一个线程,定时从这个List中去取消息发送, 然后Num--,当取到的
	消息的发送标记位为0时, 删除该节点;当收到对方该消息的响应时,把该消息从List中直接删除.
	*/

	if(ttyUart7_fd <= 0)
	{
		log_error("open ttyUart7_fd device failed\n");
		return -1;
	}
	else
	{
		ret = write(ttyUart7_fd, (unsigned char *)MsgBuf, MsgLen);
		if (ret != MsgLen)  
		{
			log_error("write ttyUart7_fd failed! MsgLen<%d> != ret<%d>\n", MsgLen, ret);	
			return -1;
		}
	}

	return ret;
}

/*
* 通过uart发送消息给MCU
*/
int SendMsgToMCU(unsigned char *MsgBuf, unsigned char MsgLen)
{
	int ret; 

	if(stSysLogCfg.log_level < LOG_INFO)	
	{
		int i = 0;
		printf("\n===send msg<len:%d>:", MsgLen);	
		for(i; i<MsgLen; i++)	
		{		
			printf("0x%02x   ", MsgBuf[i]);	
		}	
		printf("\n");
	}
	/*
	by tanqw 20200722
	如果后期出现串口数据丢失,需要重发的情况,可以在此增加一个全局UartMsgList,
	把需要发送的消息都先存放在这个List的节点中,并把发送标记置最大重发送次数Num,
	然后在face_loop中另外开一个线程,定时从这个List中去取消息发送,然后Num--,当取到的
	消息的发送标记位为0时,删除该节点;当收到对方该消息的响应时,把该消息从List中直接删除.
	*/

	if(ttyMCU_fd <= 0)
	{
		log_error("open ttyMCU device failed\n");
		return -1;
	}
	else
	{
		ret = write(ttyMCU_fd, (unsigned char *)MsgBuf, MsgLen);
		if (ret != MsgLen)  
		{
			log_error("write ttyMCU failed! MsgLen<%d> != ret<%d>\n", MsgLen, ret);	
			return -1;
		}
		flg_sendMcuMsg = true;
	}

	return ret;
}

/*
* 通过RpMsg发送消息给 M4 core
*/
int SendMsgToRpMsg(unsigned char *MsgBuf, unsigned char MsgLen)
{
	int ret; 

	if(stSysLogCfg.log_level < LOG_INFO)	
	{
		int i = 0;
		printf("\n===send Rpmsg<len:%d>:", MsgLen);	
		for(i; i<MsgLen; i++)	
		{		
			printf("0x%02x   ", MsgBuf[i]);	
		}	
		printf("\n");
	}
	/*
	by tanqw 20200722
    如果后期出现串口数据丢失,需要重发的情况,可以在此增加一个全局UartMsgList,
    把需要发送的消息都先存放在这个List的节点中,并把发送标记置最大重发次数Num,
    然后在face_loop中另外开一个线程,定时从这个List中去取消息发送,然后Num--,当
    消息的发送标记位为0时,删除该节点;当收到对方该消息的响应时,把该消息从List中直接删除.
	*/

	if(ttyRpMsg_fd <= 0)
	{
		log_error("open ttyRPMSG device failed\n");
		return -1;
	}
	else
	{
		ret = write(ttyRpMsg_fd, (unsigned char *)MsgBuf, MsgLen);
		if (ret != MsgLen)  
		{
			log_error("write ttyRPMSG failed! MsgLen<%d> != ret<%d>\n", MsgLen, ret);	
			return -1;
		}
	}

	return ret;
}

int SaveSysCfgToLocalCfgFile(pstSysCfg pstSystemCfg)
{
	FILE   *pFile = NULL;
	char	szBuffer[4096] = {0};
	int iBufferSize=0;				

	memset(szBuffer, 0, sizeof(szBuffer));
	iBufferSize += sprintf(szBuffer + iBufferSize, "recognizeThreshold=%d\n", pstSystemCfg->recognizeThreshold);
	iBufferSize += sprintf(szBuffer + iBufferSize, "posFilterThreshold=%3.2f\n", pstSystemCfg->posFilterThreshold);
	iBufferSize += sprintf(szBuffer + iBufferSize, "faceMinSize=%d\n", pstSystemCfg->faceMinSize);
	iBufferSize += sprintf(szBuffer + iBufferSize, "faceMaxSize=%d\n", pstSystemCfg->faceMaxSize);
	iBufferSize += sprintf(szBuffer + iBufferSize, "faceMinLight=%d\n", pstSystemCfg->faceMinLight);
	iBufferSize += sprintf(szBuffer + iBufferSize, "faceMaxLight=%d\n", pstSystemCfg->faceMaxLight);
	iBufferSize += sprintf(szBuffer + iBufferSize, "regSkipFace=%d\n", pstSystemCfg->regSkipFace);
	iBufferSize += sprintf(szBuffer + iBufferSize, "antifaceThreshold=%2.1f\n", pstSystemCfg->antifaceThreshold);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_reg_timeout=%d\n", pstSystemCfg->face_reg_timeout);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_recg_timeout=%d\n", pstSystemCfg->face_recg_timeout);
	iBufferSize += sprintf(szBuffer + iBufferSize, "flag_auto_recg=%d\n", pstSystemCfg->flag_auto_recg);
	iBufferSize += sprintf(szBuffer + iBufferSize, "flag_wdt_enable=%d\n", pstSystemCfg->flag_wdt_enable);
	iBufferSize += sprintf(szBuffer + iBufferSize, "flag_wifi_enable=%d\n", pstSystemCfg->flag_wifi_enable);

	pFile = fopen("./sys.cfg", "w");
	if (pFile)
	{
		fwrite(szBuffer, iBufferSize, 1, pFile);	
		
		/* 将文件同步,及时写入磁盘,不然系统重启动,导致文件内容丢失*/
		fflush(pFile);		/* 刷新内存缓冲, 将内容写入内核缓冲 */
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
			log_error("Err, can't fopen<./sys.cfg>\n");
			return -1;
		}
		memset(szMsg, 0, sizeof(szMsg));

		while (fgets(szMsg, sizeof(szMsg), pFile) != NULL)
		{
			if (strstr(szMsg, "recognizeThreshold=") != NULL) 
			{
				len = strlen("recognizeThreshold=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->recognizeThreshold));//skip "recognizeThreshold="
				if((pstSystemCfg->recognizeThreshold > 130) || (pstSystemCfg->recognizeThreshold <50))
				{
					pstSystemCfg->recognizeThreshold =  FACE_RECG_THRESHOLD_VALUE;
				}
				log_debug("recognizeThreshold<%d>	", pstSystemCfg->recognizeThreshold);
			}
			if (strstr(szMsg, "posFilterThreshold=") != NULL) 
			{
				len = strlen("posFilterThreshold=");
				sscanf(&szMsg[len], "%f", &(pstSystemCfg->posFilterThreshold));//skip "posFilterThreshold="
				if((pstSystemCfg->posFilterThreshold > 80) || (pstSystemCfg->posFilterThreshold <40))
				{
					pstSystemCfg->posFilterThreshold =  FACE_REG_POS_FILTER_THRESHOLD;
				}
				log_debug("posFilterThreshold<%3.2f>	", pstSystemCfg->posFilterThreshold);
			}
			else if (strstr(szMsg, "faceMinSize=") != NULL) 
			{
				len = strlen("faceMinSize=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->faceMinSize));//skip "faceMinSize="
				if((pstSystemCfg->faceMinSize > 400) || (pstSystemCfg->faceMinSize <50))
				{
					pstSystemCfg->faceMinSize =  FACE_DETECT_MIN_SIZE;
				}
				log_debug("faceMinSize<%d>	", pstSystemCfg->faceMinSize);
			}
			else if (strstr(szMsg, "faceMaxSize=") != NULL) 
			{
				len = strlen("faceMaxSize=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->faceMaxSize));//skip "faceMaxSize="
				if((pstSystemCfg->faceMaxSize > 480) || (pstSystemCfg->faceMaxSize <200))
				{
					pstSystemCfg->faceMaxSize =  FACE_DETECT_MAX_SIZE;
				}
				log_debug("faceMaxSize<%d>	", pstSystemCfg->faceMaxSize);
			}
			else if (strstr(szMsg, "faceMinLight=") != NULL) 
			{
				len = strlen("faceMinLight=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->faceMinLight));//skip "faceMinLight="
				if((pstSystemCfg->faceMinLight > 255) || (pstSystemCfg->faceMinLight <50))
				{
					pstSystemCfg->faceMinLight =  FACE_DETECT_MIN_LIGHT;
				}
				log_debug("faceMinLight<%d>	", pstSystemCfg->faceMinLight);
			}
			else if (strstr(szMsg, "faceMaxLight=") != NULL) 
			{
				len = strlen("faceMaxLight=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->faceMaxLight));//skip "faceMaxLight="
				if((pstSystemCfg->faceMaxLight > 255) || (pstSystemCfg->faceMaxLight <FACE_DETECT_MIN_LIGHT))
				{
					pstSystemCfg->faceMaxLight =  FACE_DETECT_MAX_LIGHT;
				}
				log_debug("faceMaxLight<%d>	", pstSystemCfg->faceMaxLight);
			}
			else if (strstr(szMsg, "regSkipFace=") != NULL) 
			{
				len = strlen("regSkipFace=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->regSkipFace));
				if((pstSystemCfg->regSkipFace > 15) || (pstSystemCfg->regSkipFace <1))
				{
					pstSystemCfg->regSkipFace =  FACE_REG_SKIP_NUM;
				}
				log_debug("regSkipFace<%d>	", pstSystemCfg->regSkipFace);
			}
			else if (strstr(szMsg, "antifaceThreshold=") != NULL) 
			{
				len = strlen("antifaceThreshold=");
				sscanf(&szMsg[len], "%f", &(pstSystemCfg->antifaceThreshold));//skip "antifaceThreshold="
				if((pstSystemCfg->antifaceThreshold > 1.0) || (pstSystemCfg->antifaceThreshold <0.0))
				{
					pstSystemCfg->antifaceThreshold =  FACE_ANTIFACE_THRESHOLD;
				}
				log_debug("antifaceThreshold<%2.1f>	", pstSystemCfg->antifaceThreshold);
			}
			else if (strstr(szMsg, "face_reg_timeout=") != NULL) 
			{
				len = strlen("face_reg_timeout=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->face_reg_timeout));//skip "face_reg_timeout="
				if((pstSystemCfg->face_reg_timeout > 60) || (pstSystemCfg->face_reg_timeout <10))
				{
					pstSystemCfg->face_reg_timeout =  FACE_REG_TIMEOUT;
				}
				log_debug("face_reg_timeout<%d>	", pstSystemCfg->face_reg_timeout);
			}
			else if (strstr(szMsg, "face_recg_timeout=") != NULL) 
			{
				len = strlen("face_recg_timeout=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->face_recg_timeout));//skip "face_recg_timeout="
				if((pstSystemCfg->face_recg_timeout > 3) || (pstSystemCfg->face_recg_timeout <10))
				{
					pstSystemCfg->face_recg_timeout =  FACE_RECG_TIMEOUT;
				}
				log_debug("face_recg_timeout<%d>	", pstSystemCfg->face_recg_timeout);
			}
			else if (strstr(szMsg, "flag_auto_recg=") != NULL) 
			{
				len = strlen("flag_auto_recg=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->flag_auto_recg));
				log_debug("flag_auto_recg<%d>	", pstSystemCfg->flag_auto_recg);
			}
			else if (strstr(szMsg, "flag_wdt_enable=") != NULL) 
			{
				len = strlen("flag_wdt_enable=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->flag_wdt_enable));
				log_debug("flag_wdt_enable<%d>	", pstSystemCfg->flag_wdt_enable);
			}
			else if (strstr(szMsg, "flag_wifi_enable=") != NULL) 
			{
				len = strlen("flag_wifi_enable=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->flag_wifi_enable));
				log_debug("flag_wifi_enable<%d>	", pstSystemCfg->flag_wifi_enable);
			}

			memset(szMsg, 0, sizeof(szMsg));
		}
	}
	else
	{
		/* 如果不存在配置文件,那么用系统默认的值, 并生成配置文件 */
		pstSystemCfg->recognizeThreshold =	FACE_RECG_THRESHOLD_VALUE;
		pstSystemCfg->posFilterThreshold = FACE_REG_POS_FILTER_THRESHOLD;
		pstSystemCfg->faceMinSize =  FACE_DETECT_MIN_SIZE;
		pstSystemCfg->faceMaxSize =	FACE_DETECT_MAX_SIZE;
		pstSystemCfg->faceMinLight = FACE_DETECT_MIN_LIGHT;
		pstSystemCfg->faceMaxLight = FACE_DETECT_MAX_LIGHT;
		pstSystemCfg->regSkipFace = FACE_REG_SKIP_NUM;
		pstSystemCfg->antifaceThreshold = FACE_ANTIFACE_THRESHOLD;
		
		pstSystemCfg->face_reg_timeout =  FACE_REG_TIMEOUT;
		pstSystemCfg->face_recg_timeout =	FACE_RECG_TIMEOUT;
		pstSystemCfg->flag_auto_recg = 0;
		pstSystemCfg->flag_wdt_enable = 0;
		pstSystemCfg->flag_wifi_enable = 1;
		log_info("Warring, not exit <./sys.cfg>.\n");

		log_debug("recognizeThreshold<%d>,posFilterThreshold<%3.2f>,  \
			faceMinSize<%d>,faceMaxSize<%d>,faceMinLight<%d>,  \
			faceMaxLight<%d>,regSkipFace<%d>,antifaceThreshold<%2.1f>, \
			face_reg_timeout<%d>,face_recg_timeout<%d>, flag_auto_recg<%d>,flag_wdt_enable<%d>, \
			flag_wifi_enable<%d>\n",	 \
			pstSystemCfg->recognizeThreshold,pstSystemCfg->posFilterThreshold, pstSystemCfg->faceMinSize, \
			pstSystemCfg->faceMaxSize,pstSystemCfg->faceMinLight,  \
			pstSystemCfg->faceMaxLight,pstSystemCfg->regSkipFace, pstSystemCfg->antifaceThreshold,\
			pstSystemCfg->face_reg_timeout, pstSystemCfg->face_recg_timeout, pstSystemCfg->flag_auto_recg,\
			pstSystemCfg->flag_wdt_enable, pstSystemCfg->flag_wifi_enable);
		
		SaveSysCfgToLocalCfgFile(pstSystemCfg);
	}

	if (NULL != pFile)
	{
		fclose(pFile);
		pFile = NULL;
	}
	
	return 0;
}

/*
describe:
	校验升级包文件头中的CRC32,如果校验通过,并去掉该64自己的文件头后,恢复正常的压缩包,加上-bakk
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

uint8_t StrGetUInt8( const uint8_t * i_pSrc )
{
    uint8_t u8Rtn = 0;
    memcpy(&u8Rtn, i_pSrc, 1);
    return u8Rtn;
}

uint16_t StrGetUInt16( const uint8_t * i_pSrc )
{
    uint16_t u16Rtn = 0;
    for ( uint8_t i=0; i<2; i++ )
    {
        uint16_t u16Temp = 0;
        memcpy(&u16Temp, i_pSrc, 1);
        u16Rtn += u16Temp << (2-1-i)*8;
        i_pSrc++;
    }
    return u16Rtn;
}

uint32_t StrGetUInt32( const uint8_t * i_pSrc )
{
    uint32_t u32Rtn = 0;
    for ( uint8_t i=0; i<4; i++ )
    {
        uint32_t u32Temp = 0;
        memcpy(&u32Temp, i_pSrc, 1);
        u32Rtn += u32Temp << (4-1-i)*8;
        i_pSrc++;
    }
    return u32Rtn;
}

void StrSetUInt8( uint8_t * io_pDst, const uint8_t i_u8Src )
{
    memcpy(io_pDst, &i_u8Src, 1);
    io_pDst++;
}

void StrSetUInt16( uint8_t * io_pDst, const uint16_t i_u16Src )
{
    for ( uint8_t i=0; i<2; i++ )
    {
        uint8_t u8Temp = (i_u16Src >> (2-1-i)*8) & 0xFF;
        memcpy(io_pDst, &u8Temp, 1);
        io_pDst++;
    }    
}

void StrSetUInt32( uint8_t * io_pDst, const uint32_t i_u32Src )
{
    for ( uint8_t i=0; i<4; i++ )
    {
        uint8_t u8Temp = (i_u32Src >> (4-1-i)*8) & 0xFF;
        memcpy(io_pDst, &u8Temp, 1);
        io_pDst++;
    }    
}

/*
// C prototype : void StrToHex(byte *pbDest, char *pszSrc, int nLen)
// parameter(s): [OUT] pbDest - 输出缓冲区
//	[IN] pszSrc - 字符串
//	[IN] nLen - 16进制数的字节数(字符串的长度/2)
// return value:
// remarks : 将字符串转化为16进制数
*/
void StrToHex(unsigned char *pbDest, char *pszSrc, int nLen)
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
 
/*
// C prototype : void HexToStr(char *pszDest, byte *pbSrc, int nLen)
// parameter(s): [OUT] pszDest - 存放目标字符串
//	[IN] pbSrc - 输入16进制数的起始地址
//	[IN] nLen - 16 进制数的字节数
// return value:
// remarks : 将16进制数转化为字符串
*/
void HexToStr(char *pszDest, unsigned char *pbSrc, int nLen)
{
	char	ddl, ddh;
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

/****************************************************************************
 * @fn      Msg_CalcFCS
 *
 * @brief   Calculate the FCS of a message buffer by XOR'ing each byte.
 *          Remember to NOT include SOP and FCS fields, so start at the CMD field.
 *
 * @param   byte *msg_ptr - message pointer
 * @param   byte len - length (in bytes) of message
 *
 * @return  result byte
 ****************************************************************************/
uint8_t Msg_UartCalcFCS( const uint8_t *msg_ptr, uint8_t len )
{
    uint8_t x;
    uint8_t xorResult;

    xorResult = 0;

    for ( x = 0; x < len; x++, msg_ptr++ )
        xorResult = xorResult ^ *msg_ptr;

    return ( xorResult );
}

// 判断一个int 变量的每个bit位的值(1 或者 0)
int GetBitStatu(int num, int pos)
{
    if(num & (1 << pos)) // 按位与之后的结果非0
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// 核查目录,若目录不存在,创建目录
bool FindOrCreateDirectory( const char* pszPath )
{
	DIR* dir;
	if(NULL==(dir=opendir(pszPath)))
		mkdir(pszPath,0775);

	closedir(dir);
	return true;
}

// 检测文件路径中的目录是否存在,,不存在则创建�
bool CheckDirectory( char* pszFilePath )
{
	char pszPath[4096];
	strcpy(pszPath,pszFilePath);
	(strrchr(pszPath,'/'))[1] = 0;

	vector< std::string > vtPath;

	const char* sep = "\\/";
	char* next_token;
	char* token = strtok_r( pszPath, sep, &next_token);
	while( token != NULL )
	{
		vtPath.push_back( token );
		token = strtok_r(NULL, sep, &next_token);
	}

	if ( vtPath.size() > 0 )
	{
		if ( vtPath[0] == "." )
			vtPath.erase( vtPath.begin() );
	}

	// 核查所有路径是否存在
	std::string strCurPath;
	for( size_t i = 0; i < (int)vtPath.size(); ++i )
	{
		strCurPath += '/';
		strCurPath += vtPath[i];
		if ( !FindOrCreateDirectory( strCurPath.c_str() ) )
		{
			return false;
		}
	}

	return true;
}

/****************************************************************************************
函数名称:MsgHead_Packet
函数功能:组包
入口参数: pszBuffer -- 数据
         HeadMark -- 消息标记
         CmdId -- 命令
         MsgLen -- 消息长度
出口参数:无
返回值: 成功返回消息的总长度( 消息头+消息体+FCS长度), 失败返回 -1
****************************************************************************************/
int MsgHead_Packet(
	char *pszBuffer,
	unsigned char HeadMark,
	unsigned char CmdId,
	unsigned char MsgLen)
{
	if (!pszBuffer)
	{
		printf("pszBuffer is NULL\n");
		return -1;
	}
	char *pTemp = pszBuffer;

	StrSetUInt8( (uint8_t *)pTemp, HeadMark );
	pTemp += sizeof(uint8_t);
	StrSetUInt8( (uint8_t *)pTemp, CmdId );
	pTemp += sizeof(uint8_t);
	StrSetUInt8( (uint8_t *)pTemp, MsgLen );
	pTemp += sizeof(uint8_t);

	return MsgLen+ sizeof(MESSAGE_HEAD);
}

/****************************************************************************************
函数名称:MsgHead_Unpacket
函数功能:解包
入口参数: pszBuffer -- 数据
         iBufferSize -- 数据大小
         HeadMark   -- 标记头
         CmdId -- 命令
         MsgLen -- 消息长度
出口参数:无
返回值: 成功返回消息内容的指针, 失败返回 -1
****************************************************************************************/
const unsigned char *MsgHead_Unpacket(
	const unsigned char *pszBuffer,
	unsigned  char iBufferSize,
	unsigned char *HeadMark,
	unsigned char *CmdId,
	unsigned char *MsgLen)
{
	if (!pszBuffer)
	{
		printf("pszBuffer is NULL\n");
		return NULL;
	}
	const unsigned char *pTemp = pszBuffer;

	*HeadMark = StrGetUInt8(pTemp);	
	pTemp += sizeof(uint8_t);
	*CmdId = StrGetUInt8(pTemp);
	pTemp += sizeof(uint8_t);
	*MsgLen = StrGetUInt8(pTemp);
	pTemp += sizeof(uint8_t);
	if (*HeadMark != HEAD_MARK && *HeadMark != HEAD_MARK_MQTT)
	{
		log_info("byVersion[0x%x] != MESSAGE_VERSION[0x%x|0x%x]\n", \
			*HeadMark, HEAD_MARK, HEAD_MARK_MQTT);
		return NULL;
	}

	if ((int)*MsgLen + sizeof(MESSAGE_HEAD) + CRC16_LEN> iBufferSize)
	{
		log_info("pstMessageHead->MsgLen + sizeof(MESSAGE_HEAD) + CRC16_LEN > iBufferSize\n");
		return NULL;
	}

	return pszBuffer + sizeof(MESSAGE_HEAD);
}

//X7->MCU:通用响应
int cmdCommRsp2MCU(unsigned char CmdId, uint8_t ret)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	/* 填充消息体 */
	StrSetUInt8((uint8_t*)pop, ret);
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t);

	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CmdId,
		MsgLen);
	
	/* 计算FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToMCU((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);
	usleep(10);

	return 0;
}


int cmdAckRsp( unsigned char ack_status)
{
	char szBuffer[16]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;
	
	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	/* 填充消息体 */
	StrSetUInt8((uint8_t *)pop, ack_status);
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t);

	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		ID_FACE_ACK_Rpt,
		MsgLen);
	
	/* 计算FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToMCU((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	return 0;
}

// 主控发送测试指令:开机同步请求
int cmdSysInitOKSyncReq(const char *strVersion)
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/* 填充消息体 */
	memcpy(pop, strVersion, Version_LEN);
	MsgLen += Version_LEN;
	pop += MsgLen;

	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_INITOK_SYNC,
		MsgLen);
	
	/* 计算 FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	log_debug("cal_crc16<0x%X>\n", cal_crc16);
	
	SendMsgToMCU((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	return 0;
}

// 主控接收指令: 开机同步响应
int cmdSysInitOKSyncRsp(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS, len=0, pramID =0, i=0;	
	unsigned char szBuffer[128]={0};
	unsigned char *pos = szBuffer;

	if((nMessageLen < sizeof(szBuffer)) && nMessageLen == 4)
	{
		memcpy(szBuffer, pszMessage, nMessageLen);
	}

	//解析设置参数
	pInitSyncInfo pt_InitSyncInfo;
	pt_InitSyncInfo = (pInitSyncInfo)pszMessage;
	stInitSyncInfo.Mcu_Ver = pt_InitSyncInfo->Mcu_Ver;
	stInitSyncInfo.PowerVal = (pt_InitSyncInfo->PowerVal>100) ? 100 : pt_InitSyncInfo->PowerVal;
	stInitSyncInfo.status = pt_InitSyncInfo->status;	

	log_debug("Mcu_Ver<%03d>,PowerVal<%d>, status<%d>\n", stInitSyncInfo.Mcu_Ver, stInitSyncInfo.PowerVal, stInitSyncInfo.status);
	//MCU 消息处理
	//更新电量显示
	//status中bit 的处理
	if(stInitSyncInfo.status & 0x01) // 防撬开关单次出发(MCU上传后置0)
	{
		//通知UI做出反应
	}
	if(stInitSyncInfo.status & 0x02)// 恢复出厂设置按钮单次触发(MCU上传后置0)
	{
		// 界面跳转到恢复出厂设置或者直接调用恢复出厂设置功能模块
	}
	if(stInitSyncInfo.status & 0x04)// 老化模式(MCU掉电后清0, 由A5配置)
	{
		// 运行老化测试程序
	}	
	
	{		
		char verbuf[4] = {0}; 
		char ver_tmp[32] = {0}; 
		
		/* 保存x7 信息到系统配置文件 */
		// 获取字符串格式的版本号
		memset(verbuf, 0, sizeof(verbuf));
		memset(ver_tmp, 0, sizeof(ver_tmp));
		sprintf(ver_tmp, "W7_X7_C4L1A3_V%s", SYS_VERSION);

		// 保存设置到系统配置文件
		update_x7_info("./config.ini", ver_tmp); 
		log_debug("X7_VERSION :<%s>.\n", ver_tmp);
	

		/* 保存MCU 信息到系统配置文件 */
		// 获取字符串格式的版本号
		memset(verbuf, 0, sizeof(verbuf));
		memset(ver_tmp, 0, sizeof(ver_tmp));
		sprintf(verbuf, "%03d", stInitSyncInfo.Mcu_Ver);
		sprintf(ver_tmp, "W7_HC130_X7_V%c.%c.%c", verbuf[0], verbuf[1], verbuf[2]);

		// 保存设置到系统配置文件
		update_mcu_info("./config.ini", ver_tmp); 
		//read_config("./config.ini");
		log_debug("MCU_VERSION:<%s>.\n", ver_tmp);
	
		system("sync");
	}

	//获取网络参数配置版本信息请求
	cmdNetworkOptVerReq();
	
	ret = SUCCESS;
	
	return ret;
}

//主控发送测试指令:心跳请求
int cmdWatchDogReq()
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/* 填充消息体 */

	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_HEART_BEAT,
		MsgLen);
	
	/*计算FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToMCU((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	return 0;
}

// 主控接收指令: 心跳响应
int cmdHeartBeatRsp(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;	
	unsigned char szBuffer[128]={0};
	unsigned char *pos = szBuffer;

	ret = StrGetUInt8( pszMessage );
	return 0;
}

// 主控接收指令:开门响应
int cmdOpenDoorRsp(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;	
	unsigned char szBuffer[128]={0};
	int iBufferSize = 0;
	unsigned char *pop = NULL;
	unsigned char MsgLen = 0;
	unsigned char	cal_FCS;

	ret = StrGetUInt8( pszMessage );

	if(stSystemCfg.flag_wifi_enable)
	{
		// 同时上报开门记录给Mqtt 模块
		cmdOpenDoorRec2Mqtt(g_uu_id,3, ret);
	}
	
	return 0;
}

// 主控接收指令:远程wifi开门响应
int cmdWifiOpenDoorRsp(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;	
	unsigned char szBuffer[128]={0};
	int iBufferSize = 0;
	unsigned char *pop = NULL;
	unsigned char MsgLen = 0;
	unsigned char	cal_FCS;

	ret = StrGetUInt8( pszMessage );

	if(stSystemCfg.flag_wifi_enable)
	{
		// 转发响应给 Mqtt 模块
		cmdCommRsp2Mqtt(CMD_WIFI_OPEN_DOOR, ret);
		usleep(200);
		// 同时上报开门记录给Mqtt 模块
		// zgx fix bug 5617: W7�������Ż���Զ�̿��ųɹ����Ž���¼����ʾ�Ķ��ǡ��������š��ɹ�
		// Զ�̿��Ų��ϱ����ż�¼���ɺ�̨���м�¼Զ�̿��ż�¼
		// ��������һ�����⣬���Ǹպ�wifi���ˣ����¿��ųɹ���Ӧû�и�Mqtt��Ȼ���̨��Ϊ�ǳ�ʱ����ʵ�����Ѿ�Զ�̿����ɹ��ˡ�

		// //ͬʱ�ϱ����ż�¼��Mqttģ��
		// cmdOpenDoorRec2Mqtt(g_uu_id, 5, ret);
	}
	
	return 0;
}

// 主控接收指令: 关机响应
int cmdCloseFaceBoardRsp(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;	
	unsigned char szBuffer[128]={0};
	unsigned char *pos = szBuffer;

	ret = StrGetUInt8( pszMessage );
	return 0;
}

// 主控接收指令: 注册状态通知响应
int cmdRegStatusNotifyRsp(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;	
	unsigned char szBuffer[128]={0};
	unsigned char *pos = szBuffer;

	ret = StrGetUInt8( pszMessage );
	return 0;
}

// 主控接收指令: 确认恢复出厂设置响应
int cmdConfirmResumeFactoryRsp(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;	
	unsigned char szBuffer[128]={0};
	unsigned char *pos = szBuffer;

	ret = StrGetUInt8( pszMessage );
	return 0;
}

// 主控接收指令: 用户注册请求
int cmdUserRegReqProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
    log_info("cmdUserRegReqProc \n");
	uint8_t ret = SUCCESS, len=0;	
	unsigned char szBuffer[128]={0};
	unsigned char *pos = szBuffer;	
	memcpy(szBuffer, pszMessage, nMessageLen);
	
	// 解析指令
	if((nMessageLen < sizeof(szBuffer)) && nMessageLen == UUID_LEN)
	{
		memcpy(&g_uu_id, pos+len, UUID_LEN);
		len += 8;
	}
	//xshx add
    int fd;
	log_info("file path :%s \n", MAIN_WORK_PATH(uuid));
    fd = open(MAIN_WORK_PATH(uuid),O_RDWR|O_CREAT|O_TRUNC);
    if(fd <0 ){
        log_info("create uuid file failed \n");
    }else {
        log_info("g_uu_id %s\n", g_uu_id.UID);
        write(fd, &g_uu_id, UUID_LEN);
        fsync(fd);
        close(fd);
        //end add
    }
	log_info("reg uuid<len=%d> : L<0x%08x>, H<0x%08x>, %s.\n", sizeof(g_uu_id), g_uu_id.tUID.L, g_uu_id.tUID.H, g_uu_id.UID);

	// 主控返回响应指令: 用户注册
	cmdCommRsp2MCU(CMD_FACE_REG, SUCCESS);;
	
	log_info("Switch to face register...\n");
	// kill face_recg progress
	system("killall -9 face_recognize");
	
	usleep(500000);
	// start face_reg progress
	system(MAIN_WORK_PATH(face_reg.sh));

	return 0;
}

//set current time
void SysTimeSet(uint8_t year,uint8_t month,uint8_t day,uint8_t hour,uint8_t min,uint8_t sec)
{
	struct tm tptr;
	struct timeval tv;

	tptr.tm_year = year + 100; // 从2000 开始的年数
	tptr.tm_mon = month - 1;
	tptr.tm_mday = day;
	tptr.tm_hour = hour;
	tptr.tm_min = min;
	tptr.tm_sec = sec;

	tv.tv_sec = mktime(&tptr);
	tv.tv_usec = 0;
	settimeofday(&tv, NULL);

	time_t		curTime;
	struct tm *loc_tm = NULL;
	curTime = time(NULL);
	loc_tm = localtime(&curTime);
	log_info("set date and time: %04d-%02d-%02d %02d:%02d:%02d\n",\
		loc_tm->tm_year+1900, loc_tm->tm_mon+1, loc_tm->tm_mday,\
		loc_tm->tm_hour, loc_tm->tm_min, loc_tm->tm_sec);
}

// 主控接收指令: 时间同步请求
int cmdSetSysTimeSynProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t year=0, month=0,day=0,hour=0,min=0,sec=0, i=0;
	uint8_t ret = SUCCESS;
	if(nMessageLen == 6)
	{
		year = StrGetUInt8( pszMessage+i );i++;
		month = StrGetUInt8( pszMessage+i );i++;
		day = StrGetUInt8( pszMessage+i );i++;
		hour = StrGetUInt8( pszMessage+i );i++;
		min = StrGetUInt8( pszMessage+i );i++;
		sec = StrGetUInt8( pszMessage+i );i++;				
	}
	else
	{
		ret = FAILED;
	}
	
	if((month==0||month>12) || (day==0 || day>31) || hour>24 || min>60 || sec>60)
	{
		ret = FAILED;
		log_error("Set Time value error, please check!SetRTCTime:%02d-%02d-%02d %02d:%02d:%02d!\n", year, month,day,hour,min,sec); 				
	}
	else
	{		
		/* 系统时间设置 */
		SysTimeSet(year, month, day, hour, min, sec);
	}
	
	// 主控返回响应指令: 时间同步
	//cmdCommRsp2MCU(CMD_BLE_TIME_SYNC, SUCCESS);;

	return 0;
}

// 主控接收指令: 删除用户请求
int cmdDeleteUserReqProcByHead(unsigned char nHead, unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = FAILED, len=0;	
	char szBuffer[128]={0};
	uUID	uu_id;

	// 解析指令
	if((nMessageLen < sizeof(szBuffer)) && nMessageLen == UUID_LEN)
	{
		memcpy(&uu_id, pszMessage, nMessageLen);
	}
	
	if(stSysLogCfg.log_level < LOG_INFO)
	{
		int i = 0;
		printf("uu_id.UID<len=%d>:", nMessageLen); 
		for(i=0; i<nMessageLen; i++)	
		{		
			printf("0x%02x	 ", *(uu_id.UID+i));	
		}	
		printf("\n");
	}		
	log_info("delete uuid : <0x%08x>, <0x%08x>.\n", uu_id.tUID.H, uu_id.tUID.L);
	if (uu_id.tUID.H == 0xFFFFFFFF && uu_id.tUID.L == 0xFFFFFFFF) {
		// clear users
		char buf[64] = {0};
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "rm -f %s", MAIN_WORK_PATH(personface.db));
		system(buf);
		system("sync");
		ret = SUCCESS;
		log_info("clear user success with cmd %s", buf);
	} else {
		// delete single user
		//ָ���		
		log_info("delete single user start");
		if(p_person_face_mgr.loadPersonFaceMgr())
		{
			ret = (p_person_face_mgr.deletePersonByUUID(uu_id) ? SUCCESS : FAILED);
			log_info("delete single user ret %d", ret);
		} else {
			log_info("delete single user failed, no person face mgr");
		}
		system("sync");
	}

	log_info("delete uuid : <0x%08x>, <0x%08x> result %d nHead 0x%2x.\n", uu_id.tUID.H, uu_id.tUID.L, ret, nHead);
	if (nHead == HEAD_MARK_MQTT) {
		//���ط�����Ӧָ���MQTT: ɾ���û���Ӧ
		cmdCommRsp2MqttByHead(HEAD_MARK_MQTT, CMD_FACE_DELETE_USER, ret);
	} else {
		cmdCommRsp2MCU(CMD_FACE_DELETE_USER, ret);
	}

	// //���ط�����Ӧָ��: ɾ���û���Ӧ
	// if(ret == true)
	// 	cmdCommRsp2MCU(CMD_FACE_DELETE_USER, SUCCESS);
	// else
	// 	cmdCommRsp2MCU(CMD_FACE_DELETE_USER, FAILED);

	return 0;
}

//���ؽ���ָ��:  ɾ���û�����
int cmdDeleteUserReqProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	cmdDeleteUserReqProcByHead(HEAD_MARK, nMessageLen, pszMessage);
}

void SetSysToFactory()
{
	char buf[64] = {0};
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "rm -f %s", MAIN_WORK_PATH(*.db));
	system(buf);
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "rm -f %s", MAIN_WORK_PATH(*.cfg));
	system(buf);
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "rm -rf /opt/smartlocker/pic/*");
	system(buf);
	system("sync");
}

// 主控返回响应指令: 请求恢复出厂设置响应
// 主控接收指令: 请求恢复出厂设置请求
int cmdReqResumeFactoryProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;

	// 返回响应
	cmdCommRsp2MCU(CMD_REQ_RESUME_FACTORY, SUCCESS);

	// 消息处理
	SetSysToFactory();
	
	/*关机 */
	cmdCloseFaceBoardReq();
	
	system("echo \"Face Deletion done, A7 enter VLLS!\""); 
	system("echo mem > /sys/power/state");
	
	return 0;
}

// 主控返回响应指令:  手机APP 请求注册激活响应
int cmdReqActiveByPhoneRsp(uint8_t ret)
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/* 填充消息体 */
	StrSetUInt8((uint8_t*)pop, ret);
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t);

	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_REG_ACTIVE_BY_PHONE,
		MsgLen);
	
	/* 计算 FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToMCU((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	return 0;
}

// 主控接收指令: 手机APP 请求注册激活请求
int cmdReqActiveByPhoneProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;
	int result = 0;

	if((access("uuid_tmp.txt", F_OK))!=-1)	 
	{
		log_info("rm -f uuid_tmp.txt;   Active OK.");
		system("rm -f uuid_tmp.txt");
		system("sync");
	}
	// 返回响应
	cmdReqActiveByPhoneRsp(ret);
	usleep(100000);
	/* 注册激活后关机 */
	cmdCloseFaceBoardReq();

	return 0;
}

// 主控接收指令: 请求人脸识别
int cmdReqFaceRecgProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;

	log_info("Switch to face recognize...\n");
	// start face_reg progress
	system(MAIN_WORK_PATH(face_recg.sh));

	return 0;
}

// 主控接收指令: WIFI SSID 设置请求
int cmdWifiSSIDProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = FAILED, len=0;	
	char wifi_ssid[WIFI_SSID_LEN_MAX+1]={0};

	// 解析指令
	if((nMessageLen < sizeof(wifi_ssid)) && nMessageLen < WIFI_SSID_LEN_MAX)
	{
		memset(wifi_ssid, 0, sizeof(wifi_ssid));
		memcpy(wifi_ssid, pszMessage, nMessageLen);
		
		// 保存设置到系统配置文件
		log_info("wifi ssid : <%s>.\n", wifi_ssid);
		update_wifi_ssid("./config.ini", wifi_ssid);	
		//read_config("./config.ini");
		
		ret = SUCCESS;
	}	

	cmdCommRsp2MCU(CMD_WIFI_SSID, ret);
	return 0;
}

// 主控接收指令: WIFI SSID 设置请求
int cmdWifiPwdProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = FAILED, len=0;	
	char  wifi_pwd[WIFI_PWD_LEN_MAX+1]={0};

	// 解析指令
	if((nMessageLen < sizeof(wifi_pwd)) && nMessageLen < WIFI_SSID_LEN_MAX)
	{
		memset(wifi_pwd, 0, sizeof(wifi_pwd));
		memcpy(wifi_pwd, pszMessage, nMessageLen);
		
		// 保存设置到系统配置文件
		log_info("wifi pwd : <%s>.\n", wifi_pwd);
		update_wifi_pwd("./config.ini", wifi_pwd);	
		//read_config("./config.ini");
		ret = SUCCESS;
	}

	cmdCommRsp2MCU(CMD_WIFI_PWD, ret);
	return 0;
}

// 主控接收指令: 设置MQTT 参数
int cmdMqttParamSetProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = FAILED, len=0;	
	uint8_t mqtt_user[MQTT_USER_LEN+1] = {0}; 
	uint8_t mqtt_pwd[MQTT_PWD_LEN+1] = {0};	
	char mqtt_user_tmp[32] = {0}; 
	char mqtt_pwd_tmp[32] = {0}; 
	
	// 解析指令
	if(nMessageLen == MQTT_USER_LEN+MQTT_PWD_LEN)
	{
		memset(mqtt_user, 0, sizeof(mqtt_user));
		memcpy(mqtt_user, pszMessage, MQTT_USER_LEN);
		memset(mqtt_pwd, 0, sizeof(mqtt_pwd));
		memcpy(mqtt_pwd, pszMessage+MQTT_USER_LEN, MQTT_PWD_LEN);
		log_info("mqtt_user<0x%02x%02x%02x%02x%02x%02x>,mqtt_pwd<0x%02x%02x%02x%02x%02x%02x%02x%02x>!\n", \
		mqtt_user[0],mqtt_user[1],mqtt_user[2],mqtt_user[3],mqtt_user[4],mqtt_user[5],		\
				mqtt_pwd[0],mqtt_pwd[1],mqtt_pwd[2],mqtt_pwd[3],mqtt_pwd[4],mqtt_pwd[5],mqtt_pwd[6],mqtt_pwd[7]);
		
		//�������õ�ϵͳ�����ļ�	
		memset(mqtt_pwd_tmp, 0, sizeof(mqtt_user_tmp));
		HexToStr(mqtt_user_tmp, mqtt_user, MQTT_USER_LEN);
		memset(mqtt_pwd_tmp, 0, sizeof(mqtt_pwd_tmp));
		HexToStr(mqtt_pwd_tmp, mqtt_pwd, MQTT_PWD_LEN);
		update_mqtt_opt("./config.ini", mqtt_user_tmp, mqtt_pwd_tmp);	
		//read_config("./config.ini");
		log_info("mqtt user :<%s>, mqtt_pwd:<%s>.\n", mqtt_user_tmp, mqtt_pwd_tmp);
		
		system("sync");
		ret = SUCCESS;
	}
	
	// 重启MQTT 以新的参数启动
	system("killall mqtt");
	system("/opt/smartlocker/mqtt &");

	cmdCommRsp2MCU(CMD_WIFI_MQTT, ret);
	return 0;
}

// 主控接收指令: 蓝牙模块状态信息上报
int cmdBTInfoRptProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = FAILED, len=0;	
	uint8_t bt_ver = 0; 
	uint8_t bt_mac[6] = {0};	
	char bt_verbuf[4] = {0}; 
	char bt_ver_tmp[32] = {0}; 
	char bt_mac_tmp[16] = {0}; 
	
	//解析指令
	if(nMessageLen == 1+6)
	{
		bt_ver = StrGetUInt8( pszMessage );
		memset(bt_mac, 0, sizeof(bt_mac));
		memcpy(bt_mac, pszMessage+1, 6);

		// 获取字符串格式的版本号
		memset(bt_verbuf, 0, sizeof(bt_verbuf));		
		memset(bt_ver_tmp, 0, sizeof(bt_ver_tmp));
		sprintf(bt_verbuf, "%03d", bt_ver);
		sprintf(bt_ver_tmp, "W7_52832_V%c.%c.%c", bt_verbuf[0], bt_verbuf[1], bt_verbuf[2]);

		// 转换为字符格式的BT mac
		memset(bt_mac_tmp, 0, sizeof(bt_mac_tmp));
		sprintf(bt_mac_tmp, "%02x%02x%02x%02x%02x%02x",	\
			bt_mac[0],bt_mac[1],bt_mac[2],bt_mac[3],bt_mac[4],bt_mac[5]);

		// 保存设置到系统配置文件
		update_bt_info("./config.ini", bt_ver_tmp, bt_mac_tmp);	
		//read_config("./config.ini");
		log_debug("bt_ver :<%s>, bt_mac:<%s>.\n", bt_ver_tmp, bt_mac_tmp);
		
		system("sync");
		ret = SUCCESS;
	}

	//cmdCommRsp2MCU(CMD_BT_INFO, ret);
	return 0;
}

// 主控接收指令: 设置wifi的MQTT server 登录URL(可能是IP+PORT, 可能是域名+PORT)
int cmdMqttSvrURLProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = FAILED, len=0;	
	char MqttSvr_Url[MQTT_SVR_URL_LEN_MAX+1]={0};

	system("killall face_recgnize");
	// 解析指令
	if((nMessageLen < sizeof(MqttSvr_Url)) && nMessageLen < MQTT_SVR_URL_LEN_MAX)
	{
		memset(MqttSvr_Url, 0, sizeof(MqttSvr_Url));
		memcpy(MqttSvr_Url, pszMessage, nMessageLen);
		
		// 保存设置到系统配置文件
		log_info("MqttSvr_Url : <%s>.\n", MqttSvr_Url);
		update_MqttSvr_opt("./config.ini", MqttSvr_Url);	
		//read_config("./config.ini");
		
		ret = SUCCESS;
	}
	cmdCommRsp2MCU(CMD_WIFI_MQTTSER_URL, ret);
	return 0;
}

// 主控接收指令: 设置NetworkOptVer
int cmdSetNetworkOptVerProc(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = FAILED, len=0;	
	char NetworkOptVer=0;
	char buftmp[4] = {0};
	// 解析指令
	//����ָ��
	NetworkOptVer = StrGetUInt8( pszMessage );
	
	// 保存设置到系统配置文件
	memset(buftmp, 0, sizeof(buftmp));
	snprintf(buftmp, sizeof(buftmp), "%d", NetworkOptVer);
	log_info("NetworkOptVer : <%s>.\n", buftmp);
	update_NetworkOptVer_info("./config.ini", buftmp);	
	//read_config("./config.ini");
	
	ret = SUCCESS;
	return 0;
}

// 主控发送指令: 获取网络参数配置版本信息请求
int cmdNetworkOptVerReq()
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/* 填充消息体 */

	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_GETNETWORK_OPTVER,
		MsgLen);
	
	/* 计算FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToMCU((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	return 0;
}

// 主控发送指令: 获取网络参数配置请求
int cmdNetworkOptReq()
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体 */

	/*填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_NETWORK_OPT,
		MsgLen);
	
	/*计算FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToMCU((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	return 0;
}

// 主控接收指令:
int cmdGetNetworkOptVer(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t  len=0;	
	uint8_t McuNetworkOptVer = 0, LocalNetworkOptVer = 0;
	char buf[4] = {0}; 

	//��ȡMcu����������汾��
	McuNetworkOptVer = StrGetUInt8( pszMessage );

	memset(buf, 0, sizeof(buf));
	// 获取本地配置文件中网络参数版本号
	int ret = read_default_config_from_file("/opt/smartlocker/config.ini", (char *)buf, "sys", "NetworkOptVer", "0");
	LocalNetworkOptVer = atoi(buf);
	log_info("McuNetworkOptVer<%d>, LocalNetworkOptVer<%d>\n", McuNetworkOptVer, LocalNetworkOptVer);
	//�������ȣ������»�ȡ
	if(ret || (McuNetworkOptVer != LocalNetworkOptVer))
	{
		cmdNetworkOptReq();
	}
	
	return 0;
}

//======= 需要通过MQTT来转发的消息=====
int SendMsgToMqtt(unsigned char *MsgBuf, unsigned char MsgLen)
{
	int ret; 
#if 0
	if(stSysLogCfg.log_level < LOG_DEBUG)	
	{
		int i = 0;
		printf("\n<<<send to MQTT MsgQueue<len:%d>:", MsgLen);	
		for(i; i<MsgLen; i++)	
		{		
			printf("0x%02x   ", MsgBuf[i]);	
		}	
		printf("\n");
	}
#endif
	MqttQMsgSend(MsgBuf, MsgLen);

	return ret;
}

//X7->MQTT: ͨ����Ӧ
int cmdCommRsp2Mqtt(unsigned char CmdId, uint8_t ret) {
	cmdCommRsp2MqttByHead(HEAD_MARK, CmdId, ret);
}

int cmdCommRsp2MqttByHead(unsigned char nHead, unsigned char CmdId, uint8_t ret)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;
	uint8_t status = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	/* 填充消息体 */
	StrSetUInt8((uint8_t*)pop, ret);
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t); 
	
	/*�����Ϣͷ*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		nHead,
		CmdId,
		MsgLen);
	
	/* 计算FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	log_info("cmdCommRsp2MqttByHead send to 0x%02x cmd 0x%02x result %d\n", nHead, CmdId, ret);
	//���Ϳ�����Ӧ��wifi��ص�mqttģ��
	SendMsgToMqtt((uint8_t*)szBuffer, iBufferSize+CRC16_LEN); 
	
	return 0;
}

// 上传开门记录到mqtt 模块
int cmdOpenDoorRec2Mqtt(uUID uu_id, uint8_t type, uint8_t ret)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;
	uint8_t status = 0;
	
	time_t	 timep; 
	time(&timep);// 返回的是日历时间, 即国际标准时间

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	/* 填充消息体 */
	memcpy(pop, uu_id.UID, sizeof(uUID));
	MsgLen += sizeof(uUID);
	pop += sizeof(uUID);
	StrSetUInt8((uint8_t*)pop, type);// 3-����������ʽ
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t); 
 	StrSetUInt32( (uint8_t*)pop, (uint32_t)timep)	;
	MsgLen += sizeof(uint32_t);
	pop += sizeof(uint32_t); 
	StrSetUInt8((uint8_t*)pop, stInitSyncInfo.PowerVal);// 电量
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t); 
	StrSetUInt8((uint8_t*)pop, ret);// 0:等待开锁;1:开锁成功; 2:开锁失败
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t); 
	
	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_OPEN_LOCK_REC,
		MsgLen);
	
	/* 计算FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	// 发送开门响应给wifi 相关的mqtt 模块
	SendMsgToMqtt((uint8_t*)szBuffer, iBufferSize+CRC16_LEN); 
	
	return 0;
}

// 主控接收指令: wifi 网络时间同步响应
int cmdWifiTimeSyncRsp(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;	
	unsigned char szBuffer[128]={0};
	int iBufferSize = 0;
	unsigned char *pop = NULL;
	unsigned char MsgLen = 0;
	unsigned char	cal_FCS;

	ret = StrGetUInt8( pszMessage );

	if(stSystemCfg.flag_wifi_enable)
	{
		cmdCommRsp2Mqtt(CMD_WIFI_TIME_SYNC, ret);
	}
	
	return 0;
}

//MCU->X7->MQTT:  WIFI 同步订单时间响应
int cmdWifiOrderTimeSyncRsp(unsigned char nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = StrGetUInt8( pszMessage );

	cmdCommRsp2Mqtt(CMD_ORDER_TIME_SYNC, ret);
	return 0;
}

// int ProcMessageFromMCU(
int ProcMessage(
	unsigned char nCommand,
	unsigned char nMessageLen,
	const unsigned char *pszMessage)
{ 
	ProcMessageByHead(HEAD_MARK, nCommand, nMessageLen, pszMessage);
}

int ProcMessageFromMQTT(
	unsigned char nCommand,
	unsigned char nMessageLen,
	const unsigned char *pszMessage)
{ 
	ProcMessageByHead((unsigned char)(HEAD_MARK_MQTT), nCommand, nMessageLen, pszMessage);
}

// source: 0x23 MCU 0x24 MQTT
int ProcMessageByHead(
	unsigned char nHead,
	unsigned char nCommand,
	unsigned char nMessageLen,
	const unsigned char *pszMessage)
{ 
	int faceId, len,ret; 
	pid_t status;
	
	log_info("======nHead[0x%X] Command[0x%X], nMessageLen<%d>\n", nHead, nCommand, nMessageLen);	
	switch (nCommand)
	{
		case CMD_INITOK_SYNC:
		{
			cmdSysInitOKSyncRsp(nMessageLen, pszMessage);
			break;
		}	
		case CMD_OPEN_DOOR:
		{
			cmdOpenDoorRsp(nMessageLen, pszMessage);
			break;
		}
		case CMD_CLOSE_FACEBOARD:
		{
			//cmdCloseFaceBoardRsp(nMessageLen, pszMessage);
			break;
		}
		case CMD_REG_STATUS_NOTIFY:
		{
			cmdRegStatusNotifyRsp(nMessageLen, pszMessage);
			break;
		}
		case CMD_FACE_REG:
		{			
			cmdUserRegReqProc(nMessageLen, pszMessage);
			break;
		}
		case CMD_FACE_REG_RLT:
		{			
			break;
		}		
		case CMD_BLE_TIME_SYNC:
		{
			cmdSetSysTimeSynProc(nMessageLen, pszMessage);
			break;
		}
		case CMD_FACE_DELETE_USER:	
		{
			// cmdDeleteUserReqProc(nMessageLen, pszMessage);
			cmdDeleteUserReqProcByHead(nHead, nMessageLen, pszMessage);
			break;
		}
		case CMD_REQ_RESUME_FACTORY:	
		{
			cmdReqResumeFactoryProc(nMessageLen, pszMessage);
			break;
		}	
		case CMD_REG_ACTIVE_BY_PHONE:	
		{
			cmdReqActiveByPhoneProc(nMessageLen, pszMessage);
			break;
		}	
		case CMD_FACE_RECOGNIZE:	
		{
			cmdReqFaceRecgProc(nMessageLen, pszMessage);
			break;
		}	
		case CMD_WIFI_SSID:	
		{
			cmdWifiSSIDProc(nMessageLen, pszMessage);
			break;
		}	
		case CMD_WIFI_PWD:	
		{
			cmdWifiPwdProc(nMessageLen, pszMessage);
			break;
		}	
		case CMD_WIFI_MQTT:	
		{
			cmdMqttParamSetProc(nMessageLen, pszMessage);
			break;
		}	
		case CMD_WIFI_TIME_SYNC:	
		{
			cmdWifiTimeSyncRsp(nMessageLen, pszMessage);
			break;
		}	
		case CMD_ORDER_TIME_SYNC:
		{
			cmdWifiOrderTimeSyncRsp(nMessageLen, pszMessage);
			break;
		}
		case CMD_WIFI_OPEN_DOOR:
		{
			cmdWifiOpenDoorRsp(nMessageLen, pszMessage);
			break;
		}
		case CMD_BT_INFO:
		{
			cmdBTInfoRptProc(nMessageLen, pszMessage);
			
			break;
		}
		case CMD_WIFI_MQTTSER_URL:
		{
			cmdMqttSvrURLProc(nMessageLen, pszMessage);
			break;
		}
		case CMD_GETNETWORK_OPTVER:
		{
			cmdGetNetworkOptVer(nMessageLen, pszMessage);
			break;
		}
		case CMD_SETNETWORK_OPTVER:
		{
			cmdSetNetworkOptVerProc(nMessageLen, pszMessage);
			break;
		}
		default:
			log_info("No effective cmd from MCU!\n");
			break;				
	}

	return 0;
}

// 需要 faceloop 来转发的消息
int SendMsgToFaceLoop(unsigned char *MsgBuf, unsigned char MsgLen)
{
	int ret; 

	/*if(stSysLogCfg.log_level < LOG_DEBUG)	
	{
		int i = 0;
		printf("\n<<<send msg to MsgQueue<len:%d>:", MsgLen);	
		for(i; i<MsgLen; i++)	
		{		
			printf("0x%02x   ", MsgBuf[i]);	
		}	
		printf("\n");
	}*/

	FaceQMsgSend(MsgBuf, MsgLen);

	return ret;
}

// 主控发送测试指令: 注册状态通知请求
/* 算法返回值:
0 表示注册成功
-1 表示未初始化
-2 表示人脸注册失败或未检测到人脸
-3 表示人脸关键点定位失败
-4 表示图像为空
-5 表示提取特征失败
-8 标识活体检测不通过
-10 表示ROI区域的亮度值高于或低于设定阀值
-11 表示检测到人脸,但人脸区域亮度值高于或低于阀值(可通过 setPara("faceMinLight", val) 和setPara("faceMaxLight", val)调整最小亮度阀值和最大亮度阀值)
-20 表示检测到的人脸面积过大
-21 表示检测到的人脸偏左
-22 表示检测到的人脸偏上
-23 表示检测到的人脸偏右
-24 表示检测到的人脸偏下
-25 表示检测到的人脸区域过小
*/
int cmdRegStatusNotifyReq(uint8_t regStatus)
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/* 填充消息体 */
	StrSetUInt8((uint8_t*)pop, regStatus);
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t);

	/*填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_REG_STATUS_NOTIFY,
		MsgLen);
	
	/*计算FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToFaceLoop((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	return 0;
}

// 主控发送指令:注册结构通知请求
/*status:
 * 0- 完成注册,待激活
 * 1- 注册失败
 * */
int cmdRegResultNotifyReq(uUID uu_id, uint8_t regResult)
{
	char tmpBuf[64] = {0};
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/* 填充消息体 */
	memcpy(pop, uu_id.UID, sizeof(uUID));
	MsgLen += sizeof(uUID);
	pop += sizeof(uUID);
	StrSetUInt8((uint8_t*)pop, regResult);
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t);

	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_FACE_REG_RLT,
		MsgLen);
	
	/* 计算 FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToFaceLoop((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	if(stSystemCfg.flag_wifi_enable)
	{
		// 发送指令给 wifi 相关的mqtt 模块
		MqttQMsgSend((uint8_t*)szBuffer, iBufferSize+CRC16_LEN); 
	}

	return 0;
}

// 主控发送测试指令:开门请求
int cmdOpenDoorReq(uUID uu_id)
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	/* 填充消息体 */
	memcpy(pop, uu_id.UID, sizeof(uUID));
	MsgLen += sizeof(uUID);
	pop += sizeof(uUID);

	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_OPEN_DOOR,
		MsgLen);
	
	/* 计算 FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToFaceLoop((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	return 0;
}

// 主控发送: 关机请求
int cmdCloseFaceBoardReq()
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/* 填充消息体 */

	/* 填充消息头 */
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_CLOSE_FACEBOARD,
		MsgLen);
	
	/* 计算 FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToFaceLoop((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);

	return 0;
}

void yuyv_to_rgba(unsigned char *yuv_buffer,unsigned char *rgb_buffer,int iWidth,int iHeight)
{
	int x;
	int z=0;
	unsigned char *ptr = rgb_buffer;
	unsigned char *yuyv= yuv_buffer;
	for (x = 0; x < iWidth*iHeight/2; x++)
	{
		int r, g, b;
		int y, u, v;

		y = yuyv[0];
		u = yuyv[3] - 128;
		v = yuyv[1] - 128;

		r = (y + (351 * v)) >> 8;
		g = (y - (86 * u) - (179 * v)) >> 8;
		b = (y + (443 * u)) >> 8;

		*(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
		*(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
		*(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);
		*(ptr++) = 0xFF;

		y = yuyv[2];
		r = (y + (351 * v)) >> 8;
		g = (y - (86 * u) - (179 * v)) >> 8;
		b = (y + (443 * u)) >> 8;

		*(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
		*(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
		*(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);
		*(ptr++) = 0xFF;

		yuyv += 4;
	}
}

/*
 * 通过设置PWM,设置IR补光灯的亮度
 * 由于调光灯的控制引脚在M4端的11脚, 所有需要通过RPMSG 通道来发送
 * */
int SetIrLightingLevel(uint8_t val)
{	
	char szBuffer[32]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;
	
	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/* 填充消息体 */
	StrSetUInt8((uint8_t*)pop, val);
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t);

	/* 填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		HEAD_MARK,
		CMD_IRLIGHT_PWM_Req,
		MsgLen);
	
	/* 计算FCS */
	unsigned short cal_crc16 = CRC16_X25((uint8_t*)szBuffer, MsgLen+sizeof(MESSAGE_HEAD));
	memcpy((uint8_t*)pop, &cal_crc16, sizeof(cal_crc16));
	
	SendMsgToMCU((uint8_t*)szBuffer, iBufferSize+CRC16_LEN);
	return 0;
}
//open
void OpenLcdBackgroud()
{
    log_info("------open Lcd bg-----\n");
    system("echo out > /sys/class/gpio/gpio77/direction");
    system("echo 1 > /sys/class/gpio/gpio77/value");
}
// 关LCD背光
void CloseLcdBackgroud()
{	
	log_info("------close Lcd bg-----\n");
	system("echo out > /sys/class/gpio/gpio77/direction");
	system("echo 0 > /sys/class/gpio/gpio77/value");
}
