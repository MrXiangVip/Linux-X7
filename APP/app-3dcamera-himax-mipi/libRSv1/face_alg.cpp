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

#include "face_alg.h"
#include "libsimplelog.h"
#include <string>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "face_db.h"
#include "per_calc.h"
#include "RSFaceSDK.h"
#include "img_dbg.h"

using namespace std;

#if CFG_FACE_DB_SQLITE3_SUPPORT
static uint8_t feature_str_buf[CFG_FACE_FEATURE_MAXLEN];
static rs_face_feature g_feature_1, g_feature_2;
static sqlite3 *face_db = NULL;
#endif
static face_alg_param_t facelib_param;
static rs_face *gpface_array = NULL;
static int32_t gface_count = 0;
static RSHandle ghlicence;
static RSHandle ghfaceRecognition;
static RSHandle ghfaceDetect;
static RSHandle ghfaceQuality;
#if (CFG_FACE_MONO_LIVENESS) || (CFG_FACE_BIN_LIVENESS)
static RSHandle ghlivenessDetect;
#endif

#if CFG_FACE_DB_SQLITE3_SUPPORT
/* Compare two faces
 * Return:
 * 1 - matched
 * 0 - not matched
 * */
static int32_t face_alg_compare_face(const face_feature_t *face1, const face_feature_t *face2)
{
	if (face1 && face2) {
		float similar = 0.0;
		//memset(&g_feature_1, 0, sizeof(rs_face_feature));
		//memset(&g_feature_2, 0, sizeof(rs_face_feature));
		g_feature_1.version = face1->version;
		memcpy(g_feature_1.feature, face1->feature, CFG_FACE_FEATURE_MAXLEN);
		g_feature_2.version = face2->version;
		memcpy(g_feature_2.feature, face2->feature, CFG_FACE_FEATURE_MAXLEN);

#if CFG_PER_TIME_CAL_EVERYSTEP
		per_calc_start();
#endif
		similar = rsRecognitionFaceVerification(ghfaceRecognition, &g_feature_1, &g_feature_2);
#if CFG_PER_TIME_CAL_EVERYSTEP
		per_calc_end();
		log_info("FeautreCompare: %d usec\n", per_calc_get_result());
#endif
		log_info("Face similar score: %d\n", (int32_t)similar);
		if (similar > facelib_param.confidence)
			return 1;
		else
			return 0;
	} else {
		log_error("Face is empty!\n");
		return RET_FAIL;
	}
}
#endif

#if (2 == CFG_FACE_LIB_LM_VER)
static char *face_alg_get_init_cfg_info(const char *file_path)
{
	int32_t file_fd = 0;
	struct stat stat_buf;
	char *tmp_buf_ptr = NULL;
	int32_t buf_len = 0;

	if (stat(file_path, &stat_buf)) {
		log_error("Unable to get file length\n");
		goto err_out;
	}
	buf_len= stat_buf.st_size;

	file_fd = open(file_path, O_RDONLY);
	if (file_fd < 0) {
		log_error("Unable to open file %s\n", file_path);
		goto err_out;
	}

	/* Allocate a memory for audio data */
	tmp_buf_ptr = (char *)malloc(buf_len);
	if (!tmp_buf_ptr) {
		log_error("Failed to allocate buffer for data\n");
		goto err_out;
	}

	if (buf_len != read(file_fd, tmp_buf_ptr, buf_len)) {
		log_error("Failed to read audio data to buffer\n");
		goto err_out;
	}

	return tmp_buf_ptr;
err_out:
	if (file_fd) {
		close(file_fd);
	}
	if (tmp_buf_ptr) {
		free(tmp_buf_ptr);
	}
	return NULL;
}

static int32_t face_alg_release_init_cfg_info(char *cfg_info)
{
	if (cfg_info) {
		free(cfg_info);
	}
	return RET_OK;
}
#endif

/* Face library init */
int32_t face_alg_init(const face_alg_param_t *param)
{
#if (2 == CFG_FACE_LIB_LM_VER)
	char *license_info = NULL;

	license_info = face_alg_get_init_cfg_info(CFG_FACE_LIB_LICENSE_PATH);
	if (NULL == license_info) {
		log_error("License file missing!\n");
		return RET_FAIL;
	}
	rsInitLicenseManagerV2(&ghlicence, license_info);
	face_alg_release_init_cfg_info(license_info);
#elif (1 == CFG_FACE_LIB_LM_VER)
	rsInitLicenseManager(&ghlicence,
			"f91d6d9bae59f8393d4634db027d7224",
			"72aa62f44418f21e43ba9321525b6fadb7aea743");
#else
	#error "No license manager version CFG_FACE_LIB_LM_VER defined!"
#endif
	if (ghlicence == NULL) {
		log_error("License falied\n");
		return RET_FAIL;
	}

	rsInitFaceDetect(&ghfaceDetect, ghlicence);
	if (ghfaceDetect == NULL) {
		log_error("rsInitFaceDetect falied\n");
		return RET_FAIL;
	}

#if CFG_FACE_DB_SQLITE3_SUPPORT
	rsInitFaceRecognition(&ghfaceRecognition, ghlicence, "no_use.db");
#else
	rsInitFaceRecognition(&ghfaceRecognition, ghlicence, CFG_READSENSE_DB_PATH);
#endif
	if (ghfaceRecognition == NULL) {
		log_error("rsInitFaceRecognition falied\n");
		return RET_FAIL;
	}

	rsRecognitionSetConfidence(ghfaceRecognition, param->confidence);

	if (param->quality_flag) {
		rsInitFaceQuality(&ghfaceQuality, ghlicence);
		if (ghfaceQuality == NULL) {
			log_error("rsInitFaceQuality falied\n");
			return RET_FAIL;
		}
	}

	if (param->living_detect_flag) {
#if CFG_FACE_MONO_LIVENESS
		rsInitLivenessDetect(&ghlivenessDetect, ghlicence);
		if (ghlivenessDetect == NULL) {
			log_error("rsInitFaceLivenessDetect falied\n");
			return RET_FAIL;
		}
#elif CFG_FACE_BIN_LIVENESS
		rsInitDepthLivenessDetect(&ghlivenessDetect, ghlicence);
		if (ghlivenessDetect == NULL) {
			log_error("rsInitDepthLivenessDetect falied\n");
			return RET_FAIL;
		}
#endif
		log_info("LIveness init successfully!\n");
	}
	memcpy(&facelib_param, param, sizeof(face_alg_param_t));

	log_info("ReadSense SDK init successfully!\n");

#if CFG_FACE_DB_SQLITE3_SUPPORT
	face_db = face_db_init((char *)CFG_FACE_DB_PATH);
	return (face_db ? RET_OK : RET_FAIL);
#else
	return RET_OK;
#endif
}

int32_t face_alg_deinit(void)
{
	if (ghfaceDetect != NULL) {
		rsUnInitFaceDetect(&ghfaceDetect);
		ghfaceDetect = NULL;
	}

	if (ghfaceRecognition != NULL) {
		rsUnInitFaceRecognition(&ghfaceRecognition);
		ghfaceRecognition = NULL;
	}

	if (ghfaceQuality != NULL) {
		rsUnInitFaceQuality(&ghfaceQuality);
		ghfaceQuality = NULL;
	}

#if CFG_FACE_MONO_LIVENESS
	if (ghlivenessDetect != NULL) {
		rsUnInitLivenessDetect(&ghlivenessDetect);
		ghlivenessDetect = NULL;
	}
#elif CFG_FACE_BIN_LIVENESS
	if (ghlivenessDetect != NULL) {
		rsUnInitDepthLivenessDetect(&ghlivenessDetect);
		ghlivenessDetect = NULL;
	}
#endif

	if (ghlicence) {
#if (2 == CFG_FACE_LIB_LM_VER)
		rsUnInitLicenseManagerV2(&ghlicence);
#elif (1 == CFG_FACE_LIB_LM_VER)
		rsUnInitLicenseManager(&ghlicence);
#endif
		ghlicence = NULL;
	}

#if CFG_FACE_DB_SQLITE3_SUPPORT
	face_db_deinit(face_db);
#endif

	return 0;
}

static int32_t face_alg_get_max_face_info(const face_image_info_t *ir_image, rs_face *out_face_info)
{
	int32_t detect_flag = 0;
	rs_face max_face;
	int32_t tmp_count = 0;

	/* Detect face */
#if CFG_PER_TIME_CAL_EVERYSTEP
	per_calc_start();
#endif
	detect_flag = rsRunFaceDetect(ghfaceDetect,
						ir_image->data,
						(rs_pixel_format)CFG_FACE_LIB_PIX_FMT,
						ir_image->width,
						ir_image->height,
						ir_image->stride,
						RS_IMG_CLOCKWISE_ROTATE_0,
						&gpface_array,
						&gface_count);
	if (-1 == detect_flag || gface_count <= 0) {
		releaseFaceDetectResult(gpface_array, gface_count);
		return RET_NO_FACE;
	}
	/* Calculate max face */
	max_face = gpface_array[0];
	if (gface_count > 1) {
		for (tmp_count = 1; tmp_count < gface_count; ++tmp_count) {
			if (gpface_array[tmp_count].rect.width > max_face.rect.width) {
				max_face = gpface_array[tmp_count];
			}
		}
	}
	
#if CFG_PER_TIME_CAL_EVERYSTEP
	per_calc_end();
	log_info("FaceDetectMax, OK: %d usec\n", per_calc_get_result());
#endif

	memcpy(out_face_info, &max_face, sizeof(rs_face));
	return RET_OK;
}

static int32_t face_alg_abstract_feature_from_info(const rs_face *in_face_info, face_feature_t *out_feature)
{
	if (in_face_info && out_feature) {
		out_feature->roll = (int32_t)(in_face_info->roll);
		out_feature->pitch = (int32_t)(in_face_info->pitch);
		out_feature->yaw   = (int32_t)(in_face_info->yaw);
		return RET_OK;
	} else {
		log_error("NULL pointer detected!\n");
		return RET_FAIL;
	}
}

#if CFG_FACE_DB_SQLITE3_SUPPORT
static int32_t face_alg_get_face_feature(const face_image_info_t *image, const rs_face *face_info, face_feature_t *out_face)
#else
static int32_t face_alg_get_face_feature(const face_image_info_t *image, const rs_face *face_info, rs_face_feature *out_face)
#endif
{
	uint32_t face_ret = 0;
#if CFG_FACE_DB_SQLITE3_SUPPORT
	rs_face_feature face_feature;
	rs_face_feature *pfacefeature = &face_feature;
#else
	rs_face_feature *pfacefeature = out_face;
#endif

	/* Get feature string */
#if CFG_PER_TIME_CAL_EVERYSTEP
	per_calc_start();
#endif
	face_ret = rsRecognitionGetFaceFeature(ghfaceRecognition,
						image->data,
						(rs_pixel_format)CFG_FACE_LIB_PIX_FMT,
						image->width,
						image->height,
						image->stride,
						(rs_face *)face_info,
						pfacefeature);
	if (face_ret < 0) {
		log_error("Get face feature failed: %d!", face_ret);
		releaseFaceDetectResult(gpface_array, gface_count);
		return RET_NO_FACE;
	}
#if CFG_PER_TIME_CAL_EVERYSTEP
	per_calc_end();
	log_info("GetFaceFeature: %d usec\n", per_calc_get_result());
#endif

#if CFG_FACE_DB_SQLITE3_SUPPORT
	out_face->face_id = -1;
	out_face->version = face_feature.version;
	memcpy(out_face->feature, (uint8_t *)face_feature.feature, CFG_FACE_FEATURE_MAXLEN);
	out_face->feature_len = CFG_FACE_FEATURE_MAXLEN;
#endif

	releaseFaceDetectResult(gpface_array, gface_count);

	return RET_OK;
}

static int32_t face_alg_check_face_liveness(face_image_info_t *img, rs_face *face)
{
	int32_t liveness_state[3] = { 0 };
	int32_t liveness_flag = 0;

#if 0
		static uint32_t grab_img = 1;
		if (0 == (grab_img % 5)) {
			static int32_t img_counter = 0;
			char str_file_name[100] = { 0 };
			FILE *fp = NULL;

			sprintf(str_file_name, "img_cap_%dx%d_%d.dep4", img->width, img->height, img_counter++);
			log_info("Image filename: %s\n", str_file_name);
			fp = fopen(str_file_name, "w+");
			fwrite(img->data, (img->width * img->height * 2), 1, fp);
			fclose(fp);
			//grab_img = 1;
		}
		grab_img++;
#endif

#if CFG_FACE_MONO_LIVENESS
	liveness_flag = rsRunLivenessDetect(ghlivenessDetect,
							img->data,
							(rs_pixel_format)CFG_FACE_LIB_PIX_FMT,
							img->width,
							img->height,
							img->stride,
							face,
							liveness_state);
#elif CFG_FACE_BIN_LIVENESS
	liveness_flag = rsRunDepthLivenessDetect(ghlivenessDetect,
							img->data,
							img->width,
							img->height,
							img->stride,
							face,
							liveness_state);
#endif
	if (0 != liveness_flag || 1 != liveness_state[0]) {
		releaseFaceDetectResult(gpface_array, gface_count);
		return RET_NO_LIVENESS;
	}
	return RET_OK;
}

static int32_t face_alg_get_face_quality(face_image_info_t *img, rs_face *face)
{
	return rsGetFaceQualityScore(ghfaceQuality,
							img->data,
							(rs_pixel_format)CFG_FACE_LIB_PIX_FMT,
							img->width,
							img->height,
							img->stride,
							face);
}

#if CFG_FACE_DB_SQLITE3_SUPPORT
	#if CFG_FACE_MONO_LIVENESS
static int32_t face_alg_check_face(face_image_info_t *ir_image, face_feature_t *out_feature)
	#elif CFG_FACE_BIN_LIVENESS
static int32_t face_alg_check_face(face_image_info_t *ir_image, face_image_info_t *depth_image, face_feature_t *out_feature)
	#endif
#else
	#if CFG_FACE_MONO_LIVENESS
static int32_t face_alg_check_face(face_image_info_t *ir_image, rs_face_feature *out_feature)
	#elif CFG_FACE_BIN_LIVENESS
static int32_t face_alg_check_face(face_image_info_t *ir_image, face_image_info_t *depth_image, rs_face_feature *out_feature)
	#endif
#endif
{
	int32_t db_ret = 0, alg_ret = 0;
	face_feature_t face_feature = { 0 };
	rs_face max_face_info;
	rs_face_feature rs_feature;
	rs_face_feature *ptmp_rs_feature = NULL;

	memset(&max_face_info, 0, sizeof(rs_face));
	alg_ret = face_alg_get_max_face_info(ir_image, &max_face_info);
	if (RET_OK == alg_ret) {
		face_alg_abstract_feature_from_info(&max_face_info, &face_feature);

		/* Check angel */
		if (facelib_param.face_landarks_flag) {
			log_debug("Landark: roll: %d, pitch: %d, yaw: %d\n", face_feature.roll, face_feature.pitch, face_feature.yaw);
			if ((abs(face_feature.roll) > CFG_FACE_ANGLE) ||
				(abs(face_feature.pitch) > CFG_FACE_ANGLE) ||
				(abs(face_feature.yaw) > CFG_FACE_ANGLE)) {
				log_debug("Angel check failed, roll:%d, pitch:%d, yaw:%d\n",
					face_feature.roll, face_feature.pitch, face_feature.yaw);
				return RET_NO_FACE;
			}
		}

		/* Check liveness */
		if (facelib_param.living_detect_flag) {
			log_debug("Doing liveness check\n");
#if CFG_FACE_MONO_LIVENESS
			if (RET_NO_LIVENESS == face_alg_check_face_liveness(ir_image, &max_face_info)) {
#elif CFG_FACE_BIN_LIVENESS
			if (RET_NO_LIVENESS == face_alg_check_face_liveness(depth_image, &max_face_info)) {
#endif
				return RET_NO_LIVENESS;
			}
			log_debug("Face liveness check pass!\n");
		}

		/* Check quality */
		if (facelib_param.quality_flag) {
			log_debug("Doing quality check\n");
			int32_t score = 0;
			score = face_alg_get_face_quality(ir_image, &max_face_info);
			log_info("Face Quality: %d\n", score * 10);
			if (score * 10 < CFG_FACE_QUALITY_THRESHHOLD) {
				log_info("Face quality too low: %d\n", score * 10);
				return RET_NO_QUALITY;
			}
		}

#if CFG_FACE_DB_SQLITE3_SUPPORT
		memset(&feature_str_buf, 0, sizeof(face_feature_t));
		face_feature.feature = feature_str_buf;
		alg_ret = face_alg_get_face_feature(ir_image, &max_face_info, &face_feature);
		if (RET_OK == alg_ret) {
			if (out_feature) {
				memcpy(out_feature, &face_feature, sizeof(face_feature_t));
			}
			db_ret = face_db_check_matched_face(face_db, &face_feature, NULL, face_alg_compare_face);
#else
		if (out_feature) {
			ptmp_rs_feature = out_feature;
		} else {
			ptmp_rs_feature = &rs_feature;
		}
		alg_ret = face_alg_get_face_feature(ir_image, &max_face_info, ptmp_rs_feature);
		if (RET_OK == alg_ret) {
			alg_ret = rsRecognitionFaceIdentification(ghfaceRecognition, ptmp_rs_feature);
			db_ret = (alg_ret < 0) ? RET_NO_RECO : alg_ret;
#endif
			return db_ret;
		} else {
			return alg_ret;		
		}
	} else {
		return alg_ret;
	}
}

/* Face detect API, use color image to detect liveness and face */
/* RET: > 0 face id, < 0 error */
#if CFG_FACE_MONO_LIVENESS
int32_t face_alg_detect(face_image_info_t *ir_image)
#elif CFG_FACE_BIN_LIVENESS
int32_t face_alg_detect(face_image_info_t *ir_image, face_image_info_t *depth_image)
#endif
{
	int32_t alg_ret = 0;

#if CFG_FACE_MONO_LIVENESS
	alg_ret = face_alg_check_face(ir_image, NULL);
#elif CFG_FACE_BIN_LIVENESS
	alg_ret = face_alg_check_face(ir_image, depth_image, NULL);
#endif
	if (RET_NO_RECO == alg_ret) {
		return RET_NO_RECO;
	} else if (RET_NO_LIVENESS == alg_ret) {
		return RET_NO_LIVENESS;
	} else if (RET_NO_FACE == alg_ret) {
		return RET_NO_FACE;
	} else if (alg_ret > 0) {
		log_info("Face matched, ID: %d\n", alg_ret);
		return alg_ret;
	} else {
		log_error("Check face failed: %d\n", alg_ret);
		return RET_FAIL;
	}
}

/* Face register API, use color image to register face */
/* RET: 0 success, < 0 error */
#if CFG_FACE_MONO_LIVENESS
int32_t face_alg_register(face_image_info_t *ir_image)
#elif CFG_FACE_BIN_LIVENESS
int32_t face_alg_register(face_image_info_t *ir_image, face_image_info_t *depth_image)
#endif
{
	int32_t alg_ret = 0;
#if CFG_FACE_DB_SQLITE3_SUPPORT
	face_feature_t face_feature = { 0 };
#else
	rs_face_feature face_feature = { 0 };
#endif

#if CFG_FACE_MONO_LIVENESS
	alg_ret = face_alg_check_face(ir_image, &face_feature);
#elif CFG_FACE_BIN_LIVENESS
	alg_ret = face_alg_check_face(ir_image, depth_image, &face_feature);
#endif

	if (RET_NO_RECO == alg_ret) {
#if 0
			{
			static int32_t img_counter = 0;
			char str_file_name[100] = { 0 };
			FILE *fp = NULL;

			sprintf(str_file_name, "img_cap_%d.nv12", img_counter++);
			cout << "Image filename: " << str_file_name << endl;
			fp = fopen(str_file_name, "w+");
			fwrite(ir_image->data, (CFG_IR_FRAME_WIDTH * CFG_IR_FRAME_HEIGHT * 3) >> 1, 1, fp);
			fclose(fp);
			}
#endif

#if CFG_FACE_DB_SQLITE3_SUPPORT
		/* Store to db */
		face_feature.face_id = face_db_alloc_faceid(face_db);
		if (face_db_insert_face(face_db, &face_feature)) {
			log_error("Insert face failed!\n");
			return RET_FAIL;
		} else {
			log_info("Face registerd, ID: %d\n", face_feature.face_id);
			return RET_OK;
		}
#else
		int32_t person_id = rsRecognitionPersonCreate(ghfaceRecognition, &face_feature);
		if (person_id > 0) {
			log_info("Face registerd, ID: %d\n", person_id);
			return RET_OK;
		} else {
			log_error("rsRecognitionPersonCreate failed!\n");
			return RET_FAIL;
		}
#endif
	} else if (RET_NO_LIVENESS == alg_ret) {
		return RET_NO_LIVENESS;
	} else if (RET_NO_FACE == alg_ret) {
		return RET_NO_FACE;
	} else if (RET_NO_QUALITY == alg_ret) {
		return RET_NO_QUALITY;
	} else if (alg_ret > 0) {
		log_info("Face already exists, ID: %d\n", alg_ret);
		return RET_ALREADY_IN_DATABASE;
	} else {
		log_error("Check face failed: %d\n", alg_ret);
		return RET_FAIL;
	}
}

