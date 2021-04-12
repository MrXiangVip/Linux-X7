  /*
 * util_crc16.h
 */
#ifndef _LINUX_CRC16_H
#define _LINUX_CRC16_H

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/types.h>
#include <asm/types.h>

/* #include <linux/bitrev.h> */

#ifdef __cplusplus
extern "C" {
#endif

uint16_t crc16_ccitt(const void *buf, int len);

uint16_t crc16_ccitt_continue(uint16_t init_crc,const void *buf, int len);

unsigned short CRC16_X25(unsigned char *puchMsg, unsigned int usDataLen);


#ifdef __cplusplus
}
#endif

#endif /* _LINUX_CRC16_H */
