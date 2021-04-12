#include <stdio.h>
#include "iniparser.h"
#include "config.h"
#include "configV2.h"

#include <fcntl.h>
#include <unistd.h>



WIFICONFIG wifiConfig;
MQTTCONFIG mqttConfig;

#if 0
unsigned long get_file_size(char *filename)  
{  
	struct stat buf;  
	if(stat(filename, &buf)<0)  
	{  
		return 0;  
	}  
	return (unsigned long)buf.st_size;  
} 

int verify_file_size(char *file) {
	unsigned long file_size = get_file_size(file);
	if (file_size > CONFIG_FILE_LEN) {
		printf("Config file %s is too large to load, which should be less than %lu\n", file, CONFIG_FILE_LEN);
		return -1;
	}
}

int iniparser_save(dictionary * d, char *inipath)  //自己实现的ini配置文档保存函数
{
	int ret = 0;
	FILE *fp = NULL;

	if (inipath == NULL || d == NULL) {
		ret = -1;
		printf("saveConfig error:%d from (filepath == NULL || head == NULL)\n",ret);
		return ret;
	}

	fp = fopen(inipath,"w");
	if (fp == NULL) {
		ret = -2;
		printf("saveConfig:open file error:%d from %s\n",ret,inipath);
		return ret;
	}

	iniparser_dump_ini(d,fp);

	fclose(fp);

	return 0;
}
#endif 

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
	update_option_if_not_exist(cnf, "wifi", "local_device_mac", mac);

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
	cnf_add_option(cnf, "wifi", "local_device_mac", mac);

	cnf_add_option(cnf, "mqtt", "client_id", mac);
	cnf_add_option(cnf, "mqtt", "username", mac);
	cnf_add_option(cnf, "mqtt", "password", mac);

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

int write_config(char *file, char *section, char *key, char *value) {
	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}
	cnf_add_option(cnf, section, key, value);
	cnf_write_file(cnf, file, "");
  	destroy_config(&cnf); // 销毁Config对象
	return 0;

#if 0
	dictionary *dic;
	if(NULL == (dic = iniparser_load(file))){
		printf("ERROR: open file failed!\n");
		return -1;
	}
	iniparser_set(dic, entry, value);
	iniparser_save(dic, file);

	//释放dic相关联的内存。该函数无返回值。
	iniparser_freedict(dic);
#endif

	return 0;
}

int write_default_config(char *file) {
	int fd;
	int wr_ret;
	int rd_ret;
	unsigned long file_size;
	// char *wr_buf = "[wifi]\r\nssid\t=\twave\r\npassword\t=\twave\r\n\r\n[mqtt]\r\nneed_reset\t=\ttrue\r\nneed_log_error\t=\ttrue\r\nserver_ip\t=\t10.0.14.62\r\nserver_port\t=\t1883\r\n";
	char *wr_buf = "[wifi]\r\nssid\t=\twireless_052E81\r\npassword\t=\t12345678\r\n\r\n[mqtt]\r\nneed_reset\t=\ttrue\r\nneed_log_error\t=\ttrue\r\nserver_ip\t=\t10.0.14.61\r\nserver_port\t=\t9883\r\n";

	fd = open(file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IROTH);//等价于fd = open("a.txt", O_RDWR | O_CREAT | O_TRUNC, 0x604);
	if(fd == -1)
	{
		perror("open file error:");//只有上面的函数设置了error全局错误号，才可使用，会根据error输出对应的错误信息
		printf("*****----------- open file %s error:", file);//只有上面的函数设置了error全局错误号，才可使用，会根据error输出对应的错误信息
		return -1;
	}
	printf("fd = %d\n", fd);
	wr_ret = write(fd, wr_buf, strlen(wr_buf));
	if(wr_ret == -1)
	{
		perror("write file error:");
		printf("*****----------- write file error: %s", file);
		close(fd);
		remove(file);
		return -1;

	}
	/* 将文件同步,及时写入磁盘,不然，系统重启动，导致文件内容丢失 */
	fsync(fd);	/* 将内核缓冲写入磁盘 */
	close(fd);
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
	memset(&wifiConfig, '\0', sizeof(WIFICONFIG));
	memset(&mqttConfig, '\0', sizeof(MQTTCONFIG));

	Config *cnf = cnf_read_config(file, '#', '=');
	if (NULL == cnf) {
		return -1; /* 创建对象失败 */
	}

	int ret = read_config_value(wifiConfig.local_device_mac, cnf, "wifi", "local_device_mac");
	ret = read_config_value_default(wifiConfig.ssid, cnf, "wifi", "ssid", "wireless_052E81");
	ret = read_config_value_default(wifiConfig.password, cnf, "wifi", "password", "12345678");

	ret = read_config_value_default(mqttConfig.need_reset, cnf, "mqtt", "need_reset", "true");
	ret = read_config_value_default(mqttConfig.need_log_error, cnf, "mqtt", "need_log_error", "true");
// #ifdef HOST
#if 1
	ret = read_config_value_default(mqttConfig.server_ip, cnf, "mqtt", "server_ip", "10.0.14.61");
	ret = read_config_value_default(mqttConfig.server_port, cnf, "mqtt", "server_port", "9883");
#else
	ret = read_config_value_default(mqttConfig.server_ip, cnf, "mqtt", "server_ip", "47.107.126.154");
	ret = read_config_value_default(mqttConfig.server_port, cnf, "mqtt", "server_port", "1883");
#endif
	ret = read_config_value(mqttConfig.client_id, cnf, "mqtt", "client_id");
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
	

#if 0
	dictionary *dic;
	if(NULL == (dic = iniparser_load(file))){
		printf("ERROR: open file %s failed!\n", file);
		if (write_default_config(file) == -1) {
			return -1;
		} else if (NULL == (dic = iniparser_load(file))) {
			printf("ERROR: second open file failed!\n");
			return -1;
		}
	}
	printf("ERROR: open file %s failed!\n", file);

	//向stdout 流中输出dic解析出的配置信息
	//这只是用于debug.
	iniparser_dump_ini(dic, stdout);
	strcpy(wifiConfig.local_device_mac, iniparser_getstring(dic, "wifi:local_device_mac", DEFAULT_MAC));
	strcpy(wifiConfig.ssid, iniparser_getstring(dic, "wifi:ssid", "wave"));
	strcpy(wifiConfig.password, iniparser_getstring(dic, "wifi:password", "wave"));

#if 0
	char server_ip[CONFIG_ITEM_LEN];
	int server_port;
	char client_id[CONFIG_ITEM_LEN];
	char username[CONFIG_ITEM_LEN];
	char password[CONFIG_ITEM_LEN];
	char sub_topic[CONFIG_ITEM_LEN*2];
	char pub_topic[CONFIG_ITEM_LEN*2];
#endif

	strcpy(mqttConfig.need_reset, iniparser_getstring(dic, "mqtt:need_reset", "true"));
	strcpy(mqttConfig.need_log_error, iniparser_getstring(dic, "mqtt:need_log_error", "true"));
	strcpy(mqttConfig.server_ip, iniparser_getstring(dic, "mqtt:server_ip", "192.168.0.1"));
	strcpy(mqttConfig.server_port, iniparser_getstring(dic, "mqtt:server_port", "1883"));
	strcpy(mqttConfig.client_id, iniparser_getstring(dic, "mqtt:client_id", wifiConfig.local_device_mac));
	strcpy(mqttConfig.username, iniparser_getstring(dic, "mqtt:username", wifiConfig.local_device_mac));
	strcpy(mqttConfig.password, iniparser_getstring(dic, "mqtt:password", wifiConfig.local_device_mac));
	strcpy(mqttConfig.sub_topic, iniparser_getstring(dic, "mqtt:sub_topic", ""));
	strcpy(mqttConfig.sub_topic_time_sync, iniparser_getstring(dic, "mqtt:sub_topic_time_sync", ""));
	// strcpy(mqttConfig.pub_topic, iniparser_getstring(dic, "mqtt:pub_topic", "testtopic"));
	strcpy(mqttConfig.pub_topic_heartbeat, iniparser_getstring(dic, "mqtt:pub_topic_heartbeat", ""));
	strcpy(mqttConfig.pub_topic_cmd_res, iniparser_getstring(dic, "mqtt:pub_topic_cmd_res", ""));
	strcpy(mqttConfig.pub_topic_pic_report, iniparser_getstring(dic, "mqtt:pub_topic_pic_report", ""));
	strcpy(mqttConfig.pub_topic_last_will, iniparser_getstring(dic, "mqtt:pub_topic_last_will", ""));
	strcpy(mqttConfig.pub_topic_time_sync, iniparser_getstring(dic, "mqtt:pub_topic_time_sync", ""));
	strcpy(mqttConfig.pub_topic_record_request, iniparser_getstring(dic, "mqtt:pub_topic_record_request", ""));
#endif

	printf("wifi config:\n");
	printf("\tlocal_device_mac: \t%s\n", wifiConfig.local_device_mac);
	printf("\tssid: \t%s\n", wifiConfig.ssid);
	printf("\tpassword: \t%s\n", wifiConfig.password);
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

#if 0
	//释放dic相关联的内存。该函数无返回值。
	iniparser_freedict(dic);
#endif

#if 0
	char buf[CONFIG_FILE_LEN];
	memset(buf, '\0', CONFIG_FILE_LEN);
	int fd = open(file,O_RDWR);
	int n = read(fd,buf,CONFIG_FILE_LEN);

	printf("the content is %s\n",buf);
#endif
  	destroy_config(&cnf); // 销毁Config对象

	return 0;
}

#if 0
int main(int argc, char **argv) {
	printf("%lu %lu\r\n", sizeof(WIFICONFIG), sizeof(MQTTCONFIG));
	// int ret = verify_file_size(argv[1]);
	read_config(argv[1]);
}
#endif
