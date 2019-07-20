#ifndef __MQTT_CONFIG_H__
#define __MQTT_CONFIG_H__

typedef enum{
  NO_TLS = 0,                       // 0: disable SSL/TLS, there must be no certificate verify between MQTT server and ESP8266
  TLS_WITHOUT_AUTHENTICATION = 1,   // 1: enable SSL/TLS, but there is no a certificate verify
  ONE_WAY_ANTHENTICATION = 2,       // 2: enable SSL/TLS, ESP8266 would verify the SSL server certificate at the same time
  TWO_WAY_ANTHENTICATION = 3,       // 3: enable SSL/TLS, ESP8266 would verify the SSL server certificate and SSL server would verify ESP8266 certificate
}TLS_LEVEL;


/*IMPORTANT: the following configuration maybe need modified*/
/***********************************************************************************************************************************************************************************************************************************************************/
#define CFG_HOLDER    		0x66666663	// 持有人标识(只有更新此数值，系统参数才会更新)		/* Change this value to load default configurations */

/*DEFAULT CONFIGURATIONS*/
// 注：【MQTT协议规定：连接服务端的每个客户端都必须有唯一的客户端标识符（ClientId）】。如果两相同ID的客户端不断重连，就会进入互踢死循环
//--------------------------------------------------------------------------------------------------------------------------------------
#define MQTT_HOST			"jjfaedp.hedevice.com" 		// MQTT服务端域名/IP地址	// the IP address or domain name of your MQTT server or MQTT broker ,such as "mqtt.yourdomain.com"
#define MQTT_PORT       	6002    										// 网络连接端口号			// the listening port of your MQTT server or MQTT broker
#define MQTT_CLIENT_ID   	"525721931"	// 官方例程中是"Device_ID"		// 客户端标识符				// the ID of yourself, any string is OK,client would use this ID register itself to MQTT server
#define MQTT_USER        	"238008" 			// MQTT用户名				// your MQTT login name, if MQTT server allow anonymous login,any string is OK, otherwise, please input valid login name which you had registered
#define MQTT_PASS        	"hJ=BS3iTv=S7=kZrX4NxU=zrDc8=" 	// MQTT密码					// you MQTT login password, same as above

#define STA_SSID 			"lian"    	// WIFI名称					// your AP/router SSID to config your device networking
#define STA_PASS 			"88291858" 	// WIFI密码					// your AP/router password
#define STA_TYPE			AUTH_WPA2_PSK

#define DEFAULT_SECURITY	NO_TLS      		// 加密传输类型【默认不加密】	// very important: you must config DEFAULT_SECURITY for SSL/TLS

#define CA_CERT_FLASH_ADDRESS 		0x77   		// 【CA证书】烧录地址			// CA certificate address in flash to read, 0x77 means address 0x77000
#define CLIENT_CERT_FLASH_ADDRESS 	0x78 		// 【设备证书】烧录地址			// client certificate and private key address in flash to read, 0x78 means address 0x78000
/*********************************************************************************************************************************************************************************************************************************************************************************/


/*Please Keep the following configuration if you have no very deep understanding of ESP SSL/TLS*/
#define CFG_LOCATION    			0x79		// 系统参数的起始扇区	/* Please don't change or if you know what you doing */
#define MQTT_BUF_SIZE       		1024		// MQTT缓存大小
#define MQTT_KEEPALIVE        		120     	// 保持连接时长			/*second*/
#define MQTT_RECONNECT_TIMEOUT    	5    		// 重连超时时长			/*second*/

#define MQTT_SSL_ENABLE				0// SSL使能	//* Please don't change or if you know what you doing */

#define QUEUE_BUFFER_SIZE      		2048		// 消息队列的缓存大小

//#define PROTOCOL_NAMEv31    		// 使用MQTT协议【v31】版本		/*MQTT version 3.1 compatible with Mosquitto v0.15*/
#define PROTOCOL_NAMEv311      		// 使用MQTT协议【v311】版本		/*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/

#endif // __MQTT_CONFIG_H__
