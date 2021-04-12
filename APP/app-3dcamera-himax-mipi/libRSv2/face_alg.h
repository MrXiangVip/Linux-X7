/*
 * Copyright 2018 NXP Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __FACE_ALG_H__
#define __FACE_ALG_H__

using namespace std;

#include "config.h"
#include <stdint.h>
#include "RSFaceSDK.h"


enum FACE_REG_TYPE
{
	CREAT_PERSON = 0,
	PERSON_ADD_FACE,
};

#define ROLL_ENABLE						0		//注册时是否需要注册Roll 角度
#define RegAddFaceTypeNUM				4
enum REG_ADD_FACE_TYPE
{
	ADDFACE_NULL = 0,
	PITCH_UP,
	PITCH_DOWN,
	YAW_LEFT,
	YAW_RIGHT,
#if ROLL_ENABLE
	ROLL_LEFT,
	ROLL_RIGHT,
#endif
};

enum RET_VALUE {
	RET_UNKOWN_ERR = -9,
	RET_ALREADY_IN_DATABASE = -8,
	RET_NO_ANGLE = -7,
	RET_NO_LIVENESS = -6,
	RET_NO_QUALITY = -5,
	RET_NO_FACE = -4,
	RET_NO_FEATURE = -3,
	RET_NO_RECO = -2,
	RET_FAIL = -1,
	RET_OK = 0,
	RET_PERSON_ADD_FACE = 1,
};

typedef struct face_alg_param {
	int32_t quality_flag;
	int32_t min_face_size_flag;
	int32_t min_face_size;
	int32_t thread_counts_flag;
	int32_t thread_counts;
	int32_t living_detect_flag;
	int32_t face_landarks_flag;
	int32_t blink_flag;
	int32_t age_gender_flag;
} face_alg_param_t;

typedef struct face_image_info {
	const uint8_t *data;
	int32_t width;
	int32_t height;
	int32_t stride;
} face_image_info_t;

/* Face library init */
int32_t face_alg_init(const face_alg_param *param, int AppType);

/* Face library deinit */
int32_t face_alg_deinit(void);

/* Face detect API, color image for recognize, depth image for liveness */
int32_t face_alg_detect(face_image_info_t *ir_image, face_image_info_t *depth_image, bool *pIsMouseOpen,  int *distance, int *pFaceNum);

/* Face register API, use color image to register face, including liveness check */
int32_t face_alg_register(face_image_info_t *ir_image, face_image_info_t *depth_image, int flag, rs_face *pRsFace, int RegAddFaceType);

RET_VALUE DeleteFaceID(int faceId);

RET_VALUE GetAlbumSizeFromDataBase(unsigned short *faceSize);

#endif
