#include <unistd.h>  
#include <stdlib.h>  
#include <stdio.h>  
#include <string.h>  
#include <sys/msg.h>  
#include <errno.h>  
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "hl_MsgQueue.h"  
#define CRC16_LEN			2  
#define HEAD_MARK_MQTT		0x24     /*  ‘#’*/

/*发送给MQTT处理进程的消息队列*/
int  MqttQMsgSend(unsigned char *cmd, unsigned char msgLen)  
{  
    msg_st data; 
    int msgid = -1;
 
    msgid = msgget((key_t)1883, 0666 | IPC_CREAT);  
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

/*发送给FaceLoop处理进程的消息队列*/
int  FaceQMsgSend(unsigned char *cmd, unsigned char msgLen)  
{  
    msg_st data; 
    int msgid = -1;
 
    msgid = msgget((key_t)1234, 0666 | IPC_CREAT);  
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

int  MessageRcv()  
{    
    int msgid = -1;  
    msg_st data;  
    long msgtype = 1;
    unsigned char MsgLen = 0;
  
    msgid = msgget((key_t)1234, 0666 | IPC_CREAT);  
    if(msgid == -1)  
    {  
        fprintf(stderr, "msgget failed with error: %d\n", errno);  
        return -1; 
    }  
 
    while(1)  
    {  
        if(msgrcv(msgid, (void*)&data, MAX_MSG_LEN+sizeof(MsgLen), msgtype, 0) == -1)  
        {  
            fprintf(stderr, "msgrcv failed with errno: %d\n", errno);  
            usleep(50*1000);
            continue;
        }  
	else
	{
		MsgLen = data.buf[0];
	#if 0
		int i = 1;
		printf("\n<<<rcv mq<len:%d>:", MsgLen);	
		for(i; i<=MsgLen; i++)	
		{		
			printf("0x%02x	 ", data.buf[i]); 
		}	
		printf("\n");
	#endif
		SendMsgToMCU(data.buf+sizeof(MsgLen), MsgLen);
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

int  MessageRcvFromMqtt()  
{    
    int msgid = -1;  
    msg_st data;  
    long msgtype = 1;
    unsigned char MsgLen = 0;
	unsigned short  Msg_CRC16 = 0,Cal_CRC16= 0;
	const unsigned char *pszMsgInfo=NULL;	
	unsigned char CmdId=0;
	unsigned char HeadMark;
  
    msgid = msgget((key_t)1235, 0666 | IPC_CREAT);  
    if(msgid == -1)  
    {  
        fprintf(stderr, "msgget failed with error: %d\n", errno);  
        return -1; 
    }  
 
    while(1)  
    {  
        if(msgrcv(msgid, (void*)&data, MAX_MSG_LEN+sizeof(MsgLen), msgtype, 0) == -1)  
        {  
            fprintf(stderr, "msgrcv failed with errno: %d\n", errno);  
            usleep(50*1000);
            continue;
		}  
		else
		{
			MsgLen = data.buf[0];
			int i = 1;
			printf("\n<<<1235 rcv mq<len:%d>:", MsgLen);	
			// unsigned char msgData[20];
#if 1
			for(i; i<=MsgLen; i++)	
			{		
				printf("%d 0x%02x\n", i, data.buf[i]); 
				// msgData[i-1] = data.buf[i];
			}	
#endif
			printf("\n");
			//校验处理
			printf("0x%02x\n", data.buf[MsgLen - CRC16_LEN + 1]);
			printf("0x%02x\n", data.buf[MsgLen - CRC16_LEN + 2]);
			printf("----- crc is 0x%02x\n", Msg_CRC16);
			memcpy(&Msg_CRC16, &(data.buf[MsgLen-CRC16_LEN + 1]), CRC16_LEN);
			printf("----- crc is 0x%02x\n", Msg_CRC16);
			// Cal_CRC16 = CRC16_X25(msgData, MsgLen-CRC16_LEN);
			Cal_CRC16 = CRC16_X25(&(data.buf[1]), MsgLen-CRC16_LEN);
			printf("----- crc is 0x%02X \n", Cal_CRC16);
			if(Msg_CRC16 != Cal_CRC16)
			{
				fprintf(stderr, "msg CRC error: Msg_CRC16<0x%02X>, Cal_CRC16<0x%02X>!\n", Msg_CRC16, Cal_CRC16);	
				goto ERROR;
			}
			fprintf(stderr, "msg CRC succeed\n");

			pszMsgInfo = MsgHead_Unpacket(
					&(data.buf[1]),
					MsgLen,
					&HeadMark,
					&CmdId,
					&MsgLen);
			if(HeadMark != HEAD_MARK_MQTT)
			{
				fprintf(stderr, "msg headmark error: 0x%x!\n", HeadMark);			
				goto ERROR;
			}
			/*处理消息*/
			ProcMessageFromMQTT(CmdId,
					MsgLen,
					pszMsgInfo);
			// ProcMessage(data.buf+sizeof(MsgLen), MsgLen);
ERROR:				
			usleep(20000);  //sleep 20ms
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



void  *JMssagePro()
{
	pthread_detach(pthread_self());
	
	MessageRcv();
	
	pthread_exit(0);
}

void  *JMssageProFromMqtt()
{
	pthread_detach(pthread_self());
	
	MessageRcvFromMqtt();
	
	pthread_exit(0);
}

int CreateMsgThread()
{
	pthread_t mMsgId;
	pthread_t mMsgIdFromMqtt;

	pthread_create(&mMsgId, NULL, &JMssagePro, NULL);
	pthread_create(&mMsgIdFromMqtt, NULL, &JMssageProFromMqtt, NULL);
	
	return 0;
}

int  Message_init()
{
   
   CreateMsgThread();
   
   return 0;
}

