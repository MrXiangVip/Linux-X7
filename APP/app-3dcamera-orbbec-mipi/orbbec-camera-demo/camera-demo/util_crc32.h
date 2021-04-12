/*
 * crc32.h
 * See linux/lib/crc32.c for license and changes
 */
#ifndef _LINUX_CRC32_H
#define _LINUX_CRC32_H

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/types.h>
#include <asm/types.h>

/* #include <linux/bitrev.h> */

#ifdef __cplusplus
extern "C" {
#endif

uint32_t util_crc32 (uint32_t crc, const unsigned char *p, size_t len);
/* extern u32  crc32_be(unsigned int crc, unsigned char const *p, size_t len); */


/*
 * Helpers for hash table generation of ethernet nics:
 *
 * Ethernet sends the least significant bit of a byte first, thus crc32_le
 * is used. The output of crc32_le is bit reversed [most significant bit
 * is in bit nr 0], thus it must be reversed before use. Except for
 * nics that bit swap the result internally...
 */
/* #define ether_crc(length, data)    bitrev32(crc32_le(~0, data, length)) */
/* #define ether_crc_le(length, data) crc32_le(~0, data, length) */

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_CRC32_H */
