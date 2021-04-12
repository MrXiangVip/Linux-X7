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
#include <linux/fb.h>

#include "RSApiHandle.h"
#include "custom.h"
#include "function.h"
#include "hl_MsgQueue.h"  
#include "uart_handle.h"

int main(int argc, char **argv)
{
	int ret=-1;
	unsigned char msgbuf[128] = {0};
	unsigned char *szBuffer = NULL;
	const unsigned char *pszMsgInfo=NULL;	
	unsigned short HeadMark;
	unsigned short CmdId, msglen = 0;
	unsigned char SessionID;
	unsigned char MsgLen,ackRlt=0;

	unsigned char  Msg_FCS = 0,Cal_FCS = 0;
	printf("SysVersion<%s>\n", SYS_VERSION);
	
	// Set the log level
	GetSysLogCfg(&stSysLogCfg);
	log_set_level(stSysLogCfg.log_level);
	log_set_simple(1);

#if USE_M4_FLAG
	//open Rpmsg channel
	tty_fd = open(RPMSG_DEV, O_RDWR);
	if(!tty_fd)
	{
		log_error("ttyRPMSG device open failed!\n");
		cmdSysHardwareAbnormalReport(HW_BUG_RPMSG);
		return -1;
	}
	log_info("open ttyRPMSG30 success!\n");
#else
	//open uart5 to comunicate
	ret = serial_com_open(&tty_fd, "/dev/ttyLP1");
	if (ret < 0)
	{
		log_error("open uart5 failed!\n");
		cmdSysHardwareAbnormalReport(HW_BUG_UART5);
		return -1;
	}
	log_info("open uart5 success!\n");
#endif
	//tell MCU that A7 has Initialized OK.
	cmdSysInitOKReport(SYS_VERSION);

	//create MsgQueue proc thread
	Message_init();
		
	while(1)
	{		
		memset(msgbuf, 0, sizeof(msgbuf));
		msglen = 0;
#if USE_M4_FLAG
		ret = ioctl(tty_fd, RPMSG_READ, msgbuf);  
		if(ret != 0)
		{		
			printf("rpmsg ioctl error: %d!\n", ret);
			break;
		}
		
		szBuffer = msgbuf+sizeof(msglen);
		memcpy(&msglen, msgbuf, sizeof(short));
#else
		ret = serial_com_read(tty_fd, msgbuf, sizeof(msgbuf), &msglen);			
		szBuffer = msgbuf;
#endif
		if(0 == msglen)
		{
			usleep(10000);
			continue;
		}
		log_info("\n>>>recv rpmsg<len:%d>\n", msglen);	
		if(msglen > sizeof(msgbuf))
		{
			continue;
		}
		if(msglen < sizeof(MESSAGE_HEAD)+FCS_LEN)
		{
			log_error("rpmsg len error: %d!\n", msglen);
			goto ERROR;
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
		
		//FCS处理
		Msg_FCS = (unsigned char)*(szBuffer+msglen-1);
		Cal_FCS = Msg_UartCalcFCS( szBuffer+sizeof(HeadMark), msglen-sizeof(HeadMark)-FCS_LEN);
		if(Msg_FCS != Cal_FCS)
		{
			log_error("rpmsg FCS error: Msg_FCS<%d>, Cal_FCS<%d>!\n", Msg_FCS, Cal_FCS);	
			goto ERROR;
		}

		pszMsgInfo = MsgHead_Unpacket(
					szBuffer,
					msglen,
					&HeadMark,
					&CmdId,
					&SessionID,
					&MsgLen);
		if(HeadMark != RCVMSG_HEAD_MARK)
		{
			log_error("rpmsg headmark error: 0x%x!\n", HeadMark);			
			goto ERROR;
		}
		
		/*返回请求消息的OK_ACK*/
		cmdAckRsp(SessionID, 1);

		/*处理消息*/
		ProcMessage(CmdId,
					SessionID,
					MsgLen,
					pszMsgInfo);
		continue;
		

ERROR:		
		/*返回请求消息的error_ACK*/
		cmdAckRsp(SessionID, 0);
		
		usleep(20000);  //sleep 20ms
	}	

	close(tty_fd);

	return 0;
}

