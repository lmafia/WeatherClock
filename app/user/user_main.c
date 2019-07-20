/* main.c -- MQTT client example
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




// 头文件
//==============================

#include "func.h"

//=================================
//预处理
//=================================

//==============================
// 类型定义
//=================================
typedef unsigned long 		u32_t;

//=================================
// 全局变量
//============================================================================
MQTT_Client mqttClient;			// MQTT客户端_结构体【此变量非常重要】
MQTT_Info mqtt_Info;
extern day_Weather weather_Info;
uint32 priv_param_start_sec;
ip_addr_t IP_Server;
struct espconn AP_NetCon;
struct espconn STA_NetCon;
os_timer_t OS_Timer_1;		// 软件定时器查询ip连接
os_timer_t OS_Timer_Dht11;   //dht11
os_timer_t OS_Timer_SNTP;
struct softap_config AP_Config;
struct station_config STA_Config;	// STA参数结构体

char waether_buf[1000]="";
uint8 times_rec =0;
//============================================================================
//自定义delay_ms
//============================================================================
void ICACHE_FLASH_ATTR delay_ms(uint32 ms)
{
	uint32 i = 0;
	for(i = 0; i < ms ; i++)
	{
		os_delay_us(1000);
	}
}
//============================================================================
// DNS_域名解析结束_回调函数【参数1：域名字符串指针 / 参数2：IP地址结构体指针 / 参数3：网络连接结构体指针】
//============================================================================
void ICACHE_FLASH_ATTR DNS_Over_Cb(const char * name, ip_addr_t *ipaddr, void *arg)
{
	struct espconn * T_arg = (struct espconn *)arg;	// 缓存网络连接结构体指针
	if(ipaddr == NULL)		// 域名解析失败
	{
		os_printf("\r\n---- DomainName Analyse Failed ----\r\n");
		espconn_gethostbyname(&STA_NetCon, DNS_Server, &IP_Server, DNS_Over_Cb);
		return;
	}

	else if (ipaddr != NULL && ipaddr->addr != 0)		// 域名解析成功
	{
		os_printf("\r\n---- DomainName Analyse Succeed ----\r\n");

		IP_Server.addr = ipaddr->addr;					// 获取服务器IP地址
		// 将解析到的服务器IP地址设为TCP连接的远端IP地址
		os_memcpy(T_arg->proto.tcp->remote_ip, &IP_Server.addr, 4);	// 设置服务器IP地址
		// 显示IP地址
		//------------------------------------------------------------------------------
		os_printf("\r\nIP_Server = %d.%d.%d.%d\r\n",						// 串口打印
				*((u8*)&IP_Server.addr),	*((u8*)&IP_Server.addr+1),
				*((u8*)&IP_Server.addr+2),	*((u8*)&IP_Server.addr+3));
		espconn_connect(T_arg);	// 连接TCP-server

	}
}
//============================================================================
// STA TCP天气连接断开成功的回调函数
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_TCP_Disconnect_Cb(void *arg)
{
	delay_ms(1000);
	os_printf("@0x04: C:%s TD: %s TW: %s TDW: %s TNW: %s TDT: %s TNT: %s\r\n",
			weather_Info.city,
			weather_Info.today_Weather.date,
			weather_Info.today_Weather.week,
			weather_Info.today_Weather.dayweather,
			weather_Info.today_Weather.nightweather,
			weather_Info.today_Weather.daytemp,
			weather_Info.today_Weather.nighttemp
	);
	delay_ms(500);
	os_printf("@0x04: ND: %s NW: %s NDW: %s NNW: %s NDT: %s DNT: %s\r\n",
			weather_Info.next_Weather.date,
			weather_Info.next_Weather.week,
			weather_Info.next_Weather.dayweather,
			weather_Info.next_Weather.nightweather,
			weather_Info.next_Weather.daytemp,
			weather_Info.next_Weather.nighttemp
	);
	delay_ms(500);
	os_printf("@0x04: AD: %s AW: %s ADW: %s ANW: %s ADT: %s ANT: %s\r\n",
			weather_Info.after_Weather.date,
			weather_Info.after_Weather.week,
			weather_Info.after_Weather.dayweather,
			weather_Info.after_Weather.nightweather,
			weather_Info.after_Weather.daytemp,
			weather_Info.after_Weather.nighttemp
	);
/*	os_printf("@0x04: C:%s TD: %s TW: %s TDW: %s TNW: %s TDT: %s TNT: %s"
						  " ND: %s NW: %s NDW: %s NNW: %s NDT: %s DNT: %s"
						  " AD: %s AW: %s ADW: %s ANW: %s ADT: %s ANT: %s\r\n",
			weather_Info.city,
			weather_Info.today_Weather.date,
			weather_Info.today_Weather.week,
			weather_Info.today_Weather.dayweather,
			weather_Info.today_Weather.nightweather,
			weather_Info.today_Weather.daytemp,
			weather_Info.today_Weather.nighttemp,
			weather_Info.next_Weather.date,
			weather_Info.next_Weather.week,
			weather_Info.next_Weather.dayweather,
			weather_Info.next_Weather.nightweather,
			weather_Info.next_Weather.daytemp,
			weather_Info.next_Weather.nighttemp,
			weather_Info.after_Weather.date,
			weather_Info.after_Weather.week,
			weather_Info.after_Weather.dayweather,
			weather_Info.after_Weather.nightweather,
			weather_Info.after_Weather.daytemp,
			weather_Info.after_Weather.nighttemp

	);*/
	delay_ms(1000);
}
//============================================================================
// STA TCP & AP UDP成功发送网络数据的回调函数
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_WIFI_Send_Cb(void *arg)
{
	/*os_printf("\nESP8266_WIFI_Send_OK\n");*/	// 向对方发送应答   发送数据
}
//============================================================================
// STA TCP接收回调函数  获取天气信息
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_Weather_Recv_Cb(void * arg, char * pdata, unsigned short len)
{

	cJSON *json;
	char* pc =waether_buf;

	if(pdata == NULL)
	{
		os_printf("erro\r\n");
		return;
	}
	//os_strcpy(temp,pdata);

	if(times_rec==0){
	pdata = (char *)os_strstr((char *)pdata, "{");
	if(pdata == NULL)
		return;
	os_memcpy(pc,pdata,800);
	times_rec++;
	}
	else{
	os_strcat(pc,pdata);
	times_rec =0;
	}
/*	os_printf("%d\r\n",os_strlen(pc));
	os_printf("%s\r\n",pc);*/
	pc = (char *)os_strstr((char *)pc, "city");
	if(pc == NULL)
		return;
	char *headOfCity = (char *)os_strstr((char *)pc, ":\"");
	if (headOfCity == NULL)
		INFO("ERRO1\r\n");

	char *tailOfCity = NULL;
	tailOfCity =(char *)os_strstr((char *)headOfCity, "\",");
	if(tailOfCity == NULL)
		INFO("erro2\r\n");

	os_memset(weather_Info.city,0,20);
	//os_bzero(weather_Info.city,sizeof(weather_Info.city));
	os_memcpy(weather_Info.city,(headOfCity+2),(tailOfCity)-(headOfCity+2));

	if(pc == NULL)
		return;
	int i = 0;
	for(i = 0; i < 3; i++)
	{
		pc = (char *)os_strstr((char *)pc, "{");
		if(pc == NULL)
			return;
		json = cJSON_Parse(pc);
		if(json == NULL)
			return;
		if(i == 0)
			write_weather(json,&weather_Info.today_Weather);
		else if(i == 1)
			write_weather(json,&weather_Info.next_Weather);
		else
			write_weather(json,&weather_Info.after_Weather);
		pc +=5;
	}

	cJSON_Delete(json);

}
//============================================================================
// STA TCP 天气连接建立成功的回调函数
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_TCP_Connect_Cb(void *arg)
{
	espconn_regist_sentcb((struct espconn *)arg, ESP8266_WIFI_Send_Cb);			// 注册网络数据发送成功的回调函数
	espconn_regist_recvcb((struct espconn *)arg, ESP8266_Weather_Recv_Cb);			// 注册网络数据接收成功的回调函数
	espconn_regist_disconcb((struct espconn *)arg,ESP8266_TCP_Disconnect_Cb);	// 注册成功断开TCP连接的回调函数

	os_printf("\n--------------- ESP8266_TCP_Connect_OK ---------------\n");
	espconn_send((struct espconn *)arg, HTTP_GET_Weather1, os_strlen(HTTP_GET_Weather1));		// 发送HTTP报文

}
//============================================================================
// STA TCP连接异常断开时的回调函数
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_TCP_Break_Cb(void *arg,sint8 err)
{
	os_printf("\nESP8266_TCP_Break\n");
	espconn_connect(&STA_NetCon);	// 连接TCP-server
}
//============================================================================
// ESP8266_STA_NetCon_Init 网络连接初始化 连接天气的
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_STA_NetCon_Init()
{
	// 结构体赋值
	//--------------------------------------------------------------------------
	STA_NetCon.type = ESPCONN_TCP ;				// 设置为TCP协议
	STA_NetCon.proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));	// 开辟内存
	// ESP8266作为TCP_Client，想要连接TCP_Server，必须知道TCP_Server的IP地址
	STA_NetCon.proto.tcp->local_port = espconn_port() ;	// 本地端口【获取可用端口】
	STA_NetCon.proto.tcp->remote_port = 80;				// 目标端口【HTTP端口号80】

	// 获取域名所对应的IP地址
	//【参数1：网络连接结构体指针 / 参数2：域名字符串指针 / 参数3：IP地址结构体指针 / 参数4：回调函数】
	//-------------------------------------------------------------------------------------------------
	espconn_gethostbyname(&STA_NetCon, DNS_Server, &IP_Server, DNS_Over_Cb);

	// 注册连接成功回调函数、异常断开回调函数
	//--------------------------------------------------------------------------------------------------
	espconn_regist_connectcb(&STA_NetCon, ESP8266_TCP_Connect_Cb);	// 注册TCP连接成功建立的回调函数
	espconn_regist_reconcb(&STA_NetCon, ESP8266_TCP_Break_Cb);		// 注册TCP连接异常断开的回调函数

	// 连接 TCP server
	//----------------------------------------------------------
	//espconn_connect(&STA_NetCon);	// 连接TCP-server
}
//============================================================================
//UDP AP 接收的回调函数;arg：espconn指针;pdata：数据指针;len：数据长度
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_WIFI_Recv_Cb(void * arg, char * pdata, unsigned short len)
{
	struct espconn *T_arg = arg;		// 缓存网络连接结构体指针

	remot_info *P_port_info = NULL;	// 远端连接信息结构体指针

/*
	// 根据数据设置LED的亮/灭
	//-------------------------------------------------------------------------------
	if(pdata[0] == 'k' || pdata[0] == 'K')
	{LED_ON;}
	else if(pdata[0] == 'g' || pdata[0] == 'G')
	{LED_OFF;}
*/
	if(pdata[0]=='H')
	{
		if(os_strcmp(mqtt_Info.switch_state,",;switch0,1;")==0)
			{espconn_send(T_arg,"sw0:K",os_strlen("sw0:K"));}
		else
			{espconn_send(T_arg,"sw0:G",os_strlen("sw0:G"));}
	}

	os_printf("\nESP8266_Receive_Data = %s\n",pdata);		// 串口打印接收到的数据


	// 获取远端信息【UDP通信是无连接的，向远端主机回应时需获取对方的IP/端口信息】
	//------------------------------------------------------------------------------------
	if(espconn_get_connection_info(T_arg, &P_port_info, 0)==ESPCONN_OK)	// 获取远端信息
	{
		T_arg->proto.udp->remote_port  = P_port_info->remote_port;		// 获取对方端口号
		T_arg->proto.udp->remote_ip[0] = P_port_info->remote_ip[0];		// 获取对方IP地址
		T_arg->proto.udp->remote_ip[1] = P_port_info->remote_ip[1];
		T_arg->proto.udp->remote_ip[2] = P_port_info->remote_ip[2];
		T_arg->proto.udp->remote_ip[3] = P_port_info->remote_ip[3];
		//os_memcpy(T_arg->proto.udp->remote_ip,P_port_info->remote_ip,4);	// 内存拷贝
	}
	os_printf("对方IP:%d.%d.%d.%d\r\n",
			    T_arg->proto.udp->remote_ip[0],
				T_arg->proto.udp->remote_ip[1],
				T_arg->proto.udp->remote_ip[2],
				T_arg->proto.udp->remote_ip[3]);


	if(pdata[0]=='H')
	{
		if(os_strcmp(mqtt_Info.switch_state,",;switch0,1;")==0)
			{espconn_send(T_arg,"sw0:K",os_strlen("sw0:K"));}
		else
			{espconn_send(T_arg,"sw0:G",os_strlen("sw0:G"));}
	}
}
//============================================================================
// UDP AP 网络连接初始化
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_AP_NetCon_Init()
{
	// ②：结构体赋值
	//…………………………………………………………………………………………………
	AP_NetCon.type = ESPCONN_UDP;		// 通信协议：UDP

	// AP_NetCon.proto.udp只是一个指针，不是真正的esp_udp型结构体变量
	AP_NetCon.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));	// 申请内存
	// 此处无需设置目标IP/端口(ESP8266作为Server，不需要预先知道Client的IP/端口)
	//--------------------------------------------------------------------------
	//AP_NetCon.proto.udp->local_port  = espconn_port();	// 获取8266可用端口
	AP_NetCon.proto.udp->local_ip[0] = 192;		// 设置本地IP地址
	AP_NetCon.proto.udp->local_ip[1] = 168;
	AP_NetCon.proto.udp->local_ip[2] = 66;
	AP_NetCon.proto.udp->local_ip[3] = 1;
	AP_NetCon.proto.udp->local_port  = 8266 ;		// 设置本地端口

	//AP_NetCon.proto.udp->remote_port = 8888;		// 设置目标端口
	//AP_NetCon.proto.udp->remote_ip[0] = 192;		// 设置目标IP地址
	//AP_NetCon.proto.udp->remote_ip[1] = 168;
	//AP_NetCon.proto.udp->remote_ip[2] = 4;
	//AP_NetCon.proto.udp->remote_ip[3] = 2;
	// ③：注册/定义回调函数
	//-------------------------------------------------------------------------------------------
	espconn_regist_sentcb(&AP_NetCon,ESP8266_WIFI_Send_Cb);	// 注册网络数据发送成功的回调函数
	espconn_regist_recvcb(&AP_NetCon,ESP8266_WIFI_Recv_Cb);	// 注册网络数据接收成功的回调函数
	// ④：调用UDP初始化API
	//----------------------------------------------
	espconn_create(&AP_NetCon);	// 初始化UDP通信

}
//============================================================================
//OS_Timer_1软件定时器初始化(ms毫秒) 它的非常重要 里面有微信配网和默认连接wifi
//============================================================================
void ICACHE_FLASH_ATTR OS_Timer_1_Init(u32 time_ms, u8 time_repetitive)
{
	os_timer_disarm(&OS_Timer_1);	// 关闭定时器
	os_timer_setfn(&OS_Timer_1,(os_timer_func_t *)OS_Timer_1_Cb, NULL);	// 设置定时器
	os_timer_arm(&OS_Timer_1, time_ms, time_repetitive);  // 使能定时器
}
//============================================================================
// SNTP定时函数：获取当前网络时间
//============================================================================
void ICACHE_FLASH_ATTR OS_Timer_SNTP_Cb()
{
	 uint32	TimeStamp;		// 时间戳
	 char * Str_RealTime;	// 实际时间的字符串
	 // 查询当前距离基准时间(1970.01.01 00:00:00 GMT+8)的时间戳(单位:秒)
	 //-----------------------------------------------------------------
	 TimeStamp = sntp_get_current_timestamp();

	 if(TimeStamp)		// 判断是否获取到偏移时间
	 {
		 //os_timer_disarm(&OS_Timer_SNTP);	// 关闭SNTP定时器

		 // 查询实际时间(GMT+8):东八区(北京时间)
		 //--------------------------------------------
		 Str_RealTime = sntp_get_real_time(TimeStamp);

		 // 【实际时间】字符串 == "周 月 日 时:分:秒 年"
		 //------------------------------------------------------------------------
/*		 os_printf("\r\n----------------------------------------------------\r\n");
		 os_printf("SNTP_TimeStamp = %d\r\n", TimeStamp);		// 时间戳
		 os_printf("\r\nSNTP_InternetTime = %s", Str_RealTime);	// 实际时间
		 os_printf("时区：%d\r\n", sntp_get_timezone());
		 os_printf("--------------------------------------------------------\r\n");*/
		os_printf("@0x03:%s\r\n",Str_RealTime);

	 }
}
//============================================================================
// OS_Timer_SNTP软件定时初始化
//============================================================================
void ICACHE_FLASH_ATTR OS_Timer_SNTP_Init(uint32 time_ms, uint8 time_repetitive)
{
	os_timer_disarm(&OS_Timer_SNTP);
	os_timer_setfn(&OS_Timer_SNTP, (os_timer_func_t *)OS_Timer_SNTP_Cb, NULL);
	os_timer_arm(&OS_Timer_SNTP, time_ms, time_repetitive);
}
//============================================================================
// SNTP初始化
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_SNTP_Init()
{
	ip_addr_t * addr = (ip_addr_t *)os_zalloc(sizeof(ip_addr_t));
	sntp_setservername(0, "us.pool.ntp.org");	// 服务器_0【域名】
	sntp_setservername(1, "ntp.sjtu.edu.cn");	// 服务器_1【域名】
	ipaddr_aton("210.72.145.44", addr);			// 点分十进制 => 32位二进制
	sntp_setserver(2, addr);					// 服务器_2【IP地址】
	os_free(addr);								// 释放addr
	sntp_init();	// SNTP初始化API
	OS_Timer_SNTP_Init(1000,1);				// 1秒重复定时(SNTP)
	ESP8266_Func_Cmd();
}
//============================================================================
// ESP8266 模式配置的初始化
//============================================================================
void ICACHE_FLASH_ATTR ESP8266_AP_STA_Init()
{
	os_memset(&STA_Config, 0, sizeof(struct station_config));	// STA参数结构体 = 0
	spi_flash_read(Sector_STA_INFO*4096,(uint32 *)&STA_Config, 96);	// 读出【STA参数】(SSID/PASS)
	STA_Config.ssid[31] = 0;		// SSID最后添'\0'
	STA_Config.password[63] = 0;	// APSS最后添'\0'
	wifi_set_opmode(0x01);
	if(wifi_station_set_config(&STA_Config) == true)	// 设置STA参数，并保存到Flash
	{
		OS_Timer_1_Init(1000, 1);
	}
	os_memset(&AP_Config, 0, sizeof(struct softap_config));
	os_strcpy(AP_Config.ssid, ESP8266_AP_SSID);
	os_strcpy(AP_Config.password, ESP8266_AP_PASSWORD);
	AP_Config.ssid_len = os_strlen(ESP8266_AP_SSID);
	AP_Config.channel = 8;
	AP_Config.authmode = AUTH_WPA_WPA2_PSK;
	AP_Config.ssid_hidden = 0;
	AP_Config.max_connection = 4;
	AP_Config.beacon_interval = 100;
}
//============================================================================
// MQTT已成功连接：ESP8266发送【CONNECT】，并接收到【CONNACK】
//============================================================================
void mqttConnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;	// 获取mqttClient指针

    INFO("MQTT: Connected\r\n");

    // 【参数2：主题过滤器 / 参数3：订阅Qos】
    //-----------------------------------------------------------------
	//MQTT_Subscribe(client, "$creq", 0);	// 订阅主题"SW_LED"，QoS=0
	//MQTT_Subscribe(client, "$dp", 0);
//	MQTT_Subscribe(client, "SW_LED", 1);
//	MQTT_Subscribe(client, "SW_LED", 2);

	// 【参数2：主题名 / 参数3：发布消息的有效载荷 / 参数4：有效载荷长度 / 参数5：发布Qos / 参数6：Retain】
	//-----------------------------------------------------------------------------------------------------------------------------------------

//	MQTT_Publish(client, "SW_LED", "ESP8266_Online", strlen("ESP8266_Online"), 2, 0);
}
//============================================================================
// MQTT成功断开连接
//============================================================================
void mqttDisconnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Disconnected\r\n");
}
//============================================================================
// MQTT成功发布消息
//============================================================================
void mqttPublishedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Published\r\n");

}
//============================================================================
// 【接收MQTT的[PUBLISH]数据】函数		【参数1：主题 / 参数2：主题长度 / 参数3：有效载荷 / 参数4：有效载荷长度】
//===============================================================================================================
void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
    char *topicBuf = (char*)os_zalloc(topic_len+1);		// 申请【主题】空间
    char *dataBuf  = (char*)os_zalloc(data_len+1);		// 申请【有效载荷】空间
    char *str;
    MQTT_Client* client = (MQTT_Client*)args;	// 获取MQTT_Client指针

    os_memcpy(topicBuf, topic, topic_len);	// 缓存主题
    topicBuf[topic_len] = 0;				// 最后添'\0'
    os_memcpy(dataBuf, data, data_len);		// 缓存有效载荷
    dataBuf[data_len] = 0;					// 最后添'\0'
    INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);	// 串口打印【主题】【有效载荷】
    INFO("Topic_len = %d, Data_len = %d\r\n", topic_len, data_len);	// 串口打印【主题长度】【有效载荷长度】

// 【技小新】添加
//########################################################################################
    // 根据接收到的主题名/有效载荷，控制LED的亮/灭
    //-----------------------------------------------------------------------------------
    if( os_strncmp(topicBuf,"$creq",5) == 0 )			// 主题 == "SW_LED"
    {
    	if(os_strncmp(dataBuf,"sw",2) == 0 )
    	{

			if( os_strcmp(dataBuf,"sw1") == 0 )		// 有效载荷 == "LED_ON"
			{
				LED_ON;		// LED亮
				os_sprintf(mqtt_Info.switch_state, SWITCH0,1);
				string_Packet_Publish(&mqttClient, mqtt_Info.switch_state);
	        	espconn_send(&AP_NetCon,"sw0:K",os_strlen("sw0:K"));
	        	os_printf("@0X01: SW0: %s TH: %s TM: %s\r\n",mqtt_Info.switch_state, mqtt_Info.hour,mqtt_Info.min);
			}

			else if( os_strcmp(dataBuf,"sw0") == 0 )	// 有效载荷 == "LED_OFF"
			{
				LED_OFF;			// LED灭
				os_sprintf(mqtt_Info.switch_state, SWITCH0,0);
				string_Packet_Publish(&mqttClient, mqtt_Info.switch_state);
	        	espconn_send(&AP_NetCon,"sw0:G",os_strlen("sw0:G"));
	        	os_printf("@0X01: SW0: %s TH: %s TM: %s\r\n",mqtt_Info.switch_state, mqtt_Info.hour,mqtt_Info.min);
			}
    	}
    	else if(os_strncmp(dataBuf,"th",2) == 0)
		{

			os_sprintf(mqtt_Info.hour,SETHOUR,dataBuf+2);
			string_Packet_Publish(&mqttClient, mqtt_Info.hour);
			os_printf("@0X01: SW0: %s TH: %s TM: %s\r\n",mqtt_Info.switch_state, mqtt_Info.hour,mqtt_Info.min);
		}
    	else if(os_strncmp(dataBuf,"tm",2)==0)
		{

			os_sprintf(mqtt_Info.min,SETMIN,dataBuf+2);
			string_Packet_Publish(&mqttClient, mqtt_Info.min);
			os_printf("@0X01: SW0: %s TH: %s TM: %s\r\n",mqtt_Info.switch_state, mqtt_Info.hour,mqtt_Info.min);
		}


    }
//########################################################################################
    os_free(str);
    os_free(topicBuf);	// 释放【主题】空间
    os_free(dataBuf);	// 释放【有效载荷】空间
}
//===============================================================================================================


// user_init：entry of user application, init user function here
//===================================================================================================================
void user_init(void)
{
	uart_init(74880,115200);// 串口波特率设为115200
	delay_ms(10);
	os_printf("\r\n=======================================\r\n");
	ESP8266_AP_STA_Init();
	//按键GPIO配置
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U,FUNC_GPIO0);
	PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO0_U);
	GPIO_DIS_OUTPUT(GPIO_ID_PIN(0));
	//灯GPIO配置
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(2),1);
	//读内存的wifi 账号密码
	os_memset(&STA_Config,0,sizeof(struct station_config));			// STA_INFO = 0
	spi_flash_read(Sector_STA_INFO*4096,(uint32 *)&STA_Config, 96);	// 读出【STA参数】(SSID/PASS)
	spi_flash_read(0x98*4096,(uint32 *)&mqtt_Info,sizeof(MQTT_Info));
	STA_Config.ssid[31] = 0;		// SSID最后添'\0'
	STA_Config.password[63] = 0;	// APSS最后添'\0'

    CFG_Load();	// 加载/更新系统参数【WIFI参数、MQTT参数】

    // 网络连接参数赋值：服务端域名【mqtt_test_jx.mqtt.iot.gz.baidubce.com】、网络连接端口【1883】、安全类型【0：NO_TLS】
	//-------------------------------------------------------------------------------------------------------------------
	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);

	// MQTT连接参数赋值：客户端标识符【..】、MQTT用户名【..】、MQTT密钥【..】、保持连接时长【120s】、清除会话【1：clean_session】
	//----------------------------------------------------------------------------------------------------------------------------
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);

	// 设置MQTT相关函数
	//--------------------------------------------------------------------------------------------------
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);			// 设置【MQTT成功连接】函数的另一种调用方式
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);	// 设置【MQTT成功断开】函数的另一种调用方式
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);			// 设置【MQTT成功发布】函数的另一种调用方式
	MQTT_OnData(&mqttClient, mqttDataCb);					// 设置【接收MQTT数据】函数的另一种调用方式


	INFO("\r\nSystem started ...\r\n");
}
//===================================================================================================================
/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}
