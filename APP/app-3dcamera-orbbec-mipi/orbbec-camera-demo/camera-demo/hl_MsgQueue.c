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

  
int  MessageSend(unsigned char *cmd, unsigned char msgLen)  
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
		SendMsgToRpMSG(data.buf+sizeof(MsgLen), MsgLen);
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

