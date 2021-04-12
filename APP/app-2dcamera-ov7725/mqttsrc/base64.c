#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "mqtt-api.h"
#include "base64.h"
#include "log.h"

#define MQTT_PUB_PACKAGE_LEN 126
#define log_info 

const char * base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char * base64_encode( char * bindata, char * base64, int binlength )
{
	int i, j;
	unsigned char current;
	log_trace("-- 0 %s binlength is %d\n", base64, binlength);

	for ( i = 0, j = 0 ; i < binlength ; i += 3 )
	{
		current = (bindata[i] >> 2) ;
		current &= (unsigned char)0x3F;
		base64[j++] = base64char[(int)current];

		current = ( (unsigned char)(bindata[i] << 4 ) ) & ( (unsigned char)0x30 ) ;
		if ( i + 1 >= binlength )
		{
			base64[j++] = base64char[(int)current];
			base64[j++] = '=';
			base64[j++] = '=';
			break;
		}
		current |= ( (unsigned char)(bindata[i+1] >> 4) ) & ( (unsigned char) 0x0F );
		base64[j++] = base64char[(int)current];

		current = ( (unsigned char)(bindata[i+1] << 2) ) & ( (unsigned char)0x3C ) ;
		if ( i + 2 >= binlength )
		{
			base64[j++] = base64char[(int)current];
			base64[j++] = '=';
			break;
		}
		current |= ( (unsigned char)(bindata[i+2] >> 6) ) & ( (unsigned char) 0x03 );
		base64[j++] = base64char[(int)current];

		current = ( (unsigned char)bindata[i+2] ) & ( (unsigned char)0x3F ) ;
		base64[j++] = base64char[(int)current];
	}
	base64[j] = '\0';
	return base64;
}

int base64_decode( const char * base64, char * bindata )
{
	int i, j;
	unsigned char k;
	unsigned char temp[4];
	for ( i = 0, j = 0; base64[i] != '\0' ; i += 4 )
	{
		memset( temp, 0xFF, sizeof(temp) );
		for ( k = 0 ; k < 64 ; k ++ )
		{
			if ( base64char[k] == base64[i] )
				temp[0]= k;
		}
		for ( k = 0 ; k < 64 ; k ++ )
		{
			if ( base64char[k] == base64[i+1] )
				temp[1]= k;
		}
		for ( k = 0 ; k < 64 ; k ++ )
		{
			if ( base64char[k] == base64[i+2] )
				temp[2]= k;
		}
		for ( k = 0 ; k < 64 ; k ++ )
		{
			if ( base64char[k] == base64[i+3] )
				temp[3]= k;
		}

		bindata[j++] = ((unsigned char)(((unsigned char)(temp[0] << 2))&0xFC)) |
			((unsigned char)((unsigned char)(temp[1]>>4)&0x03));
		if ( base64[i+2] == '=' )
			break;

		bindata[j++] = ((unsigned char)(((unsigned char)(temp[1] << 4))&0xF0)) |
			((unsigned char)((unsigned char)(temp[2]>>2)&0x0F));
		if ( base64[i+3] == '=' )
			break;

		bindata[j++] = ((unsigned char)(((unsigned char)(temp[2] << 6))&0xF0)) |
			((unsigned char)(temp[3]&0x3F));
	}
	return j;
}

void get_extension(const char *file_name,char *extension)
{
	int i=0,length;
	length=strlen(file_name);
	while(file_name[i])
	{
		if(file_name[i]=='.')
			break;
		i++;
	}
	if(i<length)
		strcpy(extension,file_name+i+1);
	else
		strcpy(extension,"\0");
}

int pubImage(const char* pub_topic, const char *filename, const char *msgId, const char *mac) {	
	//打开图片
	FILE *fp = NULL;
	unsigned int size;         //图片字节数
	char *buffer;
	char *buffer1;
	size_t result;
	char *ret1; 
	unsigned int length;
	char file_extension[20];
	get_extension(filename, file_extension);

	fp = fopen(filename, "rb");
	if (NULL == fp)
	{
		printf("open_error");
		exit(1);
	}

	//获取图片大小
	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	//分配内存存储整个图片
	buffer = (char *)malloc((size/4+1)*4);
	if (NULL == buffer)
	{
		printf("memory_error");
		exit(2);
	}

	//将图片拷贝到buffer
	result = fread(buffer, 1, size, fp);
	if (result != size)  
	{  
		printf("reading_error");  
		exit (3);  
	}  
	fclose(fp);

	//base64编码
	buffer1 = (char *)malloc((size/3+1)*4 + 1);
	if (NULL == buffer1)
	{
		printf("memory_error");
		exit(2);
	}
	ret1 = base64_encode(buffer, buffer1, size);
	length = strlen(buffer1);
	// printf("%d ------- %s\n", length, buffer1);

	int count = (length + MQTT_PUB_PACKAGE_LEN - 1) / MQTT_PUB_PACKAGE_LEN;
	int fresult = 0;
	for (int i = 0; i < count; i++) {
		int len = MQTT_PUB_PACKAGE_LEN;
		if (i == (count - 1)) {
			len = length - MQTT_PUB_PACKAGE_LEN * (count - 1);
		}
		char data[MQTT_PUB_PACKAGE_LEN + 1];
		char pub_msg[MQTT_PUB_PACKAGE_LEN + 100];
		memset(data, '\0', MQTT_PUB_PACKAGE_LEN + 1);
		memset(pub_msg, '\0', MQTT_PUB_PACKAGE_LEN + 100);
		strncpy(data, buffer1 + (i*MQTT_PUB_PACKAGE_LEN), len);
		// sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"f\\\":\\\"%s\\\"\\,\\\"t\\\":\\\"%d\\\"\\,\\\"i\\\":\\\"%d\\\"\\,\\\"d\\\":\\\"%s\\\"}", msgId, mac, file_extension, count, i+1, data);

		// sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"t\\\":\\\"%d\\\"\\,\\\"i\\\":\\\"%d\\\"\\,\\\"d\\\":\\\"%s\\\"}", msgId, count, i+1, data);
		// TODO: 倒数第二个参数可能需要导入，0代表注册图片，1代表开锁图片
		sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"t\\\":%d\\,\\\"i\\\":%d\\,\\\"p\\\":%d\\,\\\"d\\\":\\\"%s\\\"}", msgId, count, i+1, 0, data);
#ifndef BASEAPP
		// TODO:
		int ret = quickPublishMQTT(pub_topic, pub_msg);
		if (ret != 0) {
			fresult = -1;
			break;
		}
#endif
	}

	free(buffer);
	free(buffer1);
	return fresult;
}


/********************CRC16 X25***************************************/
static void InvertUint8(unsigned char *dBuf,unsigned char *srcBuf)
{   
	int i;
	unsigned char tmp[4];
	tmp[0] = 0;
	for(i=0;i< 8;i++) {
		if(srcBuf[0]& (1 << i)){
			tmp[0]|=1<<(7-i);
		}
	}
	dBuf[0] = tmp[0];
}

static void InvertUint16(unsigned short *dBuf,unsigned short *srcBuf)
{
	int i;
	unsigned short tmp[4];
	tmp[0] = 0;
	for(i=0;i< 16;i++) {
		if(srcBuf[0]& (1 << i))
			tmp[0]|=1<<(15 - i);
	}
	dBuf[0] = tmp[0];
}


unsigned short CRC16_X25(unsigned char *puchMsg, unsigned int usDataLen)
{
	unsigned short wCRCin = 0xFFFF;
	unsigned short wCPoly = 0x1021;
	unsigned char wChar = 0;

	while (usDataLen--)
	{
		wChar = *(puchMsg++);
		InvertUint8(&wChar,&wChar);
		wCRCin ^= (wChar << 8);
		for(int i = 0;i < 8;i++)
		{
			if(wCRCin & 0x8000)
				wCRCin = (wCRCin << 1) ^ wCPoly;
			else
				wCRCin = wCRCin << 1;
		}
	}
	InvertUint16(&wCRCin,&wCRCin);
	return (wCRCin^0xFFFF) ;
}


#ifdef BASEAPP
int main() {
#if 0
	char *response="h";
	int len = 1;
	// 指令执行结果上报，需要base64编码
	int base64_msg_len = (len / 3 + 1)*4 + 1;
	char *base64_msg = (char*)malloc(base64_msg_len);
	printf("before base64_encode len is %d baselen is %d\n", len, base64_msg_len);
	for (int i = 0; i < len; i++) {
		printf("0x%02x ", response[i]);
	}
	printf("end\n");
	char *tmp = base64_encode(response, base64_msg, len);
	printf("base64_encode original len is %d encoded msg is %s\n", len, base64_msg);
	return 0;
#endif

	// pubImage("testtopic", "tc2.jpg");
	char buf[13];
	buf[0] = 0x23;
	buf[1] = 0x83;
	buf[2] = 0x08;
#if 0
	buf[3] = 0x00;
	buf[4] = 0x00;
	buf[5] = 0x00;
	buf[6] = 0x00;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0x00;
	buf[10] = 0x00;
#else 
	// facerecg
	buf[3] = 0x66;
	buf[4] = 0x61;
	buf[5] = 0x63;
	buf[6] = 0x65;
	buf[7] = 0x72;
	buf[8] = 0x65;
	buf[9] = 0x63;
	buf[10] = 0x67;
#endif

	unsigned short cal_crc16 = CRC16_X25(buf, 11);
	buf[11] = (char)cal_crc16;
	buf[12] = (char)(cal_crc16 >> 8);
	printf("%d 0x%02x 0x%02x\n", cal_crc16, (unsigned char)buf[11], (unsigned char)buf[12]);
			        

	char buf1[30];
	memset(buf1, '\0', 30);
	base64_encode(buf, buf1, 13);
	printf("data is %s\n", buf1);

	buf[0] = 0x23;
	buf[1] = 0x15;
	buf[2] = 0x11;
	buf[3] = 0x00;
	buf[4] = 0x60;
	buf[5] = 0x17;
	buf[6] = 0x05;
	buf[7] = 0x2a;
	buf[8] = 0x70;
	buf[9] = 0x17;
	buf[10] = 0x05;
	buf[11] = 0x2a;
	buf[12] = 0x31;
	buf[13] = 0x32;
	buf[14] = 0x33;
	buf[15] = 0x34;
	buf[16] = 0x35;
	buf[17] = 0x36;
	buf[18] = 0x37;
	buf[19] = 0x38;
	cal_crc16 = CRC16_X25(buf, 20);
	buf[20] = (char)cal_crc16;
	buf[21] = (char)(cal_crc16 >> 8);
	printf("%d 0x%02x 0x%02x\n", cal_crc16, (unsigned char)buf[20], (unsigned char)buf[21]);
	char buf11[40];
	memset(buf11, '\0', 40);
	base64_encode(buf, buf11, 22);
	printf("data is %s\n", buf11);

	memset(buf, '\0', 30);
	buf[0] = 0x24;
	buf[1] = 0x09;
	buf[2] = 0x08;
	buf[3] = 0xFF;
	buf[4] = 0xFF;
	buf[5] = 0xFF;
	buf[6] = 0xFF;
	buf[7] = 0xFF;
	buf[8] = 0xFF;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	char buf5[30];
	memset(buf5, '\0', 30);
	base64_encode(buf, buf5, 13);
	printf("buf5 data is %s\n", buf5);
	

	char buf2[13];
	memset(buf2, '\0', 13);
	int ret = base64_decode(buf1, buf2);
	printf("ret is %d\n", ret);
	for (int i = 0; i < 13; i++) {
		printf("%d 0x%02x\n", i, (unsigned char)buf2[i]);
	}

	char buf3[22];
	memset(buf3, '\0', 22);
	char *buf4 = "IxURAGANmYBgNngAsvXAX8+ihiuVuw==";
	ret = base64_decode(buf4, buf3);
	for (int i = 0; i < 22; i++) {
		printf("%d 0x%02x\n", i, (unsigned char)buf3[i]);
	}
}
#endif
