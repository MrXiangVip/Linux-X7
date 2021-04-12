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


#include "function.h"
#include "util_crc32.h"
#include "sys_upgrade.h"
#include "hl_MsgQueue.h"  

using namespace std;

stSysCfg stSystemCfg;
stLogCfg stSysLogCfg;
int tty_fd=-1;

static unsigned char g_nSessionID = 0; //保存本地发送的sessionID,每发送一条消息都+1
static unsigned char g_OutSessionID= 0;//保存外部消息发过来的sessionID;

/****************************************************************************************
函数名称：myWriteData2File
函数功能：写数据到文件
入口参数：
出口参数：
返回值：
****************************************************************************************/
int myWriteData2File(const char * pFileName, unsigned char *pData, int iDataLen)
{
	FILE *fp = fopen(pFileName, "wb");
	if (fp != NULL)
	{
		fwrite(pData, 1, iDataLen, fp);

		/* 将文件同步,及时写入磁盘,不然，系统重启动，导致文件内容丢失 */
		fflush(fp);		/* 刷新内存缓冲，将内容写入内核缓冲 */
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
函数名称：myReadDataFromFile
函数功能：从本地文件读取数据
入口参数：
出口参数：
返回值：
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

int SendMsgToRpMSG(unsigned char *MsgBuf, unsigned char MsgLen)
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

	if(tty_fd < 0)
	{
		log_error("open ttyRPMSG device failed\n");
		return -1;
	}
	else
	{
		ret = write(tty_fd, (unsigned char *)MsgBuf, MsgLen);
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
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_reg_threshold=%d\n", pstSystemCfg->face_reg_threshold_val);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_recg_threshold=%d\n", pstSystemCfg->face_recg_threshold_val);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_reg_angle=%4.1f\n", pstSystemCfg->face_reg_angle);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_recg_angle=%4.1f\n", pstSystemCfg->face_recg_angle);
	iBufferSize += sprintf(szBuffer + iBufferSize, "auto_udisk_upgrade=%d\n", pstSystemCfg->auto_udisk_upgrade);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_reg_timeout=%d\n", pstSystemCfg->face_reg_timeout);
	iBufferSize += sprintf(szBuffer + iBufferSize, "face_recg_timeout=%d\n", pstSystemCfg->face_recg_timeout);
	iBufferSize += sprintf(szBuffer + iBufferSize, "flag_muxAngle_reg=%d\n", pstSystemCfg->flag_muxAngle_reg);
	iBufferSize += sprintf(szBuffer + iBufferSize, "flag_auto_recg=%d\n", pstSystemCfg->flag_auto_recg);
	iBufferSize += sprintf(szBuffer + iBufferSize, "flag_facePosition_Rpt=%d\n", pstSystemCfg->flag_facePosition_Rpt);
	iBufferSize += sprintf(szBuffer + iBufferSize, "flag_faceBox=%d\n", pstSystemCfg->flag_faceBox);

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
			log_error("Err, can't fopen<./sys.cfg>\n");
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
				log_debug("face_reg_threshold<%d>	", pstSystemCfg->face_reg_threshold_val);
			}
			if (strstr(szMsg, "face_recg_threshold=") != NULL) 
			{
				len = strlen("face_recg_threshold=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->face_recg_threshold_val));//skip "face_threshold="
				if((pstSystemCfg->face_recg_threshold_val > 80) || (pstSystemCfg->face_recg_threshold_val <40))
				{
					pstSystemCfg->face_recg_threshold_val =  FACE_RECG_THRESHOLD_VALUE;
				}
				log_debug("face_recg_threshold<%d>	", pstSystemCfg->face_recg_threshold_val);
			}
			else if (strstr(szMsg, "face_reg_angle=") != NULL) 
			{
				len = strlen("face_reg_angle=");
				sscanf(&szMsg[len], "%f", &(pstSystemCfg->face_reg_angle));//skip "face_reg_angle="
				if((pstSystemCfg->face_reg_angle > 40.0) || (pstSystemCfg->face_reg_angle <5.0))
				{
					pstSystemCfg->face_reg_angle =  FACE_DETECT_REGISTER_ANGEL;
				}
				log_debug("face_reg_angle<%4.1f>	", pstSystemCfg->face_reg_angle);
			}
			else if (strstr(szMsg, "face_recg_angle=") != NULL) 
			{
				len = strlen("face_recg_angle=");
				sscanf(&szMsg[len], "%f", &(pstSystemCfg->face_recg_angle));//skip "face_threshold="
				if((pstSystemCfg->face_recg_angle > 40.0) || (pstSystemCfg->face_recg_angle <5.0))
				{
					pstSystemCfg->face_recg_angle =  FACE_DETECT_RECGONIZE_ANGEL;
				}
				log_debug("face_recg_angle<%4.1f>	", pstSystemCfg->face_recg_angle);
			}
			else if (strstr(szMsg, "auto_udisk_upgrade=") != NULL) 
			{
				len = strlen("auto_udisk_upgrade=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->auto_udisk_upgrade));
				log_debug("auto_udisk_upgrade<%d>	", pstSystemCfg->auto_udisk_upgrade);
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
			else if (strstr(szMsg, "flag_muxAngle_reg=") != NULL) 
			{
				len = strlen("flag_muxAngle_reg=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->flag_muxAngle_reg));
				log_debug("flag_muxAngle_reg<%d>	", pstSystemCfg->flag_muxAngle_reg);
			}
			else if (strstr(szMsg, "flag_auto_recg=") != NULL) 
			{
				len = strlen("flag_auto_recg=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->flag_auto_recg));
				log_debug("flag_auto_recg<%d>	", pstSystemCfg->flag_auto_recg);
			}
			else if (strstr(szMsg, "flag_facePosition_Rpt=") != NULL) 
			{
				len = strlen("flag_facePosition_Rpt=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->flag_facePosition_Rpt));
				log_debug("flag_facePosition_Rpt<%d>	", pstSystemCfg->flag_facePosition_Rpt);
			}
			else if (strstr(szMsg, "flag_faceBox=") != NULL) 
			{
				len = strlen("flag_faceBox=");
				sscanf(&szMsg[len], "%d", &(pstSystemCfg->flag_faceBox));
				log_debug("flag_faceBox<%d>	", pstSystemCfg->flag_faceBox);
			}

			memset(szMsg, 0, sizeof(szMsg));
		}
	}
	else
	{
		/*如果不存在配置文件，那么用系统默认的值，并生成配置文件*/
		pstSystemCfg->face_reg_threshold_val =	FACE_REG_THRESHOLD_VALUE;
		pstSystemCfg->face_recg_threshold_val = FACE_RECG_THRESHOLD_VALUE;
		pstSystemCfg->face_reg_angle =  FACE_DETECT_REGISTER_ANGEL;
		pstSystemCfg->face_recg_angle =	FACE_DETECT_RECGONIZE_ANGEL;
		pstSystemCfg->auto_udisk_upgrade = 0;
		pstSystemCfg->face_reg_timeout =  FACE_REG_TIMEOUT;
		pstSystemCfg->face_recg_timeout =	FACE_RECG_TIMEOUT;
		pstSystemCfg->flag_muxAngle_reg = 1;
		pstSystemCfg->flag_auto_recg = 0;
		pstSystemCfg->flag_facePosition_Rpt = 0;
		pstSystemCfg->flag_faceBox = 1;
		log_info("Warring, not exit <./sys.cfg>.\n");

		log_debug("face_reg_threshold<%d>,face_recg_threshold<%d>,  \
			face_reg_angle<%4.1f>,face_recg_angle<%4.1f>,auto_udisk_upgrade<%d>,  \
			face_reg_timeout<%d>,face_recg_timeout<%d>,flag_muxAngle_reg<%d>, \
			flag_auto_recg<%d>,flag_facePosition_Rpt<%d>, flag_faceBox<%d>\n",	 \
			pstSystemCfg->face_reg_threshold_val,pstSystemCfg->face_recg_threshold_val, pstSystemCfg->face_reg_angle, \
			pstSystemCfg->face_recg_angle,pstSystemCfg->auto_udisk_upgrade,  \
			pstSystemCfg->face_reg_timeout,pstSystemCfg->face_reg_timeout, pstSystemCfg->flag_muxAngle_reg,\
			pstSystemCfg->flag_auto_recg, pstSystemCfg->flag_facePosition_Rpt, pstSystemCfg->flag_faceBox);
		
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


int faceDeleteProc(uint16_t personID)
{
	RSApiHandle rsHandle;
	int ret = -1;

	if(!rsHandle.Init(FACE_LOOP)) 
	{
		cerr << "Failed to init the ReadSense lib" << endl;
		return ret;  //-1: RS_RET_UNKOWN_ERR
	}	
					
	ret = rsHandle.DeleteFace(personID);
	if(ret == RS_RET_OK)  //0: RS_RET_OK
		cout << "face personID: id  "<< personID << endl;
	
	return ret;
}

int GetAlbumSizeFromDataBaseProc(uint16_t *faceSize)
{
	RSApiHandle rsHandle;
	int ret = -1;

	if(!rsHandle.Init(FACE_LOOP)) 
	{
		cerr << "Failed to init the ReadSense lib" << endl;
		return ret;  //-1: RS_RET_UNKOWN_ERR
	}	
					
	ret = rsHandle.GetAlbumSizeFromDataBase(faceSize);
	if(ret == RS_RET_OK)  //0: RS_RET_OK
		cout << "DB_ faceSize: "<< faceSize << endl;
	
	return ret;
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

//判断一个int变量的每个bit位的值（1或者0）
int GetBitStatu(int num, int pos)
{
    if(num & (1 << pos)) //按位与之后的结果非0
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// 核查目录，若目录不存在，创建目录
bool FindOrCreateDirectory( const char* pszPath )
{
	DIR* dir;
	if(NULL==(dir=opendir(pszPath)))
		mkdir(pszPath,0775);

	closedir(dir);
	return true;
}

// 检测文件路径中的目录是否存在，不存在则创建
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
函数名称：MsgHead_Packet
函数功能：组包
入口参数：pszBuffer--数据
		HeadMark--消息标记
		CmdId--命令
		SessionID--会话ID
		MsgLen--消息长度
出口参数：无
返回值：成功返回消息的总长度(消息头+消息体+FCS长度)，失败返回-1
****************************************************************************************/
int MsgHead_Packet(
	char *pszBuffer,
	unsigned short HeadMark,
	unsigned short CmdId,
	unsigned char SessionID,
	unsigned char MsgLen)
{
	if (!pszBuffer)
	{
		printf("pszBuffer is NULL\n");
		return -1;
	}
	char *pTemp = pszBuffer;

	StrSetUInt16( (uint8_t *)pTemp, SNDMSG_HEAD_MARK );
	pTemp += sizeof(uint16_t);
	StrSetUInt16( (uint8_t *)pTemp, CmdId );
	pTemp += sizeof(uint16_t);
	StrSetUInt8( (uint8_t *)pTemp, SessionID );
	pTemp += sizeof(uint8_t);
	StrSetUInt8( (uint8_t *)pTemp, MsgLen );
	pTemp += sizeof(uint8_t);

	return MsgLen+ sizeof(MESSAGE_HEAD);
}

/****************************************************************************************
函数名称：MsgHead_Unpacket
函数功能：解包
入口参数：pszBuffer--数据
		iBufferSize--数据大小
		HeadMark--标记头
		CmdID--命令
		SessionID--会话ID
		MsgLen--消息长度
出口参数：无
返回值：成功返回消息内容的指针，失败返回-1
****************************************************************************************/
const unsigned char *MsgHead_Unpacket(
	const unsigned char *pszBuffer,
	unsigned short iBufferSize,
	unsigned short *HeadMark,
	unsigned short *CmdId,
	unsigned char *SessionID,
	unsigned char *MsgLen)
{
	if (!pszBuffer)
	{
		printf("pszBuffer is NULL\n");
		return NULL;
	}
	const unsigned char *pTemp = pszBuffer;

	*HeadMark = StrGetUInt16(pTemp);	
	pTemp += sizeof(uint16_t);
	*CmdId = StrGetUInt16(pTemp);
	pTemp += sizeof(uint16_t);
	*SessionID = StrGetUInt8(pTemp);
	pTemp += sizeof(uint8_t);
	*MsgLen = StrGetUInt8(pTemp);
	pTemp += sizeof(uint8_t);
	if (*HeadMark != RCVMSG_HEAD_MARK)
	{
		log_info("byVersion[0x%x] != MESSAGE_VERSION[0x%x]\n", \
			*HeadMark, RCVMSG_HEAD_MARK);
		return NULL;
	}

	if ((int)*MsgLen + sizeof(MESSAGE_HEAD) + FCS_LEN> iBufferSize)
	{
		log_info("pstMessageHead->MsgLen + sizeof(MESSAGE_HEAD) + FCS_LEN> iBufferSize\n");
		return NULL;
	}

	return pszBuffer + sizeof(MESSAGE_HEAD);
}

int cmdAckRsp(unsigned short nSessionID, unsigned char ack_status)
{
	char szBuffer[16]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;
	
	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	/*填充消息体*/
	StrSetUInt8((uint8_t *)pop, ack_status);
	MsgLen += sizeof(uint8_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_ACK_Rpt,
		nSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdSysUpdateRsp(unsigned char status)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/
	StrSetUInt8((uint8_t *)pop, status);
	MsgLen += sizeof(uint8_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_APP_UPDATE_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	uint8_t cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdSetSysToVLLSModeRsp()
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_A7VLLS_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	cal_FCS = Msg_UartCalcFCS((uint8_t *)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdSetSysToShutDownModeRsp()
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_A7SHUTDOWN_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdFaceDeleteRsp(uint16_t persionID)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	/*删除处理*/
	int ret;
	unsigned short value;
	pid_t status;
	if((persionID < MAX_PERSON_ID) && (persionID < 0xff00))  // < 0x100 for kds
	{
		log_info("Delete specific face ID ...\n");						
		ret = faceDeleteProc(persionID);
		if(ret == RS_RET_OK)
			value = persionID;
		else
		{
			value = 0x0000;  //delete ID fail
			log_info("Face ID delete fail!\n");
		}
	}
	else if(persionID == 0xFFFF)  //delete the database
	{	
		log_info("Delete the database file!!!\n");
		status = system("rm -rf /opt/smartlocker/readsense.db");
		if(0 == WEXITSTATUS(status))
		{
			value = 0xffff;  //cmd run success
		}
		else
		{
			value = 0x0000;  //delete fail
			log_info("Face database delete fail!\n");
		}
	}
	system("sync");
	
	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/
	StrSetUInt16((uint8_t*)pop, value);
	MsgLen += sizeof(uint16_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_DELETE_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdGetAlbumSizeofDatabaseRsp()
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	int ret = 0;
	unsigned short faceSize = 0;
	
	ret = GetAlbumSizeFromDataBaseProc(&faceSize);
	if(ret != RS_RET_OK)
	{
		log_error("GetAlbumSizeFromDataBase() error! ret<%d>.\n", ret);
	}

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/
	StrSetUInt16((uint8_t*)pop, faceSize);
	MsgLen += sizeof(uint16_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_ALBUMSIZE_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdFaceSearchRsp(uint8_t value)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;
	
	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/
	StrSetUInt8((uint8_t*)pop, value);
	MsgLen += sizeof(uint8_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_SEARCH_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdSysInitOKReport(const char *strVersion)
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/
	memcpy(pop, strVersion, strlen(strVersion));
	MsgLen += strlen(strVersion);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_INITOK_Rpt,
		g_nSessionID++,
		MsgLen);
	
	/*计算FCS*/
	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdSysVersionRsp()
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/
	memcpy(pop, SYS_VERSION, strlen(SYS_VERSION));
	MsgLen += strlen(SYS_VERSION);
	printf("===nMsgLen<%d>,strVersion<%s>\n", MsgLen, SYS_VERSION);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_GETVER_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

//set current time
int SetCurrentTime(unsigned int *pCurTime)
{
	struct timeval tv;
	struct timezone tz;
	
	tv.tv_sec = *pCurTime;
	tv.tv_usec = 0;
	tz.tz_minuteswest	= 0;//-480;
	tz.tz_dsttime	= 0;

	settimeofday(&tv, &tz);

	time_t		curTime;
	struct tm *loc_tm = NULL;
	curTime = time(NULL);
	loc_tm = localtime(&curTime);
	log_info("set date and time dwCurTime<%d>  -%04d-%02d-%02d %02d:%02d:%02d\n",\
		*pCurTime, \
		loc_tm->tm_year+1900, loc_tm->tm_mon+1, loc_tm->tm_mday,\
		loc_tm->tm_hour, loc_tm->tm_min, loc_tm->tm_sec);
	return 0;
}

int cmdSetSysTimeSynRsp(uint8_t ret)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	/*填充消息体*/
	StrSetUInt8((uint8_t*)pop, ret);
	MsgLen += sizeof(uint8_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_SET_SYS_TIME_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdSetSysFactoryRsp()
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	/*填充消息体*/
	StrSetUInt8((uint8_t*)pop, SUCCESS);
	MsgLen += sizeof(uint8_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_SET_SYS_FACTORY_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdSetSysParamRsp(uint8_t ret)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	/*填充消息体*/
	StrSetUInt8((uint8_t*)pop, ret);
	MsgLen += sizeof(uint8_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_SET_SYS_CFG_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

/*外部MCU设置开机自动运行人脸注册程序开关处理*/
int setCfg_AutoStartRecgProc(bool flag)
{
	int result = FAILED;
	pid_t status;
	if(flag)
	{
		//copy 备份的自动运行人脸注册的rcS文件到/etc/init.d/
		status = system("cp -f /opt/smartlocker/auto_Recg_rcS  /etc/init.d/rcS");
	}
	else
	{
		//copy 备份的不自动运行人脸注册的rcS文件到/etc/init.d/
		status = system("cp -f /opt/smartlocker/no_Recg_rcS  /etc/init.d/rcS");		
	}
	if(0 == WEXITSTATUS(status))
	{
		result = SUCCESS;
	}
	else
	{
		result = FAILED;
	}
	
	return result;
}

int getValueFromMsg(unsigned char *pos, uint8_t pramID, pstSysCfg pstSystemCfg)
{
	int result = FAILED;
	switch(pramID)
	{
	case id_face_reg_threshold:
		pstSystemCfg->face_reg_threshold_val = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		break;
	case id_face_recg_threshold:
		pstSystemCfg->face_recg_threshold_val = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		break;
	case id_face_reg_angle:
		pstSystemCfg->face_reg_angle = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		break;
	case id_face_recg_angle:
		pstSystemCfg->face_reg_angle= StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		break;
	case id_auto_udisk_upgrade:
		pstSystemCfg->auto_udisk_upgrade = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		break;
	case id_face_reg_timeout:
		pstSystemCfg->face_reg_timeout = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		break;
	case id_face_recg_timeout:
		pstSystemCfg->face_recg_timeout = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		break;
	case id_flag_muxAngle_reg:
		pstSystemCfg->flag_muxAngle_reg = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		break;
	case id_flag_auto_recg:
		pstSystemCfg->flag_auto_recg = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		result = setCfg_AutoStartRecgProc(pstSystemCfg->flag_auto_recg);
		break;
	case id_flag_facePosition_Rpt:
		pstSystemCfg->flag_facePosition_Rpt = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		break;
	default:
		break;
	}
	return result;
}

int cmdSetSysCfgReqProc(unsigned short nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS, len=0, pramID =0, i=0;	
	unsigned char szBuffer[128]={0};
	unsigned char *pos = szBuffer;

	if((nMessageLen < sizeof(szBuffer)) || (nMessageLen >3))
	{
		memcpy(szBuffer, pszMessage, nMessageLen);
	}
	else
	{
		cmdSetSysParamRsp(FAILED);
		return -1;
	}

	//获取系统本地配置
	GetSysLocalCfg(&stSystemCfg);

	//解析设置参数
	for(i=0; i<ParamNum; i++)
	{
		pramID = StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		len += StrGetUInt8(pos);
		pos += sizeof(uint8_t);
		ret = getValueFromMsg(pos, pramID, &stSystemCfg);
		if(((i+1)*2+len) >= nMessageLen)
		{
			break;
		}
	}
		
	SaveSysCfgToLocalCfgFile(&stSystemCfg);
	
	cmdSetSysParamRsp(ret);
	return 0;
}

/*
按位查找是否配置该参数
*/
int SetMsgFromLocalValue(unsigned char *pos, uint16_t ParamType)
{
	int i=0, len=0;
	//获取系统本地配置
	GetSysLocalCfg(&stSystemCfg);

	for(i=0; i<sizeof(uint16_t)*8; i++)
	{
		if(GetBitStatu(ParamType, i))
		{
			switch(i+1)
			{
			case id_face_reg_threshold:
				StrSetUInt8((uint8_t*)pos, id_face_reg_threshold);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);				
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.face_reg_threshold_val);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			case id_face_recg_threshold:
				StrSetUInt8((uint8_t*)pos, id_face_recg_threshold);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);				
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.face_recg_threshold_val);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			case id_face_reg_angle:
				StrSetUInt8((uint8_t*)pos, id_face_reg_angle);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);				
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.face_reg_angle);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			case id_face_recg_angle:
				StrSetUInt8((uint8_t*)pos, id_face_recg_angle);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);				
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.face_reg_angle);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			case id_auto_udisk_upgrade:
				StrSetUInt8((uint8_t*)pos, id_auto_udisk_upgrade);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);				
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.auto_udisk_upgrade);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			case id_face_reg_timeout:
				StrSetUInt8((uint8_t*)pos, id_face_reg_timeout);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);				
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.face_reg_timeout);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			case id_face_recg_timeout:
				StrSetUInt8((uint8_t*)pos, id_face_recg_timeout);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);				
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.face_recg_timeout);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			case id_flag_muxAngle_reg:
				StrSetUInt8((uint8_t*)pos, id_flag_muxAngle_reg);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);				
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.flag_muxAngle_reg);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			case id_flag_auto_recg:
				StrSetUInt8((uint8_t*)pos, id_flag_auto_recg);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t); 			
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.flag_auto_recg);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			case id_flag_facePosition_Rpt:
				StrSetUInt8((uint8_t*)pos, id_flag_facePosition_Rpt);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				StrSetUInt8((uint8_t*)pos, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t); 			
				StrSetUInt8((uint8_t*)pos, (uint8_t)stSystemCfg.flag_facePosition_Rpt);
				pos += sizeof(uint8_t);
				len += sizeof(uint8_t);
				break;
			default:
				break;
			}

		}
	}
	
	return len;
}

int cmdGetSysCfgReqProc(unsigned short nMessageLen, const unsigned char *pszMessage)
{
	uint8_t ret = SUCCESS;
	char szBuffer[128]={0};
	int iBufferSize = 0, i=0;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	
	if(nMessageLen != sizeof(unsigned short))
	{
		ret = FAILED;		
		
		/*填充消息体*/
		StrSetUInt8((uint8_t*)pop, ret);
		MsgLen += sizeof(uint8_t);
	}
	else
	{
		unsigned short ParamType = 0;
		ParamType = StrGetUInt16(pszMessage);

		//根据需要获取的参数组建消息体
		MsgLen = SetMsgFromLocalValue((uint8_t*)pop, ParamType);
	}

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_GET_SYS_CFG_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;

	SendMsgToRpMSG((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
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
	snprintf(buf, sizeof(buf), "rm -rf /opt/pic");
	system(buf);
	system("echo \"Face Deletion done, A7 enter VLLS!\""); 
	system("echo mem > /sys/power/state");
}

int ProcMessage(
	unsigned short nCommand,
	unsigned short nSessionID,
	unsigned short nMessageLen,
	const unsigned char *pszMessage)
{ 
	int faceId, len,ret; 
	pid_t status;

	g_OutSessionID = nSessionID;
	
	log_info("======Command[0x%X], SessionID<0x%X>\n", nCommand, nSessionID);	
	switch (nCommand)
	{
		case ID_FACE_RECOGNIZE_Req:
		{
			log_info("Switch to face recognize...\n");
			system(MAIN_WORK_PATH(face_recg.sh));
			break;
		}
		case ID_FACE_REGISTER_Req:
		{
			log_info("Switch to face register...\n");	
			system(MAIN_WORK_PATH(face_register.sh));
			break;
		}
		case ID_FACE_DELETE_Req:	
		{
			unsigned short persionID;
			persionID = StrGetUInt16(pszMessage);	
			printf("delete persionID<0x%x>\n", persionID);
			cmdFaceDeleteRsp(persionID);			
			break;
		}
		case ID_FACE_ALBUMSIZE_Req:
		{
			cmdGetAlbumSizeofDatabaseRsp();			
			break;
		}
		case ID_FACE_SEARCH_Req:
		{
			unsigned short persionID=0, result=2;
			persionID = StrGetUInt16(pszMessage);	
			log_info("delete persionID<0x%x>\n", persionID);

			char filename[64] = {0};
			char pathbuf[128] = {0};
			
			memset(filename, 0, sizeof(filename));
			snprintf(filename, sizeof(filename), "%d_pic_reg_Ir.raw", persionID);
			memset(pathbuf, 0, sizeof(pathbuf));
			snprintf(pathbuf, sizeof(pathbuf), "%s%s", PIC_PATH, filename); 
			log_debug("=====get a ir_pic<%s>\n", pathbuf);
			if((access(pathbuf, F_OK))!=-1) 
			{
				result = 1;
			}
			else
			{
				result = 0;
			}
			/*00表示未搜寻到，01代表已搜寻到，02表示不支持人脸查询*/
			cmdFaceSearchRsp(result);

		/*如果支持LCD,则在屏上显示出IR图*/
		#ifdef LCD_DISPLAY_ENABLE		
			uint8_t Ir_buf[FRAME_SIZE_INBYTE];
			face_reg_Get_IR_ImgDataFromLocalFile(Ir_buf,CAM_WIDTH, CAM_HEIGHT, persionID);	
			DrawImgOnScreen_LocalFile(Ir_buf, CAM_WIDTH, CAM_HEIGHT);
		#endif
		
			break;
		}
		case ID_GET_SYS_CFG_Req:
		{
			cmdGetSysCfgReqProc(nMessageLen, pszMessage);	
			break;
		}
		case ID_SET_SYS_CFG_Req:
		{
			cmdSetSysCfgReqProc(nMessageLen, pszMessage);	
			break;
		}
		case ID_SET_SYS_FACTORY_Req:
		{			
			cmdSetSysFactoryRsp();
			SetSysToFactory();
			break;
		}
		case ID_SET_SYS_TIME_Req:
		{
			unsigned int nTime;
			uint8_t ret = SUCCESS;
			if(nMessageLen == sizeof(unsigned int))
			{
				memcpy(&nTime, pszMessage, nMessageLen);
			}
			else
			{
				ret = FAILED;
			}
				
			/*系统时间设置*/
			SetCurrentTime(&nTime);
			
			cmdSetSysTimeSynRsp(ret);
			break;
		}
		case ID_FACE_GETVER_Req:	
		{					
			cmdSysVersionRsp();			
			break;
		}
		/*系统APP & 算法库升级*/
		case ID_FACE_APP_UPDATE_Req:		
		{
			/*通知外部MCU升级状态，包括是否支持升级，回复状态0表示不支持，其他表示支持和升级进度，0xff表示升级完成*/
#ifndef APP_UPGRADE_USB	
			cmdSysUpdateRsp(0x00);
			break;
#endif	
			cmdSysUpdateRsp(0x01);			
			SysUpdateTaskProc();
			break;
		}
		case ID_SYS_OTA_UPDATE_Req:
		{
			break;
		}
		/*M4->A7 A7进入VLLS模式*/
		case ID_FACE_A7VLLS_Req:	
		{					
			cmdSetSysToVLLSModeRsp();
			
			system("echo \"Face Deletion done, A7 enter VLLS!\""); 
			system("echo mem > /sys/power/state");
			break;
		}
		case ID_FACE_A7SHUTDOWN_Req:		
		{					
			/*A7进入SHUTDOWN模式前必须要A7先进入VLLS模式*/
			/*通知M4和外部MCU*/
			cmdSetSysToShutDownModeRsp();
			
			system("echo \"Face Deletion done, A7 enter VLLS!\""); 
			system("echo mem > /sys/power/state");
			break;
		}		
		
		default:
			log_info("No effective cmd from M4!\n");
			break;				
	}

	return 0;
}

//======================需要通过Face_loop来转发的消息======================
int SendMsgToFaceLoop(unsigned char *MsgBuf, unsigned char MsgLen)
{
	int ret; 

	if(stSysLogCfg.log_level < LOG_DEBUG)	
	{
		int i = 0;
		printf("\n<<<send msg<len:%d>:", MsgLen);	
		for(i; i<MsgLen; i++)	
		{		
			printf("0x%02x   ", MsgBuf[i]);	
		}	
		printf("\n");
	}

	MessageSend(MsgBuf, MsgLen);

	return ret;
}

/*系统硬件故障上报(Include camera,lcd,sdk license ic,emmc,rpmsg,and so on)*/
int cmdSysHardwareAbnormalReport(HwAbnormalType nType)
{
	char szBuffer[128]={0};
	int iBufferSize;
	unsigned char	cal_FCS;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/
	StrSetUInt16((uint8_t*)pop, nType);
	MsgLen += sizeof(uint16_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_SYS_HW_DEBUG_Rpt,
		g_nSessionID++,
		MsgLen);
	
	/*计算FCS*/
	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToFaceLoop((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdFacePositionRpt(rs_rect *pRect)
{
	char szBuffer[64]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);
	log_debug("%d*%d,left<0x%02x>,top<0x%02x>,width<0x%02x>,height<0x%02x>\n", 
		MX6000_CAM_WIDTH, MX6000_CAM_HEIGHT,pRect->left, pRect->top,pRect->width,pRect->height);

	/*填充消息体*/
	StrSetUInt16((uint8_t*)pop, MX6000_CAM_WIDTH);
	MsgLen += sizeof(uint16_t);	
	pop += sizeof(uint16_t);
	StrSetUInt16((uint8_t*)pop, MX6000_CAM_HEIGHT);
	MsgLen += sizeof(uint16_t);
	pop += sizeof(uint16_t);
	StrSetUInt16((uint8_t*)pop, pRect->left);
	MsgLen += sizeof(uint16_t);
	pop += sizeof(uint16_t);
	StrSetUInt16((uint8_t*)pop, pRect->top);
	MsgLen += sizeof(uint16_t);
	pop += sizeof(uint16_t);
	StrSetUInt16((uint8_t*)pop, pRect->width);
	MsgLen += sizeof(uint16_t);
	pop += sizeof(uint16_t);
	StrSetUInt16((uint8_t*)pop, pRect->height);
	MsgLen += sizeof(uint16_t);
	pop += sizeof(uint16_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_POSITION_Rpt,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToFaceLoop((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdFaceRegistRsp(uint16_t persionID)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0;
	//log_debug("=======send Msg<0x%x>============\n",persionID);

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/
	StrSetUInt16((uint8_t*)pop, persionID);
	MsgLen += sizeof(uint16_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_REGISTER_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToFaceLoop((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

int cmdFaceRecogizeRsp(uint16_t persionID, bool IsMouseOpen, int faceCount)
{
	char szBuffer[128]={0};
	int iBufferSize;
	char *pop = NULL;
	unsigned char MsgLen = 0, Attribute = Face_recg_attrib_Normal;

	log_debug("IsMouseOpen<%d>, faceCount<%d>\n", IsMouseOpen, faceCount);
	
	//如果以后需要携带多个状态，可以考虑用位代表一种类型
	if(faceCount>1)
	{
		Attribute = Face_recg_attrib_MutFace;
	}
	else if(IsMouseOpen)
	{
		Attribute = Face_recg_attrib_Alarm;
	}

	memset(szBuffer, 0, sizeof(szBuffer));	
	pop = szBuffer+sizeof(MESSAGE_HEAD);

	/*填充消息体*/
	StrSetUInt16((uint8_t*)pop, persionID);
	MsgLen += sizeof(uint16_t);
	pop += sizeof(uint16_t);
	StrSetUInt8((uint8_t *)pop, Attribute);
	MsgLen += sizeof(uint8_t);
	pop += sizeof(uint8_t);

	/*填充消息头*/
	iBufferSize = MsgHead_Packet(
		szBuffer,
		SNDMSG_HEAD_MARK,
		ID_FACE_RECOGNIZE_Rsp,
		g_OutSessionID,
		MsgLen);
	
	/*计算FCS*/
	unsigned char	cal_FCS = Msg_UartCalcFCS((uint8_t*)szBuffer+HEADMARK_LEN, MsgLen+sizeof(MESSAGE_HEAD)-HEADMARK_LEN);
	szBuffer[iBufferSize] = cal_FCS;
	
	SendMsgToFaceLoop((uint8_t*)szBuffer, iBufferSize+FCS_LEN);

	return 0;
}

/****************************图片处理**************************
1.把注册的图片保存到本地磁盘空间或从本地磁盘中删除
2.维护一张图片信息列表*/
void face_reg_SaveImgToLocalFileByPersonID(imageInfo color_img, imageInfo depth_img, unsigned short PersonId)
{		
	char filename[64] = {0};
	char pathbuf[128] = {0};

	if(!CheckDirectory(PIC_PATH))
	{
		log_error("Err:%s is not exit,and creat failed!\n", PIC_PATH);
		return;
	}
		
	memset(filename, 0, sizeof(filename));
	snprintf(filename, sizeof(filename), "%d_pic_reg_Ir.raw", PersonId);
	memset(pathbuf, 0, sizeof(pathbuf));
	snprintf(pathbuf, sizeof(pathbuf), "%s%s", PIC_PATH, filename);	
	log_debug("=====creat a ir_pic<%s>\n", pathbuf);
	myWriteData2File(pathbuf, (unsigned char *)color_img.data, color_img.width * color_img.height);
	memset(filename, 0, sizeof(filename));
	snprintf(filename, sizeof(filename), "%d_pic_reg_Depth.raw", PersonId);
	memset(pathbuf, 0, sizeof(pathbuf));
	snprintf(pathbuf, sizeof(pathbuf), "%s%s", PIC_PATH, filename);	
	log_debug("=====creat a depth_pic<%s>\n", pathbuf);
	myWriteData2File(pathbuf, (unsigned char *)depth_img.data, 2 * depth_img.width * depth_img.height);

	return;
}

void face_reg_DelImgFromLocalFileByPersonID(unsigned short PersonId)
{		
	char filename[64] = {0};
	char pathbuf[128] = {0};
	
	memset(filename, 0, sizeof(filename));
	snprintf(filename, sizeof(filename), "%d_pic_reg_Depth.raw", PersonId);
	memset(pathbuf, 0, sizeof(pathbuf));
	snprintf(pathbuf, sizeof(pathbuf), "rm -f %s%s", PIC_PATH, filename);	
	log_debug("=====delete a depth_pic<%s>\n", pathbuf);	
	system(pathbuf);
	
	memset(filename, 0, sizeof(filename));
	snprintf(filename, sizeof(filename), "%d_pic_reg_Ir.raw", PersonId);
	memset(pathbuf, 0, sizeof(pathbuf));
	snprintf(pathbuf, sizeof(pathbuf), "rm -f %s%s", PIC_PATH, filename);	
	log_debug("=====delete a Ir_pic<%s>\n", pathbuf);
	system(pathbuf);
	
	return;
}

uint8_t face_reg_Get_IR_ImgDataFromLocalFile(uint8_t *pIr_img,  uint16_t width, uint16_t height,unsigned short PersonId)
{		
	uint8_t result = 0;
	char filename[64] = {0};
	char pathbuf[128] = {0};
	
	memset(filename, 0, sizeof(filename));
	snprintf(filename, sizeof(filename), "%d_pic_reg_Ir.raw", PersonId);
	memset(pathbuf, 0, sizeof(pathbuf));
	snprintf(pathbuf, sizeof(pathbuf), "%s%s", PIC_PATH, filename);	
	log_debug("=====get a ir_pic<%s>\n", pathbuf);
	result = myReadDataFromFile(pathbuf, (unsigned char *)pIr_img, width * height);
	if(result < 0)
	{
		return FAILED;
	}

	return SUCCESS;
}

uint8_t face_reg_Get_Depth_ImgDataFromLocalFile(uint8_t *pDepth_img,  uint16_t width, uint16_t height,unsigned short PersonId)
{		
	uint8_t result = 0;
	char filename[64] = {0};
	char pathbuf[128] = {0};

	memset(filename, 0, sizeof(filename));
	snprintf(filename, sizeof(filename), "%d_pic_reg_Depth.raw", PersonId);
	memset(pathbuf, 0, sizeof(pathbuf));
	snprintf(pathbuf, sizeof(pathbuf), "%s%s", PIC_PATH, filename);	
	log_debug("=====get a depth_pic<%s>\n", pathbuf);	
	result = myReadDataFromFile(pathbuf, (unsigned char *)pDepth_img, 2*width * height);
	if(result < 0)
	{
		return FAILED;
	}
	
	return SUCCESS;
}

#ifdef LCD_DISPLAY_ENABLE
static bool fbSinkInit(int *pfd, unsigned char **fbbase,
				struct fb_var_screeninfo *pvar)
{
	int fd_fb, fbsize, ret, i=0;
	const char *fb_dev = "/dev/fb0";
	struct fb_var_screeninfo var;

	do{
		if ((fd_fb = open(fb_dev, O_RDWR )) < 0) 
		{
			if(i>=5)
			{
				cerr << "Unable to open frame buffer:i=" << i << endl;
				return false;
			}
			usleep(20);
		}
		else
		{
			break;
		}
	}while(i++);

	if (ioctl(fd_fb, FBIOGET_VSCREENINFO, &var) < 0) {
		cerr << "FBIOPUT_VSCREENINFO failed" << endl;
		goto err;
	}

	// ping-pong buffer
	var.xres_virtual = var.xres;
	var.yres_virtual = 2 * var.yres;

	ret = ioctl(fd_fb, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0) {
		cerr << "FBIOPUT_VSCREENINFO failed:" << ret << endl;
		goto err;
	}

	// Map the device to memory
	fbsize = var.xres * var.yres_virtual * var.bits_per_pixel / 8;
	cout << "framebuffer info: (" << var.xres << "x" <<
		var.yres << "), bpp:" << var.bits_per_pixel << endl;

	*fbbase = (unsigned char *)mmap(0, fbsize,
					PROT_READ | PROT_WRITE,
					MAP_SHARED, fd_fb, 0);
	if (MAP_FAILED == *fbbase) {
		cerr << "Failed to map framebuffer device to memory" << endl;
		goto err;
	}

	*pvar = var;
	return true;

err:
	close(fd_fb);
	return false;
}

static void fbSinkUninit(int fd, unsigned char *fbbase, int fbsize)
{
	if (fbbase)
		munmap(fbbase, fbsize);
	close(fd);
}

static void GrayToRGBA(unsigned short *pInput, unsigned int nInputSize, unsigned char *pOutput, bool flg_draw_facebox)
{
#ifdef MIPI_LCD_ST7701S
	unsigned short *pInputTmp = pInput;
	if(flg_draw_facebox)
	{
		int32_t x = 0, y = 0;
		unsigned int color = 0xFF00FF00;//green
		
		rs_rect face_box;
		face_alg_get_last_detected_box(&face_box);
		int x1 = face_box.left;
		int y1 = face_box.top;
		int x2 = x1 + face_box.width;
		int y2 = y1 + face_box.height;
		log_debug("%s: box: (x1,y1)=(%d,%d), (x2,y2)=(%d,%d), nInputSize<%d>\n",
			__func__, x1, y1, x2, y2, nInputSize);

		//	 Face box coordinates in rotated and downscaled portrait 400x640 NIR image:
		//
		//		(0,0)
		//		  +-------------------------+--> X
		//		  | 						|
		//		  | (x1,y1) 				|
		//		  |                                    |
		//		  | 	*------------*		|
		//		  | 	|			 |		|
		//		  | 	|	o	 o	 |		|
		//		  | 	|			 |		|
		//		  | 	|	   L	 |		|
		//		  | 	|	  ___	 |		|
		//		  | 	|			 |		|
		//		  | 	*------------*		|
		//		  | 	 \			/            |
		//		  | 	 /			\  (x2,y2)
		//		  | 	/			 \		|
		//		  |    +--------------+ 	|
		//		  | 						|
		//		  +-------------------------+
		//		  | 					(399,639)
		//		  v
		//		  X
		
		for (y = 0; y< CAM_HEIGHT; ++y) 
		{
			for (x = 0; x < CAM_WIDTH; ++x) 
			{
				if( ((y== y1) && (x>x1 && x<x2))   //上边框
				||((y== y2) && (x>x1 && x<x2))	//下边框
				||((x== x1) && (y>y1 && y<y2))	//左边框
				||((x== x2) && (y>y1 && y<y2))	//右边框
				)
				{
					memcpy(pOutput, &color, 4);
				}
				else
				{
					*pOutput = (unsigned char)((*pInputTmp >> 2) & 0xFF);
					*(pOutput + 1) = *pOutput;
					*(pOutput + 2) = *pOutput;
					*(pOutput + 3) = 0xFF;
				}
				
				pOutput += 4;
				pInputTmp++;
			}
		}
	}
	else
	{
		for (unsigned int i = 0; i < nInputSize; i++)
		{
			*pOutput = (unsigned char)((*pInputTmp) & 0xFF);
			*(pOutput + 1) = *pOutput;
			*(pOutput + 2) = *pOutput;
			*(pOutput + 3) = 0xFF;
			pOutput += 4;
			pInputTmp++;
		}	
	}
#endif
}


static void GrayToRGB565Scale(unsigned short *pInput, unsigned int nInputSize, unsigned char *pOutput)
{
#ifdef SPI_LCD_MIAOYING
	unsigned short *pInputTmp = pInput;
	unsigned char *pInputGray;
	for (unsigned int i = 0; i < nInputSize; i++)
	{
		*pInputGray = (unsigned char)((*pInputTmp) & 0xFF);
		*pOutput = ((*pInputGray << 3) & 0xE0) | ((*pInputGray >> 3) & 0x1F);
		*(pOutput + 1) = (*pInputGray & 0xF8) | ((*pInputGray >> 5) & 0x07);
		pOutput += 2;
		(SPI_LCD_WIDTH - 1 == i % SPI_LCD_WIDTH) ? (pInputTmp += (CAM_WIDTH + 2)) : (pInputTmp += 2);
	}
#endif
}

static void blitToFB(unsigned char *fbbase,
			int fbwidth, int fbheight,
			unsigned char *v4lbase,
			int camwidth, int camheight)
{
	int offsetx, x, y;
#if defined(MIPI_LCD_ST7701S)
	unsigned int *fbpixel;
	unsigned int *campixel;

	offsetx = (camwidth - fbwidth) / 2;
	fbpixel = (unsigned int*)fbbase; // bpp=32
	campixel = (unsigned int*)(v4lbase + offsetx * CAM_IMAGE_BPP);

	for (y = 0; y < camheight; y++) {
		memcpy((void*)fbpixel, (void*)campixel, fbwidth * 4);
		campixel += camwidth;
		fbpixel += fbwidth;
	}
#elif defined(SPI_LCD_MIAOYING)
	unsigned short *fbpixel;
	unsigned short *campixel;

	offsetx = (camwidth - fbwidth) / 2;
	fbpixel = (unsigned short*)fbbase; // bpp=16
	campixel = (unsigned short*)(v4lbase + offsetx * CAM_IMAGE_BPP);

	for (y = 0; y < fbheight; y++) 
	{
		memcpy((void*)fbpixel, (void*)campixel, fbwidth * 2);
		campixel += camwidth;
		fbpixel += fbwidth;
	}
#endif
}

int DrawRawImgOnScreen(uint16_t *pIr_img, uint16_t width, uint16_t height, bool flg_draw_facebox)
{
	uint8_t result = FAILED;
	int fbfd=0, xres=0, yres=0, ret=0;
	unsigned char *fbBase, *blitSrc;
	struct fb_var_screeninfo var;
	int fbIndex = 0, fbBufSize;
#if defined(MIPI_LCD_ST7701S)
	unsigned char rgbaBuffer[FRAME_SIZE_INBYTE * GRAY_TO_RGBA_BPP];
#elif defined(SPI_LCD_MIAOYING)
	unsigned char rgb565scaleBuffer[SPI_LCD_SIZE_INBYTE];
#endif
	
	// open and setup the framebuffer for preview 
	if(!fbSinkInit(&fbfd, &fbBase, &var)) 
	{	
		cmdSysHardwareAbnormalReport(HW_BUG_LCD);
		return FAILED;
	}
	cout << "fbSinkInit success" << endl;
	fbBufSize = var.xres * var.yres * var.bits_per_pixel / 8;
	log_debug("=====fbBufSize<%d>, x:%d, y:%d, bpp:%d.\n",fbBufSize, var.xres, var.yres,var.bits_per_pixel);

	// directly blit to screen fb
#if defined(MIPI_LCD_ST7701S)
	// IR image to RGBA
	GrayToRGBA((uint16_t *)pIr_img, height*width, rgbaBuffer, flg_draw_facebox);
	blitSrc = rgbaBuffer;
	//cout << "====gray to rgba,height*width:" << Ir_img.height <<"*" << Ir_img.width << endl;

	blitToFB(fbBase + fbIndex * fbBufSize,
			var.xres, var.yres,
			blitSrc,
			CAM_WIDTH, CAM_HEIGHT);
	var.yoffset = var.yres * fbIndex;
	ioctl(fbfd, FBIOPAN_DISPLAY, &var);
	fbIndex = !fbIndex;
#elif defined(SPI_LCD_MIAOYING)
	GrayToRGB565Scale((uint8_t *)pIr_img, var.xres * var.yres, rgb565scaleBuffer);
	blitSrc = rgb565scaleBuffer;
	//cout << "====gray to rgba,height*width:" << Ir_img.height <<"*" << Ir_img.width << endl;

	blitToFB(fbBase + fbIndex * fbBufSize,
			var.xres, var.yres,
			blitSrc,
			SPI_LCD_WIDTH, SPI_LCD_HEIGHT);
		//cout << "blit to fb end" << endl;
#endif

	fbSinkUninit(fbfd, fbBase, fbBufSize * 2);

	return SUCCESS;
}

static void GrayToRGBA_LocalFile(unsigned char *pInput, unsigned int nInputSize, unsigned char *pOutput)
{
#ifdef MIPI_LCD_ST7701S
	unsigned char *pInputTmp = pInput;
	for (unsigned int i = 0; i < nInputSize; i++)
	{
		*pOutput = (unsigned char)((*pInputTmp) & 0xFF);
		*(pOutput + 1) = *pOutput;
		*(pOutput + 2) = *pOutput;
		*(pOutput + 3) = 0xFF;
		pOutput += 4;
		pInputTmp++;
	}	
#endif
}

static void GrayToRGB565Scale_LocalFile(unsigned char *pInput, unsigned int nInputSize, unsigned char *pOutput)
{
#ifdef SPI_LCD_MIAOYING
	unsigned char *pInputTmp = pInput;
	unsigned char *pInputGray;
	for (unsigned int i = 0; i < nInputSize; i++)
	{
		*pInputGray = (unsigned char)((*pInputTmp) & 0xFF);
		*pOutput = ((*pInputGray << 3) & 0xE0) | ((*pInputGray >> 3) & 0x1F);
		*(pOutput + 1) = (*pInputGray & 0xF8) | ((*pInputGray >> 5) & 0x07);
		pOutput += 2;
		(SPI_LCD_WIDTH - 1 == i % SPI_LCD_WIDTH) ? (pInputTmp += (CAM_WIDTH + 2)) : (pInputTmp += 2);
	}
#endif
}

int DrawImgOnScreen_LocalFile(uint8_t *pIr_img, uint16_t width, uint16_t height)
{
	uint8_t result = FAILED;
	int fbfd=0, xres=0, yres=0, ret=0;
	unsigned char *fbBase, *blitSrc;
	struct fb_var_screeninfo var;
	int fbIndex = 0, fbBufSize;
#if defined(MIPI_LCD_ST7701S)
	unsigned char rgbaBuffer[FRAME_SIZE_INBYTE * GRAY_TO_RGBA_BPP];
#elif defined(SPI_LCD_MIAOYING)
	unsigned char rgb565scaleBuffer[SPI_LCD_SIZE_INBYTE];
#endif
	
	// open and setup the framebuffer for preview 
	if(!fbSinkInit(&fbfd, &fbBase, &var)) 
	{	
		log_error("open LCD failed!\n");
		cmdSysHardwareAbnormalReport(HW_BUG_LCD);
		return FAILED;
	}
	cout << "fbSinkInit success" << endl;
	fbBufSize = var.xres * var.yres * var.bits_per_pixel / 8;
	log_debug("=====fbBufSize<%d>, x:%d, y:%d, bpp:%d.\n",fbBufSize, var.xres, var.yres,var.bits_per_pixel);

	// directly blit to screen fb
#if defined(MIPI_LCD_ST7701S)
	// IR image to RGBA
	GrayToRGBA_LocalFile((uint8_t *)pIr_img, height*width, rgbaBuffer);
	blitSrc = rgbaBuffer;
	//cout << "====gray to rgba,height*width:" << Ir_img.height <<"*" << Ir_img.width << endl;

	blitToFB(fbBase + fbIndex * fbBufSize,
			var.xres, var.yres,
			blitSrc,
			CAM_WIDTH, CAM_HEIGHT);
	var.yoffset = var.yres * fbIndex;
	ioctl(fbfd, FBIOPAN_DISPLAY, &var);
	fbIndex = !fbIndex;
#elif defined(SPI_LCD_MIAOYING)
	GrayToRGB565Scale_LocalFile((uint8_t *)pIr_img, var.xres * var.yres, rgb565scaleBuffer);
	blitSrc = rgb565scaleBuffer;
	//cout << "====gray to rgba,height*width:" << Ir_img.height <<"*" << Ir_img.width << endl;

	blitToFB(fbBase + fbIndex * fbBufSize,
			var.xres, var.yres,
			blitSrc,
			SPI_LCD_WIDTH, SPI_LCD_HEIGHT);
		//cout << "blit to fb end" << endl;
#endif

	fbSinkUninit(fbfd, fbBase, fbBufSize * 2);

	return SUCCESS;
}

#endif


