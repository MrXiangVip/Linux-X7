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

#ifndef __FACE_DB_H__
#define __FACE_DB_H__

#include "config.h"
#include "sqlite3.h"
#include <stdint.h>

using namespace std;

typedef struct face_feature {
	int32_t version;
	int32_t face_id;
	int32_t roll;
	int32_t pitch;
	int32_t yaw;
	int32_t isliving;
	int32_t gender; // 1 - Male;
	int32_t gender_score;
	int32_t age;
	int32_t age_score;
	int32_t isblink;
	uint8_t *feature;
	uint32_t feature_len;
	int32_t quality;
} face_feature_t;

typedef int32_t (*face_db_compare)(const face_feature_t *, const face_feature_t *);

typedef struct face_feature_compare {
	face_feature_t *in_face;
	face_db_compare compare_func;
} face_feature_compare_t;

/* Face db init */
sqlite3 *face_db_init(char *db_path);

/* Face db deinit*/
int32_t face_db_deinit(sqlite3 *face_db);

/* Alloc face id */
int32_t face_db_alloc_faceid(sqlite3 *face_db);

/* Face db insert API */
int32_t face_db_insert_face(sqlite3 *face_db, face_feature_t *face);

/* Face db update API */
int32_t face_db_update_face(sqlite3 *face_db, face_feature_t *face);

/* Face db delete API */
int32_t face_db_delete_face(sqlite3 *face_db, face_feature_t *face);

/* Check matched face */
int32_t face_db_check_matched_face(sqlite3 *face_db, const face_feature_t *in_face, face_feature_t *out_face,
									face_db_compare compare_func);

#endif
