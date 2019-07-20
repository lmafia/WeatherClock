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
#include "user_interface.h"
#include <string.h>
#include "mqtt_msg.h"
#include "user_config.h"
#define MQTT_MAX_FIXED_HEADER_SIZE 3

#include "osapi.h"

// 【CONNECT】控制报文的Flag标志位
//---------------------------------------------------------------
enum mqtt_connect_flag
{
  MQTT_CONNECT_FLAG_USERNAME = 1 << 7,			// Username
  MQTT_CONNECT_FLAG_PASSWORD = 1 << 6,			// Password
  MQTT_CONNECT_FLAG_WILL_RETAIN = 1 << 5,		// Will Retain
  MQTT_CONNECT_FLAG_WILL = 1 << 2,				// Will Flag
  MQTT_CONNECT_FLAG_CLEAN_SESSION = 1 << 1		// Clean Session
};

// 【CONNECT】控制报文的可变头长度 == 12Byte(v31)/10Byte(v311)
//-----------------------------------------------------------------
struct __attribute((__packed__)) mqtt_connect_variable_header
{
  uint8_t lengthMsb;					// 协议名[MQlsdp/MQTT]长度
  uint8_t lengthLsb;					// 6/4
#if defined(PROTOCOL_NAMEv31)
  uint8_t magic[6];						// MQIsdp
#elif defined(PROTOCOL_NAMEv311)
  uint8_t magic[4];						// "MQTT"
#else
#error "Please define protocol name"
#endif
  uint8_t version;						// 协议版本
  uint8_t flags;						// 连接标志
  uint8_t keepaliveMsb;					// keepalive
  uint8_t keepaliveLsb;
};

// 将[参数字符串]字段添加到报文缓存区，报文长度+=[参数字符串]所占长度	【注：字符串前需添加两字节的前缀】
//========================================================================================================
static int ICACHE_FLASH_ATTR append_string(mqtt_connection_t* connection, const char* string, int len)
{
  if(connection->message.length + len + 2 > connection->buffer_length)	// 判断报文是否过长
  return -1;

  // 设置字符串前的两字节前缀，表示此字符串的长度
  //--------------------------------------------------------------------------
  connection->buffer[connection->message.length++] = len >> 8;		// 高八位
  connection->buffer[connection->message.length++] = len & 0xff;	// 低八位

  memcpy(connection->buffer+connection->message.length, string, len);	// 将[参数字符串]添加到报文缓存区

  connection->message.length += len;	// 报文长度 += [参数字符串]所占长度

  return len + 2;	// 返回[参数字符串]在MQTT控制报文中所占长度
}
//========================================================================================================


// 添加【报文标识符】字段
//========================================================================================================
static uint16_t ICACHE_FLASH_ATTR append_message_id(mqtt_connection_t* connection, uint16_t message_id)
{
  // If message_id is zero then we should assign one, otherwise
  // we'll use the one supplied by the caller
  while(message_id == 0)
    message_id = ++connection->message_id;	// 【报文标识符】++

  if(connection->message.length + 2 > connection->buffer_length)	// 报文过长
    return 0;

  connection->buffer[connection->message.length++] = message_id >> 8;	// 添加【报文标识符】字段
  connection->buffer[connection->message.length++] = message_id & 0xff;	// 2字节

  return message_id;	// 返回【报文标识符】
}
//========================================================================================================


// 设置报文长度 = 3(暂时设为【固定报头】长度(3)，之后添加【可变报头】、【有效载荷】)
//====================================================================================
static int ICACHE_FLASH_ATTR init_message(mqtt_connection_t* connection)
{
  connection->message.length = MQTT_MAX_FIXED_HEADER_SIZE;	// 报文长度 = 3(固定报头)
  return MQTT_MAX_FIXED_HEADER_SIZE;
}
//====================================================================================

// 报文无效
//====================================================================================
static mqtt_message_t* ICACHE_FLASH_ATTR fail_message(mqtt_connection_t* connection)
{
  connection->message.data = connection->buffer;	// 报文指针指向报文缓存区首地址
  connection->message.length = 0;					// 报文长度 = 0
  return &connection->message;
}
//====================================================================================

// 设置【MQTT控制报文】的固定报头
//------------------------------------------------------------------------------------------------------------------------------
// 注：	在【PUBLISH】报文中，报文类型标志位由重复分发标志[dup][Bit3]、服务质量[qos][Bit2～1]、报文保留标志[retain][Bit1=0]组成。
//		其余类型报文的报文类型标志位是固定的。
//==============================================================================================================================
static mqtt_message_t* ICACHE_FLASH_ATTR fini_message(mqtt_connection_t* connection, int type, int dup, int qos, int retain)
{
  int remaining_length = connection->message.length - MQTT_MAX_FIXED_HEADER_SIZE;	// 获取【可变报头】+【有效载荷】长度

  // 设置固定报头(固定头中的剩余长度使用变长度编码方案，详情请参考MQTT协议手册)
  //----------------------------------------------------------------------------------------------------------
  if(remaining_length > 127)	// 【可变报头】+【有效载荷】长度大于127，剩余长度占2字节
  {
    connection->buffer[0] = ((type&0x0f)<<4)|((dup&1)<<3)|((qos&3)<<1)|(retain&1);	// 固定头的首字节赋值

    connection->buffer[1] = 0x80 | (remaining_length % 128);	// 剩余长度的第一个字节  最高位固定1？？
    connection->buffer[2] = remaining_length / 128;				// 剩余长度的第二个字节

    connection->message.length = remaining_length + 3;			// 报文的整个长度
    connection->message.data = connection->buffer;				// MQTT报文指针 -> 出站报文缓存区首地址
  }
  else	//if(remaining_length<= 127) // 剩余长度占1字节
  {
	// 			buffer[0] = 无
    connection->buffer[1] = ((type&0x0f)<<4)|((dup&1)<<3)|((qos&3)<<1)|(retain&1);	// 固定头的首字节赋值

    connection->buffer[2] = remaining_length;			// 固定头中的[剩余长度](可变报头+负载数据)

    connection->message.length = remaining_length + 2;	// 报文的整个长度  这个2就是固定+剩余长度
    connection->message.data = connection->buffer + 1;	// MQTT报文指针 -> 出站报文缓存区首地址+1
  }

  return &connection->message;		// 返回报文首地址【报文数据、报文整体长度】
}
//==============================================================================================================================

// 初始化MQTT报文缓存区
// mqtt_msg_init(&client->mqtt_state.mqtt_connection, client->mqtt_state.out_buffer, client->mqtt_state.out_buffer_length);
//==========================================================================================================================
void ICACHE_FLASH_ATTR mqtt_msg_init(mqtt_connection_t* connection, uint8_t* buffer, uint16_t buffer_length)
{
  memset(connection, 0, sizeof(mqtt_connection_t));	// mqttClient->mqtt_state.mqtt_connection = 0
  connection->buffer = buffer;							// buffer = (uint8_t *)os_zalloc(1024)			【缓存区指针】
  connection->buffer_length = buffer_length;			// buffer_length = 1024							【缓存区长度】
}
//==========================================================================================================================


// 计算接收到的网络数据中，报文的实际长度(通过【剩余长度】得到)
//===============================================================================================
int ICACHE_FLASH_ATTR mqtt_get_total_length(uint8_t* buffer, uint16_t length)
{
  int i;
  int totlen = 0;	// 报文总长度(字节数)

  // 计算剩余长度的值
  //--------------------------------------------------------------------
  for(i = 1; i <length; ++i)	// 解析【剩余长度】字段(从buffer[1]开始)
  {
	  totlen += (buffer[i]&0x7f)<<(7*(i-1));

	  // 如果剩余长度的当前字节的值<0x80，则表示此字节是最后的字节
	  //-----------------------------------------------------------------------
	  if((buffer[i]&0x80) == 0)		// 当前字节的值<0x80
	  {
		  ++i;		// i == 固定报头长度 == 报文类型(1字节) + 剩余长度字段

		  break;	// 跳出循环
	  }
  }

  totlen += i;		// 报文总长度 = 固定报头长度 + 剩余长度的值(【可变报头】+【有效载荷】的长度)

  return totlen;	// 返回报文总长度
}
//===============================================================================================


// 获取【PUBLISH】报文中的主题名(指针)、主题名长度
//=========================================================================================
const char* ICACHE_FLASH_ATTR mqtt_get_publish_topic(uint8_t* buffer, uint16_t* length)
{
	int i;
	int totlen = 0;
	int topiclen;

	// 计算剩余长度的值
	//--------------------------------------------------------------------
	for(i = 1; i<*length; ++i)	// 解析【剩余长度】字段(从buffer[1]开始)
	{
		totlen += (buffer[i]&0x7f)<<(7*(i-1));

		// 如果剩余长度的当前字节的值<0x80，则表示此字节是最后的字节
		//-----------------------------------------------------------------
		if((buffer[i] & 0x80) == 0)		// 当前字节的值<0x80
		{
			++i;	// i == 固定报头长度 == 报文类型(1字节) + 剩余长度字段

			break;	// 跳出循环
		}
	}

	totlen += i;	// 报文总长度（这句可删，没有用）


	if(i + 2 >= *length)	// 如果没有载荷，则返回NULL
		return NULL;

	topiclen = buffer[i++] << 8;	// 获取主题名长度(2字节)
	topiclen |= buffer[i++];		// 前缀

	if(i + topiclen > *length)	// 报文出错(没有主题名)，返回NULL
		return NULL;

	*length = topiclen;		// 返回主题名长度

	return (const char*)(buffer+i);	// 返回主题名(指针)
}
//=========================================================================================


// 获取【PUBLISH】报文的载荷(指针)、载荷长度
//========================================================================================================
const char* ICACHE_FLASH_ATTR mqtt_get_publish_data(uint8_t* buffer, uint16_t* length)
{
	int i;
	int totlen = 0;
	int topiclen;
	int blength = *length;
	*length = 0;

	// 计算剩余长度的值
	//--------------------------------------------------------------------
	for(i = 1; i<blength; ++i)	// 解析【剩余长度】字段(从buffer[1]开始)
	{
		totlen += (buffer[i]&0x7f)<<(7*(i-1));

		// 如果剩余长度的当前字节的值<0x80，则表示此字节是最后的字节
		//-----------------------------------------------------------------
		if((buffer[i] & 0x80) == 0)
		{
			++i;	// i == 固定报头长度 == 报文类型(1字节) + 剩余长度字段

			break;	// 跳出循环
		}
	}

	totlen += i;	// 报文总长度 = 剩余长度表示的值 + 固定头所占字节


	if(i + 2 >= blength)	// 如果没有载荷，则返回NULL
		return NULL;

	topiclen = buffer[i++] << 8;	// 获取主题名长度(2字节)
	topiclen |= buffer[i++];		// 前缀

	if(i+topiclen >= blength)		// 报文出错(没有主题名)，返回NULL
		return NULL;

	i += topiclen;	// i = 【固定报头】+【主题名(包括前缀)】

	// Qos > 0
	//----------------------------------------------------------------------------
	if(mqtt_get_qos(buffer)>0)	// 当Qos>0时，【主题名】字段后面是【报文标识符】
	{
		if(i + 2 >= blength)	// 报文错误（无载荷）
			return NULL;

		i += 2;			// i = 【固定报头】+【可变报头】
	}

	if(totlen < i)		// 报文错误，返回NULL
		return NULL;

	if(totlen <= blength)		// 报文总长度 <= 网络接收数据长度
		*length = totlen - i;	// 【有效载荷】长度 = 报文长度- (【固定报头】长度+【可变报头】长度)

	else						// 报文总长度 > 网络接收数据长度【丢失数据/未接收完毕】
		*length = blength - i;	// 有效载荷长度 = 网络接收数据长度 - (【固定报头】长度+【可变报头】长度)

	return (const char*)(buffer + i);	// 返回有效载荷首地址
}
//========================================================================================================


// 获取报文中的【报文标识符】
//=========================================================================================
uint16_t ICACHE_FLASH_ATTR mqtt_get_id(uint8_t* buffer, uint16_t length)
{
  if(length < 1)	// 报文无效
  return 0;

  // 判断目标报文的类型
  //----------------------------
  switch(mqtt_get_type(buffer))
  {
  	// 【PUBLISH】报文
	//…………………………………………………………………………………………………………
    case MQTT_MSG_TYPE_PUBLISH:
    {
      int i;
      int topiclen;

      for(i = 1; i < length; ++i)		// 查找【有效载荷】首地址
      {
        if((buffer[i] & 0x80) == 0)		// 判断当前字节是否为【剩余长度】的尾字节
        {
          ++i;		// 指向有效载荷首地址
          break;
        }
      }

      if(i + 2 >= length)	// 报文错误（无主题名）
        return 0;

      topiclen = buffer[i++] << 8;	// 主题名长度
      topiclen |= buffer[i++];

      if(i + topiclen >= length)	// 报文错误
        return 0;

      i += topiclen;				// 指向【报文标识符】/【有效载荷】

      // Qos > 0
      //----------------------------------------------------------------------------
      if(mqtt_get_qos(buffer) > 0)	// 当Qos>0时，【主题名】字段后面是【报文标识符】
      {
        if(i + 2 >= length)		// 报文错误（无载荷）
          return 0;

        //i += 2;				// &buffer[i]指向【报文标识符】
      }

      else 	// Qos == 0
      {
    	  return 0;		// 当Qos==0时，没有【报文标识符】
      }

      return (buffer[i] << 8) | buffer[i + 1];		//获取【报文标识符】
	}
    //…………………………………………………………………………………………………………


    // 以下类型的报文，【报文标识符】固定是控制报文的【第2、第3字节】
    //--------------------------------------------------------------------------------
    case MQTT_MSG_TYPE_PUBACK:		// 【PUBACK】报文
    case MQTT_MSG_TYPE_PUBREC:		// 【PUBREC】报文
    case MQTT_MSG_TYPE_PUBREL:		// 【PUBREL】报文
    case MQTT_MSG_TYPE_PUBCOMP:		// 【PUBCOMP】报文
    case MQTT_MSG_TYPE_SUBACK:		// 【SUBACK】报文
    case MQTT_MSG_TYPE_UNSUBACK:	// 【UNSUBACK】报文
    case MQTT_MSG_TYPE_SUBSCRIBE:	// 【SUBSCRIBE】报文
    {
      // This requires the remaining length to be encoded in 1 byte,
      // which it should be.
      if(length >= 4 && (buffer[1] & 0x80) == 0)	// 判断【固定头、可变头】是否正确
        return (buffer[2] << 8) | buffer[3];		// 返回【报文标识符】
      else
        return 0;
    }
    //--------------------------------------------------------------------------------


    // 【CONNECT】【CONNACK】【UNSUBSCRIBE】【PINGREQ】【PINGRESP】【DISCONNECT】
    //---------------------------------------------------------------------------
    default:
      return 0;
  }
}
//=========================================================================================


// 配置【CONNECT】控制报文
// mqtt_msg_connect(&client->mqtt_state.mqtt_connection, client->mqtt_state.connect_info)
//================================================================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_connect(mqtt_connection_t* connection, mqtt_connect_info_t* info)
{
  struct mqtt_connect_variable_header* variable_header;		// 【CONNECT】报文的【可变报头】指针

  init_message(connection);		// 设置报文长度 = 3(暂时设为【固定报头】长度(3)，之后添加【可变报头】、【有效载荷】)

  // 判断消息长度是否超过缓存区长度						// 【注：[message.length]是指TCP传输的整个MQTT报文长度】
  //------------------------------------------------------------------------------------------------------------
  if(connection->message.length + sizeof(*variable_header) > connection->buffer_length)		// 判断MQTT报文长度
    return fail_message(connection);

  // 跳过了对【固定报头】的赋值，只为【固定报头】保留了3个字节的空间。	注：剩余长度最多占两字节。

  // 获取【可变报头】指针，并更新报文长度
  //----------------------------------------------------------------------------------------------------------------------------
  variable_header = (void*)(connection->buffer + connection->message.length);	// 【可变报头】指针 = 报文缓存区指针+3(固定报头)
  connection->message.length += sizeof(*variable_header);						// 报文长度 == 固定报头 + 可变报头


  // 协议名、协议级别赋值
  //-----------------------------------------------
  variable_header->lengthMsb = 0;	// lengthMsb
#if defined(PROTOCOL_NAMEv31)
  variable_header->lengthLsb = 6;	// lengthLsb
  memcpy(variable_header->magic, "MQIsdp", 6);
  variable_header->version = 3;		// v31版本 = 3

#elif defined(PROTOCOL_NAMEv311)
  variable_header->lengthLsb = 4;	// lengthLsb
  memcpy(variable_header->magic, "MQTT", 4);
  variable_header->version = 4;		// v311版本 = 4
#else
#error "Please define protocol name"
#endif


  //----------------------------------------------------------------------
  variable_header->flags = 0;	// 连接标志字节 = 0（暂时清0，待会赋值）

  // 保持连接时长赋值
  //----------------------------------------------------------------------
  variable_header->keepaliveMsb = info->keepalive >> 8;		// 赋值高字节
  variable_header->keepaliveLsb = info->keepalive & 0xff;	// 赋值低字节


  // clean_session = 1：客户端和服务端必须丢弃之前的任何会话并开始一个新的会话
  //----------------------------------------------------------------------------
  if(info->clean_session)
    variable_header->flags |= MQTT_CONNECT_FLAG_CLEAN_SESSION; //clean_session=1


  // 判断是否存在[client_id]，存在则设置[client_id]字段
  //----------------------------------------------------------------------------
  if(info->client_id != NULL && info->client_id[0] != '\0')
  {
	// 将[client_id]字段添加到报文缓存区，报文长度+=[client_id]所占长度
	//--------------------------------------------------------------------------
	if(append_string(connection, info->client_id, strlen(info->client_id)) < 0)
	return fail_message(connection);
  }
  else
    return fail_message(connection);	// 报文出错

  // 判断是否存在[will_topic]
  //---------------------------------------------------------------------------------
  if(info->will_topic != NULL && info->will_topic[0] != '\0')
  {
	// 将[will_topic]字段添加到报文缓存区，报文长度+=[will_topic]所占长度
	//--------------------------------------------------------------------------
	if(append_string(connection, info->will_topic,strlen(info->will_topic))<0)
    return fail_message(connection);

	// 将[will_message]字段添加到报文缓存区，报文长度+=[will_message]所占长度
	//----------------------------------------------------------------------------
	if(append_string(connection,info->will_message,strlen(info->will_message))<0)
    return fail_message(connection);

	// 设置【CONNECT】报文中的Will标志位：[Will Flag]、[Will QoS]、[Will Retain]
	//--------------------------------------------------------------------------
	variable_header->flags |= MQTT_CONNECT_FLAG_WILL;		// 遗嘱标志位 = 1
	if(info->will_retain)
    variable_header->flags |= MQTT_CONNECT_FLAG_WILL_RETAIN;// WILL_RETAIN = 1
	variable_header->flags |= (info->will_qos & 3) << 3;	// will质量赋值
  }

  // 判断是否存在[username]
  //----------------------------------------------------------------------------
  if(info->username != NULL && info->username[0] != '\0')
  {
	// 将[username]字段添加到报文缓存区，报文长度+=[username]所占长度
	//--------------------------------------------------------------------------
    if(append_string(connection, info->username, strlen(info->username)) < 0)
      return fail_message(connection);

    // 设置【CONNECT】报文中的[username]标志位
    //--------------------------------------------------------------------------
    variable_header->flags |= MQTT_CONNECT_FLAG_USERNAME;	// username = 1
  }

  // 判断是否存在[password]
  //----------------------------------------------------------------------------
  if(info->password != NULL && info->password[0] != '\0')
  {
	// 将[password]字段添加到报文缓存区，报文长度+=[password]所占长度
	//--------------------------------------------------------------------------
    if(append_string(connection, info->password, strlen(info->password)) < 0)
      return fail_message(connection);

    // 设置【CONNECT】报文中的[password]标志位
    //--------------------------------------------------------------------------
    variable_header->flags |= MQTT_CONNECT_FLAG_PASSWORD;	// password = 1
  }

  // 设置【CONNECT】报文固定头：类型[Bit7～4=1]、[Bit3=0]、[Bit2～1=0]、[Bit1=0]
  //----------------------------------------------------------------------------
  return fini_message(connection, MQTT_MSG_TYPE_CONNECT, 0, 0, 0);
}
//================================================================================================================================

// 配置【PUBLISH】报文，并获取【PUBLISH】报文[指针]、[长度]
// 【参数2：主题名 / 参数3：发布消息的有效载荷 / 参数4：有效载荷长度 / 参数5：发布Qos / 参数6：Retain / 参数7：报文标识符指针】
//===================================================================================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR
mqtt_msg_publish(mqtt_connection_t* connection, const char* topic, const char* data, int data_length, int qos, int retain, uint16_t* message_id)
{
  init_message(connection);		// 设置报文长度 = 3

  if(topic == NULL || topic[0] == '\0')		// 没有"topic"则错误
    return fail_message(connection);

  if(append_string(connection, topic, strlen(topic)) < 0)	// 添加【主题】字段
    return fail_message(connection);

  if(qos > 0)	// 当【QoS>0】，需要【报文标识符】
  {
    if((*message_id = append_message_id(connection, 0)) == 0)	// 添加【报文标识符】字段
      return fail_message(connection);
  }
  else	// if(qos = 0)	// 当【QoS=0】，不需要【报文标识符】
    *message_id = 0;

  // 判断报文长度是否大于缓存区
  //----------------------------------------------------------------------------
  if(connection->message.length + data_length > connection->buffer_length)
    return fail_message(connection);

  memcpy(connection->buffer + connection->message.length, data, data_length);	// 添加【有效载荷】字段
  connection->message.length += data_length;	// 设置报文长度


  // 设置【PUBLISH】报文固定头：类型[Bit7～4=1]、[Bit3=0]、[Bit2～1=0]、[Bit1=0]
  //----------------------------------------------------------------------------
  return fini_message(connection, MQTT_MSG_TYPE_PUBLISH, 0, qos, retain);
}
//===================================================================================================================================================

// 配置【PUBACK】报文
//=====================================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_puback(mqtt_connection_t* connection, uint16_t message_id)
{
  init_message(connection);		// 设置报文长度 = 3

  if(append_message_id(connection, message_id) == 0)	// 添加【报文标识符】字段
    return fail_message(connection);

  return fini_message(connection, MQTT_MSG_TYPE_PUBACK, 0, 0, 0);	// 配置固定报头
}
//=====================================================================================================


// 配置【PUBREC】报文
//=====================================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pubrec(mqtt_connection_t* connection, uint16_t message_id)
{
  init_message(connection);		// 设置报文长度 = 3

  if(append_message_id(connection, message_id) == 0)	// 添加【报文标识符】字段
    return fail_message(connection);

  return fini_message(connection, MQTT_MSG_TYPE_PUBREC, 0, 0, 0);	// 配置固定报头
}
//=====================================================================================================


// 配置【PUBREL】报文
//=====================================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pubrel(mqtt_connection_t* connection, uint16_t message_id)
{
  init_message(connection);		// 设置报文长度 = 3

  if(append_message_id(connection, message_id) == 0)	// 添加【报文标识符】字段
    return fail_message(connection);

  return fini_message(connection, MQTT_MSG_TYPE_PUBREL, 0, 1, 0);	// 配置固定报头
}
//=====================================================================================================


// 配置【PUBCOMP】报文
//=====================================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pubcomp(mqtt_connection_t* connection, uint16_t message_id)
{
  init_message(connection);		// 设置报文长度 = 3

  if(append_message_id(connection, message_id) == 0)	// 添加【报文标识符】字段
    return fail_message(connection);

  return fini_message(connection, MQTT_MSG_TYPE_PUBCOMP, 0, 0, 0);	// 配置固定报头
}
//=====================================================================================================


// 配置【SUBSCRIBE】报文：【参数2：订阅主题 / 参数3：订阅质量 / 参数4：报文标识符】
//=======================================================================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_subscribe(mqtt_connection_t* connection, const char* topic, int qos, uint16_t* message_id)
{
  init_message(connection);		// 报文长度 = 3(固定头)

  if(topic == NULL || topic[0] == '\0')		// 主题无效
    return fail_message(connection);

  if((*message_id = append_message_id(connection, 0)) == 0)	// 添加[报文标识符]
    return fail_message(connection);

  if(append_string(connection, topic, strlen(topic)) < 0)	// 添加[主题]
    return fail_message(connection);

  if(connection->message.length + 1 > connection->buffer_length)// 判断报文长度
    return fail_message(connection);

  connection->buffer[connection->message.length++] = qos;	// 设置消息质量QoS


  // 设置【SUBSCRIBE】报文的固定报头
  //-----------------------------------------------------------------
  return fini_message(connection, MQTT_MSG_TYPE_SUBSCRIBE, 0, 1, 0);
}
//=======================================================================================================================================


// 配置【UNSUBSCRIBE】报文：【参数2：取消订阅主题 /  参数3：报文标识符】
//===============================================================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_unsubscribe(mqtt_connection_t* connection, const char* topic, uint16_t* message_id)
{
	init_message(connection);		// 报文长度 = 3(固定头)

	if(topic == NULL || topic[0] == '\0')		// 主题无效
		return fail_message(connection);

	if((*message_id = append_message_id(connection, 0)) == 0)		// 添加[报文标识符]
		return fail_message(connection);

	if(append_string(connection, topic, strlen(topic)) < 0)		// 添加[主题]
		return fail_message(connection);

	// 设置【UNSUBSCRIBE】报文的固定报头
	//-----------------------------------------------------------------
	return fini_message(connection, MQTT_MSG_TYPE_UNSUBSCRIBE, 0, 1, 0);
}
//===============================================================================================================================


// 配置【PINGREQ】报文
//=================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pingreq(mqtt_connection_t* connection)
{
  init_message(connection);
  return fini_message(connection, MQTT_MSG_TYPE_PINGREQ, 0, 0, 0);
}
//=================================================================================


// 配置【PINGRESP】报文
//=================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_pingresp(mqtt_connection_t* connection)
{
  init_message(connection);
  return fini_message(connection, MQTT_MSG_TYPE_PINGRESP, 0, 0, 0);
}
//=================================================================================

// 配置【DISCONNECT】报文
//====================================================================================
mqtt_message_t* ICACHE_FLASH_ATTR mqtt_msg_disconnect(mqtt_connection_t* connection)
{
  init_message(connection);
  return fini_message(connection, MQTT_MSG_TYPE_DISCONNECT, 0, 0, 0);
}
//====================================================================================
