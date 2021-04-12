#ifndef _MQTT_API_H_
#define _MQTT_API_H_

#define MQTT_LINK_ID_DEFAULT 		0

#define MQTT_SCHEME_TCP 						1
#define MQTT_SCHEME_TLS_NO_CERT					2
#define MQTT_SCHEME_TLS_SERVER_CERT				3
#define MQTT_SCHEME_TLS_CLIENT_CERT				4
#define MQTT_SCHEME_TLS_BOTH_CERT				5
#define MQTT_SCHEME_WEBSOCKET_TCP				6
#define MQTT_SCHEME_WEBSOCKET_TLS_NO_CERT		7
#define MQTT_SCHEME_WEBSOCKET_TLS_SERVER_CERT	8
#define MQTT_SCHEME_WEBSOCKET_TLS_CLIENT_CERT	9
#define MQTT_SCHEME_WEBSOCKET_TLS_BOTH_CERT		10		

#define MQTT_CERT_KEY_ID_DEFAULT	0
#define MQTT_CA_ID_DEFAULT			0

#define MQTT_STATE_UNINITIALIZED		0
#define MQTT_STATE_SET_BY_MQTTUSERCFG	1
#define MQTT_STATE_SET_BY_MQTTCONNCFG	2
#define MQTT_STATE_DISCONNECTED			3
#define MQTT_STATE_ESTABLISHED			4
#define MQTT_STATE_CONNECTED_TOPIC_NONE	5
#define MQTT_STATE_CONNECTED_TOPIC_SUB	6

#define MQTT_QOS_AT_MOST_ONCE			0
#define MQTT_QOS_AT_LEAST_ONCE			1
#define MQTT_QOS_EXACTLY_ONCE			2

#define MQTT_RETAIN_OFF		0
#define MQTT_RETAIN_ON		1

#define MQTT_AT_CMD_SUCCESS	0

#ifdef __cplusplus
extern "C" {
#endif
	// --------------------------- 主要接口 start ------------------------
	// 连接WIFI
	int connectWifi(const char* ssid, const char* password); 
	// 配置MQTT用户参数
	int setupMQTT(int linkId, int scheme, const char* clientId, const char* username, const char* password, int certKeyId, int caId, const char* path);
	// 配置MQTT连接参数
	int setupMQTTConnConfig(int linkId, int keepAlive, int disableCleanSession, const char* lwtTopic, const char* lwtMsg, int lwtQos, int lwtRetain); 

	// 连接MQTT服务器
	int connectMQTT(int linkId, const char* hostIp, int port, int reconnect);

	// 订阅MQTT主题
	int subscribeMQTT(int linkId, const char* topic, int qos);

	// 取消订阅MQTT主题
	int unsubscribeMQTT(int linkId, const char* topic);

	// 发布MQTT主题
	int publishMQTT(int linkId, const char* topic, const char* data, int qos, int retain);

	// 断开MQTT连接
	int disconnectMQTT(int linkId);
	// --------------------------- 主要接口 end ------------------------

	// --------------------------- 次要接口 start ------------------------
	/*
	// 配置MQTT ClientId
	int setupMQTTClientId(int linkId, const char* clientId);
	// 配置MQTT username 
	int setupMQTTUsername(int linkId, const char* username);
	// 配置MQTT password
	int setupMQTTPassword(int linkId, const char* password);
	// 配置MQTT Lastwill Connection
	int setupMQTTConnection(int linkId, int keepAlive, int disableCleanSession, const char* lwtTopic, const char* lwtMsg, int lwtQos, int lwtRetain);
	*/
	// --------------------------- 次要接口 end ------------------------

	// --------------------------- 辅助接口 start ------------------------
	int quickConnectMQTT(const char* clientId, const char* username, const char* password, const char* hostIp, const char* port, const char* lwtTopic, const char* lwtMsg);
	int quickSubscribeMQTT(const char* topic);
	int quickUnsubscribeMQTT(const char* topic);
	int quickPublishMQTT(const char* topic, const char* data);
	int quickDisconnectMQTT();
	// --------------------------- 辅助接口 end ------------------------
#ifdef __cplusplus
}
#endif

#endif
