
#ifndef __HL_MESSAGE_QUE_H__
#define __HL_MESSAGE_QUE_H__


#ifdef __cplusplus
extern "C"{
#endif /* _cplusplus */

#define MAX_MSG_LEN	128

typedef struct   
{  
    long msg_type;  
    unsigned char  buf[MAX_MSG_LEN];  
}msg_st;


int  MessageRcv();
/*发送给MQTT处理进程的消息队列*/
int  MqttQMsgSend(unsigned char *cmd, unsigned char msgLen);  
/*发送给FaceLoop处理进程的消息队列*/
int  FaceQMsgSend(unsigned char *cmd, unsigned char msgLen);
int  Message_init();

#ifdef __cplusplus
}
#endif /* _cplusplus */

#endif

