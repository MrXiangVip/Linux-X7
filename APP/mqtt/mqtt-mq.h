
#ifndef _MQTT_MQ_H_
#define _MQTT_MQ_H_


#ifdef __cplusplus
extern "C"{
#endif /* _cplusplus */

#define MAX_MSG_LEN	128

extern int w7_battery_level;
extern char w7_firmware_version[10];

typedef struct   
{  
    long msg_type;  
    unsigned char  buf[MAX_MSG_LEN];  
}msg_st;


int  MessageRcv(int key);
int  MessageSend(int key, const char *cmd, unsigned char msgLen);
int  Message_init();

#ifdef __cplusplus
}
#endif /* _cplusplus */

#endif

