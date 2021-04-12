#include <unistd.h>  
#include <stdlib.h>  
#include <stdio.h>  
#include <string.h>  
#include <sys/msg.h>  
#include <errno.h>  
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "mqtt-mq.h"  
#include "log.h"
#include "util.h"

extern stLogCfg stSysLogCfg;               //记录系统日志配置信息
#ifdef HOST2
int mqtt_init_done=1;
#else
extern int mqtt_init_done;
#endif

#ifndef HOST2
extern int SendMsgToMQTT(char* response, int len); 
#endif
  
int  MessageSend(int key, const char *cmd, unsigned char msgLen)  
{  
    msg_st data; 
    int msgid = -1;
 
    msgid = msgget((key_t)key, 0666 | IPC_CREAT);  
    if(msgid == -1)  
    {  
        fprintf(stderr, "msgget failed with error: %d\n", errno);  
        return -1; 
    }  

    if (msgLen >= MAX_MSG_LEN)
    {
	return -1;
    }
    
    data.msg_type = 1; 
    memcpy(data.buf, &msgLen, sizeof(msgLen));
    memcpy(data.buf+sizeof(msgLen), cmd, msgLen);
	        
    if(msgsnd(msgid, (void*)&data, MAX_MSG_LEN, 0) == -1)  
    {  
        fprintf(stderr, "msgsnd failed\n");  
        return -1;
    }  

    return 0;
}

int w7_battery_level = -1;
char w7_firmware_version[10];

int  MessageRcv(int key) 
{    
    int msgid = -1;  
    msg_st data;  
    long msgtype = 1;
    unsigned char MsgLen = 0;
  
    msgid = msgget((key_t)key, 0666 | IPC_CREAT);  
    if(msgid == -1)  
    {  
        fprintf(stderr, "msgget failed with error: %d\n", errno);  
        return -1; 
    }  
 
    while(1)  
    {  
		if (mqtt_init_done == 0) {
			usleep(50*1000);
			continue;
		}
        if(msgrcv(msgid, (void*)&data, MAX_MSG_LEN+sizeof(MsgLen), msgtype, 0) == -1)  
        {  
            fprintf(stderr, "msgrcv failed with errno: %d\n", errno);  
            usleep(50*1000);
            continue;
        }  
	else
	{
		MsgLen = data.buf[0];
	#if 1
		int i = 1;
		printf("\n<<<rcv mq<len:%d>:", MsgLen);	
		for(i = 1; i<=MsgLen; i++)	
		{		
			printf("0x%02x	 ", data.buf[i]); 
		}	
		printf("\n");
	#endif
		unsigned char *msg = data.buf + sizeof(MsgLen);
		// printf("----- %d %d\n", (int)(char)(msg[0]), 0x23);
		log_debug("----- msg %s\n", msg);
		if (strncmp((const char*)msg, "log_level=", strlen("log_level")) == 0) {
			char logmsg[256];
			char first[256];
			char last[256];
			strncpy(logmsg, (const char*)msg, MsgLen);  
			mysplit(logmsg, first, last, "=");
			int loglevel = atoi(last);
			if (loglevel >= 0 && loglevel <=5 ) {
				stSysLogCfg.log_level = loglevel;
				log_info("set loglevel to %d\n", loglevel);
			}
			continue;
		}
		if ((int)(char)(msg[0]) == 0x23) {
			printf("---- from MCU\n");
			if ((int)(char)(msg[1]) == 0x01) {
				log_info("---- MCU boot sync\n");
				// 开机同步
				w7_battery_level = (int)(char)(msg[5]);
				int ver_major = (int)(char)(msg[3] & 0x0F);
				int ver_minor = (int)(char)((msg[4] & 0xF0) >> 4);
				int ver_incremental = (int)(char)(msg[4] & 0x0F);
				sprintf(w7_firmware_version, "%d.%d.%d", ver_major, ver_minor, ver_incremental);
				printf("-------------- bat is %d\n", w7_battery_level);
				printf("-------------- ver is %s\n", w7_firmware_version);
			} else {
			}
		} else {
			log_debug("===== not MCU command\n");
		}
		// TODO:
		// SendMsgToMCU(data.buf+sizeof(MsgLen), MsgLen);
#ifndef HOST2
		SendMsgToMQTT((char*)(data.buf+sizeof(MsgLen)), MsgLen);
#endif
	}
        

        usleep(10*1000); 
    }  
      
    if(msgctl(msgid, IPC_RMID, 0) == -1)  
    {  
        fprintf(stderr, "msgctl(IPC_RMID) failed\n");  
        return -1;  
    }   

    return 0;
}

#if 0
int mqtt_mq_init(int key, (void*)callback(int)) {
	pthread_t mMsgId;
	pthread_create(&mMsgId, NULL, callback, key);
	pthread_exit(0);
}
#endif

void  *JMssagePro(void *p)
{
	pthread_detach(pthread_self());
	
	MessageRcv(1883);
	
	pthread_exit(0);
}

int CreateMsgThread()
{
	pthread_t mMsgId;

	pthread_create(&mMsgId, NULL, &JMssagePro, NULL);
	
	return 0;
}

int  Message_init()
{
   
   CreateMsgThread();
   
   return 0;
}

