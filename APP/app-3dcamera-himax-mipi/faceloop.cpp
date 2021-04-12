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

#include "face_alg.h"
#include "custom.h"
#include "function.h"


int main(int argc, char **argv)
{
	int ret=-1, rpmsg_fd=-1;

	unsigned char rpmsg_buf[128] = {0};
	unsigned char *szBuffer = NULL;
	const unsigned char *pszMsgInfo=NULL;	
	unsigned short HeadMark;
	unsigned short CmdId;
	unsigned char SessionID;
	unsigned char MsgLen;

	unsigned char  Msg_FCS = 0,Cal_FCS = 0;
	printf("face_loop starting...\n");

	//tell MCU that A7 has Initialized OK.
	cmdSysInitOKReport(SYS_VERSION);
	
	// Set the log level
	stLogCfg stSysLogCfg;
	GetSysLogCfg(&stSysLogCfg);
	log_set_level(stSysLogCfg.log_level);
	log_set_simple(1);
	
	rpmsg_fd = open(RPMSG_DEV, O_RDWR);
	if(!rpmsg_fd)
	{
		printf("ttyRPMSG device open failed!\n");
		cmdSysHardwareAbnormalReport(HW_BUG_RPMSG);
		return -1;
	}
		
	while(1)
	{
		memset(rpmsg_buf, 0, sizeof(rpmsg_buf));
		ret = ioctl(rpmsg_fd, RPMSG_READ, rpmsg_buf);  
		if(ret != 0)
		{		
			printf("rpmsg ioctl error: %d!\n", ret);
			break;
		}
		
		int rpmsg_len = 0;
		memcpy(&rpmsg_len, rpmsg_buf, sizeof(int));
		if(0 == rpmsg_len)
		{
			usleep(50000);
			continue;
		}
		log_info("\n>>>recv rpmsg:len<%d>\n", rpmsg_len);	
		if(rpmsg_len > sizeof(rpmsg_buf))
		{
			continue;
		}
#if 1		
		int i = rpmsg_len;
		for(i; i>0; i--)	
		{		
			log_info("rpmsg_buf[%d]: 0x%02x   ", i, rpmsg_buf[i-1+4]);	
		}	
		log_info("\n====end====\n");	
#endif
		if(rpmsg_len < sizeof(MESSAGE_HEAD)+FCS_LEN)
		{
			log_error("rpmsg len error: %d!\n", rpmsg_len);
			goto ERROR;
		}
		szBuffer = rpmsg_buf+sizeof(rpmsg_len);
		
		//FCS处理
		Msg_FCS = (unsigned char)*(szBuffer+rpmsg_len-1);
		Cal_FCS = Msg_UartCalcFCS( szBuffer+sizeof(HeadMark), rpmsg_len-sizeof(HeadMark)-FCS_LEN);
		if(Msg_FCS != Cal_FCS)
		{
			log_error("rpmsg FCS error: Msg_FCS<%d>, Cal_FCS<%d>!\n", Msg_FCS, Cal_FCS);	
			goto ERROR;
		}

		pszMsgInfo = MsgHead_Unpacket(
					szBuffer,
					rpmsg_len,
					&HeadMark,
					&CmdId,
					&SessionID,
					&MsgLen);
		if(HeadMark != RCVMSG_HEAD_MARK)
		{
			log_error("rpmsg headmark error: 0x%x!\n", HeadMark);			
			goto ERROR;
		}

		/*处理消息*/
		ProcMessage(CmdId,
					SessionID,
					MsgLen,
					pszMsgInfo);

ERROR:			
		usleep(50000);  //sleep 50ms
	}	

	close(rpmsg_fd);

	return 0;
}

