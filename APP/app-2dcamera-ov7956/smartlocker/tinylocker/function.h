/**************************************************************************
 * 	FileName:	 face_loop.cpp
 *	Description:	define the common function interface and constant.
 *	Copyright(C):	2018-2020 Wave Group Inc.
 *	Version:		V 1.0
 *	Author:		tanqw
 *	Created:		2019-7-12
 *	Updated:		
 *					
**************************************************************************/
 
#ifndef __WF_FUNCTION_H__
#define __WF_FUNCTION_H__

#ifdef __cplusplus
extern "C" {
#endif


#define RPMSG_DEV		"/dev/ttyRPMSG30"
#define MAIN_WORK_PATH(file_name)		"/opt/smartlocker/"#file_name

#define uint8_t       unsigned char
#define uint16_t      unsigned short
#define uint32_t      unsigned int
#define uint64_t      unsigned long long
#define size_t        unsigned long
#define int32_t       int
#define BOOL	unsigned char

#define YES			1
#define NO			0

#define FAILED		-1
#define SUCCESS		0


typedef enum
{
	RPMSG_CMD,
	RPMSG_READ,
}para_rpmsg_ioctl_cmd;

/*M4<->A7 RpMSG CMD TYPE*/
typedef enum
{
	CMD_INVALID = 0U,
	CMD_FACE_RECOGNITION = 1U,
	CMD_FACE_REGISTRATION = 2U, 
	CMD_FACE_DELETION = 3U,    	
	CMD_SYS_A7VLLS = 0xfb,
	CMD_SYS_A7SHUTDOWN = 0xfc,
	CMD_SYS_APP_UPDATE = 0xfd,
	RPMSG_A7_INIT_OK = 0xfe,
}face_cmd_type_t;

typedef struct 
{
	unsigned short face_reg_threshold_val;			/*注册阈值:人脸相似度，值越大，越严格*/
	unsigned short face_recg_threshold_val;			/*识别阈值:人脸相似度，值越大，越严格*/
	float 		face_detect_reg_angle;		/*注册角度*/
	float 		face_detect_recg_angle;		/*识别角度*/
	BOOL		auto_udisk_upgrade;			/*自动检测U盘升级， 0:no, 1:yes*/
}stSysCfg, *pstSysCfg;

/*-----------------------------------SYSTEM UPGRADE------------------------------------------*/
#define IH_MAGIC	0x27051956	/* Image Magic Number		*/
#define IH_NMLEN		32	/* Image Name Length		*/
/*
 * Legacy format image header,
 * all data in network byte order (aka natural aka bigendian).
 */
typedef struct image_header {
	uint32_t	ih_magic;	/* Image Header Magic Number	*/
	uint32_t	ih_hcrc;	/* Image Header CRC Checksum	*/
	uint32_t	ih_time;	/* Image Creation Timestamp	*/
	uint32_t	ih_size;	/* Image Data Size		*/
	uint32_t	ih_load;	/* Data	 Load  Address		*/
	uint32_t	ih_ep;		/* Entry Point Address		*/
	uint32_t	ih_dcrc;	/* Image Data CRC Checksum	*/
	uint8_t		ih_os;		/* Operating System		*/
	uint8_t		ih_arch;	/* CPU architecture		*/
	uint8_t		ih_type;	/* Image Type			*/
	uint8_t		ih_comp;	/* Compression Type		*/
	uint8_t		ih_name[IH_NMLEN];	/* Image Name		*/
} image_header_t;

void myWriteData2File(const char * pFileName, unsigned char *pData, int iDataLen);

int SendMsgToRpMSG(unsigned char *MsgBuf, unsigned char MsgLen);

int GetSysLocalCfg(pstSysCfg pstSystemCfg);

int CheckImageFileCRC32(const char * SrcFilePath, const char * DstFilePath,  uint32_t *crc32_val);

int SetOv7956ModuleLed(BOOL value);

#ifdef __cplusplus
}
#endif

#endif // __WF_FUNCTION_H__

