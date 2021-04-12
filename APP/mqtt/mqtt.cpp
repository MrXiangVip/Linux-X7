#if 0
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif

#include <string.h>
#include <signal.h>
#include <thread>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include "iniparser.h"
#include "uart-handle.h"
#include "config.h"
#include "util.h"
#include "mqtt-api.h"
#include "mqtt-mq.h"
#include "cJSON.h"
#include "base64.h"
#include "log.h"

#define DEFAULT_HEADER "IN:"
#define DEFAULT_HEADER_LEN (strlen(DEFAULT_HEADER))
#define DEFAULT_SEPARATOR ":"
#define TIME_OUT_RES 2

#if 0
#define log_err printf
#define log_info printf
#endif

#define MQTT_MSG_ID_LEN 50
#define MQTT_AT_LEN 256
#define AT_SEND_BUF_LEN 256
#define UART_RX_BUF_LEN 8292

#define SLEEP_INTERVER_USEC 10000
#define DEFAULT_CMD_TIMEOUT_USEC 1000000
#define DEFAULT_RETRY_TIME 3

#define AT_CMD_NONE 0
#define AT_CMD_SENT 1
#define AT_CMD_OK 2
#define AT_CMD_ERROR 3
#define AT_CMD_TIMEOUT 4

using namespace std;

volatile sig_atomic_t loop = 1;
static int uart_fd;
int mqtt_init_done = 0;
int g_mcu_complete = 0;

char mqtt_msg_id[MQTT_MSG_ID_LEN];
// AT命令状态
int at_state = AT_CMD_NONE;
int at_cmd_timeout_usec = -1;
char device_mac_from_module[MAC_LEN];

char wifi_rssi[MQTT_AT_LEN];

#ifdef LOOP_MQTT_PUBLISH
// 消息发布
char *pub_msg = NULL;
int pub_len = 0;
int pub_state = AT_CMD_NONE;
#endif


#if HOST
const char* TTY_WIFI="/dev/ttyUSB1";
#else
const char* TTY_WIFI="/dev/ttyLP3";
#endif

void sig_handler(int signum)
{
	loop = 0;
	log_info("Uart Fun Exit!\n"); 
}

char *getFirmwareVersion() {
	return "1.0.0";
}

static int random_gen = 1;
char* gen_msgId() {
	char *msgId = (char*)malloc(200);
	memset(msgId, '\0', 200);
	// mac
    struct timeval tv;
	gettimeofday(&tv,NULL);
	// 可能秒就够了
	// long id = tv.tv_sec*1000000 + tv.tv_usec;
	// sprintf(msgId, "%s%d%06d%03d", wifiConfig.local_device_mac, tv.tv_sec, tv.tv_usec, random_gen);
	// sprintf(msgId, "%s%d%d", wifiConfig.local_device_mac, tv.tv_sec, random_gen);
	sprintf(msgId, "%d%06d%03d", tv.tv_sec, tv.tv_usec, random_gen);
	log_info("generate msgId is %s\n", msgId);
	// 3位random
	// if (++random_gen >= 1000) {
	// 1位random
	if (++random_gen >= 1000) {
		random_gen = 1;
	}
	return msgId;
}

int del_char(char *src, char sep)
{
	char *pTmp = src;
	unsigned int iSpace = 0;

	while (*src != '\0') {
		if (*src != sep) {
			*pTmp++ = *src;
		} else {
			iSpace++;
		}

		src++;
	}

	*pTmp = '\0';

	return iSpace;
}

#if 0
// 向标准错误输出信息，告诉用户时间到了
void sig_alarm_handler(int signo)
{
	// write(STDERR_FILENO, msg, len);
	log_info("-------- sig_alarm_handler\n");
}

// 建立信号处理机制
void init_sigaction(void)
{
	struct sigaction tact;
	/*信号到了要执行的任务处理函数为prompt_info*/
	tact.sa_handler = sig_alarm_handler;
	tact.sa_flags = 0;
	/*初始化信号集*/
	sigemptyset(&tact.sa_mask);
	/*建立信号处理机制*/
	sigaction(SIGALRM, &tact, NULL);
}
void init_time()
{
	struct itimerval value;
	/*设定执行任务的时间间隔为2秒0微秒*/
	value.it_value.tv_sec = 2;
	value.it_value.tv_usec = 0;
	/*设定初始时间计数也为2秒0微秒*/
	value.it_interval = value.it_value;
	/*设置计时器ITIMER_REAL*/
	setitimer(ITIMER_REAL, &value, NULL);
}
#endif
void sig_alarm_handler(int);
typedef void (*sighandler_t)(int);

void sig_alarm_handler(int sig_num)
{
	log_debug("%s, signal number:%d g_mcu_complete %d\n", __FUNCTION__, sig_num, g_mcu_complete);
	if(sig_num = SIGALRM)
	{
		if (g_mcu_complete == 0) {
			char pub_msg[MQTT_AT_LEN];
			memset(pub_msg, '\0', MQTT_AT_LEN);
			sprintf(pub_msg, "%s{\\\"msgId\\\":\\\"%s\\\"\\,\\\"result\\\":%d}", DEFAULT_HEADER, mqtt_msg_id, TIME_OUT_RES);
			MessageSend(1883, pub_msg, strlen(pub_msg));
		}
	}
}

int sendStatusToMCU(int biz, int ret) {
	char payload_bin[MQTT_AT_LEN];
	memset(payload_bin, '\0', MQTT_AT_LEN);
	payload_bin[0] = 0x23;
	payload_bin[1] = 0x12;
	payload_bin[2] = 0x02;
	payload_bin[3] = biz;
	payload_bin[4] = (ret == 0 ? 0x00 : 0x01);
	unsigned short cal_crc16 = CRC16_X25(reinterpret_cast<unsigned char*>(payload_bin), 5);
	payload_bin[5] = (char)cal_crc16;
	payload_bin[6] = (char)(cal_crc16 >> 8);
	for (int i = 0; i < 7; i++) {
		log_debug("sendStatusToMCU %d 0x%x", i, (char)payload_bin[i]);
	}
	char payload_str[15];
	HexToStr(payload_str, reinterpret_cast<char*>(&payload_bin), 7);
	log_info("sednStatusToMCU %s", payload_str);
	int result = MessageSend(1234, payload_bin, 7);
	return result;
}

int handlePayload(char *payload) {
	if (payload != NULL) {
		int payload_len = strlen(payload);
		int payload_bin_len = (payload_len / 4 - 1) * 3 + 1;
		char payload_bin[MQTT_AT_LEN];
		int ret1 = base64_decode(payload, payload_bin);
		log_info("decode payload_bin_len is %d ret1 is %d", payload_bin_len, ret1);
		for (int i = 0; i < ret1; i++) {
			log_info("0x%02x ", (unsigned char)payload_bin[i]);
		}
		log_info("\n");
		g_mcu_complete = 0;
		MessageSend(1234, payload_bin, ret1);
		alarm(5);
	} else {
		char pub_msg[50];
		memset(pub_msg, '\0', 50);
		sprintf(pub_msg, "%s{data:%s}", DEFAULT_HEADER, "No Payload");
		// NOTE: 此处必须异步操作
		MessageSend(1883, pub_msg, strlen(pub_msg));
	}

	return 0;
}

int SendMsgToMQTT(char* mqtt_payload, int len) {
	char msg[MQTT_AT_LEN];
	memset(msg, '\0', MQTT_AT_LEN);
	char first[MQTT_AT_LEN];
	char pub_topic[MQTT_AT_LEN];
	memset(pub_topic, '\0', MQTT_AT_LEN);
	char pub_msg[MQTT_AT_LEN];
	memset(pub_msg, '\0', MQTT_AT_LEN);
	log_debug("--- mqttpayload len is %d\n", len);
	// 主动上报功能
	if (mqtt_payload != NULL && strncmp(mqtt_payload, DEFAULT_HEADER, DEFAULT_HEADER_LEN) == 0) {
		// 内部异步发布消息
		mysplit(mqtt_payload, first, msg, ":");
		log_debug("========================================= msg 1 is %s\n", msg);
		if (strncmp(msg, "heartbeat", 9) == 0) {
			char *msgId = gen_msgId();
			if (w7_firmware_version != NULL && strlen(w7_firmware_version) > 0) {
				sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"wifi_rssi\\\":%s\\,\\\"battery\\\":%d\\,\\\"version\\\":\\\"%s\\\"}", msgId, wifiConfig.local_device_mac, wifi_rssi, w7_battery_level, w7_firmware_version);
			} else {
				sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"wifi_rssi\\\":%s\\,\\\"battery\\\":%d\\,\\\"version\\\":\\\"%s\\\"}", msgId, wifiConfig.local_device_mac, wifi_rssi, w7_battery_level, getFirmwareVersion());
			}
			freePointer(&msgId);
			memset(pub_topic, '\0', 100);
			if (mqttConfig.pub_topic_heartbeat != NULL && strlen(mqttConfig.pub_topic_heartbeat) > 0) {
				strcpy(pub_topic, mqttConfig.pub_topic_heartbeat);
			} else {
				sprintf(pub_topic, "WAVE/DEVICE/%s/HEARTBEAT", wifiConfig.local_device_mac);
			}
		} else {
			sprintf(pub_msg, "%s", msg);
			memset(pub_topic, '\0', 50);
			if (mqttConfig.pub_topic_cmd_res != NULL && strlen(mqttConfig.pub_topic_cmd_res) > 0) {
				strcpy(pub_topic, mqttConfig.pub_topic_cmd_res);
			} else {
				sprintf(pub_topic, "WAVE/DEVICE/%s/INSTRUCTION/RESPONSE", wifiConfig.local_device_mac);
			}
			log_debug("-------- pub_topic is %s\n", pub_topic);
			mqtt_msg_id[0] = '\0';
		}
		// int ret = quickPublishMQTT(mqttConfig.pub_topic, pub_msg);
		int ret = quickPublishMQTT(pub_topic, pub_msg);
		return 0;
	} else { 
		if ((int)(char)(mqtt_payload[0]) == 0x23) {
			g_mcu_complete = 1;
			alarm(0);
			memset(pub_topic, '\0', 50);
			if (mqttConfig.pub_topic_cmd_res != NULL && strlen(mqttConfig.pub_topic_cmd_res) > 0) {
				strcpy(pub_topic, mqttConfig.pub_topic_cmd_res);
			} else {
				sprintf(pub_topic, "WAVE/DEVICE/%s/INSTRUCTION/RESPONSE", wifiConfig.local_device_mac);
			}
			log_debug("-------- pub_topic is %s\n", pub_topic);
			log_debug("---- from MCU\n");
			if ((int)(char)(mqtt_payload[1]) == 0x83 && (int)(char)(mqtt_payload[2]) == 0x01) {
				// 远程开锁指令反馈
				// 远程开锁成功
				if ((int)(char)(mqtt_payload[3]) == 0x00) {
					printf("---- unlock door successfully\n");
					sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"result\\\":\\\"%d\\\"}", mqtt_msg_id, wifiConfig.local_device_mac, 0);
				} else {
					// 远程开锁失败
					printf("---- unlock door fail\n");
					// sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mqtt_payloadult\\\":\\\"%d\\\"\\}", mqtt_msg_id, 1);
					sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"result\\\":\\\"%d\\\"}", mqtt_msg_id, wifiConfig.local_device_mac, 500);
				}
				// int ret = quickPublishMQTT(mqttConfig.pub_topic, pub_msg);
				int ret = quickPublishMQTT(pub_topic, pub_msg);
				mqtt_msg_id[0] = '\0';
				return 0;
			} else if ((int)(char)(mqtt_payload[1]) == 0x15 && (int)(char)(mqtt_payload[2]) == 0x01) {
				// 远程设置合同时间指令反馈
				// 远程设置合同时间成功
				if ((int)(char)(mqtt_payload[3]) == 0x01) {
					log_error("---- setup contract successfully\n");
					sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"result\\\":\\\"%d\\\"}", mqtt_msg_id, wifiConfig.local_device_mac, 0);
				} else {
					// 远程设置合同时间失败
					log_error("---- setup contract fail\n");
					// sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mqtt_payloadult\\\":\\\"%d\\\"\\}", mqtt_msg_id, 1);
					sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mqtt_payloadult\\\":\\\"%d\\\"}", mqtt_msg_id, 500);
					sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"result\\\":\\\"%d\\\"}", mqtt_msg_id, wifiConfig.local_device_mac, 500);
				}
				// int ret = quickPublishMQTT(mqttConfig.pub_topic, pub_msg);
				int ret = quickPublishMQTT(pub_topic, pub_msg);
				log_error("------- before sendStatus ToMCU ret %d\n", ret);
				sendStatusToMCU(0x04, ret);
				mqtt_msg_id[0] = '\0';
				return 0;
			} else if ((int)(char)(mqtt_payload[1]) == 0x14 && (int)(char)(mqtt_payload[2]) == 0x0f) {
				// 开门记录上报指令
				if ((int)(char)(mqtt_payload[2]) == 0x0f) {
					log_info("unlock record upload to server\n");
#if 0
					char uid[17];
					HexToStr(uid, (char*)(&mqtt_payload[3]), 8);
					log_info("unlock record uid %s\n", uid);
#else
					char uid[9];
					strncpy(uid, (char*)(&mqtt_payload[3]), 8);
					uid[8] = '\0';
#endif
					int unlockType = (int)(char)mqtt_payload[11];
					unsigned int unlockTime = (unsigned int)StrGetUInt32((char*)(&mqtt_payload[12]));
					int batteryLevel = (int)mqtt_payload[16];
					int unlockStatus = (int)(char)mqtt_payload[17];
					log_info("unlock record uid %s, unlockType %d, unlockTime %u, batteryLevel %d, unlockStatus %d\n", uid, unlockType, unlockTime, batteryLevel, unlockStatus);
					sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"userId\\\":\\\"%s\\\"\\, \\\"unlockMethod\\\":%d\\,\\\"unlockTime\\\":%u\\,\\\"batteryPower\\\":%d\\,\\\"unlockState\\\":%d}", gen_msgId(), wifiConfig.local_device_mac, uid, unlockType, unlockTime, batteryLevel, unlockStatus);
					memset(pub_topic, '\0', 100);
					if (mqttConfig.pub_topic_record_request != NULL && strlen(mqttConfig.pub_topic_record_request) > 0) {
						strcpy(pub_topic, mqttConfig.pub_topic_record_request);
					} else {
						sprintf(pub_topic, "WAVE/DEVICE/%s/W7/RECORD/REQUEST", wifiConfig.local_device_mac);
					}
					int ret = quickPublishMQTT(pub_topic, pub_msg);
					sendStatusToMCU(0x04, ret);
					return ret;
				} else {
					log_warn("unlock record length is %d instead of 15\n", (int)(char)(mqtt_payload[2]));
				}
			} else if ((int)(char)(mqtt_payload[1]) == 0x07 && (int)(char)(mqtt_payload[2]) == 0x09 && (int)(char)(mqtt_payload[11]) == 0x00) {
				// 传图指令
				char uuid[17];
				HexToStr(uuid, reinterpret_cast<char*>(&mqtt_payload[3]), 8);
				char uuidStr[9];
				strncpy(uuidStr, reinterpret_cast<char*>(&mqtt_payload[3]), 8);
				uuidStr[8] = '\0';
#if 1
				for (int x = 0; x < 14; x++) {
					log_trace("uuid mqtt_payload[%d] is %d\n", x, (int)(char)(mqtt_payload[x]));
				}
#endif
				log_debug("uuid %s mqtt_payload[1] is %d mqtt_payload[2] is %d mqtt_payload[11] is %d\n", uuid, (int)(char)(mqtt_payload[1]), (int)(char)(mqtt_payload[2]), (int)(char)(mqtt_payload[11]));
				char filename[50];
				memset(filename, '\0', sizeof(filename));
				log_debug("uuid is %s\n", uuid);
#ifdef HOST
				sprintf(filename, "./ID_%s.jpg", uuid);
				log_debug("filename is %s\n", filename);
#else
				sprintf(filename, "/opt/smartlocker/pic/ID_%s.jpg", uuid);
#endif
				char *msgId = gen_msgId();
				log_debug("filename is %s msgId is %s\n", filename, msgId);
				log_debug("before send img msgId is set to %s\n", msgId);
				char pub_topic_tmp[MQTT_AT_LEN];
				if(filename != NULL && strlen(filename) > 0 && (access(filename,F_OK))!=-1)   {   
					memset(pub_topic, '\0', 100);
					if (mqttConfig.pub_topic_pic_report != NULL && strlen(mqttConfig.pub_topic_pic_report) > 0) {
						strcpy(pub_topic, mqttConfig.pub_topic_pic_report);
					} else {
						sprintf(pub_topic, "WAVE/W7/%s/P/R", wifiConfig.local_device_mac);
					}
					memset(pub_topic_tmp, '\0', MQTT_AT_LEN);
					if (mqttConfig.pub_topic_pic_report_u != NULL && strlen(mqttConfig.pub_topic_pic_report_u) > 0) {
						strcpy(pub_topic_tmp, mqttConfig.pub_topic_pic_report_u);
					} else {
						sprintf(pub_topic_tmp, "WAVE/W7/%s/U/R", wifiConfig.local_device_mac);
					}
					sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"userId\\\":\\\"%s\\\"}", msgId, uuidStr);
					int ret = pubImage(pub_topic, filename, msgId, wifiConfig.local_device_mac);
					ret = quickPublishMQTT(pub_topic_tmp, pub_msg);
					sendStatusToMCU(0x04, ret);
					return ret;
				}
				freePointer(&msgId);
			} else {
			}
		}
		log_debug("SendMsgToMQTT final g_mcu_complete %d\n", msg, g_mcu_complete);
		return -1;
	}

#if 0
	char pub_msg[250];
	memset(pub_msg, '\0', 250);
	sprintf(pub_topic, "%s", wifiConfig.local_device_mac);
	// 命令执行中，有结果上报
	if (strlen(mqtt_msg_id) > 0) {
			printf("========================================= msg 3 is %s g_mcu_complete %d\n", msg, g_mcu_complete);
#if 0
		// 指令执行结果上报，需要base64编码
		int base64_msg_len = (len / 3 + 1)*4 + 1;
		char *base64_msg = (char*)malloc(base64_msg_len);
		memset(base64_msg, '\0', base64_msg_len);
		log_info("before base64_encode g_mcu_complete %d len is %d baselen is %d\n", g_mcu_complete, len, base64_msg_len);
		for (int i = 0; i < len; i++) {
			log_info("0x%02x ", mqtt_payload[i]);
		}
		log_info("end\n");
		char *tmp = base64_encode(reinterpret_cast<char*>(mqtt_payload), base64_msg, len);
		log_info("base64_encode original len is %d encoded msg is %s - %s\n", len, base64_msg, tmp);
		// sprintf(pub_msg, "%s", base64_msg);
		sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"result\\\":\\\"%s\\\"}", mqtt_msg_id, wifiConfig.local_device_mac, base64_msg);
		freePointer(&base64_msg);

		memset(pub_topic, '\0', 100);
		if (mqttConfig.pub_topic_cmd_res != NULL && strlen(mqttConfig.pub_topic_cmd_res) > 0) {
			strcpy(pub_topic, mqttConfig.pub_topic_cmd_res);
		} else {
			sprintf(pub_topic, "wave/device/%s/instruction/mqtt_payload", wifiConfig.local_device_mac);
		}
#endif
	} else {
		// 主动上报功能
		if (strncmp(res, DEFAULT_HEADER, DEFAULT_HEADER_LEN) == 0) {
			// sprintf(pub_msg, "{data:%s}", msg);
			if (strncmp(msg, "heartbeat", 9) == 0) {
				char *msgId = NULL;
				// char *msgId = (char*)malloc(200);
				// memset(msgId, '\0', 200);
				msgId = gen_msgId();
				if (w7_firmware_version != NULL && strlen(w7_firmware_version) > 0) {
					sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"battery\\\":%d\\,\\\"version\\\":\\\"%s\\\"}", msgId, wifiConfig.local_device_mac, w7_battery_level, w7_firmware_version);
				} else {
					sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"battery\\\":%d\\,\\\"version\\\":\\\"%s\\\"}", msgId, wifiConfig.local_device_mac, w7_battery_level, getFirmwareVersion());
				}
				// sprintf(pub_msg, "msgId:%s\\,mac:%s\\,v:%s", msgId, wifiConfig.local_device_mac, "1");
				// sprintf(pub_msg, "heartbeat");
				printf("before freePointer \n");
				freePointer(&msgId);
				memset(pub_topic, '\0', 100);
				if (mqttConfig.pub_topic_heartbeat != NULL && strlen(mqttConfig.pub_topic_heartbeat) > 0) {
					strcpy(pub_topic, mqttConfig.pub_topic_heartbeat);
				} else {
					sprintf(pub_topic, "wave/device/%s/heartbeat", wifiConfig.local_device_mac);
				}
			} else {
				// 上游也可以先判断是否为timeout或者busy  
				sprintf(pub_msg, "%s", msg);
			}
		} else {
			// 指令执行结果上报，需要base64编码
			int base64_msg_len = (len / 3 + 1)*4 + 1;
			char *base64_msg = (char*)malloc(base64_msg_len);
			memset(base64_msg, '\0', base64_msg_len);
			log_info("before base64_encode len is %d baselen is %d\n", len, base64_msg_len);
			for (int i = 0; i < len; i++) {
				log_info("0x%02x ", mqtt_payload[i]);
			}
			log_info("end\n");
			char *tmp = base64_encode(reinterpret_cast<char*>(mqtt_payload), base64_msg, len);
			log_info("base64_encode original len is %d encoded msg is %s - %s\n", len, base64_msg, tmp);
			// sprintf(pub_msg, "%s", base64_msg);
			sprintf(pub_msg, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"\\,\\\"result\\\":\\\"%s\\\"}", mqtt_msg_id, wifiConfig.local_device_mac, base64_msg);
			freePointer(&base64_msg);

			memset(pub_topic, '\0', 50);
			if (mqttConfig.pub_topic_cmd_res != NULL && strlen(mqttConfig.pub_topic_cmd_res) > 0) {
				strcpy(pub_topic, mqttConfig.pub_topic_cmd_res);
			} else {
				sprintf(pub_topic, "wave/device/%s/instruction/mqtt_payload", wifiConfig.local_device_mac);
			}
		}
	}
	// int ret = quickPublishMQTT(mqttConfig.pub_topic, pub_msg);
	int ret = quickPublishMQTT(pub_topic, pub_msg);
	mqtt_msg_id[0] = '\0';
	freePointer(&res);
#endif
}

int handleJsonMsg(char *jsonMsg) {
	cJSON *mqtt = NULL;
	cJSON *msg_id = NULL;
	cJSON *data = NULL;
	cJSON *timestamp = NULL;
	char *msg_idStr = NULL;
	char *dataStr = NULL;
	char *tsStr = NULL;

	mqtt = cJSON_Parse(jsonMsg);
	msg_id = cJSON_GetObjectItem(mqtt, "msgId");
	msg_idStr = cJSON_GetStringValue(msg_id);

	memset(mqtt_msg_id, '\0', MQTT_MSG_ID_LEN);
	log_debug("------ handleJsonMsg msg_idStr is %s\n", msg_idStr);
	if (msg_idStr != NULL && strlen(msg_idStr) > 0) {
		strcpy(mqtt_msg_id, msg_idStr);
		log_debug("------ cp msg_idStr done\n");
	}
	int result = 0;

	data = cJSON_GetObjectItem(mqtt, "data");
	if (data != NULL) { 
		dataStr = cJSON_GetStringValue(data);
		log_debug("---JSON data is %s\n", dataStr);
		if (dataStr != NULL && strlen(dataStr)) {
			// TODO: 这种是透传模式，可能要增加一个type的判断，确定走透传模式还是解析模式
			result = handlePayload(dataStr);
		}
	}

	timestamp = cJSON_GetObjectItem(mqtt, "time");
	if (timestamp != NULL) {
		tsStr = cJSON_GetStringValue(timestamp);
		log_debug("--- timestamp is %s len is %d\n", tsStr, strlen(tsStr));
		if (tsStr != NULL && strlen(tsStr) > 0) {

			int currentSec = atoi(tsStr);
			if (currentSec > 1000000000) {
				log_debug("__++_+++++++ currentSec is %d can setTimestamp\n", currentSec);
				setTimestamp(currentSec);
			} else {
				log_debug("__++_+++++++ currentSec is %d dont setTimestamp\n", currentSec);
			}

			char payload_bin[MQTT_AT_LEN];
			memset(payload_bin, '\0', MQTT_AT_LEN);
			payload_bin[0] = 0x23;
			payload_bin[1] = 0x8a;
			payload_bin[2] = 0x0a;
			int len = strlen(tsStr);
			strncpy(&payload_bin[3], tsStr, len);
			unsigned short cal_crc16 = CRC16_X25(reinterpret_cast<unsigned char*>(payload_bin), 3 + len);
			payload_bin[len + 3] = (char)cal_crc16;
			payload_bin[len + 4] = (char)(cal_crc16 >> 8);
			result = MessageSend(1234, payload_bin, 5 + len);
		}
	}
#if 0
	if (dataStr != NULL) {
		cJSON_Delete(dataStr);
		dataStr = NULL;
	}
	if (data != NULL) {
		cJSON_Delete(data);
		data = NULL;
	}
	if (msg_id != NULL) {
		cJSON_Delete(msg_id);
		msg_id = NULL;
	}
#endif
	if (mqtt != NULL) {
		cJSON_Delete(mqtt);
		mqtt = NULL;
	}
	return result;
}

int analyzeMQTTMsgInternal(char *msg) {
	static int ana_timeout_count = 0;
	int result = -1;
	char *pNext;

	pNext = (char *)strstr(msg,"+MQTTSUBRECV:"); //必须使用(char *)进行强制类型转换(虽然不写有的编译器中不会出现指针错误)
	if (pNext != NULL) {
		char firstWithLinkId[MQTT_AT_LEN];
		char topic[MQTT_AT_LEN];
		char dataLen[MQTT_AT_LEN];
		char data[MQTT_AT_LEN];
		char lastWithTopic[MQTT_AT_LEN];
		char lastWithDataLen[MQTT_AT_LEN];
		mysplit(msg, firstWithLinkId, lastWithTopic, ",\"");

#if 0
		printf("--- firstWithLinkId is %s\n", firstWithLinkId);
		char *linkId = (char*)strtok(firstWithLinkId, "+MQTTSUBRECV:");
		printf("--- linkId is %s\n", linkId);
		printf("--- lastWithTopic is %s\n", lastWithTopic);
#endif

		mysplit(lastWithTopic, topic, lastWithDataLen, "\",");

#if 0
		printf("--- lastWithDataLen is %s\n", lastWithDataLen);
#endif

		mysplit(lastWithDataLen, dataLen, data, ",");

#if 0
		printf("--- topic is %s\n", topic);
		printf("--- data is %s\n", data);
#endif

		int data_len = atoi(dataLen);

#if 0
		printf("--- dataLen is %d\n", data_len);
#endif

		if (data != NULL) {
			// printf("---- %d\n", ana_timeout_count);
			if (strlen(data) >= data_len) {
				// char *payload = (char*)malloc(data_len + 1);
				// memset(payload, '\0', data_len + 1);
				char payload[MQTT_AT_LEN];
				strncpy(payload, data, data_len);
				payload[data_len] = '\0';
				ana_timeout_count = 0;
				log_info("payload is %s len is %d\n", payload, strlen(payload));
				int ret = handleJsonMsg(payload);
				if (ret < 0)  {
					result = -3; // command fail
				} else {
					result = 0;
				}
				// freePointer(&payload);
			} else {
				if (++ana_timeout_count > 10) {
					log_debug("analyze timeout\n");
					result = -2;
					ana_timeout_count = 0;
				}
			}
		}

	}
	// 下面的初始化会导致strtok 段错误
	// char *data = "+MQTTSUBRECV:0,\"testtopic\",26,{ \"msg\": \"Hello, World!\" }";

	return result;
}

int analyzeMQTTMsg(char *msg) {
	int result = -1;
	char line[MQTT_AT_LEN];
	int j = 0;

	int msgLen = strlen(msg);
	for (int i = 0; i < msgLen; i++) {
		char c = msg[i];
		if (c == '\r') {
		}
		else if (c == '\n') {
			line[j] = '\0';
			j = 0;
			// printf("line is %s\n", line);
			int ret = analyzeMQTTMsgInternal(line);
			if (ret == 0) {
				result = 0;
			} else if (result != 0 && ret == -3) {
				result = -3;
			} else if ((result != 0 || result != -3) && ret == -2) {
				result = -2;
			}
		} else {
			line[j++] = c;
		}
	}
	return result;
}

int runATCmd(const char *at_cmd, int retry_times, int cmd_timeout_usec) {
	int i = 0;
	// unsigned char at_send_buf[AT_SEND_BUF_LEN];
	char buf[AT_SEND_BUF_LEN] = {0};
	unsigned int msg_len = 0;
	memset(buf, '\0', sizeof(buf));
	// sprintf(buf, "%s\r\n", reinterpret_cast<unsigned char*>(at_cmd));
	sprintf(buf, "%s\r\n", at_cmd);

	at_cmd_timeout_usec = -1;

	for (i = 0; i < retry_times; i++) {
		int ret = serial_com_write(uart_fd, buf, strlen(buf), &msg_len);
		if (ret < 0) {
			log_warn("send AT cmd %s failed! retries %d\n", at_cmd, (i+1));
			usleep(SLEEP_INTERVER_USEC*i);
			continue;
		}
		log_debug("----------------------------------- START CMD ------------------------");
		log_debug("execute AT cmd: %s | success msg_len %d!", at_cmd, msg_len);
		// TODO: 
		at_state = AT_CMD_SENT;
		at_cmd_timeout_usec = cmd_timeout_usec;
		while (loop) {
			usleep(SLEEP_INTERVER_USEC);
			if (at_state != AT_CMD_SENT) {
				break;
			}
		}
		if (at_state != AT_CMD_OK) {
			log_info("get AT Cmd response error or timeout");
			continue;
		} else {
			log_debug("get AT Cmd response OK");
			break;
		}
	}
	if (i == retry_times) {
		log_warn("Failed to run AT cmd %s!", at_cmd);
		return -1;
	}
	if (at_state == AT_CMD_OK) {
		at_state = AT_CMD_NONE;
		log_debug("get AT Cmd return OK\n-------------------------------------------------------------\n");
		return 0;
	} else if (at_state == AT_CMD_ERROR || at_state == AT_CMD_TIMEOUT) {
		at_state = AT_CMD_NONE;
		return -1;
	}
	return -1;
}

void UartRxProcCB(void) {
	log_info("========= start wifi module UART RX proc callback =========");
	fd_set fs_read;
	struct timeval tmo;
	int fd_act = 0;
	char rx_buf[UART_RX_BUF_LEN];
	int rx_len = 0;
	unsigned int len = 0;

	static int timeout_count = 0;

	while(loop)
	{
		tmo.tv_sec = 0;
		tmo.tv_usec = 100000;

		FD_ZERO(&fs_read);
		FD_SET(uart_fd, &fs_read);

		fd_act = select((uart_fd+1),&fs_read, NULL, NULL, &tmo);

		if(fd_act <= 0) {
			// now is in connectmqtt mode
			if (AT_CMD_SENT == at_state) {
				// get something
				if (rx_len > 0) {
					// printf("SENT uart_fd get %s\n", rx_buf);
					char *second=(char*)strstr(rx_buf, "\r\nOK\r\n");
					if (second != NULL) {
						at_state = AT_CMD_OK;
						rx_len = 0;
						log_debug("----------------");
						log_debug("uart rx get OK:\"%s\"", rx_buf);
						log_debug("----------------");
						// printf("find OK\n");
					}
					second = (char*)strstr(rx_buf, "\r\nERROR\r\n");
					if (second != NULL) {
						at_state = AT_CMD_ERROR;
						rx_len = 0;
						log_debug("----------------");
						log_debug("uart rx get ERROR:\"%s\"", rx_buf);
						log_debug("----------------");
					}
					second = (char*)strstr(rx_buf, "\r\n+CWJAP:\"");
					if (second != NULL) {
						rx_len = 0;
						log_debug("----------------");
						log_debug("uart rx get wifi RSSI:\n%s\n", second);
						log_debug("----------------");
						// +CWJAP:"wireless_052E81","c8:ee:a6:05:2e:81",1,-40,0,0,3,0
						char field11[MQTT_AT_LEN];
						char field21[MQTT_AT_LEN];
						char field22[MQTT_AT_LEN];
						char field31[MQTT_AT_LEN];
						char field32[MQTT_AT_LEN];
						char field41[MQTT_AT_LEN];
						char field42[MQTT_AT_LEN];
						char field5[MQTT_AT_LEN];
						memset(field42, '\0', MQTT_AT_LEN);
						mysplit(second, field11, field21, ",");
						mysplit(field21, field22, field31, ",");
						mysplit(field31, field32, field41, ",");
						mysplit(field41, field42, field5, ",");
						if (field42 != NULL && strlen(field42) > 0) {
							log_debug("RSSI is %s\n", field42);
							strcpy(wifi_rssi, field42);
						}
					}
					second = (char*)strstr(rx_buf, "+CIPSTAMAC:\"");
					if (second != NULL) {
						rx_len = 0;
						log_debug("----------------");
						log_debug("uart rx get mac:\n%s", second);
						log_debug("----------------");
						int mac_len = strlen(device_mac_from_module);
						if (mac_len == 0) {
							char cmd_atmac[MQTT_AT_LEN];
							char mac_atmacstr[MQTT_AT_LEN];
							char mac_atmac[MQTT_AT_LEN];
							char dumpstr[MQTT_AT_LEN];
							mysplit(second, cmd_atmac, mac_atmacstr, ":\"");
							log_trace("***** %s, %s\n", cmd_atmac, mac_atmacstr);
							mysplit(mac_atmacstr, mac_atmac, dumpstr, "\"");
							for (char* ptr = mac_atmac; *ptr; ptr++) {  
								// *ptr = tolower(*ptr);  //转小写
								*ptr = toupper(*ptr);  //转大写
							} 
							log_trace("***** %s, %s\n", mac_atmac, dumpstr);
							if (mac_atmac != NULL && strlen(mac_atmac) > 0) {
								del_char(mac_atmac, ':');
								strcpy(device_mac_from_module, mac_atmac);
								if (strncmp(device_mac_from_module, wifiConfig.local_device_mac, 12) != 0) {
									update_mac(DEFAULT_CONFIG_FILE, device_mac_from_module);
								}
#if 0
								write_config(DEFAULT_CONFIG_FILE, "wifi:local_device_mac", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:client_id", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:username", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:password", device_mac_from_module);
								char topic[CONFIG_ITEM_LEN];
								memset(topic, '\0', CONFIG_ITEM_LEN);
								sprintf(topic, "WAVE/DEVICE/%s/HEARTBEAT", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:pub_topic_heartbeat", topic);
								memset(topic, '\0', CONFIG_ITEM_LEN);
								sprintf(topic, "WAVE/DEVICE/%s/INSTRUCTION/RESPONSE", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:pub_topic_cmd_res", topic);
								memset(topic, '\0', CONFIG_ITEM_LEN);
								sprintf(topic, "WAVE/W7/%s/P/R", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:pub_topic_pic_report", topic);
								memset(topic, '\0', CONFIG_ITEM_LEN);
								sprintf(topic, "WAVE/DEVICE/%s/TIME/REQUEST", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:pub_topic_time_sync", topic);
								memset(topic, '\0', CONFIG_ITEM_LEN);
								sprintf(topic, "WAVE/DEVICE/%s/INSTRUCTION/ISSUE", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:sub_topic", topic);
								memset(topic, '\0', CONFIG_ITEM_LEN);
								sprintf(topic, "WAVE/DEVICE/%s/TIME/RESPONSE", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:sub_topic_time_sync", topic);
								memset(topic, '\0', CONFIG_ITEM_LEN);
								sprintf(topic, "WAVE/DEVICE/%s/W7/RECORD/REQUEST", device_mac_from_module);
								write_config(DEFAULT_CONFIG_FILE, "mqtt:sub_topic_record_request", topic);
								read_config(DEFAULT_CONFIG_FILE);
#endif
							}
							log_info("**** device_mac_from_module is %s\n", device_mac_from_module);
						}
					}
					second = (char*)strstr(rx_buf, "+MQTTSUBRECV:");
					if (second != NULL) {
						log_info("################## receive subscribe message from mqtt server");
						// something get, need to analyze and handle
						// printf("something get %s\n", rx_buf);
						int ret = 0;
						ret = analyzeMQTTMsg(second);
						if (ret == 0 || ret == -2) {
							log_info("#################### receive done, reset rx_buf");
							rx_len = 0;
							memset(rx_buf, '\0', UART_RX_BUF_LEN);
						}
					}
				} else {
					timeout_count++;
					if (timeout_count * tmo.tv_usec >= at_cmd_timeout_usec) {
						log_info("at cmd timeout\n");
						at_state = AT_CMD_TIMEOUT;
					}
				}
			} else {
				timeout_count = 0;
				// now is in subtopic mode
				// printf("NOT SENT\n");
				if (rx_len <= 0) {
					// really nothing get
					// printf("nonthing get\n");
					continue;
				} else {
					// something get, need to analyze and handle
					// printf("something get %s\n", rx_buf);
					int ret = 0;
					ret = analyzeMQTTMsg(rx_buf);
					if (ret == 0 || ret == -2) {
						log_debug("======= message is handled, then reset rx_buf\n");
						rx_len = 0;
						memset(rx_buf, '\0', UART_RX_BUF_LEN);
					}
				}
			}
		} else {

			if(FD_ISSET(uart_fd, &fs_read)) {
				ioctl(uart_fd,FIONREAD,&len);
				if (rx_len + len >= UART_RX_BUF_LEN) {
					len = UART_RX_BUF_LEN - rx_len;
				}
				// int ret = 0;
				// ret = serial_com_read(uart_fd, rx_buf + rx_len, len, &len);
				serial_com_read(uart_fd, rx_buf + rx_len, len, &len);
				rx_len += len;
				rx_buf[rx_len] = '\0';
			}

		}
	}

}


int verifyLocalDeviceMac() {
	char mac[MAC_LEN];
	memset(mac, '\0', MAC_LEN);
	int device_mac_len = strlen(wifiConfig.local_device_mac);
	log_info("mac in config is %s\n", wifiConfig.local_device_mac);
	log_info("default mac is %s\n", DEFAULT_MAC);
	log_info("compare returns %d\n", strncmp(wifiConfig.local_device_mac, DEFAULT_MAC, 12));
	if (device_mac_len > 0 && strncmp(wifiConfig.local_device_mac, DEFAULT_MAC, 12) != 0) {
		return update_mac(DEFAULT_CONFIG_FILE, wifiConfig.local_device_mac);
	}
	return -1;
}

void send_heartbeat(int signo) {
	// return;
	// MessageSend(""):
	log_info("************ heart beat %d ************", signo);

	int res = runATCmd("AT+CWJAP?", 1, 1000000);

	char pub_msg[50];
	memset(pub_msg, '\0', 50);
	sprintf(pub_msg, "%s%s", DEFAULT_HEADER, "heartbeat");
	MessageSend(1883, pub_msg, strlen(pub_msg));
}

void UartTxProcCB(void) {
	usleep(100000);
	log_info("========= start wifi module UART TX proc callback =========");
	int ret = 0;

// #if HOST
	if (strncmp("true", mqttConfig.need_reset, 4) == 0 ) {
		ret = runATCmd("AT+RST", 2, 1000000);
		sleep(1);
		sendStatusToMCU(0x00, ret);
	}
	if (strncmp("true", mqttConfig.need_log_error, 4) == 0 ) {
		ret = runATCmd("AT+SYSLOG=1", 1, 1000000);
	}
// #endif
	ret = runATCmd("AT+CWMODE=3", 1, 1000000);
#if 0
	ret = runATCmd("AT+UART_CUR=921600,8,1,0,1", 1, 1000000);
	if (ret == 0) {
		// 关闭WIFI串口
		ret = serial_com_close(uart_fd);
		// 打开WIFI串口
		ret = serial_com_open(&uart_fd, TTY_WIFI, B921600);
		if (ret < 0) {
			log_err("open serial port %s failed!\n", TTY_WIFI);
			return;
		}
		log_info("open serial port %s with B921600\n", TTY_WIFI);
	}
#endif

	// 检查MAC，以便更新Topic参数
	// ret = verifyLocalDeviceMac();
	// if (ret < 0) {
		ret = runATCmd("AT+CIPSTAMAC?", 2, 1000000);
		if (ret < 0) {
			log_warn("--------- Failed to get module mac\n");
			return;
		}
	// }
	log_info("--------- get module mac done\n");

	// 连接WIFI
	ret = connectWifi(wifiConfig.ssid, wifiConfig.password);
	sendStatusToMCU(0x01, ret);
	if (ret < 0) {
		log_warn("--------- Failed to connect to WIFI\n");
		return;
	}
	log_info("--------- connect to WIFI done\n");

	char lwtMsg[50];
	sprintf(lwtMsg, "{\\\"username\\\":\\\"%s\\\"\\,\\\"state\\\":\\\"0\\\"}", wifiConfig.local_device_mac);
	// 连接MQTT
	ret = quickConnectMQTT(mqttConfig.client_id, mqttConfig.username, mqttConfig.password, mqttConfig.server_ip, mqttConfig.server_port, mqttConfig.pub_topic_last_will, lwtMsg);
	sendStatusToMCU(0x02, ret);
	if (ret < 0) {
		log_warn("--------- Failed to connect to MQTT\n");
		return;
	}
	log_info("--------- connect to mqtt done\n");

	ret = quickPublishMQTT(mqttConfig.pub_topic_last_will, "{}");

	// 订阅Topic
	ret = quickSubscribeMQTT(mqttConfig.sub_topic_cmd);
	sendStatusToMCU(0x03, ret);
	if (ret < 0) {
		log_warn("--------- Failed to subscribe topic\n");
		return;
	}
	log_info("--------- subscribe topic done\n");
	ret = quickSubscribeMQTT(mqttConfig.sub_topic_time_sync);
	if (ret < 0) {
		log_warn("--------- Failed to subscribe topic for time sync\n");
	} else {
		log_warn("--------- subscribe topic for time sync done\n");
	}

	char timeSync[MQTT_AT_LEN];
	memset(timeSync, '\0', MQTT_AT_LEN);
	char *msgId = gen_msgId();
	sprintf(timeSync, "{\\\"msgId\\\":\\\"%s\\\"\\,\\\"mac\\\":\\\"%s\\\"}", msgId, wifiConfig.local_device_mac);
	char pub_topic[100];
	memset(pub_topic, '\0', 100);
	if (mqttConfig.pub_topic_time_sync != NULL && strlen(mqttConfig.pub_topic_time_sync) > 0) {
		strcpy(pub_topic, mqttConfig.pub_topic_time_sync);
	} else {
		sprintf(pub_topic, "wave/w7/%s/time/request", wifiConfig.local_device_mac);
	}
	ret = quickPublishMQTT(pub_topic, timeSync);

	log_info("------- uart_tx_thread done\n");
	mqtt_init_done = 1;
	at_state = AT_CMD_NONE;

	// 持续发送心跳包
	struct sigaction act;
	union sigval tsval;
	act.sa_handler = send_heartbeat;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(50, &act, NULL);

	int count = 300;
	while ( loop )
	{
		// if (count++ > 300) {
		if (count++ > 300) {
			count = 0;
			/*向主进程发送信号，实际上是自己给自己发信号*/
			sigqueue(getpid(), 50, tsval);
		}
		usleep(100000); /*睡眠2秒*/
	}
#ifdef LOOP_MQTT_PUBLISH
	// 持续发布Topic
	while(loop) {
		if (pub_len > 0) {
			int ret = quickPublishMQTT(mqttConfig.pub_topic, pub_msg);
			if (ret == 0) {
				pub_len = 0;
			}
		}
		usleep(100000);
	}
#endif
}

#ifdef AUTO_PUBLISH_IMAGE
void ImageProcCB(void) {
	while (mqtt_init_done != 1) {
		usleep(100000);
	}
	log_info("========= start wifi module Image proc callback =========\n");
	int ret = 0;

#if HOST
	const char *image_file = "face.img";
#else 
	const char *image_file = "face.img";
#endif
	while (loop) {
		usleep(100000);
		if((access(image_file,F_OK))!=-1)   {   
			char pub_topic[100];
			memset(pub_topic, '\0', 100);
			if (mqttConfig.pub_topic_pic_report != NULL && strlen(mqttConfig.pub_topic_pic_report) > 0) {
				strcpy(pub_topic, mqttConfig.pub_topic_pic_report);
			} else {
				sprintf(pub_topic, "WAVE/W7/%s/P/R", wifiConfig.local_device_mac);
			}
			int ret = pubImage(pub_topic, msg, gen_msgId(), wifiConfig.local_device_mac);
#if 0
			if (ret == 0) {
				remove(image_file);
			}
#endif
		}
	}
}
#endif 

/*
 * 处理WIFI模块交互的消息
 */
static void WifiModProcCB(void) {
	log_info("========= start wifi module proc callback =========\n");

	// 获取配置文件
	int ret = read_config(DEFAULT_CONFIG_FILE);
	if (ret != 0) {
		log_error("Fail to read config file-%s!\n", DEFAULT_CONFIG_FILE);
		return;
	}

	// 打开WIFI串口
	ret = serial_com_open(&uart_fd, TTY_WIFI, B115200);
	if (ret < 0) {
		log_error("open serial port %s failed!\n", TTY_WIFI);
		return;
	}

	log_info("open serial port %s success!\n", TTY_WIFI);
	static std::thread *UartRxThread;
	UartRxThread = new std::thread(UartRxProcCB);

	static std::thread *UartTxThread;
	UartTxThread = new std::thread(UartTxProcCB);

#ifdef AUTO_PUBLISH_IMAGE
	static std::thread *ImageThread;
	ImageThread = new std::thread(ImageProcCB);
#endif

	UartRxThread->join();
	UartTxThread->join();

	// 关闭WIFI串口
	ret = serial_com_close(uart_fd);
	if (ret < 0) {
		log_error("close serial port failed! ret %d\n", ret);
		return;
	}
	log_info("close serial port success!\n");
}

int main(int argc, char **argv) {
	signal(SIGTERM, sig_handler);	
	signal(SIGINT, sig_handler);	
	//拦截定时器信号。
	signal(SIGALRM, sig_alarm_handler);

	// Set the log level
	GetSysLogCfg(&stSysLogCfg);
	log_set_level(stSysLogCfg.log_level);
	log_set_simple(1);

	w7_battery_level = 0;
	memset(w7_firmware_version, '\0', sizeof(w7_firmware_version));
	memset(wifi_rssi, '\0', MQTT_AT_LEN);
	wifi_rssi[0] = '0';

	memset(mqtt_msg_id, '\0', MQTT_MSG_ID_LEN);
	memset(device_mac_from_module, '\0', MAC_LEN);

	Message_init();

	static std::thread *WifiModProcThread;
	WifiModProcThread = new std::thread(WifiModProcCB);
	WifiModProcThread->join();

	delete WifiModProcThread;
	return 0;
}

