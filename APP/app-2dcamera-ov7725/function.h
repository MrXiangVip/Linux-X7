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
#include "face_comm.h"
#include "util_crc16.h"
#include "sig_timer.h"
#include "per_calc.h"
#include "libsimplelog.h"
#include "FitApiHandle.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIC_PATH 		"/opt/pic/"

#define FAILED		1
#define SUCCESS		0

#define UUID_LEN				8	// uuid����
#define WIFI_SSID_LEN_MAX		31	// wifi ssid ��󳤶�
#define WIFI_PWD_LEN_MAX		63	// wifi pwd ��󳤶�
#define MQTT_USER_LEN 			6	// MQTT USER ��󳤶�
#define MQTT_PWD_LEN 			8	// MQTT pwd ��󳤶�
#define MQTT_SVR_URL_LEN_MAX	63	// mqtt server url ��󳤶�

typedef enum
{
	RPMSG_CMD,
	RPMSG_READ,
}para_rpmsg_ioctl_cmd;

//���ò����ṹ��
// recognizeThreshold -- ʶ����ֵ��50~150, Ĭ�� 85
// posFilterThreshold -- ע��������̬������ֵ��0.8~1.0, Ĭ�� 0.93�� ֵԽ��Խ����
// faceMinSize -- ���������С���ڣ� 0~width�� Ĭ��150
// faceMaxSize -- ���������󴰿ڣ� 0~width�� Ĭ��400
// faceMinLight -- ��������������ȣ� 0~255�� Ĭ��80
// faceMaxLight -- ��������������ȣ� 0~255�� Ĭ��180
// regSkipFace -- ע������ʱ���������������� 0~20�� Ĭ��10 
// antifaceThreshold -- ��������ֵ��0.0~1.0��Ĭ�� 0.5
typedef struct 
{
	unsigned short 	recognizeThreshold;			/*ע����ֵ:�������ƶȣ�ֵԽ��Խ�ϸ�*/
	float				posFilterThreshold;			
	unsigned short 	faceMinSize;					
	unsigned short 	faceMaxSize;					
	unsigned char		faceMinLight;				
	unsigned char 	faceMaxLight;				
	unsigned char 	regSkipFace;				
	float				antifaceThreshold;	
	
	unsigned char face_reg_timeout;		/*����ע��ִ�г�ʱʱ��*/	
	unsigned char face_recg_timeout;		/*����ʶ��ִ�г�ʱʱ��*/	
	BOOL		flag_auto_recg;			/*����ʶ��ʱ��סͼ�񿪹أ�Ĭ�Ͽ���Ϊ1 */			
	BOOL		flag_wdt_enable;			/*���Ź�ʹ�ܿ���*/
	BOOL		flag_wifi_enable;			/*wifiʹ�ܿ���*/
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

typedef struct _stRpMsgHead
{
	unsigned char		HeadMark;		/* ǰ����ʶ��������Ϊ1bytes������У����Ϣ�Ƿ�Ϸ� */
	unsigned char  	CmdID;			/* ��Ϣ�������Ϊ1bytes�� */
	unsigned char 	MsgLen;			/* ���ݳ��ȣ����ݳ���Ϊ1bytes�� */    
}MESSAGE_HEAD, *PMESSAGE_HEAD;

#define HEADMARK_LEN		1  
#define HEAD_MARK			0x23     /*  ��#��*/
#define HEAD_MARK_MQTT		0x24     /*  ��for mqtt��*/
#define CRC16_LEN			2  
#define MAX_PERSON_ID		200

#define Version_LEN			5   /*�汾���ȹ̶�Ϊ5, e.g 1.0.1*/

// M4��A7֮�����Ϣ
#define ID_FACE_RECOGNIZE_Req          		0x0011
#define ID_FACE_RECOGNIZE_Rsp				0x8011
#define ID_FACE_REGISTER_Req          			0x0012
#define ID_FACE_REGISTER_Rsp          			0x8012
#define ID_SYS_HW_DEBUG_Rpt				0x80f9

#define ID_FACE_ACK_Rpt          				0xff

#define CMD_INITOK_SYNC          				1		// ����ͬ��
#define CMD_HEART_BEAT		          		2		// ����
#define CMD_OPEN_DOOR						3		// ����
#define CMD_CLOSE_FACEBOARD				4		// �ػ�
#define CMD_FACE_REG						5		// ע������
#define CMD_REG_STATUS_NOTIFY			6		// ע��״̬֪ͨ
#define CMD_FACE_REG_RLT					7		//ע����
#define CMD_REG_ACTIVE_BY_PHONE			8		// ע�ἤ��
#define CMD_FACE_DELETE_USER				9		// ɾ���û�
#define CMD_BLE_TIME_SYNC					10		// ͨ��BLE����ϵͳ��ʱ
#define CMD_REQ_RESUME_FACTORY			11		// ����ָ��������ã� MCU����
#define CMD_UPGRADE_CTRL					12		// ��������
#define CMD_UPGRADE_PACKAGE				13		// ������������
#define CMD_FACE_RECOGNIZE				14		// MCU������������ʶ��
#define CMD_WIFI_SSID						15		// ����wifi��SSID
#define CMD_WIFI_PWD						16		// ����wifi��PWD
#define CMD_WIFI_MQTT						17		// ����MQTT����
#define CMD_WIFI_CONN_STATUS				18		// �ϱ�wifi����״̬
#define CMD_DEV_DEBUG						19		// �ϱ��豸����״̬
#define CMD_OPEN_LOCK_REC					20		// �ϱ����ż�¼
#define CMD_ORDER_TIME_SYNC				21		// Զ�̶���ʱ��ͬ��
#define CMD_BT_INFO						22		// �ϱ�����ģ����Ϣ
#define CMD_WIFI_MQTTSER_URL				23		// ����wifi��MQTT server��¼URL��������IP+Port������������+Port��
#define CMD_GETNETWORK_OPTVER			24		// ���ػ�ȡMCU��flash�洢������������ð汾��
#define CMD_SETNETWORK_OPTVER			25		// ������������汾��,ÿ�����ú��Լ�1������flash
#define CMD_NETWORK_OPT					26		// �����������
#define CMD_WIFI_OPEN_DOOR				0x83	// Զ��wifi����
#define CMD_WIFI_TIME_SYNC				0x8A	// ͨ��wifi����ϵͳʱ��
#define CMD_IRLIGHT_PWM_Req				0xE0	// ����IR�����

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


/*�������Ͷ���*/
typedef enum
{
	HW_BUG_NOERROR=0,
	HW_BUG_CAMERA=1,
	HW_BUG_LCD,
	HW_BUG_ENCRYPTIC,
	HW_BUG_RPMSG,
	HW_BUG_EMMC,	
	HW_BUG_UART5,
	HW_BUG_NOFACEBASE, //���������ݿ�
}HwAbnormalType;

/* ����ע��״̬���Ͷ���:
-1-ע�Ὺʼ
0-ע��ɹ�
1-ע����ɣ��ȴ�����
2-ע��ʧ��
*/
typedef enum
{
	REG_STATUS_START=-1,
	REG_STATUS_OK=0,
	REG_STATUS_FAILED=1,
	REG_STATUS_OVER=2,
}RegResultType;

/*����ʶ�������Զ���*/
typedef enum
{
	Face_recg_attrib_Normal=0, 		//�����޸澯������״��
	Face_recg_attrib_MutFace,	//�����޸澯
	Face_recg_attrib_Alarm,		//�澯����
}RecgRltAttribType;

typedef struct 
{
	unsigned char		Mcu_Ver;			/*MCU�汾����ʮ������ֵ123��ʾ�汾v1.2.3*/
	unsigned char 	PowerVal;			/*����: 1~100*/
	unsigned char 	status;				/*Ĭ��Ϊ0;��1������Bit0 ���˿��ص��δ�����MCU�ϴ�����0����Bit1 �ָ��������ð�ť���δ�����MCU�ϴ�����0����Bit2 �ϻ�ģʽ��MCU�������0����A5����*/	
}InitSyncInfo, *pInitSyncInfo;

/*ʱ��ṹ��*/
typedef struct
{
	unsigned char	year;
	unsigned char	 month;
	unsigned char	 day;
	unsigned char	 hour;
	unsigned char	 min;
	unsigned char	 sec;
}Time, *pTime;

/*�û�ע��Info�ṹ��*/
typedef struct
{	
	//uint8_t	opt_type;		//����0:����ע�� 1:��ע��
	//uint8_t 	user_type;		//��ɫ0:��ͨ�û�1:����Ա
	//uint8_t	locker_scene;	//Ӧ�ó���
	uUID	uu_id;			//��С�����ṩUUID
	//Time	curTime;			//��ǰʱ��
	//Time	overTime;   		 //��������ʱ��
}UserReg_Info, pUserReg_Info;


extern stLogCfg stSysLogCfg;
extern int ttyMCU_fd;
extern int ttyRpMsg_fd;
extern int ttyUart7_fd;
extern stSysCfg stSystemCfg;

extern int g_person_id;
extern FIT face;
extern uUID g_uu_id;
extern bool flg_sendMcuMsg;

void StrToHex(unsigned char *pbDest, char *pszSrc, int nLen);
void HexToStr(char *pszDest, unsigned char *pbSrc, int nLen);

int myWriteData2File(const char * pFileName, unsigned char *pData, int iDataLen);
int myReadDataFromFile(const char * pFileName, unsigned char *pData, int iDataLen);

int SendMsgToWifiMod(unsigned char *MsgBuf, unsigned char MsgLen);
int SendMsgToMCU(uint8_t *MsgBuf, uint8_t MsgLen);

int GetSysLocalCfg(pstSysCfg pstSystemCfg);
int CheckImageFileCRC32(const char * SrcFilePath, const char * DstFilePath,  uint32_t *crc32_val);

void StrSetUInt8( uint8_t * io_pDst, const uint8_t i_u8Src );
void StrSetUInt16( uint8_t * io_pDst, const uint16_t i_u16Src );
void StrSetUInt32( uint8_t * io_pDst, const uint32_t i_u32Src );
uint8_t Msg_UartCalcFCS( const uint8_t *msg_ptr, uint8_t len );
int MsgHead_Packet(uint8_t *pszBuffer,uint8_t HeadMark,uint8_t CmdId,uint8_t MsgLen);
const uint8_t *MsgHead_Unpacket(const uint8_t *pszBuffer,uint8_t iBufferSize,uint8_t *HeadMark,uint8_t *CmdId,uint8_t *MsgLen);

int faceDeleteProc(uint16_t personID);
/*�����ⲿ������Ϣ��ackӦ�𣬸���sessionID����Ӧ*/
int cmdAckRsp( unsigned char ack_status);
/*ϵͳ׼�������ϱ�����*/
int cmdSysInitOKSyncReq(const char *strVersion);
/*WatchDogι������*/
int cmdWatchDogReq();

/*����ע��״̬֪ͨ����*/
int cmdRegStatusNotifyReq(uint8_t regStatus);
/* ע����֪ͨ����*/
int cmdRegResultNotifyReq(uUID uu_id,uint8_t regResult);

/*����ʶ�����ָ���MCU��������*/
int cmdOpenDoorReq(uUID uu_id);
/*����IDɾ����Ӧ*/
int cmdFaceDeleteRsp(uint8_t persionID);
//�ػ�����
int cmdCloseFaceBoardReq();


/*��ȡϵͳ����汾��*/
int cmdSysVersionRsp();

/*ͨ������PWM������Ir����Ƶ�����*/
int SetIrLightingLevel(uint8_t val);
void OpenLcdBackgroud();
void CloseLcdBackgroud();

int cmdCommRsp2MCU(unsigned char CmdId, uint8_t ret);//X7->MCU: ͨ����Ӧ
int cmdCommRsp2MqttByHead(unsigned char nHead, unsigned char CmdId, uint8_t ret);//X7->MQTT: ͨ����Ӧ
int cmdCommRsp2Mqtt(unsigned char CmdId, uint8_t ret);//X7->MQTT: ͨ����Ӧ
int cmdOpenDoorRec2Mqtt(uUID uu_id, uint8_t type, uint8_t ret);//�ϴ����ż�¼��Mqttģ��

int cmdNetworkOptVerReq();

int ProcMessage(
	uint8_t nCommand,
	uint8_t nMessageLen,
	const uint8_t *pszMessage);
int ProcMessageByHead(
	uint8_t head,
	uint8_t nCommand,
	uint8_t nMessageLen,
	const uint8_t *pszMessage);
int ProcMessageFromMQTT(
	uint8_t nCommand,
	uint8_t nMessageLen,
	const uint8_t *pszMessage);

void yuyv_to_rgba(unsigned char *yuv_buffer,unsigned char *rgb_buffer,int iWidth,int iHeight);

#ifdef __cplusplus
}
#endif

#endif // __WF_FUNCTION_H__

