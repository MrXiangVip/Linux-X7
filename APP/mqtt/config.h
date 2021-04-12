#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifdef HOST
#define DEFAULT_CONFIG_FILE "config.ini.host"
#else
#define DEFAULT_CONFIG_FILE "/opt/smartlocker/config.ini"
#endif
#define MAC_LEN 20
#define CONFIG_ITEM_LEN 128
#define DEFAULT_MAC "FFFFFFFFFFFF"

typedef struct wifi_config {
	char local_device_mac[MAC_LEN];

	char ssid[CONFIG_ITEM_LEN];
	char password[CONFIG_ITEM_LEN];
} WIFICONFIG;

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

extern WIFICONFIG wifiConfig;
extern MQTTCONFIG mqttConfig;

#ifdef __cplusplus
extern "C"  {
#endif

	int read_config(char *file);
	int update_mac(char *file, char *mac);
	// int write_config(const char *file, const char *entry, const char *value);

#ifdef __cplusplus
}
#endif

#endif // _CONFIG_H_
