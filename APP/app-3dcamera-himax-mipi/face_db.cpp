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
#include "libsimplelog.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite3.h>
#include "config.h"
#include "face_alg.h"
#include "face_db.h"

#define FACE_DB_INIT_CMD 				"CREATE TABLE "         CFG_FACE_DB_NAME " ("  \
			"ID INT PRIMARY KEY     NOT NULL," \
			"FEATURE        BLOB    NOT NULL);"
#define FACE_DB_INSERT_CMD 				"INSERT into "          CFG_FACE_DB_NAME " (ID,FEATURE) VALUES (%d, ?);"
#define FACE_DB_UPDATE_CMD 				"UPDATE"  CFG_FACE_DB_NAME " set FEATURE = ? where ID=%d;"
#define FACE_DB_DELETE_CMD				"DELETE from "          CFG_FACE_DB_NAME " where ID=%d;"
#define FACE_DB_SELECT_ALL_CMD			"SELECT * from "        CFG_FACE_DB_NAME
#define FACE_DB_SELECT_FACEID_CMD		"SELECT ID from "       CFG_FACE_DB_NAME
#define FACE_DB_SELECT_COUNT_CMD		"SELECT COUNT(*) from " CFG_FACE_DB_NAME
#define FACE_DB_SELECT_MAX_FACEID_CMD	"SELECT MAX(ID) from "  CFG_FACE_DB_NAME

static uint32_t cur_face_id = 0;
static char sql_cmd[100] = { 0 };

static int32_t face_db_get_max_faceid_callback(void *out_id, int32_t argc, char **argv, char **azColName)
{
	int32_t *tmp = (int32_t *)out_id;
	int32_t max_id = atoi(argv[0]);

	*tmp = max_id;

	return 0;
}

static int32_t face_db_get_row_cnts_callback(void *out_cnts, int32_t argc, char **argv, char **azColName)
{
	int32_t *tmp = (int32_t *)out_cnts;
	int32_t row_cnts = atoi(argv[0]);

	*tmp = row_cnts;

	return 0;
}

/* Return:
 * 1 - Emtpy
 * 0 - Non-empty
 * */
static int32_t face_db_check_if_empty(sqlite3 *pface_db)
{
	int32_t rc = 0;
	int32_t row_cnts = 0;
	char *zErrMsg = 0;

	rc = sqlite3_exec(pface_db, FACE_DB_SELECT_COUNT_CMD,
			face_db_get_row_cnts_callback, (void *)&row_cnts, &zErrMsg);
	if (rc != SQLITE_OK) {
		/* The faceDB is empty */
		log_error("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return RET_FAIL;
	} else {
		log_debug("DB Row Counts: %d\n", row_cnts);
		return row_cnts ? 0 : 1;
	}
}

static int32_t face_db_get_max_faceid(sqlite3 *pface_db)
{
	int32_t rc = 0;
	int32_t tmp_id = 0;
	char *zErrMsg = 0;

	rc = sqlite3_exec(pface_db, FACE_DB_SELECT_MAX_FACEID_CMD,
			face_db_get_max_faceid_callback, (void *)&tmp_id, &zErrMsg);
	if (rc != SQLITE_OK) {
		log_error("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return 0;
	} else {
		log_debug("MAX Face ID in DB: %d\n", tmp_id);
		return tmp_id;
	}
}

/* Face db init */
sqlite3 *face_db_init(char *db_path)
{
	int32_t is_new = 0;
	char *zErrMsg = 0;
	int32_t rc = 0;
	sqlite3 *tmp_db = NULL;

	/* Check if database file exists*/
	if ((access(db_path, F_OK)) == -1)
		is_new = 1;

	/* Open database */
	rc = sqlite3_open(db_path, &tmp_db);
	if (rc) {
		log_error("Can't open database: %s\n", sqlite3_errmsg(tmp_db));
		return NULL;
	} else {
		log_info("Open database successfully\n");
	}

	/* Create SQL statement */
	if (1 == is_new) {
		rc = sqlite3_exec(tmp_db, FACE_DB_INIT_CMD,
							NULL, 0, &zErrMsg);
		if (SQLITE_OK != rc) {
			log_error("SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			return NULL;
		} else {
			log_info("SQL Table created successfully\n");
		}
		cur_face_id = 0;
	} else {
		if (face_db_check_if_empty(tmp_db)) {
			cur_face_id = 0;
		} else {
			cur_face_id = face_db_get_max_faceid(tmp_db);
		}
	}
	return tmp_db;
}

int32_t face_db_deinit(sqlite3 *pface_db)
{
	if (pface_db) {
		sqlite3_close(pface_db);
		cur_face_id = 0;
	}
	return RET_OK;
}

int32_t face_db_alloc_faceid(sqlite3 *pface_db)
{
	return ++cur_face_id;
}

/* Face db insert API */
int32_t face_db_insert_face(sqlite3 *pface_db, face_feature_t *face)
{
	int32_t rc = 0;
	int32_t db_ret = RET_OK;
	sqlite3_stmt *stmt = NULL;

	sprintf(sql_cmd, FACE_DB_INSERT_CMD, face->face_id);
	/* Execute SQL statement */
	rc = sqlite3_prepare_v2(pface_db, sql_cmd, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		log_error("SQL prepare failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		return RET_FAIL;
	}
	rc = sqlite3_bind_blob(stmt, 1, face->feature, (int32_t)face->feature_len, NULL);
	if (rc != SQLITE_OK) {
		log_error("SQL bind blob failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		db_ret = RET_FAIL;
		goto err_rtn;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		log_error("SQL step failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		db_ret = RET_FAIL;
	}
err_rtn:
	rc = sqlite3_finalize(stmt);
	if (rc != SQLITE_OK) {
		log_error("SQL finalize failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		db_ret = RET_FAIL;
	}
	return db_ret;
}

/* Face db update API */
int32_t face_db_update_face(sqlite3 *pface_db, face_feature_t *face)
{
	int32_t rc = 0;
	int32_t db_ret = RET_OK;
	sqlite3_stmt *stmt = NULL;

	sprintf(sql_cmd, FACE_DB_INSERT_CMD, face->face_id);
	/* Execute SQL statement */
	rc = sqlite3_prepare_v2(pface_db, sql_cmd, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		log_error("SQL prepare failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		return RET_FAIL;
	}
	rc = sqlite3_bind_blob(stmt, 1, face->feature, (int32_t)face->feature_len, NULL);
	if (rc != SQLITE_OK) {
		log_error("SQL bind blob failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		db_ret = RET_FAIL;
		goto err_rtn;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		log_error("SQL step failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		db_ret = RET_FAIL;
	}
err_rtn:
	rc = sqlite3_finalize(stmt);
	if (rc != SQLITE_OK) {
		log_error("SQL finalize failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		db_ret = RET_FAIL;
	}
	return db_ret;
}

/* Face db delete API */
int32_t face_db_delete_face(sqlite3 *pface_db, face_feature_t *face)
{
	int32_t rc = 0;
	char *zErrMsg = NULL;

	/* Execute SQL statement */
	memset(sql_cmd, 0, sizeof(sql_cmd));
	sprintf(sql_cmd, FACE_DB_DELETE_CMD, face->face_id);
	rc = sqlite3_exec(pface_db, sql_cmd, NULL, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		log_error("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return RET_FAIL;
	} else {
		log_info("Face %d deleted successfully\n", face->face_id);
		return RET_OK;
	}
}

/* Check if the face already exist */
/* Returen: > 0 - Face ID, 0 - Face not exists */
int32_t face_db_check_matched_face(sqlite3 *pface_db,
									const face_feature_t *in_face,
									face_feature_t *out_face,
									face_db_compare compare_func)
{
	int32_t rc = 0;
	int32_t ret_val = RET_OK;
	sqlite3_stmt *stmt = NULL;

	if (face_db_check_if_empty(pface_db)) {
		return RET_NO_RECO;
	}

	rc = sqlite3_prepare_v2(pface_db, FACE_DB_SELECT_ALL_CMD, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		log_error("SQL prepare failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		return RET_FAIL;
	}

	rc = sqlite3_step(stmt);
	while (rc != SQLITE_DONE && rc != SQLITE_OK) {
		int32_t face_id = sqlite3_column_int(stmt, 0);
		const void *feature_data = sqlite3_column_blob(stmt, 1);
		int32_t feature_len = sqlite3_column_bytes(stmt, 1);
		face_feature_t face_in_db;
		int32_t similar = 0;

		face_in_db.version = in_face->version;
		face_in_db.face_id = face_id;
		face_in_db.feature = (uint8_t *)feature_data;
		face_in_db.feature_len = feature_len;

		similar = (*compare_func)(&face_in_db, in_face);
		if (1 == similar) {
			ret_val = face_id;
			break;
		} else {
			ret_val = RET_NO_RECO;
		}
		rc = sqlite3_step(stmt);
	}
	rc = sqlite3_finalize(stmt);
	if (rc != SQLITE_OK) {
		log_error("SQL finalize failed: %d - %s!\n", rc, sqlite3_errmsg(pface_db));
		return RET_FAIL;
	}
	return ret_val;
}

