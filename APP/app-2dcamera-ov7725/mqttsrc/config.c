#include <stdio.h>
#include "iniparser.h"
#include "config.h"
#include "configV2.h"
#include "util.h"

#include <fcntl.h>
#include <unistd.h>

VERSIONCONFIG versionConfig;
BTWIFICONFIG btWifiConfig;
MQTTCONFIG mqttConfig;

int update_option_if_not_exist(Config *cnf, char *section, char *key, char *value) {
	if (!cnf_has_option(cnf, section, key)) {
		return cnf_add_option(cnf, section, key, value);
	}
	return -1;
}

int update_topic_if_not_exist(Config *cnf, char *topic_name, char *section, char *key, char *mac) {
	char topic[CONFIG_ITEM_LEN];
	memset(topic, '\0', CONFIG_ITEM_LEN);
	sprintf(topic, topic_name, mac);
	return update_option_if_not_exist(cnf, section, key, topic);
}

int update_topic_overwrite(Config *cnf, char *topic_name, char *section, char *key, char *mac) {
	char topic[CONFIG_ITEM_LEN];
	memset(topic, '\0', CONFIG_ITEM_LEN);
	sprintf(topic, topic_name, mac);
	return cnf_add_option(cnf, section, key, topic);
}

int update_mac(char *file, char *mac) {
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}

#if 0
	update_option_if_not_exist(cnf, "wifi", "wifi_mac", mac);

	update_option_if_not_exist(cnf, "mqtt", "client_id", mac);
	update_option_if_not_exist(cnf, "mqtt", "username", mac);
	update_option_if_not_exist(cnf, "mqtt", "password", mac);

	update_topic_if_not_exist(cnf, "WAVE/DEVICE/%s/INSTRUCTION/ISSUE", "sub_topic", "sub_topic_cmd", mac);
	update_topic_if_not_exist(cnf, "WAVE/DEVICE/%s/TIME/RESPONSE", "sub_topic", "sub_topic_time_sync", mac);

	update_topic_if_not_exist(cnf, "WAVE/DEVICE/%s/HEARTBEAT", "pub_topic", "pub_topic_heartbeat", mac);
	update_topic_if_not_exist(cnf, "WAVE/USER/%s/WILL", "pub_topic", "pub_topic_last_will", mac);
	update_topic_if_not_exist(cnf, "WAVE/DEVICE/%s/TIME/REQUEST", "pub_topic", "pub_topic_time_sync", mac);
	update_topic_if_not_exist(cnf, "WAVE/DEVICE/%s/INSTRUCTION/RESPONSE", "pub_topic", "pub_topic_cmd_res", mac);

	update_topic_if_not_exist(cnf, "WAVE/DEVICE/%s/W7/RECORD/REQUEST", "pub_topic", "pub_topic_record_request", mac);
	update_topic_if_not_exist(cnf, "WAVE/W7/%s/U/R", "pub_topic", "pub_topic_pic_report_u", mac);
	update_topic_if_not_exist(cnf, "WAVE/W7/%s/P/R", "pub_topic", "pub_topic_pic_report", mac);
#else
	cnf_add_option(cnf, "wifi", "wifi_mac", mac);

	cnf_add_option(cnf, "mqtt", "client_id", mac);
	// update_option_if_not_exist(cnf, "mqtt", "username", mac);
	// update_option_if_not_exist(cnf, "mqtt", "password", mac);

	update_topic_overwrite(cnf, "WAVE/DEVICE/%s/INSTRUCTION/ISSUE", "sub_topic", "sub_topic_cmd", mac);
	update_topic_overwrite(cnf, "WAVE/DEVICE/%s/TIME/RESPONSE", "sub_topic", "sub_topic_time_sync", mac);

	update_topic_overwrite(cnf, "WAVE/DEVICE/%s/HEARTBEAT", "pub_topic", "pub_topic_heartbeat", mac);
	update_topic_overwrite(cnf, "WAVE/USER/%s/WILL", "pub_topic", "pub_topic_last_will", mac);
	update_topic_overwrite(cnf, "WAVE/DEVICE/%s/TIME/REQUEST", "pub_topic", "pub_topic_time_sync", mac);
	update_topic_overwrite(cnf, "WAVE/DEVICE/%s/INSTRUCTION/RESPONSE", "pub_topic", "pub_topic_cmd_res", mac);

	update_topic_overwrite(cnf, "WAVE/DEVICE/%s/W7/RECORD/REQUEST", "pub_topic", "pub_topic_record_request", mac);
	update_topic_overwrite(cnf, "WAVE/W7/%s/U/R", "pub_topic", "pub_topic_pic_report_u", mac);
	update_topic_overwrite(cnf, "WAVE/W7/%s/P/R", "pub_topic", "pub_topic_pic_report", mac);
#endif

	cnf_write_file(cnf, file, "mac updated");
  	destroy_config(&cnf); // 销毁Config对象

	read_config(file);
	return 0;
}

int update_wifi_ssid(char *file, char *ssid) 
{
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}

	cnf_add_option(cnf, "wifi", "ssid", ssid);//???Ƿ?ʽ????

	cnf_write_file(cnf, file, "mac updated");
  	destroy_config(&cnf); // 销毁Config对象

	read_config(file);
	return 0;
}

int update_wifi_pwd(char *file, char *password) 
{
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}

	cnf_add_option(cnf, "wifi", "password", password);//???Ƿ?ʽ????

	cnf_write_file(cnf, file, "mac updated");
	destroy_config(&cnf); // 销毁Config对象

	read_config(file);
	return 0;
}

int update_mqtt_opt(char *file, char *username, char *password) 
{
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}

	cnf_add_option(cnf, "mqtt", "username", username);//???Ƿ?ʽ????
	cnf_add_option(cnf, "mqtt", "password", password);//???Ƿ?ʽ????

	cnf_write_file(cnf, file, "mac updated");
	destroy_config(&cnf); // 销毁Config对象

	read_config(file);
	return 0;
}

int update_MqttSvr_opt(char *file, char *MqttSvrUrl) 
{
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}

	if (MqttSvrUrl == NULL || strchr(MqttSvrUrl, ':') == NULL) {
		printf("Failed to save MqttSvrUrl %s\n", MqttSvrUrl);
		return -1;
	}

	cnf_add_option(cnf, "mqtt", "server_url", MqttSvrUrl);//???Ƿ?ʽ????

	cnf_write_file(cnf, file, "MqttSvrUrl updated");
	destroy_config(&cnf); // 销毁Config对象

	read_config(file);
	return 0;
}

int update_bt_info(char *file, char *version, char *mac) 
{
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}

	cnf_add_option(cnf, "bt", "version", version);//???Ƿ?ʽ????
	cnf_add_option(cnf, "bt", "bt_mac", mac);//???Ƿ?ʽ????

	cnf_write_file(cnf, file, "bt info updated");
	destroy_config(&cnf); // 销毁Config对象

	read_config(file);
	return 0;
}

int update_mcu_info(char *file, char *version) 
{
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}

	cnf_add_option(cnf, "mcu", "version", version);//???Ƿ?ʽ????

	cnf_write_file(cnf, file, "mcu info updated");
	destroy_config(&cnf); // 销毁Config对象

	read_config(file);
	return 0;
}

int update_x7_info(char *file, char *version) 
{
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}

	cnf_add_option(cnf, "x7", "version", version);//???Ƿ?ʽ????

	cnf_write_file(cnf, file, "mcu info updated");
	destroy_config(&cnf); // 销毁Config对象

	read_config(file);
	return 0;
}

int update_NetworkOptVer_info(char *file, char *NetworkOptVer) 
{
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; 
	}

	cnf_add_option(cnf, "sys", "NetworkOptVer", NetworkOptVer);

	cnf_write_file(cnf, file, "NetworkOptVer updated");
	destroy_config(&cnf); 

	read_config(file);
	return 0;
}

int write_config(char *file, char *section, char *key, char *value) {
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}
	cnf_add_option(cnf, section, key, value);
	cnf_write_file(cnf, file, "");
  	destroy_config(&cnf); // 销毁Config对象
	return 0;
}

int read_default_config_from_file(char *file, char *dst, char *section, char *key, char *default_value) {
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}
	int res = read_config_value_default(dst, cnf, section, key, default_value);
	destroy_config(&cnf);
	return res;
}

int read_config_value_default(char *dst, Config* cnf, char *section, char *key, char *default_value) {
	bool res = cnf_get_value(cnf, section, key);
	if (res && !str_empty(cnf->re_string)) {
		strcpy(dst, cnf->re_string);
		return 0;
	} else if (!str_empty(default_value)) {
		cnf_add_option(cnf, section, key, default_value);
		strcpy(dst, default_value);
		return 0;
	}
	return -1;
}

int read_config_value(char *dst, Config* cnf, char *section, char *key) {
	return read_config_value_default(dst, cnf, section, key, "");
}

int read_config(char *file) {
	memset(&versionConfig, '\0', sizeof(VERSIONCONFIG));
	memset(&btWifiConfig, '\0', sizeof(BTWIFICONFIG));
	memset(&mqttConfig, '\0', sizeof(MQTTCONFIG));

	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}
	int ret = 0;
	ret = read_config_value(versionConfig.sys_ver, cnf, "sys", "version");
	ret = read_config_value(versionConfig.bt_ver, cnf, "bt", "version");
	ret = read_config_value(versionConfig.x7_ver, cnf, "x7", "version");
	ret = read_config_value(versionConfig.mcu_ver, cnf, "mcu", "version");

	ret = read_config_value(btWifiConfig.bt_mac, cnf, "bt", "bt_mac");
	ret = read_config_value(btWifiConfig.wifi_mac, cnf, "wifi", "wifi_mac");
	// ret = read_config_value_default(btWifiConfig.ssid, cnf, "wifi", "ssid", "wireless_052E81");
	// ret = read_config_value_default(btWifiConfig.password, cnf, "wifi", "password", "12345678");
	ret = read_config_value(btWifiConfig.ssid, cnf, "wifi", "ssid");
	ret = read_config_value(btWifiConfig.password, cnf, "wifi", "password");

	ret = read_config_value_default(mqttConfig.need_reset, cnf, "mqtt", "need_reset", "true");
	ret = read_config_value_default(mqttConfig.need_log_error, cnf, "mqtt", "need_log_error", "true");
// #ifdef HOST

	char server_url[CONFIG_ITEM_LEN];
	memset(server_url, '\0', CONFIG_ITEM_LEN);
#if 1
	// ret = read_config_value_default(server_url, cnf, "mqtt", "server_url", "10.0.14.61:9883");
	ret = read_config_value(server_url, cnf, "mqtt", "server_url");
	// ret = read_config_value_default(mqttConfig.server_ip, cnf, "mqtt", "server_ip", "10.0.14.61");
	// ret = read_config_value_default(mqttConfig.server_port, cnf, "mqtt", "server_port", "9883");
#else
	ret = read_config_value_default(mqttConfig.server_url, cnf, "mqtt", "server_url", "47.107.126.154:1883");
	// ret = read_config_value_default(mqttConfig.server_ip, cnf, "mqtt", "server_ip", "47.107.126.154");
	// ret = read_config_value_default(mqttConfig.server_port, cnf, "mqtt", "server_port", "1883");
#endif
	mysplit(server_url, mqttConfig.server_ip, mqttConfig.server_port, ":");

	// ret = read_config_value(mqttConfig.client_id, cnf, "mqtt", "client_id");
	ret = read_config_value_default(mqttConfig.client_id, cnf, "mqtt", "client_id", btWifiConfig.wifi_mac);
	// ret = read_config_value_default(mqttConfig.username, cnf, "mqtt", "username", btWifiConfig.wifi_mac);
	// ret = read_config_value_default(mqttConfig.password, cnf, "mqtt", "password", btWifiConfig.wifi_mac);
	ret = read_config_value(mqttConfig.username, cnf, "mqtt", "username");
	ret = read_config_value(mqttConfig.password, cnf, "mqtt", "password");

	ret = read_config_value(mqttConfig.sub_topic_cmd, cnf, "sub_topic", "sub_topic_cmd");
	ret = read_config_value(mqttConfig.sub_topic_time_sync, cnf, "sub_topic", "sub_topic_time_sync");
	
	ret = read_config_value(mqttConfig.pub_topic_heartbeat, cnf, "pub_topic", "pub_topic_heartbeat");
	ret = read_config_value(mqttConfig.pub_topic_cmd_res, cnf, "pub_topic", "pub_topic_cmd_res");
	ret = read_config_value(mqttConfig.pub_topic_pic_report_u, cnf, "pub_topic", "pub_topic_pic_report_u");
	ret = read_config_value(mqttConfig.pub_topic_pic_report, cnf, "pub_topic", "pub_topic_pic_report");
	ret = read_config_value(mqttConfig.pub_topic_last_will, cnf, "pub_topic", "pub_topic_last_will");
	ret = read_config_value(mqttConfig.pub_topic_time_sync, cnf, "pub_topic", "pub_topic_time_sync");
	ret = read_config_value(mqttConfig.pub_topic_record_request, cnf, "pub_topic", "pub_topic_record_request");

	cnf_write_file(cnf, file, "");

  	destroy_config(&cnf); // 销毁Config对象

	return 0;
}
	
int print_w7_config(void) {
	printf("version config:\n");
	printf("\tsys_ver: \t%s\n", versionConfig.sys_ver);
	printf("\tbt_ver: \t%s\n", versionConfig.bt_ver);
	printf("\tx7_ver: \t%s\n", versionConfig.x7_ver);
	printf("\tmcu_ver: \t%s\n", versionConfig.mcu_ver);

	printf("bt wifi config:\n");
	printf("\tbt_mac: \t%s\n", btWifiConfig.bt_mac);
	printf("\twifi_mac: \t%s\n", btWifiConfig.wifi_mac);
	printf("\tssid: \t%s\n", btWifiConfig.ssid);
	printf("\tpassword: \t%s\n", btWifiConfig.password);

	printf("mqtt config:\n");
	printf("\tneed_reset: \t%s\n", mqttConfig.need_reset);
	printf("\tneed_log_error: \t%s\n", mqttConfig.need_log_error);
	printf("\tserver_ip: \t%s\n", mqttConfig.server_ip);
	printf("\tserver_port: \t%s\n", mqttConfig.server_port);
	printf("\tclient_ip: \t%s\n", mqttConfig.client_id);
	printf("\tusername: \t%s\n", mqttConfig.username);
	printf("\tpassword: \t%s\n", mqttConfig.password);
	printf("\tsub_topic_cmd: \t%s\n", mqttConfig.sub_topic_cmd);
	printf("\tsub_topic_time_sync: \t%s\n", mqttConfig.sub_topic_time_sync);
	printf("\tpub_topic_heartbeat: \t%s\n", mqttConfig.pub_topic_heartbeat);
	printf("\tpub_topic_cmd_res: \t%s\n", mqttConfig.pub_topic_cmd_res);
	printf("\tpub_topic_pic_report_u: \t%s\n", mqttConfig.pub_topic_pic_report_u);
	printf("\tpub_topic_pic_report: \t%s\n", mqttConfig.pub_topic_pic_report);
	printf("\tpub_topic_last_will: \t%s\n", mqttConfig.pub_topic_last_will);
	printf("\tpub_topic_time_sync: \t%s\n", mqttConfig.pub_topic_time_sync);
	printf("\tpub_topic_record_request: \t%s\n", mqttConfig.pub_topic_record_request);

	return 0;
}

#if 0
int main(int argc, char **argv) {
	printf("%lu %lu\r\n", sizeof(BTWIFICONFIG), sizeof(MQTTCONFIG));
	// int ret = verify_file_size(argv[1]);
	read_config(argv[1]);
}
#endif
