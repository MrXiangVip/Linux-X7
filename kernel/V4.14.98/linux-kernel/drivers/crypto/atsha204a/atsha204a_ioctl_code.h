/* atsha204a_ioctl_code.h
 *
 * Copyright (C) 2016 SENSETIME, Inc.
 *
 */
 
#define ATSHA204A_ZONE_CONFIG              (  0x00)      //!< Configuration zone
#define ATSHA204A_ZONE_OTP                 (  0x01)      //!< OTP (One Time Programming) zone
#define ATSHA204A_ZONE_DATA                (  0x02)      //!< Data zone
#define ATSHA204A_ZONE_MASK                (  0x03)      //!< Zone mask
#define ATSHA204A_ZONE_COUNT_FLAG          (  0x80)      //!< Zone bit 7 set: Access 32 bytes, otherwise 4 bytes.
#define ATSHA204A_ZONE_ACCESS_4            (     4)      //!< Read or write 4 bytes.
#define ATSHA204A_ZONE_ACCESS_32           (    32)      //!< Read or write 32 bytes.
#define ATSHA204A_ADDRESS_MASK_CONFIG      (0x001F)      //!< Address bits 5 to 7 are 0 for Configuration zone.
#define ATSHA204A_ADDRESS_MASK_OTP         (0x000F)      //!< Address bits 4 to 7 are 0 for OTP zone.
#define ATSHA204A_ADDRESS_MASK             (0x007F)      //!< Address bit 7 to 15 are always 0.

#define ATSHA204A_SN_CMD                     (0x10)       //!< get atsha204a sn 

#define ATSHA204A_NONCE_CMD                  (0x11)       //!< atsha204a run nonce command 

#define ATSHA204A_MAC_CMD                    (0x12)       //!< atsha204a run mac command 

#define ATSHA204A_WRITE_CMD                  (0x13)       //!< atsha204a run writer command 

#define ATSHA204A_READ_CMD                   (0x14)       //!< atsha204a run read command 

struct atsha204a_sn {
	unsigned char  out_response[9];
};

struct atsha204a_read {
	unsigned char in_zone;
	unsigned char in_word_addr;
	unsigned char out_response[32];
};


struct atsha204a_nonce {
	unsigned char in_mode;
	unsigned char in_numin[20];
	unsigned char out_response[32];
};

struct atsha204a_mac {
	unsigned char in_mode;
	unsigned char in_slot;
	unsigned char in_challenge_len;
	unsigned char in_challenge[32];
	unsigned char out_response[32];
};
