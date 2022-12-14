/**************************************************************************
 * 	FileName:	 function.cpp
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
#include "RSApiHandle.h"
#include "face_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libsimplelog.h"


#define RPMSG_DEV		"/dev/ttyRPMSG30"
#define PIC_PATH 		"/opt/pic/"

#define FAILED		0
#define SUCCESS		1

typedef enum
{
	RPMSG_CMD,
	RPMSG_READ,
}para_rpmsg_ioctl_cmd;

//配置参数结构体
typedef struct 
{
	unsigned short face_reg_threshold_val;			/*注册阈值:人脸相似度，值越大，越严格*/
	unsigned short face_recg_threshold_val;			/*识别阈值:人脸相似度，值越大，越严格*/
	float 		face_reg_angle;					/*注册角度*/
	float 		face_recg_angle;					/*识别角度*/
	BOOL		auto_udisk_upgrade;				/*自动检测U盘升级， 0:no, 1:yes*/
	unsigned char face_reg_timeout;				/*人脸注册执行超时时间*/	
	unsigned char face_recg_timeout;				/*人脸识别执行超时时间*/	
	BOOL		flag_muxAngle_reg;				/*多角度注册标记， 0:no, 1:yes*/
	BOOL		flag_auto_recg;					/*开机自动开启人脸识别标记， 0:no, 1:yes*/
	BOOL		flag_facePosition_Rpt;			/*人脸坐标位置上报标记， 0:no, 1:yes*/
	BOOL		flag_faceBox;					/*是否人脸框标记， 0:no, 1:yes*/
}stSysCfg, *pstSysCfg;

#define FACE_REG_TIMEOUT 			30    //单张人脸注册超时时间
#define FACE_RECG_TIMEOUT 			5	// 人脸识别超时时间

//配置参数ID
#define ParamNum			10   //参数个数，必须与下面的参数类型个数一致
enum DecompTypes
{
	id_face_reg_threshold =1,			/*注册阈值:人脸相似度，值越大，越严格*/
	id_face_recg_threshold,			/*识别阈值:人脸相似度，值越大，越严格*/
	id_face_reg_angle,				/*注册角度*/
	id_face_recg_angle,				/*识别角度*/
	id_auto_udisk_upgrade,			/*自动检测U盘升级， 0:no, 1:yes*/
	id_face_reg_timeout,				/*人脸注册执行超时时间*/	
	id_face_recg_timeout,				/*人脸识别执行超时时间*/
	id_flag_muxAngle_reg,			/*多角度注册开关， 0:no, 1:yes*/
	id_flag_auto_recg,				/*开机自动开启人脸识别开关， 0:no, 1:yes*/
	id_flag_facePosition_Rpt,			/*人脸坐标位置上报开关， 0:no, 1:yes*/
};

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

typedef struct _stRpMsgHead
{
	unsigned short HeadMark;		/* 前导标识符（长度为2bytes）用于校验消息是否合法 */
	unsigned short CmdID;			/* 消息命令（长度为2bytes） */
	unsigned char SessionID;		/* 会话ID（长度为1bytes） */
	unsigned char MsgLen;			/* 消息长度（长度为1bytes，不包括包头、CRC校验） */    
}MESSAGE_HEAD, *PMESSAGE_HEAD;

#define HEADMARK_LEN		2  
#if USE_M4_FLAG 
#define SNDMSG_HEAD_MARK				(0x4137)  /*A7*/
#define RCVMSG_HEAD_MARK				(0x4D34)  /*M4*/
#else
#define SNDMSG_HEAD_MARK				(0x4D34)  /*M4*/
#define RCVMSG_HEAD_MARK				(0x4D43)  /*MC*/
#endif
#define FCS_LEN				1  
#define MAX_PERSON_ID		5000

// M4与A7之间的消息
#define ID_FACE_RECOGNIZE_Req          		0x0001
#define ID_FACE_RECOGNIZE_Rsp          		0x8001
#define ID_FACE_REGISTER_Req          			0x0002
#define ID_FACE_REGISTER_Rsp          			0x8002
#define ID_FACE_DELETE_Req          			0x0003
#define ID_FACE_DELETE_Rsp          			0x8003
#define ID_FACE_ALBUMSIZE_Req				0x0004
#define ID_FACE_ALBUMSIZE_Rsp				0x8004
#define ID_FACE_SEARCH_Req					0x0005
#define ID_FACE_SEARCH_Rsp					0x8005
#define ID_FACE_POSITION_Rpt				0x0006

#define ID_GET_SYS_CFG_Req					0x00f2
#define ID_GET_SYS_CFG_Rsp					0x80f2
#define ID_FACE_APP_UPDATE_Req          		0x00f3
#define ID_FACE_APP_UPDATE_Rsp          		0x80f3
#define ID_SYS_OTA_UPDATE_Req          		0x00f4
#define ID_SYS_OTA_UPDATE_Rsp          		0x80f4
#define ID_SET_SYS_CFG_Req					0x00f5
#define ID_SET_SYS_CFG_Rsp					0x80f5
#define ID_GET_SYS_LOG_Req					0x00f6
#define ID_GET_SYS_LOG_Rsp					0x80f6
#define ID_SET_SYS_FACTORY_Req			0x00f7
#define ID_SET_SYS_FACTORY_Rsp			0x80f7
#define ID_SET_SYS_TIME_Req				0x00f8
#define ID_SET_SYS_TIME_Rsp				0x80f8
#define ID_SYS_HW_DEBUG_Rpt				0x80f9
#define ID_FACE_M4VLLS_Req          			0x00fa
#define ID_FACE_M4VLLS_Rsp          			0x80fa
#define ID_FACE_A7VLLS_Req          			0x00fb
#define ID_FACE_A7VLLS_Rsp          			0x80fb
#define ID_FACE_A7SHUTDOWN_Req          		0x00fc
#define ID_FACE_A7SHUTDOWN_Rsp          		0x80fc
#define ID_FACE_GETVER_Req          			0x00fd
#define ID_FACE_GETVER_Rsp          			0x80fd
#define ID_FACE_INITOK_Rpt          			0x00fe
#define ID_FACE_ACK_Rpt          				0x00ff

/*注册消息时响应类型*/
#define ID_REG_DISTANSE_NEAR				0xff04
#define ID_REG_DISTANSE_FAR				0xff05
#define ID_REG_ADDFACE_NULL				0xff10
#define ID_REG_PITCH_UP					0xff11
#define ID_REG_PITCH_UP_TOO_BIG			0xff12   //0xff12抬头角度过大
#define ID_REG_PITCH_UP_TOO_SMALL		0xff13	//0xff13抬头角度过小
#define ID_REG_PITCH_DOWN					0xff21	
#define ID_REG_PITCH_DOWN_TOO_BIG		0xff22	//0xff22低头角度过大
#define ID_REG_PITCH_DOWN_TOO_SMALL		0xff23	//0xff23低头角度过小
#define ID_REG_YAW_LEFT					0xff31
#define ID_REG_YAW_LEFT_TOO_BIG			0xff32	//0xff32左偏脸角度过大
#define ID_REG_YAW_LEFT_TOO_SMALL		0xff33	//0xff33左偏脸角度过小
#define ID_REG_YAW_RIGHT					0xff41
#define ID_REG_YAW_RIGHT_TOO_BIG			0xff42	//0xff42右偏脸角度过大
#define ID_REG_YAW_RIGHT_TOO_SMALL		0xff43	//0xff43右偏脸角度过小
#define ID_REG_ROLL_LEFT					0xff51
#define ID_REG_ROLL_LEFT_TOO_BIG			0xff52	//0xff52头靠左肩角度过大
#define ID_REG_ROLL_LEFT_TOO_SMALL		0xff53	//0xff53头靠左肩角度过小
#define ID_REG_ROLL_RIGHT					0xff61
#define ID_REG_ROLL_RIGHT_TOO_BIG		0xff62	//0xff62头靠右肩角度过大
#define ID_REG_ROLL_RIGHT_TOO_SMALL		0xff63	//0xff63头靠右肩角度过小


/*硬件故障类型定义*/
typedef enum
{
	HW_BUG_NOERROR=0,
	HW_BUG_CAMERA=1,
	HW_BUG_LCD,
	HW_BUG_ENCRYPTIC,
	HW_BUG_RPMSG,
	HW_BUG_EMMC,	
	HW_BUG_UART5,
}HwAbnormalType;

/*人脸识别结果属性定义*/
typedef enum
{
	Face_recg_attrib_Normal=0, 		//单人无告警的正常状况
	Face_recg_attrib_MutFace,	//多人无告警
	Face_recg_attrib_Alarm,		//告警触发
}RecgRltAttribType;

extern stLogCfg stSysLogCfg;
extern int tty_fd;
extern stSysCfg stSystemCfg;


int myWriteData2File(const char * pFileName, unsigned char *pData, int iDataLen);
int myReadDataFromFile(const char * pFileName, unsigned char *pData, int iDataLen);

int SendMsgToRpMSG(uint8_t *MsgBuf, uint8_t MsgLen);

int GetSysLocalCfg(pstSysCfg pstSystemCfg);
int CheckImageFileCRC32(const char * SrcFilePath, const char * DstFilePath,  uint32_t *crc32_val);

void StrSetUInt8( uint8_t * io_pDst, const uint8_t i_u8Src );
void StrSetUInt16( uint8_t * io_pDst, const uint16_t i_u16Src );
void StrSetUInt32( uint8_t * io_pDst, const uint32_t i_u32Src );
uint8_t Msg_UartCalcFCS( const uint8_t *msg_ptr, uint8_t len );
int MsgHead_Packet(uint8_t *pszBuffer,uint16_t HeadMark,uint16_t CmdId,uint8_t SessionID,uint8_t MsgLen);
const uint8_t *MsgHead_Unpacket(const uint8_t *pszBuffer,uint16_t iBufferSize,uint16_t *HeadMark,uint16_t *CmdId,uint8_t *SessionID,uint8_t *MsgLen);

int faceDeleteProc(uint16_t personID);
/*获取数据库中已注册的PersonID数量*/
int GetAlbumSizeFromDataBaseProc(uint16_t *faceSize);
/*所有外部请求消息的ack应答，根据sessionID来对应*/
int cmdAckRsp(unsigned short nSessionID, unsigned char ack_status);
/*系统准备就绪上报请求*/
int cmdSysInitOKReport(const char *strVersion);
/*硬件故障上报请求*/
int cmdSysHardwareAbnormalReport(HwAbnormalType nType);
/*人脸注册响应*/
int cmdFaceRegistRsp(uint16_t persionID);
/*人脸识别响应*/
int cmdFaceRecogizeRsp(uint16_t persionID, bool IsMouseOpen, int faceCount);
/*人脸ID删除响应*/
int cmdFaceDeleteRsp(uint8_t persionID);
/*获取数据库中已注册人脸数*/
int cmdGetAlbumSizeofDatabaseRsp();
/*上报人脸坐标位置给MCU*/
int cmdFacePositionRpt(rs_rect *pRect);

/*获取系统软件版本号*/
int cmdSysVersionRsp();
/*系统软件升级*/
int cmdSysUpdateRsp(uint8_t status);
/*设置系统进入VLLS模式*/
int cmdSetSysToVLLSModeRsp();
/*设置系统进入ShutDown模式*/
int cmdSetSysToShutDownModeRsp();
/*获取系统配置*/
int cmdGetSysCfgReqProc(unsigned short nMessageLen, const unsigned char *pszMessage);
/*设置系统配置*/
int cmdSetSysCfgReqProc(unsigned short nMessageLen, const unsigned char *pszMessage);

/*------------------------本地人脸图片-------------------------------------*/
/*注册时通过PersonID保存注册的正脸到本地路径:"/opt/pic/" */
void face_reg_SaveImgToLocalFileByPersonID(imageInfo color_img, imageInfo depth_img, unsigned short personId);
/*删除注册时保存的正脸照片*/
void face_reg_DelImgFromLocalFileByPersonID(unsigned short personId);
/*通过PersonID从本地路径:"/opt/pic/"获取已经注册的正脸*/
uint8_t face_reg_Get_IR_ImgDataFromLocalFile(uint8_t *pIr_img, uint16_t width, uint16_t height,unsigned short personId);
uint8_t face_reg_Get_Depth_ImgDataFromLocalFile(uint8_t *pDepth_img, uint16_t width, uint16_t height,unsigned short personId);
/*把外部请求查询的PersonID的人脸显示在LCD上*/
#ifdef LCD_DISPLAY_ENABLE
int DrawRawImgOnScreen(uint16_t *pIr_img, uint16_t width, uint16_t height, bool flg_draw_facebox);
int DrawImgOnScreen_LocalFile(uint8_t *pIr_img, uint16_t width, uint16_t height);
#endif
int ProcMessage(
	uint16_t nCommand,
	uint16_t nSessionID,
	uint16_t nMessageLen,
	const uint8_t *pszMessage);

#ifdef __cplusplus
}
#endif

#endif // __WF_FUNCTION_H__

