/* mqtt.c
*  Protocol: http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "mqtt_msg.h"
#include "debug.h"
#include "user_config.h"
#include "mqtt.h"
#include "queue.h"

#define MQTT_TASK_PRIO           	2			// MQTT任务优先级
#define MQTT_TASK_QUEUE_SIZE    	1			// MQTT任务消息深度
#define MQTT_SEND_TIMOUT        	5			// MQTT发送超时

#ifndef QUEUE_BUFFER_SIZE
#define QUEUE_BUFFER_SIZE             2048		// 队列缓存 = 2048
#endif

unsigned char *default_certificate;			// 默认证书指针
unsigned int default_certificate_len = 0;		// 默认证书长度
unsigned char *default_private_key;			// 默认密钥指针
unsigned int default_private_key_len = 0;		// 默认密钥长度

os_event_t mqtt_procTaskQueue[MQTT_TASK_QUEUE_SIZE];	// MQTT任务结构体（定义为结构体数组的形式）


// 域名解析成功_回调
//=============================================================================================
LOCAL void ICACHE_FLASH_ATTR mqtt_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;		 // 获取TCP连接指针

    MQTT_Client* client = (MQTT_Client *)pConn->reverse; // 获取mqttClient指针

    if (ipaddr == NULL)		// 域名解析失败
    {
        INFO("DNS: Found, but got no ip, try to reconnect\r\n");

        client->connState = TCP_RECONNECT_REQ;	// TCP重连请求(等待5秒)

        return;
    }

    INFO("DNS: found ip %d.%d.%d.%d\n",		// 打印域名对应的IP地址
         *((uint8 *) &ipaddr->addr),
         *((uint8 *) &ipaddr->addr + 1),
         *((uint8 *) &ipaddr->addr + 2),
         *((uint8 *) &ipaddr->addr + 3));

    // 判断IP地址是否正确(?=0)
    //----------------------------------------------------------------------------------------
    if (client->ip.addr == 0 && ipaddr->addr != 0)	// 未保存IP地址：mqttClient->ip.addr == 0
    {
        os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);	// IP赋值

        // 根据安全类型，调用不同的TCP连接方式
        //-------------------------------------------------------------------------------------------------
        if (client->security)		// 安全类型 != 0
        {
#ifdef MQTT_SSL_ENABLE
            if(DEFAULT_SECURITY >= ONE_WAY_ANTHENTICATION )		// 单向认证【ONE_WAY_ANTHENTICATION = 2】
            {
                espconn_secure_ca_enable(ESPCONN_CLIENT,CA_CERT_FLASH_ADDRESS);
            }

            if(DEFAULT_SECURITY >= TWO_WAY_ANTHENTICATION)		// 双向认证【TWO_WAY_ANTHENTICATION = 3】
            {
                espconn_secure_cert_req_enable(ESPCONN_CLIENT,CLIENT_CERT_FLASH_ADDRESS);
            }

            espconn_secure_connect(client->pCon);				// 不认证【TLS_WITHOUT_AUTHENTICATION = 1】
#else
            INFO("TCP: Do not support SSL\r\n");
#endif
        }

        else	// 安全类型 = 0 = NO_TLS
        {
            espconn_connect(client->pCon);		// TCP连接(作为Client连接Server)
        }

        client->connState = TCP_CONNECTING;		// TCP正在连接

        INFO("TCP: connecting...\r\n");
    }

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);		// 安排任务MQTT_Task
}
//=============================================================================================


// ESP8266获取服务端【PUBLISH】报文的【主题】、【有效载荷】
//===================================================================================================================================
LOCAL void ICACHE_FLASH_ATTR deliver_publish(MQTT_Client* client, uint8_t* message, int length)
{
    mqtt_event_data_t event_data;

    event_data.topic_length = length;	// 主题名长度初始化
    event_data.topic = mqtt_get_publish_topic(message, &event_data.topic_length);	// 获取【PUBLISH】报文的主题名(指针)、主题名长度

    event_data.data_length = length;	// 有效载荷长度初始化
    event_data.data = mqtt_get_publish_data(message, &event_data.data_length);	// 获取【PUBLISH】报文的载荷(指针)、载荷长度


    // 进入【接收MQTT的[PUBLISH]数据】函数
    //-------------------------------------------------------------------------------------------------------------------------
    if (client->dataCb)
        client->dataCb((uint32_t*)client, event_data.topic, event_data.topic_length, event_data.data, event_data.data_length);
}
//===================================================================================================================================


// 向MQTT服务器发送【心跳】报文（报文不写入队列，TCP直接发送）
//===================================================================================================================================
void ICACHE_FLASH_ATTR mqtt_send_keepalive(MQTT_Client *client)
{
    INFO("\r\nMQTT: Send keepalive packet to %s:%d!\r\n", client->host, client->port);

    client->mqtt_state.outbound_message = mqtt_msg_pingreq(&client->mqtt_state.mqtt_connection);	// 设置【PINGREQ】报文
    client->mqtt_state.pending_msg_type = MQTT_MSG_TYPE_PINGREQ;
    client->mqtt_state.pending_msg_type = mqtt_get_type(client->mqtt_state.outbound_message->data);	// 获取报文类型
    client->mqtt_state.pending_msg_id = mqtt_get_id(client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);

    // TCP发送成功/5秒计时结束 => 报文发送结束(sendTimeout=0)
    //-------------------------------------------------------------------------
    client->sendTimeout = MQTT_SEND_TIMOUT;	// 发送MQTT报文时，sendTimeout=5

    INFO("MQTT: Sending, type: %d, id: %04X\r\n", client->mqtt_state.pending_msg_type, client->mqtt_state.pending_msg_id);

    err_t result = ESPCONN_OK;

    if (client->security)	// 安全类型 != 0
    {
#ifdef MQTT_SSL_ENABLE
        result = espconn_secure_send(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
#else
        INFO("TCP: Do not support SSL\r\n");
#endif
    }
    else 	// 安全类型 = 0 = NO_TLS
    {
        result = espconn_send(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
    }

    client->mqtt_state.outbound_message = NULL;		// 报文发送完后，清除出站报文指针


    if(ESPCONN_OK == result)	// 网络数据发送成功
    {
        client->keepAliveTick = 0;		// 心跳计数 = 0

        client->connState = MQTT_DATA;	// ESP8266当前状态 = MQTT传输数据

        system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task
    }

    else 	// 【心跳请求】发送失败
    {
        client->connState = TCP_RECONNECT_DISCONNECTING;		// TCP暂时断开(断开后会重连)

        system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task
    }
}
//===================================================================================================================================


/**
  * @brief  Delete tcp client and free all memory
  * @param  mqttClient: The mqtt client which contain TCP client
  * @retval None
  */
// 删除TCP连接、释放pCon内存、清除TCP连接指针
//=======================================================================
void ICACHE_FLASH_ATTR mqtt_tcpclient_delete(MQTT_Client *mqttClient)
{
    if (mqttClient->pCon != NULL)
    {
        INFO("Free memory\r\n");

        espconn_delete(mqttClient->pCon);			// 删除传输连接

        if (mqttClient->pCon->proto.tcp)			// 如果是TCP连接
            os_free(mqttClient->pCon->proto.tcp);	// 释放TCP内存

        os_free(mqttClient->pCon);	// 释放pCon内存

        mqttClient->pCon = NULL;	// 清除mqttClient->pCon指针
    }
}
//=======================================================================


/**
  * @brief  Delete MQTT client and free all memory
  * @param  mqttClient: The mqtt client
  * @retval None
  */
// 删除MQTT客户端，并释放所有相关内存
//=====================================================================================
void ICACHE_FLASH_ATTR mqtt_client_delete(MQTT_Client *mqttClient)
{
    mqtt_tcpclient_delete(mqttClient);	// 删除TCP连接、释放pCon内存、清除TCP连接指针

    if (mqttClient->host != NULL)
    {
        os_free(mqttClient->host);		// 释放主机(域名/IP)内存
        mqttClient->host = NULL;
    }

    if (mqttClient->user_data != NULL)
    {
        os_free(mqttClient->user_data);	// 释放用户数据内存
        mqttClient->user_data = NULL;
    }

    if(mqttClient->connect_info.client_id != NULL)
    {
        os_free(mqttClient->connect_info.client_id);// 释放client_id内存
        mqttClient->connect_info.client_id = NULL;
    }

    if(mqttClient->connect_info.username != NULL)
    {
        os_free(mqttClient->connect_info.username);	// 释放username内存
        mqttClient->connect_info.username = NULL;
    }

    if(mqttClient->connect_info.password != NULL)
    {
        os_free(mqttClient->connect_info.password);	// 释放password内存
        mqttClient->connect_info.password = NULL;
    }

    if(mqttClient->connect_info.will_topic != NULL)
    {
        os_free(mqttClient->connect_info.will_topic);// 释放will_topic内存
        mqttClient->connect_info.will_topic = NULL;
    }

    if(mqttClient->connect_info.will_message != NULL)
    {
        os_free(mqttClient->connect_info.will_message);// 释放will_message内存
        mqttClient->connect_info.will_message = NULL;
    }

    if(mqttClient->mqtt_state.in_buffer != NULL)
    {
        os_free(mqttClient->mqtt_state.in_buffer);	// 释放in_buffer内存
        mqttClient->mqtt_state.in_buffer = NULL;
    }

    if(mqttClient->mqtt_state.out_buffer != NULL)
    {
        os_free(mqttClient->mqtt_state.out_buffer);	// 释放out_buffer内存
        mqttClient->mqtt_state.out_buffer = NULL;
    }
}
//=============================================================================

/**
  * @brief  Client received callback function.
  * @param  arg: contain the ip link information
  * @param  pdata: received data
  * @param  len: the lenght of received data
  * @retval None
  */

// TCP成功接收网络数据：【服务端->客户端ESP8266】
//==================================================================================================================================================
void ICACHE_FLASH_ATTR mqtt_tcpclient_recv(void *arg, char *pdata, unsigned short len)
{
    uint8_t msg_type;	// 消息类型
    uint8_t msg_qos;	// 服务质量
    uint16_t msg_id;	// 消息ID

    struct espconn *pCon = (struct espconn*)arg;		// 获取TCP连接指针

    MQTT_Client *client = (MQTT_Client *)pCon->reverse;	// 获取mqttClient指针

    client->keepAliveTick = 0;	// 只要收到网络数据，客户端(ESP8266)心跳计数 = 0

// 读取数据包
//----------------------------------------------------------------------------------------------------------------------------
READPACKET:
    INFO("TCP: data received %d bytes\r\n",len);

// 【技小新】添加
//###############################################################################################################################
    INFO("TCP: data received %d,%d,%d,%d \r\n", *pdata,*(pdata+1),*(pdata+2),*(pdata+3));	// 查看【确认连接请求】报文的具体值	#
//###############################################################################################################################

    if (len<MQTT_BUF_SIZE && len>0)		// 接收到的数据长度在允许范围内
    {
        os_memcpy(client->mqtt_state.in_buffer, pdata, len);	// 获取接收数据，存入【入站报文缓存区】

        msg_type = mqtt_get_type(client->mqtt_state.in_buffer);	// 获取【报文类型】
        msg_qos = mqtt_get_qos(client->mqtt_state.in_buffer);	// 获取【消息质量】
        msg_id = mqtt_get_id(client->mqtt_state.in_buffer, client->mqtt_state.in_buffer_length);	// 获取报文中的【报文标识符】

        // 根据ESP8266运行状态，执行相应操作
        //-------------------------------------------------------------------------
        switch (client->connState)
        {
        	case MQTT_CONNECT_SENDING:				// 【MQTT_CONNECT_SENDING】
            if (msg_type == MQTT_MSG_TYPE_CONNACK) 	// 判断消息类型!=【CONNACK】
            {
            	// ESP8266发送 != 【CONNECT】报文
            	//--------------------------------------------------------------
                if (client->mqtt_state.pending_msg_type!=MQTT_MSG_TYPE_CONNECT)
                {
                    INFO("MQTT: Invalid packet\r\n");	// 网络数据出错

                    if (client->security)	// 安全类型 != 0
                    {
#ifdef MQTT_SSL_ENABLE
                        espconn_secure_disconnect(client->pCon);// 断开TCP连接
#else
                        INFO("TCP: Do not support SSL\r\n");
#endif
                    }
                    else 	// 安全类型 = 0 = NO_TLS
                    {
                        espconn_disconnect(client->pCon);	// 断开TCP连接
                    }
                }

                // ESP8266发送 == 【CONNECT】报文
                //---------------------------------------------------------------------------------
                else
                {
                    INFO("MQTT: Connected to %s:%d\r\n", client->host, client->port);

                    client->connState = MQTT_DATA;	// ESP8266状态改变：【MQTT_DATA】

                    if (client->connectedCb)		// 执行[mqttConnectedCb]函数(MQTT连接成功函数)
                    client->connectedCb((uint32_t*)client);	// 参数 = mqttClient
                }
            }
            break;


        // 当前ESP8266状态 == MQTT_DATA || MQTT_KEEPALIVE_SEND
        //-----------------------------------------------------
        case MQTT_DATA:
        case MQTT_KEEPALIVE_SEND:

            client->mqtt_state.message_length_read = len;	// 获取接收网络数据的长度

            // 计算接收到的网络数据中，报文的实际长度(通过【剩余长度】得到)
            //------------------------------------------------------------------------------------------------------------------------------
            client->mqtt_state.message_length = mqtt_get_total_length(client->mqtt_state.in_buffer, client->mqtt_state.message_length_read);


            // 当前ESP8266状态 ==【MQTT_DATA || MQTT_KEEPALIVE_SEND】，根据接收的报文类型，决定ESP8266接下来的动作
            //-----------------------------------------------------------------------------------------------------------------------
            switch (msg_type)
            {
            // ESP8266接收到【SUBACK】报文：订阅确认
            //-----------------------------------------------------------------------------------------------------------------------
            case MQTT_MSG_TYPE_SUBACK:
            	// 判断ESP8266之前发送的报文是否是【订阅主题】
            	//-------------------------------------------------------------------------------------------------------------
                if (client->mqtt_state.pending_msg_type==MQTT_MSG_TYPE_SUBSCRIBE && client->mqtt_state.pending_msg_id==msg_id)
                    INFO("MQTT: Subscribe successful\r\n");		//
                break;
            //-----------------------------------------------------------------------------------------------------------------------


            // ESP8266接收到【UNSUBACK】报文：取消订阅确认
          	//-----------------------------------------------------------------------------------------------------------------------
            case MQTT_MSG_TYPE_UNSUBACK:
            	// 判断ESP8266之前发送的报文是否是【订阅主题】
            	//------------------------------------------------------------------------------------------------------------------
                if (client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE && client->mqtt_state.pending_msg_id == msg_id)
                    INFO("MQTT: UnSubscribe successful\r\n");
                break;
        	//-----------------------------------------------------------------------------------------------------------------------


         	// ESP8266接收到【PUBLISH】报文：发布消息
        	//--------------------------------------------------------------------------------------------------------------------------------
            case MQTT_MSG_TYPE_PUBLISH:

                if (msg_qos == 1)	// 【服务端->客户端】发布消息 Qos=1
                    client->mqtt_state.outbound_message = mqtt_msg_puback(&client->mqtt_state.mqtt_connection, msg_id);	// 配置【PUBACK】报文
                else if (msg_qos == 2)	// 【服务端->客户端】发布消息 Qos=2
                    client->mqtt_state.outbound_message = mqtt_msg_pubrec(&client->mqtt_state.mqtt_connection, msg_id);	// 配置【PUBREC】报文

                if (msg_qos == 1 || msg_qos == 2)
                {
                    INFO("MQTT: Queue response QoS: %d\r\n", msg_qos);

                    // 将ESP8266应答报文(【PUBACK】或【PUBREC】)，写入队列
                    //-------------------------------------------------------------------------------------------------------------------------------
                    if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1)
                    {
                        INFO("MQTT: Queue full\r\n");
                    }
                }

                /// 获取服务端【PUBLISH】报文的【主题】、【有效载荷】
                //---------------------------------------------------------------------------------------------
                deliver_publish(client, client->mqtt_state.in_buffer, client->mqtt_state.message_length_read);

                break;
           	//-----------------------------------------------------------------------------------------------------------------------

            // ESP8266接收到【PUBACK】报文：发布消息应答
            //-------------------------------------------------------------------------------------------------------------------
            case MQTT_MSG_TYPE_PUBACK:
                if (client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && client->mqtt_state.pending_msg_id == msg_id)
                {
                    INFO("MQTT: received MQTT_MSG_TYPE_PUBACK, finish QoS1 publish\r\n");
                }
                break;
            //-------------------------------------------------------------------------------------------------------------------


            // Qos == 2
            //-------------------------------------------------------------------------------------------------------------------------------------
            case MQTT_MSG_TYPE_PUBREC:
                client->mqtt_state.outbound_message = mqtt_msg_pubrel(&client->mqtt_state.mqtt_connection, msg_id);
                if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
                    INFO("MQTT: Queue full\r\n");
                }
                break;
            case MQTT_MSG_TYPE_PUBREL:
                client->mqtt_state.outbound_message = mqtt_msg_pubcomp(&client->mqtt_state.mqtt_connection, msg_id);
                if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
                    INFO("MQTT: Queue full\r\n");
                }
                break;
            case MQTT_MSG_TYPE_PUBCOMP:
                if (client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && client->mqtt_state.pending_msg_id == msg_id) {
                    INFO("MQTT: receive MQTT_MSG_TYPE_PUBCOMP, finish QoS2 publish\r\n");
                }
                break;
            //-------------------------------------------------------------------------------------------------------------------------------------


            // ESP8266接收到【PINGREQ】报文：【这个是服务端发送给客户端的，正常情况下，ESP8266不会收到此报文】
            //-------------------------------------------------------------------------------------------------------------------------------------
            case MQTT_MSG_TYPE_PINGREQ:
                client->mqtt_state.outbound_message = mqtt_msg_pingresp(&client->mqtt_state.mqtt_connection);
                if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
                    INFO("MQTT: Queue full\r\n");
                }
                break;
            //-------------------------------------------------------------------------------------------------------------------------------------


            // ESP8266接收到【PINGRESP】报文：心跳应答
            //-----------------------------------------
            case MQTT_MSG_TYPE_PINGRESP:
                // Ignore	// 心跳应答报文则忽略
                break;
            //-----------------------------------------
            }


            // NOTE: this is done down here and not in the switch case above
            // because the PSOCK_READBUF_LEN() won't work inside a switch
            // statement due to the way protothreads resume.

            // ESP8266接收到【PUBLISH】报文：发布消息
            //-------------------------------------------------------------------------------------
            if (msg_type == MQTT_MSG_TYPE_PUBLISH)
            {
                len = client->mqtt_state.message_length_read;

                // 报文实际长度 < 网络数据长度
                //------------------------------------------------------------------------------
                if (client->mqtt_state.message_length < client->mqtt_state.message_length_read)
                {
                    //client->connState = MQTT_PUBLISH_RECV;
                    //Not Implement yet
                    len -= client->mqtt_state.message_length;		// 跳过之前解析过的报文
                    pdata += client->mqtt_state.message_length;		// 指向之后为解析的网络数据

                    INFO("Get another published message\r\n");

                    goto READPACKET;	// 重新解析网络数据
                }
            }
            break;		// case MQTT_DATA:
            			// case MQTT_KEEPALIVE_SEND:
            //-------------------------------------------------------------------------------------
        }
    }

    else 	// 接收到的报文出错
    {
        INFO("ERROR: Message too long\r\n");
    }

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task
}
//==================================================================================================================================================


/**
  * @brief  Client send over callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */

// TCP发送成功_回调函数
//================================================================================
void ICACHE_FLASH_ATTR mqtt_tcpclient_sent_cb(void *arg)
{
    struct espconn *pCon = (struct espconn *)arg;		// 获取TCP连接指针

    MQTT_Client* client = (MQTT_Client *)pCon->reverse;	// 获取mqttClient指针

    INFO("TCP: Sent\r\n");


    client->sendTimeout = 0;	// TCP发送成功/报文发送5秒计时结束 => 报文发送结束(sendTimeout=0)

    client->keepAliveTick =0;	// 只要TCP发送成功，客户端(ESP8266)心跳计数 = 0


    // 当前是MQTT客户端 发布消息/发送心跳 状态
    //--------------------------------------------------------------------------
    if ((client->connState==MQTT_DATA || client->connState==MQTT_KEEPALIVE_SEND)
                && client->mqtt_state.pending_msg_type==MQTT_MSG_TYPE_PUBLISH)
    {
        if (client->publishedCb)					// 执行[MQTT发布成功]函数
            client->publishedCb((uint32_t*)client);
    }

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task
}
//================================================================================


// MQTT定时(1秒)【功能：心跳计时、重连计时、TCP发送计时】
//================================================================================
void ICACHE_FLASH_ATTR mqtt_timer(void *arg)
{
    MQTT_Client* client = (MQTT_Client*)arg;

    // 如果当前是[MQTT_DATA]状态，则进行存活计时操作
    //--------------------------------------------------------------------------
    if (client->connState == MQTT_DATA)		// MQTT_DATA
    {
        client->keepAliveTick ++;	// 客户端(ESP8266)心跳计数++

        // 判断客户端(ESP8266)心跳计数 ?> 保持连接的1/2时间
        //--------------------------------------------------------------------------------------------------------------------------
        if (client->keepAliveTick >= (client->mqtt_state.connect_info->keepalive)-10)	//【注：官方例程中是：判断是否超过保持连接时长】
        {
            client->connState = MQTT_KEEPALIVE_SEND;	// MQTT_KEEPALIVE_SEND 任务是发送一次心跳包

            system_os_post(MQTT_TASK_PRIO,0,(os_param_t)client);// 安排任务
        }
    }

    // 重连等待计时：当进入重连请求状态后，需等待5秒，之后进行重新连接
    //--------------------------------------------------------------------------
    else if (client->connState == TCP_RECONNECT_REQ)	// TCP重连请求(等待5秒)
    {
        client->reconnectTick ++;	// 重连计时++

        if (client->reconnectTick > MQTT_RECONNECT_TIMEOUT)	// 重连请求超过了
        {
            client->reconnectTick = 0;	// 重连计时 = 0

            client->connState = TCP_RECONNECT;	// TCP重新连接

            system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务

            // 重连等待计时结束
            //-----------------------------------------------------------------
            if (client->timeoutCb)							// 未创建超时回调函数 这里没有写回调函数
                client->timeoutCb((uint32_t*)client);
        }
    }

    // TCP发送成功/报文发送5秒计时结束 => 报文发送结束(sendTimeout=0)
    //----------------------------------------------------------------
    if (client->sendTimeout>0)		// 发送MQTT报文时，sendTimeout=5
        client->sendTimeout --;		// sendTimeout每秒递减(直到=0)
}
//================================================================================


// TCP断开成功_回调函数
//================================================================================
void ICACHE_FLASH_ATTR mqtt_tcpclient_discon_cb(void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;	  // 获取TCP连接指针

    MQTT_Client*client = (MQTT_Client*)pespconn->reverse; // 获取mqttClient指针

    INFO("TCP: Disconnected callback\r\n");

    // 不执行重连操作
    //……………………………………………………………………………………………………
    if(TCP_DISCONNECTING == client->connState)	// 当前状态如果是：TCP正在断开
    {
        client->connState = TCP_DISCONNECTED;	// ESP8266状态改变：TCP成功断开
    }

    else if(MQTT_DELETING == client->connState)// 当前状态如果是：MQTT正在删除
    {
        client->connState = MQTT_DELETED;		// ESP8266状态改变：MQTT删除
    }
    //……………………………………………………………………………………………………


    // 只要不是上面两种状态，那么当TCP断开连接后，进入【TCP重连请求(等待5秒)】
    //……………………………………………………………………………………………………
    else
    {
        client->connState = TCP_RECONNECT_REQ;	// TCP重连请求(等待5秒)
    }

    if (client->disconnectedCb)					// 调用[MQTT成功断开连接]函数
        client->disconnectedCb((uint32_t*)client);
    //……………………………………………………………………………………………………


    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task
}
//================================================================================


/**
  * @brief  Tcp client connect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */

// TCP连接成功的回调函数
//============================================================================================================================================
void ICACHE_FLASH_ATTR mqtt_tcpclient_connect_cb(void *arg)
{
    struct espconn *pCon = (struct espconn *)arg;		 // 获取TCP连接指针
    MQTT_Client* client = (MQTT_Client *)(pCon->reverse);// 获取mqttClient指针

    // 注册回调函数
    //--------------------------------------------------------------------------------------
    espconn_regist_disconcb(client->pCon, mqtt_tcpclient_discon_cb);	// TCP断开成功_回调
    espconn_regist_recvcb(client->pCon, mqtt_tcpclient_recv);			// TCP接收成功_回调
    espconn_regist_sentcb(client->pCon, mqtt_tcpclient_sent_cb);		// TCP发送成功_回调

    INFO("MQTT: Connected to broker %s:%d\r\n", client->host, client->port);

    // 【CONNECT】报文发送准备
    //……………………………………………………………………………………………………………………………………………………………………………………
    // 初始化MQTT报文缓存区
    //-----------------------------------------------------------------------------------------------------------------------
	mqtt_msg_init(&client->mqtt_state.mqtt_connection, client->mqtt_state.out_buffer, client->mqtt_state.out_buffer_length);

	// 配置【CONNECT】控制报文，并获取【CONNECT】报文[指针]、[长度]
    //----------------------------------------------------------------------------------------------------------------------------
    client->mqtt_state.outbound_message = mqtt_msg_connect(&client->mqtt_state.mqtt_connection, client->mqtt_state.connect_info);

    // 获取待发送的报文类型(此处是【CONNECT】报文)
    //----------------------------------------------------------------------------------------------
    client->mqtt_state.pending_msg_type = mqtt_get_type(client->mqtt_state.outbound_message->data);

    // 获取待发送报文中的【报文标识符】（【CONNECT】报文中没有）
    //-------------------------------------------------------------------------------------------------------------------------------------
    client->mqtt_state.pending_msg_id = mqtt_get_id(client->mqtt_state.outbound_message->data,client->mqtt_state.outbound_message->length);

    // TCP发送成功/报文发送5秒计时结束 => 报文发送结束(sendTimeout=0)
    //-------------------------------------------------------------------------
    client->sendTimeout = MQTT_SEND_TIMOUT;	// 发送MQTT报文时，sendTimeout=5

    INFO("MQTT: Sending, type: %d, id: %04X\r\n", client->mqtt_state.pending_msg_type, client->mqtt_state.pending_msg_id);
    //……………………………………………………………………………………………………………………………………………………………………………………

    // TCP：发送【CONNECT】报文
    //-----------------------------------------------------------------------------------------------------------------------------
    if (client->security)	// 安全类型 != 0
    {
#ifdef MQTT_SSL_ENABLE
        espconn_secure_send(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
#else
        INFO("TCP: Do not support SSL\r\n");
#endif
    }
    else	// 安全类型 = 0 = NO_TLS
    {
    	// TCP发送：数据=[client->mqtt_state.outbound_message->data]、长度=[client->mqtt_state.outbound_message->length]
    	//-----------------------------------------------------------------------------------------------------------------
        espconn_send(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
    }
    //-----------------------------------------------------------------------------------------------------------------------------

    client->mqtt_state.outbound_message = NULL;		// 报文发送完后，清除出站报文指针

    client->connState = MQTT_CONNECT_SENDING;		// 状态设为：MQTT【CONNECT】报文发送中【MQTT_CONNECT_SENDING】

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task

}
//============================================================================================================================================


/**
  * @brief  Tcp client connect repeat callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */

// TCP异常中断_回调
//==============================================================================
void ICACHE_FLASH_ATTR mqtt_tcpclient_recon_cb(void *arg, sint8 errType)
{
    struct espconn *pCon = (struct espconn *)arg;		// 获取TCP连接指针
    MQTT_Client* client = (MQTT_Client *)pCon->reverse;	// 获取mqttClient指针

    INFO("TCP: Reconnect to %s:%d\r\n", client->host, client->port);

    client->connState = TCP_RECONNECT_REQ;		// TCP重连请求(等待5秒)

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task
}
//==============================================================================

/**
  * @brief  MQTT publish function.
  * @param  client:     MQTT_Client reference
  * @param  topic:         string topic will publish to
  * @param  data:         buffer data send point to
  * @param  data_length: length of data
  * @param  qos:        qos
  * @param  retain:        retain
  * @retval TRUE if success queue
  */

// ESP8266向主题发布消息：【参数2：主题名 / 参数3：发布消息的有效载荷 / 参数4：有效载荷长度 / 参数5：发布Qos / 参数6：Retain】
//============================================================================================================================================
BOOL ICACHE_FLASH_ATTR MQTT_Publish(MQTT_Client *client, const char* topic, const char* data, int data_length, int qos, int retain)
{
    uint8_t dataBuffer[MQTT_BUF_SIZE];	// 解析后报文缓存(1204字节)
    uint16_t dataLen;					// 解析后报文长度

    // 配置【PUBLISH】报文，并获取【PUBLISH】报文[指针]、[长度]
    //------------------------------------------------------------------------------------------
    client->mqtt_state.outbound_message = mqtt_msg_publish(&client->mqtt_state.mqtt_connection,
                                          topic, data, data_length,
                                          qos, retain,
                                          &client->mqtt_state.pending_msg_id);

    if (client->mqtt_state.outbound_message->length == 0)	// 判断报文是否正确
    {
        INFO("MQTT: Queuing publish failed\r\n");
        return FALSE;
    }

    // 串口打印：【PUBLISH】报文长度,(队列装填数量/队列大小)
    //--------------------------------------------------------------------------------------------------------------------------------------------------------------------
    INFO("MQTT: queuing publish, length: %d, queue size(%d/%d)\r\n", client->mqtt_state.outbound_message->length, client->msgQueue.rb.fill_cnt, client->msgQueue.rb.size);

    // 将报文写入队列，并返回写入字节数(包括特殊码)
    //----------------------------------------------------------------------------------------------------------------------------------
    while (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1)
    {
        INFO("MQTT: Queue full\r\n");	// 队列已满

        // 解析队列中的数据包
        //-----------------------------------------------------------------------------------------------
        if (QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == -1)	// 解析失败 = -1
        {
            INFO("MQTT: Serious buffer error\r\n");

            return FALSE;
        }
    }

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务

    return TRUE;
}
//============================================================================================================================================


/**
  * @brief  MQTT subscibe function.
  * @param  client:     MQTT_Client reference
  * @param  topic:		string topic will subscribe
  * @param  qos:        qos
  * @retval TRUE if success queue
  */

// ESP8266订阅主题【参数2：主题过滤器 / 参数3：订阅Qos】
//============================================================================================================================================
BOOL ICACHE_FLASH_ATTR MQTT_Subscribe(MQTT_Client *client, char* topic, uint8_t qos)
{
    uint8_t dataBuffer[MQTT_BUF_SIZE];		// 解析后报文缓存(1204字节)
    uint16_t dataLen;						// 解析后报文长度

    // 配置【SUBSCRIBE】报文，并获取【SUBSCRIBE】报文[指针]、[长度]
    //----------------------------------------------------------------------------------------------------------------------------------------
    client->mqtt_state.outbound_message=mqtt_msg_subscribe(&client->mqtt_state.mqtt_connection,topic, qos,&client->mqtt_state.pending_msg_id);

    INFO("MQTT: queue subscribe, topic\"%s\", id: %d\r\n", topic, client->mqtt_state.pending_msg_id);


    // 将报文写入队列，并返回写入字节数(包括特殊码)
    //----------------------------------------------------------------------------------------------------------------------------------
    while(QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1)
    {
        INFO("MQTT: Queue full\r\n");

        // 解析队列中的报文
        //-----------------------------------------------------------------------------------------------
        if (QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == -1)	// 解析失败 = -1
        {
            INFO("MQTT: Serious buffer error\r\n");

            return FALSE;
        }
    }

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task

    return TRUE;
}
//============================================================================================================================================

/**
  * @brief  MQTT un-subscibe function.
  * @param  client:     MQTT_Client reference
  * @param  topic:   String topic will un-subscribe
  * @retval TRUE if success queue
  */

// ESP8266取消订阅主题
//============================================================================================================================================
BOOL ICACHE_FLASH_ATTR MQTT_UnSubscribe(MQTT_Client *client, char* topic)
{
    uint8_t dataBuffer[MQTT_BUF_SIZE];	// 解析后报文缓存(1204字节)
    uint16_t dataLen;					// 解析后报文长度

    // 配置【UNSUBSCRIBE】报文	【参数2：取消订阅主题 /  参数3：报文标识符】
    //----------------------------------------------------------------------------------------------------------------------------------------
    client->mqtt_state.outbound_message = mqtt_msg_unsubscribe(&client->mqtt_state.mqtt_connection,topic,&client->mqtt_state.pending_msg_id);

    INFO("MQTT: queue un-subscribe, topic\"%s\", id: %d\r\n", topic, client->mqtt_state.pending_msg_id);

    // 将报文写入队列，并返回写入字节数(包括特殊码)
	//----------------------------------------------------------------------------------------------------------------------------------
    while (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1)
    {
        INFO("MQTT: Queue full\r\n");

        // 解析队列中的报文
        //-----------------------------------------------------------------------------------------------
        if (QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == -1)	// 解析失败 = -1
        {
            INFO("MQTT: Serious buffer error\r\n");
            return FALSE;
        }
    }
    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task

    return TRUE;
}
//============================================================================================================================================

/**
  * @brief  MQTT ping function.
  * @param  client:     MQTT_Client reference
  * @retval TRUE if success queue
  */

// 向MQTT服务器发送【心跳】报文（将报文写入队列，任务函数中发送）
//============================================================================================================================================
BOOL ICACHE_FLASH_ATTR MQTT_Ping(MQTT_Client *client)
{
    uint8_t dataBuffer[MQTT_BUF_SIZE];
    uint16_t dataLen;
    client->mqtt_state.outbound_message = mqtt_msg_pingreq(&client->mqtt_state.mqtt_connection);	// 配置【PINGREQ】报文

    if(client->mqtt_state.outbound_message->length == 0)	// 报文错误
    {
        INFO("MQTT: Queuing publish failed\r\n");
        return FALSE;
    }

    INFO("MQTT: queuing publish, length: %d, queue size(%d/%d)\r\n", client->mqtt_state.outbound_message->length, client->msgQueue.rb.fill_cnt, client->msgQueue.rb.size);


    // 将报文写入队列，并返回写入字节数(包括特殊码)
	//----------------------------------------------------------------------------------------------------------------------------------
    while(QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1)
    {
        INFO("MQTT: Queue full\r\n");

        // 解析队列中的报文
        //-----------------------------------------------------------------------------------------------
        if(QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == -1)	// 解析失败 = -1
        {
            INFO("MQTT: Serious buffer error\r\n");
            return FALSE;
        }
    }

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);	// 安排任务MQTT_Task

    return TRUE;
}
//============================================================================================================================================


// MQTT任务函数【任务：根据ESP8266运行状态，执行相应操作】
//--------------------------------------------------------------------------------------------
// TCP_RECONNECT_REQ			TCP重连请求(等待5秒)	退出Tsak(5秒后，进入TCP_RECONNECT状态)
//--------------------------------------------------------------------------------------------
// TCP_RECONNECT				TCP重新连接				执行MQTT连接准备，并设置ESP8266状态
//--------------------------------------------------------------------------------------------
// MQTT_DELETING				MQTT正在删除			TCP断开连接
// TCP_DISCONNECTING			TCP正在断开
// TCP_RECONNECT_DISCONNECTING	TCP暂时断开(断开后会重连)
//--------------------------------------------------------------------------------------------
// TCP_DISCONNECTED				TCP成功断开				删除TCP连接，并释放pCon内存
//--------------------------------------------------------------------------------------------
// MQTT_DELETED					MQTT已删除				删除MQTT客户端，并释放相关内存
//--------------------------------------------------------------------------------------------
// MQTT_KEEPALIVE_SEND			MQTT心跳				向服务器发送心跳报文
//--------------------------------------------------------------------------------------------
// MQTT_DATA					MQTT数据传输			TCP发送队列中的报文
//====================================================================================================================================
void ICACHE_FLASH_ATTR MQTT_Task(os_event_t *e)	// 不判断消息类型
{
	INFO("\r\n------------- MQTT_Task -------------\r\n");

    MQTT_Client* client = (MQTT_Client*)e->par;		// 【e->par】 == 【mqttClient指针的值】,所以需类型转换

    uint8_t dataBuffer[MQTT_BUF_SIZE];	// 数据缓存区(1204字节)

    uint16_t dataLen;					// 数据长度

    if (e->par == 0)		// 没有mqttClient指针，错误
    return;


    // 根据ESP8266运行状态，执行相应操作
    //………………………………………………………………………………………………………………………………………………………………………
    switch (client->connState)
    {
    	// TCP重连请求(等待5秒)，退出Tsak
    	//---------------------------------
    	case TCP_RECONNECT_REQ:		break;
    	//---------------------------------


    	// TCP重新连接：执行MQTT连接准备，并设置ESP8266状态
    	//--------------------------------------------------------------------------------
    	case TCP_RECONNECT:

    		mqtt_tcpclient_delete(client);	// 删除TCP连接、释放pCon内存、清除TCP连接指针

    		MQTT_Connect(client);			// MQTT连接准备：TCP连接、域名解析等

    		INFO("TCP: Reconnect to: %s:%d\r\n", client->host, client->port);

    		client->connState = TCP_CONNECTING;		// TCP正在连接

    		break;
    	//--------------------------------------------------------------------------------


    	// MQTT正在删除、TCP正在断开、【心跳请求】报文发送失败：TCP断开连接
    	//------------------------------------------------------------------
    	case MQTT_DELETING:
    	case TCP_DISCONNECTING:
    	case TCP_RECONNECT_DISCONNECTING:
    		if (client->security)	// 安全类型 != 0
    		{
#ifdef MQTT_SSL_ENABLE
    			espconn_secure_disconnect(client->pCon);
#else
    			INFO("TCP: Do not support SSL\r\n");
#endif
    		}
    		else 	// 安全类型 = 0 = NO_TLS
    		{
    			espconn_disconnect(client->pCon);	// TCP断开连接
    		}

    		break;
    	//------------------------------------------------------------------


    	// TCP成功断开
    	//--------------------------------------------------------------------------------
    	case TCP_DISCONNECTED:
    		INFO("MQTT: Disconnected\r\n");
    		mqtt_tcpclient_delete(client);	// 删除TCP连接、释放pCon内存、清除TCP连接指针
    		break;
    	//--------------------------------------------------------------------------------


    	// MQTT已删除：ESP8266的状态为[MQTT已删除]后，将MQTT相关内存释放
    	//--------------------------------------------------------------------
    	case MQTT_DELETED:
    		INFO("MQTT: Deleted client\r\n");
    		mqtt_client_delete(client);		// 删除MQTT客户端，并释放相关内存
    		break;


    	// MQTT客户端存活报告
    	//--------------------------------------------------------------------
    	case MQTT_KEEPALIVE_SEND:
    		mqtt_send_keepalive(client);	// 向MQTT服务器发送【心跳】报文
    		break;


    	// MQTT传输数据状态
    	//-------------------------------------------------------------------------------------------------------------------------------
    	case MQTT_DATA:
    		if (QUEUE_IsEmpty(&client->msgQueue) || client->sendTimeout != 0)
    		{
    			break;	// 【队列为空 || 发送未结束】，不执行操作
    		}

    		// 【队列非空 && 发送结束】：解析并发送 队列中的报文
    		//--------------------------------------------------------------------------------------------------------
    		if (QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == 0)	// 解析成功 = 0
    		{
				client->mqtt_state.pending_msg_type = mqtt_get_type(dataBuffer);		// 获取报文中的【报文类型】
				client->mqtt_state.pending_msg_id = mqtt_get_id(dataBuffer, dataLen);	// 获取报文中的【报文标识符】

				client->sendTimeout = MQTT_SEND_TIMOUT;	// 发送MQTT报文时，sendTimeout=5

				INFO("MQTT: Sending, type: %d, id: %04X\r\n", client->mqtt_state.pending_msg_type, client->mqtt_state.pending_msg_id);


				// 发送报文
				//-----------------------------------------------------------------------
				if (client->security)	// 安全类型 != 0
				{
#ifdef MQTT_SSL_ENABLE
					espconn_secure_send(client->pCon, dataBuffer, dataLen);
#else
					INFO("TCP: Do not support SSL\r\n");
#endif
				}
				else	// 安全类型 = 0 = NO_TLS
				{
					espconn_send(client->pCon, dataBuffer, dataLen);	// TCP发送数据包
				}

				client->mqtt_state.outbound_message = NULL;		// 报文发送完后，清除出站报文指针

				break;
    		}
        break;
    }
    //………………………………………………………………………………………………………………………………………………………………………

}	// 函数【MQTT_Task】结束
//====================================================================================================================================


/**
  * @brief  MQTT initialization connection function
  * @param  client:     MQTT_Client reference
  * @param  host:     Domain or IP string
  * @param  port:     Port to connect
  * @param  security:        1 for ssl, 0 for none
  * @retval None
  */

// 网络连接参数赋值：服务端域名【mqtt_test_jx.mqtt.iot.gz.baidubce.com】、网络连接端口【1883】、安全类型【0：NO_TLS】
//====================================================================================================================
void ICACHE_FLASH_ATTR MQTT_InitConnection(MQTT_Client *mqttClient, uint8_t* host, uint32_t port, uint8_t security)
{
    uint32_t temp;

    INFO("MQTT_InitConnection\r\n");

    os_memset(mqttClient, 0, sizeof(MQTT_Client));		// 【MQTT客户端】结构体 = 0

    temp = os_strlen(host);								// 服务端域名/IP的字符串长度
    mqttClient->host = (uint8_t*)os_zalloc(temp+1);		// 申请空间，存放服务端域名/IP地址字符串

    os_strcpy(mqttClient->host, host);					// 字符串拷贝
    mqttClient->host[temp] = 0;							// 最后'\0'

    mqttClient->port = port;							// 网络端口号 = 1883

    mqttClient->security = security;					// 安全类型 = 0 = NO_TLS
}
//====================================================================================================================

/**
  * @brief  MQTT initialization mqtt client function
  * @param  client:     MQTT_Client reference
  * @param  clientid: 	MQTT client id
  * @param  client_user:MQTT client user
  * @param  client_pass:MQTT client password
  * @param  client_pass:MQTT keep alive timer, in second
  * @retval None
  */

// MQTT连接参数赋值：客户端标识符【..】、MQTT用户名【..】、MQTT密钥【..】、保持连接时长【120s】、清除会话【1：clean_session】
//======================================================================================================================================================
void ICACHE_FLASH_ATTR
MQTT_InitClient(MQTT_Client *mqttClient, uint8_t* client_id, uint8_t* client_user, uint8_t* client_pass, uint32_t keepAliveTime, uint8_t cleanSession)
{
    uint32_t temp;

    INFO("MQTT_InitClient\r\n");

    // MQTT【CONNECT】报文的连接参数 赋值
    //---------------------------------------------------------------------------------------------------------------
    os_memset(&mqttClient->connect_info, 0, sizeof(mqtt_connect_info_t));		// MQTT【CONNECT】报文的连接参数 = 0

    temp = os_strlen(client_id);
    mqttClient->connect_info.client_id = (uint8_t*)os_zalloc(temp + 1);			// 申请【客户端标识符】的存放内存
    os_strcpy(mqttClient->connect_info.client_id, client_id);					// 赋值【客户端标识符】
    mqttClient->connect_info.client_id[temp] = 0;								// 最后'\0'

    if (client_user)	// 判断是否有【MQTT用户名】
    {
        temp = os_strlen(client_user);
        mqttClient->connect_info.username = (uint8_t*)os_zalloc(temp + 1);
        os_strcpy(mqttClient->connect_info.username, client_user);				// 赋值【MQTT用户名】
        mqttClient->connect_info.username[temp] = 0;
    }

    if (client_pass)	// 判断是否有【MQTT密码】
    {
        temp = os_strlen(client_pass);
        mqttClient->connect_info.password = (uint8_t*)os_zalloc(temp + 1);
        os_strcpy(mqttClient->connect_info.password, client_pass);				// 赋值【MQTT密码】
        mqttClient->connect_info.password[temp] = 0;
    }

    mqttClient->connect_info.keepalive = keepAliveTime;							// 保持连接 = 120s
    mqttClient->connect_info.clean_session = cleanSession;						// 清除会话 = 1 = clean_session
    //--------------------------------------------------------------------------------------------------------------

    // 设置mqtt_state部分参数
    //------------------------------------------------------------------------------------------------------------------------------------------------------------
    mqttClient->mqtt_state.in_buffer = (uint8_t *)os_zalloc(MQTT_BUF_SIZE);		// 申请in_buffer内存【入站报文缓存区】
    mqttClient->mqtt_state.in_buffer_length = MQTT_BUF_SIZE;					// 设置in_buffer大小
    mqttClient->mqtt_state.out_buffer = (uint8_t *)os_zalloc(MQTT_BUF_SIZE);	// 申请out_buffer内存【出站报文缓存区】
    mqttClient->mqtt_state.out_buffer_length = MQTT_BUF_SIZE;					// 设置out_buffer大小
    mqttClient->mqtt_state.connect_info = &(mqttClient->connect_info);			// MQTT【CONNECT】报文的连接参数(指针)，赋值给mqttClient->mqtt_state.connect_info


    // 初始化MQTT出站报文缓存区
    //----------------------------------------------------------------------------------------------------------------------------------
    mqtt_msg_init(&mqttClient->mqtt_state.mqtt_connection, mqttClient->mqtt_state.out_buffer, mqttClient->mqtt_state.out_buffer_length);

    QUEUE_Init(&mqttClient->msgQueue, QUEUE_BUFFER_SIZE);	// 消息队列初始化【队列可以存放一个/多个MQTT报文】


    // 创建任务：任务函数【MQTT_Task】、优先级【2】、任务指针【mqtt_procTaskQueue】、消息深度【1】
    //---------------------------------------------------------------------------------------------
    system_os_task(MQTT_Task, MQTT_TASK_PRIO, mqtt_procTaskQueue, MQTT_TASK_QUEUE_SIZE);

    // 安排任务：参数1=任务等级 / 参数2=消息类型 / 参数3=消息参数
    //-----------------------------------------------------------------------------------------------
    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)mqttClient);	// 参数3的类型必须为【os_param_t】型
}
//======================================================================================================================================================


// 设置遗嘱：遗嘱主题【...】、遗嘱消息【...】、遗嘱质量【Will_Qos=0】、遗嘱保持【Will_Retain=0】
//====================================================================================================================
void ICACHE_FLASH_ATTR
MQTT_InitLWT(MQTT_Client *mqttClient, uint8_t* will_topic, uint8_t* will_msg, uint8_t will_qos, uint8_t will_retain)
{
    uint32_t temp;
    temp = os_strlen(will_topic);
    mqttClient->connect_info.will_topic = (uint8_t*)os_zalloc(temp + 1);		// 申请【遗嘱主题】的存放内存
    os_strcpy(mqttClient->connect_info.will_topic, will_topic);					// 赋值【遗嘱主题】
    mqttClient->connect_info.will_topic[temp] = 0;								// 最后'\0'

    temp = os_strlen(will_msg);
    mqttClient->connect_info.will_message = (uint8_t*)os_zalloc(temp + 1);
    os_strcpy(mqttClient->connect_info.will_message, will_msg);					// 赋值【遗嘱消息】
    mqttClient->connect_info.will_message[temp] = 0;

    mqttClient->connect_info.will_qos = will_qos;			// 遗嘱质量【Will_Qos=0】
    mqttClient->connect_info.will_retain = will_retain;		// 遗嘱保持【Will_Retain=0】
}
//====================================================================================================================

/**
  * @brief  Begin connect to MQTT broker
  * @param  client: MQTT_Client reference
  * @retval None
  */

// WIFI连接、SNTP成功后 => MQTT连接准备(设置TCP连接、解析域名)
//============================================================================================================================================
void ICACHE_FLASH_ATTR MQTT_Connect(MQTT_Client *mqttClient)
{
    //espconn_secure_set_size(0x01,6*1024);	 // SSL双向认证时才需使用	// try to modify memory size 6*1024 if ssl/tls handshake failed

	// 开始MQTT连接前，判断是否存在MQTT的TCP连接。如果有，则清除之前的TCP连接
	//------------------------------------------------------------------------------------
    if (mqttClient->pCon)
    {
        // Clean up the old connection forcefully - using MQTT_Disconnect
        // does not actually release the old connection until the
        // disconnection callback is invoked.

        mqtt_tcpclient_delete(mqttClient);	// 删除TCP连接、释放pCon内存、清除TCP连接指针
    }

    // TCP连接设置 把读取内存出来的参数接配置
    //------------------------------------------------------------------------------------------------------
    mqttClient->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));	// 申请pCon内存
    mqttClient->pCon->type = ESPCONN_TCP;										// 设为TCP连接
    mqttClient->pCon->state = ESPCONN_NONE;
    mqttClient->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));		// 申请esp_tcp内存
    mqttClient->pCon->proto.tcp->local_port = espconn_port();					// 获取ESP8266可用端口
    mqttClient->pCon->proto.tcp->remote_port = mqttClient->port;				// 设置端口号
    mqttClient->pCon->reverse = mqttClient;										// mqttClient->pCon->reverse 缓存 mqttClient指针

    espconn_regist_connectcb(mqttClient->pCon, mqtt_tcpclient_connect_cb);		// 注册TCP连接成功的回调函数
    espconn_regist_reconcb(mqttClient->pCon, mqtt_tcpclient_recon_cb);			// 注册TCP异常中断的回调函数


    //---------------------------------------------------------------------------------------------------
    mqttClient->keepAliveTick = 0;	// MQTT客户端(ESP8266)心跳计数
    mqttClient->reconnectTick = 0;	// 重连等待计时：当进入重连请求状态后，需等待5秒，之后进行重新连接

    // 设置MQTT定时(1秒)【功能：心跳计时、重连计时、TCP发送计时】
    //---------------------------------------------------------------------------------------------------
    os_timer_disarm(&mqttClient->mqttTimer);
    os_timer_setfn(&mqttClient->mqttTimer, (os_timer_func_t *)mqtt_timer, mqttClient);	// mqtt_timer
    os_timer_arm(&mqttClient->mqttTimer, 1000, 1);										// 1秒定时(重复)


    // 打印SSL配置：安全类型[NO_TLS == 0]
    //--------------------------------------------------------------------------------------------------------------------------------------------------------------
    os_printf("your ESP SSL/TLS configuration is %d.[0:NO_TLS\t1:TLS_WITHOUT_AUTHENTICATION\t2ONE_WAY_ANTHENTICATION\t3TWO_WAY_ANTHENTICATION]\n",DEFAULT_SECURITY);


    // 解析点分十进制形式的IP地址
    //------------------------------------------------------------------------------------------------------------------
    if (UTILS_StrToIP(mqttClient->host, &mqttClient->pCon->proto.tcp->remote_ip))	// 解析IP地址(点分十进制字符串形式)
    {
        INFO("TCP: Connect to ip  %s:%d\r\n", mqttClient->host, mqttClient->port);	// 打印IP地址

        // 根据安全类型，调用不同的TCP连接方式
        //-------------------------------------------------------------------------------------------------
        if (mqttClient->security)	// 安全类型 != 0
        {
#ifdef MQTT_SSL_ENABLE

            if(DEFAULT_SECURITY >= ONE_WAY_ANTHENTICATION )		// 单向认证【ONE_WAY_ANTHENTICATION = 2】
            {
                espconn_secure_ca_enable(ESPCONN_CLIENT,CA_CERT_FLASH_ADDRESS);
            }

            if(DEFAULT_SECURITY >= TWO_WAY_ANTHENTICATION)		// 双向认证【TWO_WAY_ANTHENTICATION = 3】
            {
                espconn_secure_cert_req_enable(ESPCONN_CLIENT,CLIENT_CERT_FLASH_ADDRESS);
            }

            espconn_secure_connect(mqttClient->pCon);			// 不认证【TLS_WITHOUT_AUTHENTICATION = 1】
#else
            INFO("TCP: Do not support SSL\r\n");
#endif
        }

        else	// 安全类型 = 0 = NO_TLS
        {
            espconn_connect(mqttClient->pCon);	// TCP连接(作为Client连接Server)
        }
    }

    // 解析域名
    //----------------------------------------------------------------------------------------------
    else
    {
        INFO("TCP: Connect to domain %s:%d\r\n", mqttClient->host, mqttClient->port);

        espconn_gethostbyname(mqttClient->pCon, mqttClient->host, &mqttClient->ip, mqtt_dns_found);
    }

    mqttClient->connState = TCP_CONNECTING;		// TCP正在连接
}
//============================================================================================================================================


// TCP断开连接（Clean up the old connection forcefully）
//==============================================================================
void ICACHE_FLASH_ATTR MQTT_Disconnect(MQTT_Client *mqttClient)
{
    mqttClient->connState = TCP_DISCONNECTING;	// ESP8266状态改变：TCP正在断开

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)mqttClient);	// 安排任务

    os_timer_disarm(&mqttClient->mqttTimer);	// 取消MQTT定时
}
//==============================================================================


// 删除MQTT客户端
//==============================================================================
void ICACHE_FLASH_ATTR MQTT_DeleteClient(MQTT_Client *mqttClient)
{
    mqttClient->connState = MQTT_DELETING;	// ESP8266状态改变：MQTT正在删除

    system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)mqttClient);	// 安排任务

    os_timer_disarm(&mqttClient->mqttTimer);	// 取消MQTT定时
}
//==============================================================================


// 函数调用重定义
//………………………………………………………………………………………………………………………………………
// 执行 mqttClient->connectedCb(...) => mqttConnectedCb(...)
//------------------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR MQTT_OnConnected(MQTT_Client*mqttClient, MqttCallback connectedCb)
{
    mqttClient->connectedCb = connectedCb;	// 函数名【mqttConnectedCb】
}

// 执行 mqttClient->disconnectedCb(...) => mqttDisconnectedCb(...)
//-------------------------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR MQTT_OnDisconnected(MQTT_Client *mqttClient, MqttCallback disconnectedCb)
{
    mqttClient->disconnectedCb = disconnectedCb;	// 函数名【mqttDisconnectedCb】
}

// 执行 mqttClient->dataCb(...) => mqttDataCb(...)
//------------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR MQTT_OnData(MQTT_Client *mqttClient, MqttDataCallback dataCb)
{
    mqttClient->dataCb = dataCb;	// 函数名【mqttDataCb】
}

// 执行 mqttClient->publishedCb(...) => mqttPublishedCb(...)
//-------------------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR MQTT_OnPublished(MQTT_Client *mqttClient, MqttCallback publishedCb)
{
    mqttClient->publishedCb = publishedCb;	// 函数名【mqttPublishedCb】
}

// 执行 mqttClient->timeoutCb(...) => 【...】未定义函数
//--------------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR MQTT_OnTimeout(MQTT_Client *mqttClient, MqttCallback timeoutCb)
{
    mqttClient->timeoutCb = timeoutCb;
}
//………………………………………………………………………………………………………………………………………
