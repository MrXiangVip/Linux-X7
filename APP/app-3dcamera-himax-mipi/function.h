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

#include "face_alg.h"
#include "libsimplelog.h"
#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RPMSG_DEV		"/dev/ttyRPMSG30"
#define MAIN_WORK_PATH(file_name)		"/opt/smartlocker/"#file_name
#define PIC_PATH 		"/opt/pic/"
#define BOOL	unsigned char

#define YES			1
#define NO			0

#define FAILED		0
#define SUCCESS		1

#define CFG_PER_TIME_CAL_OVERALL		1

typedef enum
{
	RPMSG_CMD,
	RPMSG_READ,
}para_rpmsg_ioctl_cmd;

//���ò����ṹ��
typedef struct 
{
	unsigned short face_reg_threshold_val;			/*ע����ֵ:�������ƶȣ�ֵԽ��Խ�ϸ�*/
	unsigned short face_recg_threshold_val;			/*ʶ����ֵ:�������ƶȣ�ֵԽ��Խ�ϸ�*/
	float 		face_reg_angle;					/*ע��Ƕ�*/
	float 		face_recg_angle;					/*ʶ��Ƕ�*/
	BOOL		auto_udisk_upgrade;				/*�Զ����U�������� 0:no, 1:yes*/
	unsigned char face_reg_timeout;				/*����ע��ִ�г�ʱʱ��*/	
	unsigned char face_recg_timeout;				/*����ʶ��ִ�г�ʱʱ��*/	
	BOOL		flag_muxAngle_reg;				/*��Ƕ�ע���ǣ� 0:no, 1:yes*/
}stSysCfg, *pstSysCfg;

#define FACE_REG_TIMEOUT 			30    //��������ע�ᳬʱʱ��
#define FACE_RECG_TIMEOUT 			5	// ����ʶ��ʱʱ��

//���ò���ID
#define ParamNum			8   //��������������������Ĳ������͸���һ��
enum DecompTypes
{
	id_face_reg_threshold =1,			/*ע����ֵ:�������ƶȣ�ֵԽ��Խ�ϸ�*/
	id_face_recg_threshold,			/*ʶ����ֵ:�������ƶȣ�ֵԽ��Խ�ϸ�*/
	id_face_reg_angle,				/*ע��Ƕ�*/
	id_face_recg_angle,				/*ʶ��Ƕ�*/
	id_auto_udisk_upgrade,			/*�Զ����U�������� 0:no, 1:yes*/
	id_face_reg_timeout,				/*����ע��ִ�г�ʱʱ��*/	
	id_face_recg_timeout,				/*����ʶ��ִ�г�ʱʱ��*/
	id_flag_muxAngle_reg,			/*��Ƕ�ע���ǣ� 0:no, 1:yes*/
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
	unsigned short HeadMark;		/* ǰ����ʶ��������Ϊ2bytes������У����Ϣ�Ƿ�Ϸ� */
	unsigned short CmdID;			/* ��Ϣ�������Ϊ2bytes�� */
	unsigned char SessionID;		/* �ỰID������Ϊ1bytes�� */
	unsigned char MsgLen;			/* ��Ϣ���ȣ�����Ϊ1bytes����������ͷ��CRCУ�飩 */    
}MESSAGE_HEAD, *PMESSAGE_HEAD;

#define HEADMARK_LEN		2  
#define SNDMSG_HEAD_MARK				(0x4137)  /*A7*/
#define RCVMSG_HEAD_MARK				(0x4D34)  /*M4*/

#define FCS_LEN				1  
#define MAX_PERSON_ID		5000

// M4��A7֮�����Ϣ
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

#define ID_GET_SYS_CFG_Req					0x00f2
#define ID_GET_SYS_CFG_Rsp					0x80f2
#define ID_FACE_APP_UPDATE_Req          		0x00f3
#define ID_FACE_APP_UPDATE_Rsp          		0x80f3
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

/*ע����Ϣʱ��Ӧ����*/
#define ID_REG_DISTANSE_NEAR				0xff04
#define ID_REG_DISTANSE_FAR				0xff05
#define ID_REG_ADDFACE_NULL				0xff10
#define ID_REG_PITCH_UP					0xff11
#define ID_REG_PITCH_UP_TOO_BIG			0xff12   //0xff12̧ͷ�Ƕȹ���
#define ID_REG_PITCH_UP_TOO_SMALL		0xff13	//0xff13̧ͷ�Ƕȹ�С
#define ID_REG_PITCH_DOWN					0xff21	
#define ID_REG_PITCH_DOWN_TOO_BIG		0xff22	//0xff22��ͷ�Ƕȹ���
#define ID_REG_PITCH_DOWN_TOO_SMALL		0xff23	//0xff23��ͷ�Ƕȹ�С
#define ID_REG_YAW_LEFT					0xff31
#define ID_REG_YAW_LEFT_TOO_BIG			0xff32	//0xff32��ƫ���Ƕȹ���
#define ID_REG_YAW_LEFT_TOO_SMALL		0xff33	//0xff33��ƫ���Ƕȹ�С
#define ID_REG_YAW_RIGHT					0xff41
#define ID_REG_YAW_RIGHT_TOO_BIG			0xff42	//0xff42��ƫ���Ƕȹ���
#define ID_REG_YAW_RIGHT_TOO_SMALL		0xff43	//0xff43��ƫ���Ƕȹ�С
#define ID_REG_ROLL_LEFT					0xff51
#define ID_REG_ROLL_LEFT_TOO_BIG			0xff52	//0xff52ͷ�����Ƕȹ���
#define ID_REG_ROLL_LEFT_TOO_SMALL		0xff53	//0xff53ͷ�����Ƕȹ�С
#define ID_REG_ROLL_RIGHT					0xff61
#define ID_REG_ROLL_RIGHT_TOO_BIG		0xff62	//0xff62ͷ���Ҽ�Ƕȹ���
#define ID_REG_ROLL_RIGHT_TOO_SMALL		0xff63	//0xff63ͷ���Ҽ�Ƕȹ�С



/*Ӳ���������Ͷ���*/
typedef enum
{
	HW_BUG_CAMERA,
	HW_BUG_LCD,
	HW_BUG_ENCRYPTIC,
	HW_BUG_RPMSG,
	HW_BUG_EMMC,	
}HwAbnormalType;

/*����ʶ�������Զ���*/
typedef enum
{
	Face_recg_attrib_Normal=0, 		//�����޸澯������״��
	Face_recg_attrib_MutFace,	//�����޸澯
	Face_recg_attrib_Alarm,		//�澯����
}RecgRltAttribType;

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
int GetAlbumSizeFromDataBaseProc(uint16_t *faceSize);

int cmdSysHardwareAbnormalReport(HwAbnormalType nType);
int cmdSysInitOKReport(const char *strVersion);
int cmdFaceRegistRsp(uint16_t persionID);
int cmdFaceRecogizeRsp(uint16_t persionID, bool IsMouseOpen, int faceCount);
int cmdFaceDeleteRsp(uint8_t persionID);
int cmdGetAlbumSizeofDatabaseRsp();
int cmdSysVersionRsp();
int cmdSysUpdateRsp(uint8_t status);
int cmdSetSysToVLLSModeRsp();
int cmdSetSysToShutDownModeRsp();

int cmdGetSysCfgReqProc(unsigned short nMessageLen, const unsigned char *pszMessage);
int cmdSetSysCfgReqProc(unsigned short nMessageLen, const unsigned char *pszMessage);

void face_reg_SaveImgToLocalFileByPersonID(face_image_info color_img, face_image_info depth_img, unsigned short personId);
void face_reg_DelImgFromLocalFileByPersonID(unsigned short personId);
uint8_t face_reg_GetImgDataFromLocalFileByPersonID(face_image_info *pIr_img, face_image_info *pDepth_img, unsigned short personId);
#if CFG_ENABLE_PREVIEW
int DrawImgOnScreen(unsigned short personId);
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

