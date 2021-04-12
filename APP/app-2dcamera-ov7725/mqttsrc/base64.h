#ifndef _BASE64_H_
#define _BASE64_H_


#ifdef __cplusplus
extern "C" {
#endif
int base64_decode( const char * base64, char * bindata );
char* base64_encode( char * bindata, char * base64, int binlength);
int pubImage(const char* pub_topic, const char *filename, const char*msgId, const char* mac);
unsigned short CRC16_X25(unsigned char *puchMsg, unsigned int usDataLen);
#ifdef __cplusplus
}
#endif

#endif
