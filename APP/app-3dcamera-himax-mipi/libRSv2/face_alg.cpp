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
#include "../function.h"

using namespace std;
int person_id = -1;  //hannah move out for id transfer
static stSysCfg stSystemCfg;

static face_alg_param_t facelib_param;
static rs_face *gpface_array = NULL;
static int32_t gface_count = 0;
static RSHandle ghlicence;
static RSHandle ghfaceRecognition;
static RSHandle ghfaceDetect;
static RSHandle ghfaceQuality;
#if (CFG_FACE_BIN_LIVENESS)
static RSHandle ghlivenessDetect;
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
int32_t face_alg_init(const face_alg_param_t *param, int AppType)
{
	GetSysLocalCfg(&stSystemCfg);

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


	rsInitFaceRecognition(&ghfaceRecognition, ghlicence, CFG_READSENSE_DB_PATH);
	if (ghfaceRecognition == NULL) {
		log_error("rsInitFaceRecognition falied\n");
		return RET_FAIL;
	}

	if(FACE_REG == AppType)
	{
		rsRecognitionSetConfidence(ghfaceRecognition, stSystemCfg.face_reg_threshold_val);
	}
	else if(FACE_RECG == AppType)
	{
		rsRecognitionSetConfidence(ghfaceRecognition, stSystemCfg.face_recg_threshold_val);
	}
	else
	{
		rsRecognitionSetConfidence(ghfaceRecognition, FACE_THRESHOLD_VALUE);
	}

	if (param->quality_flag) {
		rsInitFaceQuality(&ghfaceQuality, ghlicence);
		if (ghfaceQuality == NULL) {
			log_error("rsInitFaceQuality falied\n");
			return RET_FAIL;
		}
	}

	if (param->living_detect_flag) {
#if CFG_FACE_BIN_LIVENESS
	#if MODE_3D_LIVENESS
		rsInitDepthLivenessDetect(&ghlivenessDetect, ghlicence);
		if (ghlivenessDetect == NULL) {
			log_error("rsInitDepthLivenessDetect falied\n");
			return RET_FAIL;
		}
	#endif
	#if MODE_MUTI_LIVENESS
		rsInitMutiLivenessDetect(&ghlivenessDetect, ghlicence);
		if (ghlivenessDetect == NULL) {
			log_error("rsInitDepthLivenessDetect falied\n");
			return RET_FAIL;
		}
	#endif
#endif
		log_info("LIveness init successfully!\n");
	}
	memcpy(&facelib_param, param, sizeof(face_alg_param_t));

	log_info("ReadSense SDK init successfully!\n");

	return RET_OK;
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

#if CFG_FACE_BIN_LIVENESS
	#if MODE_3D_LIVENESS
		if (ghlivenessDetect != NULL) {
			rsUnInitDepthLivenessDetect(&ghlivenessDetect);
			ghlivenessDetect = NULL;
		}
	#endif	
	#if MODE_MUTI_LIVENESS
		if (ghlivenessDetect != NULL) {
			rsUnInitMutiLivenessDetect(&ghlivenessDetect);
			ghlivenessDetect = NULL;
		}
	#endif
	
#endif

	if (ghlicence) {
#if (2 == CFG_FACE_LIB_LM_VER)
		rsUnInitLicenseManagerV2(&ghlicence);
#elif (1 == CFG_FACE_LIB_LM_VER)
		rsUnInitLicenseManager(&ghlicence);
#endif
		ghlicence = NULL;
	}

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
	max_face  = gpface_array[0];
	if (gface_count > 1) 
	{
		for (tmp_count = 1; tmp_count < gface_count; ++tmp_count) 
		{
			if (gpface_array[tmp_count].rect.width > max_face.rect.width) 
			{
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

static int32_t face_alg_get_face_feature(const face_image_info_t *image, const rs_face *face_info, rs_face_feature *out_face)
{
	uint32_t face_ret = 0;
	rs_face_feature *pfacefeature = out_face;

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
		return RET_NO_FEATURE;
	}
#if CFG_PER_TIME_CAL_EVERYSTEP
	per_calc_end();
	log_info("GetFaceFeature: %d usec\n", per_calc_get_result());
#endif

	releaseFaceDetectResult(gpface_array, gface_count);

	return RET_OK;
}

static int32_t face_alg_check_face_liveness(face_image_info_t *ir_image, face_image_info_t *depth_img, rs_face *face)
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

#if CFG_FACE_BIN_LIVENESS
	#if MODE_3D_LIVENESS
		liveness_flag = rsRunDepthLivenessDetect(ghlivenessDetect,
								(unsigned short *)depth_img->data,
								depth_img->width,
								depth_img->height,
								depth_img->stride,
								face,
								liveness_state);
	#endif
	#if MODE_MUTI_LIVENESS
		liveness_flag = rsRunMutiLivenessDetect(ghlivenessDetect,
								ir_image->data,
								(unsigned short *)depth_img->data,
								CFG_FACE_LIB_PIX_FMT,
								ir_image->width,
								ir_image->height,
								ir_image->stride,
								face,
								liveness_state);
	#endif
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

static int32_t face_alg_recg_check_face(face_image_info_t *ir_image, face_image_info_t *depth_image, rs_face_feature *out_feature, 
											int32_t face_angle, bool *pIsMouseOpen,  int *distance, int *pFaceNum)
{
	int32_t db_ret = 0, alg_ret = 0;
	face_feature_t face_feature = { 0 };
	rs_face max_face_info;
	rs_face_feature rs_feature;
	rs_face_feature *ptmp_rs_feature = NULL;

	memset(&max_face_info, 0, sizeof(rs_face));
	alg_ret = face_alg_get_max_face_info(ir_image, &max_face_info);
	if (RET_OK == alg_ret) 
	{
		face_alg_abstract_feature_from_info(&max_face_info, &face_feature);
		//get face count
		*pFaceNum = gface_count;

		/* Check angel */
		if (facelib_param.face_landarks_flag) 
		{
			log_debug("Landark: roll: %d, pitch: %d, yaw: %d\n", face_feature.roll, face_feature.pitch, face_feature.yaw);
			if ((abs(face_feature.roll) > face_angle) ||
				(abs(face_feature.pitch) > face_angle) ||
				(abs(face_feature.yaw) > face_angle)) 
			{
				log_debug("Angel check failed, roll:%d, pitch:%d, yaw:%d\n",
					face_feature.roll, face_feature.pitch, face_feature.yaw);
				return RET_NO_FACE;
			}
		}
		
		// open mouse detect
		*pIsMouseOpen = rsIsMouseOpen(ir_image->data,
							PIX_FORMAT_GRAY,
							ir_image->width,
							ir_image->height,
							ir_image->stride,
							&max_face_info);
		log_info("IsMouseOpen = %d	", *pIsMouseOpen);

#if CFG_FACE_BIN_LIVENESS
		//log_info("stride = %d	", depth_image->stride);
		*distance =  rsRunDepthFaceDistance(ghlivenessDetect, 
							(const	unsigned short *)depth_image->data,
							depth_image->width,
							depth_image->height,
							depth_image->width * 2,
							&max_face_info);
		log_info("distance = %d.\n", *distance); 

		/* Check liveness */
		if (facelib_param.living_detect_flag) 
		{
			log_debug("Doing liveness check\n");

			if (RET_NO_LIVENESS == face_alg_check_face_liveness(ir_image, depth_image, &max_face_info)) 
			{
				return RET_NO_LIVENESS;
			}
			log_debug("Face liveness check pass!\n");
		}
#endif

#if CFG_PER_TIME_CAL_EVERYSTEP
		per_calc_start();
#endif
		/* Check quality */
		if (facelib_param.quality_flag) 
		{
			log_debug("Doing quality check\n");
			int32_t score = 0;
			score = face_alg_get_face_quality(ir_image, &max_face_info);
			log_info("Face Quality: %d\n", score * 10);
			if (score  < CFG_FACE_QUALITY_THRESHHOLD) 
			{
				log_info("Face quality too low: %d\n", score);
				return RET_NO_QUALITY;
			}
		}
#if CFG_PER_TIME_CAL_EVERYSTEP
		per_calc_end();
		log_info("Face quality checkpass: %d usec\n", per_calc_get_result());
#endif

		if (out_feature) 
		{
			ptmp_rs_feature = out_feature;
		}
		else 
		{
			ptmp_rs_feature = &rs_feature;
		}
		alg_ret = face_alg_get_face_feature(ir_image, &max_face_info, ptmp_rs_feature);
		if (RET_OK == alg_ret) 
		{
#if CHECK_FACE_VERIFICATION		
			float conf = 0;
			conf = rsRecognitionGetConfidence(ghfaceRecognition);
			log_info("Face confidence: %f\n", conf);
#endif //CHECK_FACE_VERIFICATION
			alg_ret = rsRecognitionFaceIdentification(ghfaceRecognition, ptmp_rs_feature);
			db_ret = (alg_ret < 0) ? RET_NO_RECO : alg_ret;
			return db_ret;
		} 
		else 
		{
			return alg_ret;		
		}
	} 
	else 
	{
		return alg_ret;
	}
}

/* Face detect API, use color image to detect liveness and face */
/* RET: > 0 face id, < 0 error */
int32_t face_alg_detect(face_image_info_t *ir_image, face_image_info_t *depth_image, bool *pIsMouseOpen,  int *distance, int *pFaceNum)
{
	int32_t alg_ret = 0;

	alg_ret = face_alg_recg_check_face(ir_image, depth_image, NULL, (int32_t)stSystemCfg.face_recg_angle, pIsMouseOpen,  distance, pFaceNum);
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
int32_t face_alg_register(face_image_info_t *ir_image, face_image_info_t *depth_image, int flag, rs_face *pRsFace, int RegAddFaceType)
{
	int32_t db_ret = 0, alg_ret = 0;
	face_feature_t face_feature = { 0 };

	memset(pRsFace, 0, sizeof(rs_face));
	alg_ret = face_alg_get_max_face_info(ir_image, pRsFace);
	if (RET_NO_FACE == alg_ret) 
	{
		return RET_NO_FACE;
	}

	face_alg_abstract_feature_from_info(pRsFace, &face_feature);

	/* Check angel */
	if (facelib_param.face_landarks_flag) 
	{
		log_debug("Landark: roll: %d, pitch: %d, yaw: %d\n", face_feature.roll, face_feature.pitch, face_feature.yaw);
		
		if(flag == CREAT_PERSON)
		{
			if ((abs(face_feature.roll) > (int32_t)stSystemCfg.face_reg_angle) ||
				(abs(face_feature.pitch) > (int32_t)stSystemCfg.face_reg_angle) ||
				(abs(face_feature.yaw) > (int32_t)stSystemCfg.face_reg_angle)) 
			{
				log_debug("Angel check failed, roll:%d, pitch:%d, yaw:%d\n",
					face_feature.roll, face_feature.pitch, face_feature.yaw);
				return RET_NO_ANGLE;
			}
		}
		else
		{		
			switch(RegAddFaceType)
			{
			case PITCH_UP://抬头
				if(pRsFace->pitch > ADDFACE_REG_DETECT_MAX_ANGEL || pRsFace->pitch < ADDFACE_REG_DETECT_MIN_ANGEL ||
						abs(pRsFace->roll) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
						abs(pRsFace->yaw) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
				{
					log_info("####Info:RS_RET_NO_QUALITY####Face pitch: %f\n", pRsFace->pitch);
					return RET_NO_ANGLE;
				}
			break;
				
			case PITCH_DOWN://低头
				if(pRsFace->pitch > -ADDFACE_REG_DETECT_MIN_ANGEL || pRsFace->pitch < -ADDFACE_REG_DETECT_MAX_ANGEL ||
						abs(pRsFace->roll) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
						abs(pRsFace->yaw) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
				{
					log_info("####Info:RS_RET_NO_QUALITY####Face pitch: %f\n", pRsFace->pitch);
					return RET_NO_ANGLE;
				}
			break;
			
			case YAW_LEFT://脸偏左
				if(pRsFace->yaw> -ADDFACE_REG_DETECT_MIN_ANGEL || pRsFace->yaw< -ADDFACE_REG_DETECT_MAX_ANGEL ||
						abs(pRsFace->roll) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
						abs(pRsFace->pitch) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
				{
					log_info("####Info:RS_RET_NO_QUALITY####Face yaw: %f\n", pRsFace->yaw);
					return RET_NO_ANGLE;
				}
			break;
			
			case YAW_RIGHT://脸偏右
				if(pRsFace->yaw> ADDFACE_REG_DETECT_MAX_ANGEL || pRsFace->yaw< ADDFACE_REG_DETECT_MIN_ANGEL ||
						abs(pRsFace->roll) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
						abs(pRsFace->pitch) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
				{
					log_info("####Info:RS_RET_NO_QUALITY####Face yaw: %f\n", pRsFace->yaw);
					return RET_NO_ANGLE;
				}
			break;
#if ROLL_ENABLE			
			case ROLL_LEFT://头偏左
				if(pRsFace->roll > ADDFACE_REG_DETECT_MAX_ANGEL || pRsFace->roll < ADDFACE_REG_DETECT_MIN_ANGEL ||
						abs(pRsFace->pitch) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
						abs(pRsFace->yaw) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
				{
					log_info("####Info:RS_RET_NO_QUALITY####Face roll: %f\n", pRsFace->roll);
					return RET_NO_ANGLE;
				}
			break;
			
			case ROLL_RIGHT://头偏右
				if(pRsFace->roll> -ADDFACE_REG_DETECT_MIN_ANGEL || pRsFace->roll < -ADDFACE_REG_DETECT_MAX_ANGEL ||
						abs(pRsFace->pitch) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL  ||
						abs(pRsFace->yaw) > stSystemCfg.face_reg_angle+ADDFACE_REG_OFFSET_ANGEL) 
				{
					log_info("####Info:RS_RET_NO_QUALITY####Face roll: %f\n", pRsFace->roll);
					return RET_NO_ANGLE;
				}
			break;
#endif
			default:
				break;
			}
		}
	}

#if CFG_FACE_BIN_LIVENESS
	/* Check liveness */
	if (facelib_param.living_detect_flag) 
	{
		log_debug("Doing liveness check\n");

		if (RET_NO_LIVENESS == face_alg_check_face_liveness(ir_image, depth_image, pRsFace)) 
		{
			return RET_NO_LIVENESS;
		}
		log_debug("Face liveness check pass!\n");
	}
#endif

#if CFG_PER_TIME_CAL_EVERYSTEP
	per_calc_start();
#endif
	/* Check quality */
	if (facelib_param.quality_flag) 
	{
		log_debug("Doing quality check\n");
		int32_t score = 0;
		score = face_alg_get_face_quality(ir_image, pRsFace);
		if (score  < CFG_FACE_QUALITY_THRESHHOLD) 
		{
			log_info("Face quality too low: %d\n", score);
			return RET_NO_QUALITY;
		}
	}
#if CFG_PER_TIME_CAL_EVERYSTEP
	per_calc_end();
	log_info("Face quality checkpass: %d usec\n", per_calc_get_result());
#endif

	rs_face_feature rs_feature;
	alg_ret = face_alg_get_face_feature(ir_image, pRsFace, &rs_feature);
	if (RET_NO_FEATURE == alg_ret) 
	{
		return RET_NO_FEATURE;
	}
	
#if CHECK_FACE_VERIFICATION		
	float conf = 0;
	conf = rsRecognitionGetConfidence(ghfaceRecognition);
	log_info("Face confidence: %f\n", conf);
#endif //CHECK_FACE_VERIFICATION

	if(flag == CREAT_PERSON)
	{
		// Compare with the faces in database
		db_ret = rsRecognitionFaceIdentification(ghfaceRecognition, &rs_feature);
		if (db_ret < 0) 
		{
#if 0
			{
			static int32_t img_counter = 0;
			char str_file_name[100] = { 0 };
			FILE *fp = NULL;

			sprintf(str_file_name, "img_cap_%d.nv12", img_counter++);
			printf("Image filename: %s\n", str_file_name);
			fp = fopen(str_file_name, "w+");
			fwrite(ir_image->data, (CFG_IR_FRAME_WIDTH * CFG_IR_FRAME_HEIGHT * 3) >> 1, 1, fp);
			fclose(fp);
			}
#endif
			person_id = rsRecognitionPersonCreate(ghfaceRecognition, &rs_feature);
			if (person_id > 0) 
			{
				log_info("Face registerd, ID: %d\n", person_id);
				face_reg_SaveImgToLocalFileByPersonID(*ir_image, *depth_image, person_id);
				return RET_PERSON_ADD_FACE;
			} 
			else 
			{
				log_error("rsRecognitionPersonCreate failed!\n");
				return RET_FAIL;
			}
		}
		else 
		{
			//face is there
			person_id = 0; 
			log_info("face is already in the database,id<%d>\n",db_ret);
			return RET_ALREADY_IN_DATABASE;
		}
	 } 
	 else
	 {
		/*为PersonID添加多个人脸FaceID*/
		static int num = 0;
		printf("=====For personID<%d> add a face<%d>\n", person_id, num+1);
		rsRecognitionPersonAddFace(ghfaceRecognition, &rs_feature, person_id);
		if(++num >= RegAddFaceTypeNUM)
			return RET_OK;
		else
			return RET_PERSON_ADD_FACE;
	}

	return RET_OK;
} 

//added for deleted specific face ID
RET_VALUE DeleteFaceID(int personId)
{
	int ret = -1;
	
	ret = rsRecognitionPersonDelete(ghfaceRecognition, personId);
	if(ret == 0)
	{
		face_reg_DelImgFromLocalFileByPersonID(personId);
		return RET_OK;
	}
	
	return RET_UNKOWN_ERR;	
}

/*获取人脸数据库中已保存的人数*/
RET_VALUE GetAlbumSizeFromDataBase(unsigned short *faceSize)
{
	int ret = -1;
	
	*faceSize = rsRecognitionGetAlbumSize(ghfaceRecognition);
	if(faceSize >= 0)
		return RET_OK;
	
	return RET_UNKOWN_ERR;	
}

