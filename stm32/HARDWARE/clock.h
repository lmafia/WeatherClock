#ifndef CLOCK_H
#define CLOCK_H


#include "stm32f10x.h"
#include "string.h"

#define IDLE 0X00
#define MQTT 0X01
#define DHT11 0X02
#define SNTP 0x03
#define WEATHER 0X04
#define CLOCK 0x05

typedef struct
{
	char city[10];
	char adcode[10];
}Ip_City;

typedef struct
{
	char date[12];
	char week[4];
	char dayweather[18];
	char nightweather[18];
	char daytemp[4];
	char nighttemp[4];
}Weather;

typedef struct 
{
	char week[5];
	char mon[5];
	int year;
	int day;
	int hour;
	int min;
	int sec;
}Time;

typedef struct
{
	char temper[12];
	char hum[12];
	char temper1[12];
	char hum1[12];
}Temper_Hum;

typedef struct
{
	int hour;
	int min;
}Clock_time;

typedef struct
{
	Weather today_Weather;
	Weather next_Weather;
	Weather after_Weather;
}day_Weather;

typedef struct 
{
	uint8_t switch0;
	uint8_t state;
	day_Weather weather;
	Time    time;
	Clock_time clock_Time;
	Ip_City    ip_City;
	Temper_Hum temper_Hum; 
}clock_Class;


void write_Time(Time *pTime,char *SNTP_Time);
void write_MQTT(clock_Class *pclock, char *Clock_Info)  ;
void write_Weather(Ip_City *pCity,day_Weather *weather_Info,  char* Weather_Info);
void write_Dht11_Info(Temper_Hum *ptemper, char *pDHT11);
extern clock_Class weather_Clock;
#endif