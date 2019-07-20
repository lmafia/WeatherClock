#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define USE_OPTIMIZE_PRINTF		// 优化打印
#define ESP8266_AP_SSID     "Hellow Iot"
#define ESP8266_AP_PASSWORD  "lky123456"

#define ESP8266_STA_SSID     "PDCN"
#define ESP8266_STA_PASSWORD  "1234567890"

#define		Sector_STA_INFO		0x99			// 【STA参数】保存扇区



typedef struct
{
	char city[10];
	char adcode[10];
}ip_City;

typedef struct
{
	char date[12];
	char week[3];
	char dayweather[20];
	char nightweather[20];
	char daytemp[3];
	char nighttemp[3];
}weather;

typedef struct
{
	char city[20];
	weather today_Weather;
	weather next_Weather;
	weather after_Weather;
}day_Weather;

typedef struct
{
	uint8_t hour[20];
	uint8_t min[20];
	uint8_t switch_state[20];
}MQTT_Info;
#define LED_OFF GPIO_OUTPUT_SET(GPIO_ID_PIN(2),1);
#define LED_ON GPIO_OUTPUT_SET(GPIO_ID_PIN(2),0);
#define TEMPER_HUM ",;humidity,%d.%d;temperature,%d.%d;"
#define SETHOUR ",;sethour,%s;"
#define SETMIN ",;setmin,%s;"
#define SWITCH0 ",;switch0,%d;"
#define DNS_Server "restapi.amap.com"

#define HTTP_GET_IP  "GET https://restapi.amap.com/v3/weather/weatherInfo?city=adcode&key=2b49a8c2b4e406e2da0f88de15ca2197&extensions=all HTTP/1.1  \r\n"  \
\
"Host:restapi.amap.com\r\n\r\n"



#define HTTP_GET_Weather1   "GET https://restapi.amap.com/v3/weather/weatherInfo?city=440100&key=2b49a8c2b4e406e2da0f88de15ca2197&extensions=all HTTP/1.1  \r\n"  \
\
"Host:restapi.amap.com\r\n"\
"Connection:close\r\n\r\n"

#define HTTP_GET_Weather   "GET https://api.seniverse.com/v3/weather/daily.json?key=githndjyckwnsrm2&location=ip&language=zh-Hans&unit=c&start=0&days=3 HTTP/1.1  \r\n"  \
\
"Host:api.seniverse.com\r\n"\
"Connection:close\r\n\r\n"

#endif

