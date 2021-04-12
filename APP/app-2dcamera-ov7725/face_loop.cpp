/**************************************************************************
 * 	FileName:	 face_loop.cpp
 *	Description:	Deal with message from M4,and upgrade system app and algorithm
 *	Copyright(C):	2018-2020 Wave Group Inc.
 *	Version:		V 1.0
 *	Author:		tanqw
 *	Created:		
 *	Updated:		2019-08-13
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
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "custom.h"
#include "function.h"
#include "hl_MsgQueue.h"  
#include "uart_handle.h"

/*
* �����ⲿMCU֮��ͨ�ŵĴ�����Ϣ
*/
static void McuMsgProcCB()
{
	int ret=-1, i = 0;
	unsigned char msgbuf[128] = {0};	
	unsigned char msgbuf_over[64] = {0};
	unsigned char *szBuffer = NULL;
	const unsigned char *pszMsgInfo=NULL;	
	unsigned char HeadMark;
	unsigned char CmdId=0;
	unsigned short msglen = 0, leftLen=0;
	unsigned char MsgLen=0;
	BOOL	Ismsgleft = false;
	unsigned short  Msg_CRC16 = 0,Cal_CRC16= 0;
	
	//open uart to comunicate
	ret = serial_com_open(&ttyMCU_fd, UART6_DEV);
	if (ret < 0)
	{
		log_error("open uart6 failed!\n");
		return;
	}
	log_info("open uart6 success!\n");

	//tell MCU that A7 has Initialized OK.
	cmdSysInitOKSyncReq(SYS_VERSION);

	//create MsgQueue proc thread
	Message_init();
	
	while(1)
	{		
		memset(msgbuf, 0, sizeof(msgbuf));
		msglen = 0;
		ret = serial_com_read(ttyMCU_fd, msgbuf, sizeof(msgbuf), &msglen);			
		szBuffer = msgbuf;

FLAG1:
		if(0 == msglen)
		{
			usleep(10000);
			continue;
		}
		log_info("\n>>>recv msg<len:%d>\n", msglen);	
		if(msglen > sizeof(msgbuf))
		{
			continue;
		}
		if(msglen < sizeof(MESSAGE_HEAD)+CRC16_LEN)
		{
			log_error("msg len error: %d!\n", msglen);
			goto ERROR;
		}
		if(stSysLogCfg.log_level < LOG_INFO)
		{
			printf("====RcvMsg<len=%d>:", msglen);	
			for(i=0; i<msglen; i++)	
			{		
				printf("0x%02x   ", *(szBuffer+i));	
			}	
			printf("\n");
		}
		if(msglen > *(szBuffer+2) +5)
		{//�洢�Ӵ��ڻ�ȡ����Ϣ���ಿ��
			//printf("*****<%d > %d>:", msglen, *(szBuffer+2));	
			leftLen = msglen - (*(szBuffer+2)+5);
			msglen = *(szBuffer+2)+5;
			memset(msgbuf_over, 0, sizeof(msgbuf_over));
			memcpy(msgbuf_over, szBuffer+msglen, leftLen);
			Ismsgleft =  true;
		}
		else
		{
			Ismsgleft =  false;
		}
		
		//У�鴦��
		memcpy(&Msg_CRC16, szBuffer+msglen-CRC16_LEN, CRC16_LEN);
		Cal_CRC16 = CRC16_X25(szBuffer, msglen-CRC16_LEN);
		if(Msg_CRC16 != Cal_CRC16)
		{
			log_error("msg CRC error: Msg_CRC16<0x%02X>, Cal_CRC16<0x%02X>!\n", Msg_CRC16, Cal_CRC16);	
			goto ERROR;
		}

		pszMsgInfo = MsgHead_Unpacket(
					szBuffer,
					msglen,
					&HeadMark,
					&CmdId,
					&MsgLen);
		if(HeadMark != HEAD_MARK)
		{
			log_error("msg headmark error: 0x%x!\n", HeadMark);			
			goto ERROR;
		}
		
		/*������Ϣ*/
		ProcMessage(CmdId,
					MsgLen,
					pszMsgInfo);

		if(Ismsgleft)
		{//��δ����Ĵ������ݼ�������
			msglen = leftLen;
			memset(msgbuf, 0, sizeof(msgbuf));
			memcpy(msgbuf, msgbuf_over, leftLen);
			
			printf("@@@@@LeftMsg<len=%d>:", msglen);	
			goto FLAG1;
		}
		continue;
		

ERROR:				
		usleep(20000);  //sleep 20ms
	}	

	close(ttyMCU_fd);

	return;
}

/*
* ����M4��ͨ�ŵ�RPMSGͨ����Ϣ
*/
static void RpMsgProcCB()
{
	int ret=-1;
	unsigned char msgbuf[128] = {0};	
	unsigned char *szBuffer = NULL;
	const unsigned char *pszMsgInfo=NULL;	
	unsigned char HeadMark;
	unsigned char CmdId=0;
	unsigned short msglen = 0;
	unsigned char MsgLen;
	unsigned short	Msg_CRC16 = 0,Cal_CRC16= 0;

	//open Rpmsg channel
	ttyRpMsg_fd = open(RPMSG_DEV, O_RDWR);
	if(!ttyRpMsg_fd)
	{
		log_error("ttyRPMSG device open failed!\n");
		return;
	}
	log_info("open ttyRPMSG30 success!\n");
	
	while(1)
	{		
		memset(msgbuf, 0, sizeof(msgbuf));
		msglen = 0;
		ret = ioctl(ttyRpMsg_fd, RPMSG_READ, msgbuf);  
		if(ret != 0)
		{		
			printf("rpmsg ioctl error: %d!\n", ret);
			break;
		}
		
		szBuffer = msgbuf+sizeof(msglen);
		memcpy(&msglen, msgbuf, sizeof(short));
		if(0 == msglen)
		{
			usleep(10000);
			continue;
		}
		log_info("\n>>>recv msg<len:%d>\n", msglen);	
		if(msglen > sizeof(msgbuf))
		{
			continue;
		}
		if(msglen < sizeof(MESSAGE_HEAD)+CRC16_LEN)
		{
			log_error("msg len error: %d!\n", msglen);
			continue;
		}
		if(stSysLogCfg.log_level < LOG_INFO)
		{
			int i = 0;
			printf("====RcvMsg<len=%d>:", msglen);	
			for(i=0; i<msglen; i++)	
			{		
				printf("0x%02x   ", *(szBuffer+i));	
			}	
			printf("\n");
		}
		
		//У�鴦��
		memcpy(&Msg_CRC16, szBuffer+msglen-CRC16_LEN, CRC16_LEN);
		Cal_CRC16 = CRC16_X25(szBuffer, msglen-CRC16_LEN);
		if(Msg_CRC16 != Cal_CRC16)
		{
			log_error("msg CRC error: Msg_CRC16<0x%02X>, Cal_CRC16<0x%02X>!\n", Msg_CRC16, Cal_CRC16);	
			continue;
		}

		pszMsgInfo = MsgHead_Unpacket(
					szBuffer,
					msglen,
					&HeadMark,
					&CmdId,
					&MsgLen);
		if(HeadMark != HEAD_MARK)
		{
			log_error("msg headmark error: 0x%x!\n", HeadMark);			
			continue;
		}
		
		/*������Ϣ*/
		ProcMessage(CmdId,
					MsgLen,
					pszMsgInfo);
		continue;
	}	

	close(ttyRpMsg_fd);

	return;
}

/*
* ����ʱִ�е�Task
*/
static void WatchDogProcCB()
{
	int num = 0;
	while(1)
	{
		sleep(1);   
		num++;
		
		if(flg_sendMcuMsg)
		{
			//���һֱ����Ϣ�����Ͳ���Ҫι��
			num = 0;
			flg_sendMcuMsg = false;
		}

		if(num >= 8)
		{
			//ι��
			cmdWatchDogReq();
			usleep(100);
		}
	}
}

int main(int argc, char **argv)
{
	printf("SysVersion<%s>\n", SYS_VERSION);
#if 0
	/*��������ͷʹ�ܽ�*/
	system("echo 153 > /sys/class/gpio/export");
	system("echo out > /sys/class/gpio/gpio153/direction");
	system("echo 1 > /sys/class/gpio/gpio153/value");
	system("echo 25 > /sys/class/gpio/unexport");
#endif					
	
	// Set the log level
	GetSysLogCfg(&stSysLogCfg);
	log_set_level(stSysLogCfg.log_level);
	log_set_simple(1);

	// get global local cfg from file
	GetSysLocalCfg(&stSystemCfg);

	static std::thread *McuMsgProcThread;
	McuMsgProcThread = new std::thread(McuMsgProcCB);

	static std::thread *RpMsgProcThread;
	RpMsgProcThread = new std::thread(RpMsgProcCB);

	static std::thread *WatchDogProcThread;
	if(stSystemCfg.flag_wdt_enable)
	{
		WatchDogProcThread = new std::thread(WatchDogProcCB);
	}
	
	//���̵߳ȴ����߳��˳�
	McuMsgProcThread->join();
	RpMsgProcThread->join();

	//�˳��߳�
	delete McuMsgProcThread;
	delete RpMsgProcThread;	
	if(stSystemCfg.flag_wdt_enable)
	{
		delete WatchDogProcThread;
	}
	
	return 0;
}

