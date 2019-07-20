/* 
 * File:   mqtt_msg.h
 * Author: Minh Tuan
 *
 * Created on July 12, 2014, 1:05 PM
 */

#ifndef MQTT_MSG_H
#define	MQTT_MSG_H
#include "mqtt_config.h"
#include "c_types.h"
#ifdef	__cplusplus
extern "C" {
#endif

/*
* Copyright (c) 2014, Stephen Robinson
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*/
/* 7			6			5			4			3			2			1			0*/
/*|      --- Message Type----			|  DUP Flag	|	   QoS Level		|	Retain	|
/*										Remaining Length								 */


// MQTT协议：控制报文的类型
//-----------------------------------------------------------------------------
enum mqtt_message_type
{
  MQTT_MSG_TYPE_CONNECT = 1,		// 客户端请求连接服务端	【客－＞服】
  MQTT_MSG_TYPE_CONNACK = 2,		// 连接报文确认			【服－＞客】
  MQTT_MSG_TYPE_PUBLISH = 3,		// 发布消息
  MQTT_MSG_TYPE_PUBACK = 4,			// 发布消息确认
  MQTT_MSG_TYPE_PUBREC = 5,			//
  MQTT_MSG_TYPE_PUBREL = 6,			//
  MQTT_MSG_TYPE_PUBCOMP = 7,		//
  MQTT_MSG_TYPE_SUBSCRIBE = 8,		// 客户端订阅请求		【客－＞服】
  MQTT_MSG_TYPE_SUBACK = 9,			// 订阅请求报文确认		【服－＞客】
  MQTT_MSG_TYPE_UNSUBSCRIBE = 10,	// 客户端取消订阅请求	【客－＞服】
  MQTT_MSG_TYPE_UNSUBACK = 11,		// 取消订阅报文确认		【服－＞客】
  MQTT_MSG_TYPE_PINGREQ = 12,		// 心跳请求				【客－＞服】
  MQTT_MSG_TYPE_PINGRESP = 13,		// 心跳响应				【服－＞客】
  MQTT_MSG_TYPE_DISCONNECT = 14		// 客户端断开连接		【客－＞服】
};

// MQTT控制报文[指针]、[长度]
//------------------------------------------------------------------------
typedef struct mqtt_message
{
  uint8_t* data;	// MQTT控制报文指针
  uint16_t length;	// MQTT控制报文长度(配置报文时，作为报文指针的偏移值)

} mqtt_message_t;
//------------------------------------------------------------------------

// MQTT报文
//------------------------------------------------------------------------
typedef struct mqtt_connection
{
  mqtt_message_t message;	// MQTT控制报文[指针]、[长度]

  uint16_t message_id;		// 报文标识符
  uint8_t* buffer;			// 出站报文缓存区指针	buffer = os_zalloc(1024)
  uint16_t buffer_length;	// 出站报文缓存区长度	1024

} mqtt_connection_t;
//------------------------------------------------------------------------

// MQTT【CONNECT】报文的连接参数
//---------------------------------------
typedef struct mqtt_connect_info
{
  char* client_id;		// 客户端标识符
  char* username;		// MQTT用户名
  char* password;		// MQTT密码
  char* will_topic;		// 遗嘱主题
  char* will_message;  	// 遗嘱消息
  int keepalive;		// 保持连接时长
  int will_qos;			// 遗嘱消息质量
  int will_retain;		// 遗嘱保留
  int clean_session;	// 清除会话
} mqtt_connect_info_t;
//---------------------------------------


static inline int ICACHE_FLASH_ATTR mqtt_get_type(uint8_t* buffer) { return (buffer[0] & 0xf0) >> 4; }
static inline int ICACHE_FLASH_ATTR mqtt_get_dup(uint8_t* buffer) { return (buffer[0] & 0x08) >> 3; }
static inline int ICACHE_FLASH_ATTR mqtt_get_qos(uint8_t* buffer) { return (buffer[0] & 0x06) >> 1; }
static inline int ICACHE_FLASH_ATTR mqtt_get_retain(uint8_t* buffer) { return (buffer[0] & 0x01); }

void ICACHE_FLASH_ATTR mqtt_msg_init(mqtt_connection_t* connection, uint8_t* buffer, uint16_t buffer_length);
int ICACHE_FLASH_ATTR mqtt_get_total_length(uint8_t* buffer, uint16_t length);
const char* ICACHE_FLASH_ATTR mqtt_get_publish_topic(uint8_t* buffer, uint16_t* length);
const char* ICACHE_FLASH_ATTR mqtt_get_publish_data(uint8_t* buffer, uint16_t* length);
uint16_t ICACHE_FLASH_ATTR mqtt_get_id(uint8_t* buffer, uint16_t length);

mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_connect(mqtt_connection_t* connection, mqtt_connect_info_t* info);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_publish(mqtt_connection_t* connection, const char* topic, const char* data, int data_length, int qos, int retain, uint16_t* message_id);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_puback(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pubrec(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pubrel(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pubcomp(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_subscribe(mqtt_connection_t* connection, const char* topic, int qos, uint16_t* message_id);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_unsubscribe(mqtt_connection_t* connection, const char* topic, uint16_t* message_id);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pingreq(mqtt_connection_t* connection);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pingresp(mqtt_connection_t* connection);
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_disconnect(mqtt_connection_t* connection);


#ifdef	__cplusplus
}
#endif

#endif	/* MQTT_MSG_H */

