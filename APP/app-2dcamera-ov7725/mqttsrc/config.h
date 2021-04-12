#ifndef _CONFIG_H_
#define _CONFIG_H_

#include"configV2.h"

#ifdef HOST
#define DEFAULT_CONFIG_FILE "config.ini.host"
#else
#define DEFAULT_CONFIG_FILE "/opt/smartlocker/config.ini"
#endif
#define MAC_LEN 20
#define CONFIG_ITEM_LEN 128
#define DEFAULT_MAC "FFFFFFFFFFFF"

typedef struct version_config {
	char sys_ver[CONFIG_ITEM_LEN];
	char bt_ver[CONFIG_ITEM_LEN];
	char x7_ver[CONFIG_ITEM_LEN];
	char mcu_ver[CONFIG_ITEM_LEN];
} VERSIONCONFIG;

typedef struct btwifi_config {
	char bt_mac[MAC_LEN];
	char wifi_mac[MAC_LEN];

	char ssid[CONFIG_ITEM_LEN];
	char password[CONFIG_ITEM_LEN];
} BTWIFICONFIG;

typedef struct mqtt_config {
	char need_reset[CONFIG_ITEM_LEN];
	char need_log_error[CONFIG_ITEM_LEN];

	char server_ip[CONFIG_ITEM_LEN];
	char server_port[CONFIG_ITEM_LEN];
	char client_id[CONFIG_ITEM_LEN];
	char username[CONFIG_ITEM_LEN];
	char password[CONFIG_ITEM_LEN];

	// 订阅主题
	char sub_topic_cmd[CONFIG_ITEM_LEN];
	char sub_topic_time_sync[CONFIG_ITEM_LEN];

	// Pub主题：心跳
	char pub_topic_heartbeat[CONFIG_ITEM_LEN];
	// Pub主题：命令执行之后返回
	char pub_topic_cmd_res[CONFIG_ITEM_LEN];
	// Pub主题：图片上报用户
	char pub_topic_pic_report_u[CONFIG_ITEM_LEN];
	// Pub主题：图片上报
	char pub_topic_pic_report[CONFIG_ITEM_LEN];
	// Pub主题：遗愿
	char pub_topic_last_will[CONFIG_ITEM_LEN];
	// Pub主题：时间同步
	char pub_topic_time_sync[CONFIG_ITEM_LEN];
	// Pub主题：开门记录同步
	char pub_topic_record_request[CONFIG_ITEM_LEN];
} MQTTCONFIG;

extern VERSIONCONFIG versionConfig;
extern BTWIFICONFIG btWifiConfig;
extern MQTTCONFIG mqttConfig;

#ifdef __cplusplus
extern "C"  {
#endif

	int read_config(char *file);
	int read_default_config_from_file(char *file, char *dst, char *section, char *key, char *default_value);
	int read_config_value_default(char *dst, Config* cnf, char *section, char *key, char *default_value) ;
	int update_mac(char *file, char *mac);
	int print_w7_config(void);
	// int write_config(const char *file, const char *entry, const char *value);
	int write_config(char *file, char *section, char *key, char *value);
	
	int update_wifi_ssid(char *file, char *ssid) ;
	int update_wifi_pwd(char *file, char *password) ;
	int update_mqtt_opt(char *file, char *username, char *password) ;
	int update_bt_info(char *file, char *version, char *mac) ;
	int update_MqttSvr_opt(char *file, char *MqttSvrUrl) ;
	int update_mcu_info(char *file, char *version) ;
	int update_x7_info(char *file, char *version) ;
	int update_NetworkOptVer_info(char *file, char *version) ;//???????????????ð汾??

#ifdef __cplusplus
}
#endif

#endif // _CONFIG_H_
