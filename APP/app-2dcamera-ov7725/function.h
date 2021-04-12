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

#define UUID_LEN				8	// uuid长度
#define WIFI_SSID_LEN_MAX		31	// wifi ssid 最大长度
#define WIFI_PWD_LEN_MAX		63	// wifi pwd 最大长度
#define MQTT_USER_LEN 			6	// MQTT USER 最大长度
#define MQTT_PWD_LEN 			8	// MQTT pwd 最大长度
#define MQTT_SVR_URL_LEN_MAX	63	// mqtt server url 最大长度

typedef enum
{
	RPMSG_CMD,
	RPMSG_READ,
}para_rpmsg_ioctl_cmd;

//配置参数结构体
// recognizeThreshold -- 识别阈值，50~150, 默认 85
// posFilterThreshold -- 注册相邻姿态过滤阈值，0.8~1.0, 默认 0.93， 值越大越宽松
// faceMinSize -- 人脸检测最小窗口， 0~width， 默认150
// faceMaxSize -- 人脸检测最大窗口， 0~width， 默认400
// faceMinLight -- 人脸区域最低亮度， 0~255， 默认80
// faceMaxLight -- 人脸区域最高亮度， 0~255， 默认180
// regSkipFace -- 注册启动时跳过的人脸张数， 0~20， 默认10 
// antifaceThreshold -- 活体检测阈值，0.0~1.0，默认 0.5
typedef struct 
{
	unsigned short 	recognizeThreshold;			/*注册阈值:人脸相似度，值越大，越严格*/
	float				posFilterThreshold;			
	unsigned short 	faceMinSize;					
	unsigned short 	faceMaxSize;					
	unsigned char		faceMinLight;				
	unsigned char 	faceMaxLight;				
	unsigned char 	regSkipFace;				
	float				antifaceThreshold;	
	
	unsigned char face_reg_timeout;		/*人脸注册执行超时时间*/	
	unsigned char face_recg_timeout;		/*人脸识别执行超时时间*/	
	BOOL		flag_auto_recg;			/*人脸识别时卡住图像开关，默认开启为1 */			
	BOOL		flag_wdt_enable;			/*看门狗使能开关*/
	BOOL		flag_wifi_enable;			/*wifi使能开关*/
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
	unsigned char		HeadMark;		/* 前导标识符（长度为1bytes）用于校验消息是否合法 */
	unsigned char  	CmdID;			/* 消息命令（长度为1bytes） */
	unsigned char 	MsgLen;			/* 数据长度（数据长度为1bytes） */    
}MESSAGE_HEAD, *PMESSAGE_HEAD;

#define HEADMARK_LEN		1  
#define HEAD_MARK			0x23     /*  ‘#’*/
#define HEAD_MARK_MQTT		0x24     /*  ‘for mqtt’*/
#define CRC16_LEN			2  
#define MAX_PERSON_ID		200

#define Version_LEN			5   /*版本长度固定为5, e.g 1.0.1*/

// M4与A7之间的消息
#define ID_FACE_RECOGNIZE_Req          		0x0011
#define ID_FACE_RECOGNIZE_Rsp				0x8011
#define ID_FACE_REGISTER_Req          			0x0012
#define ID_FACE_REGISTER_Rsp          			0x8012
#define ID_SYS_HW_DEBUG_Rpt				0x80f9

#define ID_FACE_ACK_Rpt          				0xff

#define CMD_INITOK_SYNC          				1		// 开机同步
#define CMD_HEART_BEAT		          		2		// 心跳
#define CMD_OPEN_DOOR						3		// 开门
#define CMD_CLOSE_FACEBOARD				4		// 关机
#define CMD_FACE_REG						5		// 注册请求
#define CMD_REG_STATUS_NOTIFY			6		// 注册状态通知
#define CMD_FACE_REG_RLT					7		//注册结果
#define CMD_REG_ACTIVE_BY_PHONE			8		// 注册激活
#define CMD_FACE_DELETE_USER				9		// 删除用户
#define CMD_BLE_TIME_SYNC					10		// 通过BLE设置系统对时
#define CMD_REQ_RESUME_FACTORY			11		// 请求恢复出厂设置， MCU发起
#define CMD_UPGRADE_CTRL					12		// 升级控制
#define CMD_UPGRADE_PACKAGE				13		// 升级单包传输
#define CMD_FACE_RECOGNIZE				14		// MCU主动请求人脸识别
#define CMD_WIFI_SSID						15		// 设置wifi的SSID
#define CMD_WIFI_PWD						16		// 设置wifi的PWD
#define CMD_WIFI_MQTT						17		// 设置MQTT参数
#define CMD_WIFI_CONN_STATUS				18		// 上报wifi连接状态
#define CMD_DEV_DEBUG						19		// 上报设备故障状态
#define CMD_OPEN_LOCK_REC					20		// 上报开门记录
#define CMD_ORDER_TIME_SYNC				21		// 远程订单时间同步
#define CMD_BT_INFO						22		// 上报蓝牙模块信息
#define CMD_WIFI_MQTTSER_URL				23		// 设置wifi的MQTT server登录URL（可能是IP+Port，可能是域名+Port）
#define CMD_GETNETWORK_OPTVER			24		// 主控获取MCU中flash存储的网络参数设置版本号
#define CMD_SETNETWORK_OPTVER			25		// 设置网络参数版本号,每次设置后都自加1，存入flash
#define CMD_NETWORK_OPT					26		// 网络参数设置
#define CMD_WIFI_OPEN_DOOR				0x83	// 远程wifi开门
#define CMD_WIFI_TIME_SYNC				0x8A	// 通过wifi设置系统时间
#define CMD_IRLIGHT_PWM_Req				0xE0	// 设置IR补光灯

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


/*故障类型定义*/
typedef enum
{
	HW_BUG_NOERROR=0,
	HW_BUG_CAMERA=1,
	HW_BUG_LCD,
	HW_BUG_ENCRYPTIC,
	HW_BUG_RPMSG,
	HW_BUG_EMMC,	
	HW_BUG_UART5,
	HW_BUG_NOFACEBASE, //无人脸数据库
}HwAbnormalType;

/* 人脸注册状态类型定义:
-1-注册开始
0-注册成功
1-注册完成，等待激活
2-注册失败
*/
typedef enum
{
	REG_STATUS_START=-1,
	REG_STATUS_OK=0,
	REG_STATUS_FAILED=1,
	REG_STATUS_OVER=2,
}RegResultType;

/*人脸识别结果属性定义*/
typedef enum
{
	Face_recg_attrib_Normal=0, 		//单人无告警的正常状况
	Face_recg_attrib_MutFace,	//多人无告警
	Face_recg_attrib_Alarm,		//告警触发
}RecgRltAttribType;

typedef struct 
{
	unsigned char		Mcu_Ver;			/*MCU版本，如十进制数值123表示版本v1.2.3*/
	unsigned char 	PowerVal;			/*电量: 1~100*/
	unsigned char 	status;				/*默认为0;置1描述：Bit0 防撬开关单次触发（MCU上传后置0）；Bit1 恢复出厂设置按钮单次触发（MCU上传后置0）；Bit2 老化模式（MCU掉电后清0，由A5配置*/	
}InitSyncInfo, *pInitSyncInfo;

/*时间结构体*/
typedef struct
{
	unsigned char	year;
	unsigned char	 month;
	unsigned char	 day;
	unsigned char	 hour;
	unsigned char	 min;
	unsigned char	 sec;
}Time, *pTime;

/*用户注册Info结构体*/
typedef struct
{	
	//uint8_t	opt_type;		//操作0:重新注册 1:新注册
	//uint8_t 	user_type;		//角色0:普通用户1:管理员
	//uint8_t	locker_scene;	//应用场景
	uUID	uu_id;			//由小程序提供UUID
	//Time	curTime;			//当前时间
	//Time	overTime;   		 //订单结束时间
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
/*所有外部请求消息的ack应答，根据sessionID来对应*/
int cmdAckRsp( unsigned char ack_status);
/*系统准备就绪上报请求*/
int cmdSysInitOKSyncReq(const char *strVersion);
/*WatchDog喂狗请求*/
int cmdWatchDogReq();

/*人脸注册状态通知请求*/
int cmdRegStatusNotifyReq(uint8_t regStatus);
/* 注册结果通知请求*/
int cmdRegResultNotifyReq(uUID uu_id,uint8_t regResult);

/*人脸识别完后发指令给MCU开锁请求*/
int cmdOpenDoorReq(uUID uu_id);
/*人脸ID删除响应*/
int cmdFaceDeleteRsp(uint8_t persionID);
//关机请求
int cmdCloseFaceBoardReq();


/*获取系统软件版本号*/
int cmdSysVersionRsp();

/*通过设置PWM，设置Ir补光灯的亮度*/
int SetIrLightingLevel(uint8_t val);
void OpenLcdBackgroud();
void CloseLcdBackgroud();

int cmdCommRsp2MCU(unsigned char CmdId, uint8_t ret);//X7->MCU: 通用响应
int cmdCommRsp2MqttByHead(unsigned char nHead, unsigned char CmdId, uint8_t ret);//X7->MQTT: 通用响应
int cmdCommRsp2Mqtt(unsigned char CmdId, uint8_t ret);//X7->MQTT: 通用响应
int cmdOpenDoorRec2Mqtt(uUID uu_id, uint8_t type, uint8_t ret);//上传开门记录给Mqtt模块

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

