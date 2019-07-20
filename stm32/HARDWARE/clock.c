
#include "clock.h"
#include "Lcd_Driver.h"
#include "logo.h"
#include "usart.h"
clock_Class weather_Clock;


void write_Time(Time *pTime,char *SNTP_Time)
{

	char str[10];
	POINT_COLOR = WHITE  ;
	sscanf((const char*)SNTP_Time, "@0x03:%s %s %d  %d:%d:%d %d",
	pTime->week, pTime->mon, &(pTime->day), &(pTime->hour), &(pTime->min),&(pTime->sec), &(pTime->year));
	sprintf(str,"%02d:%02d:%02d",pTime->hour,pTime->min,pTime->sec);
	Lcd_Show_String(55,123,200,100,32,str);
	//Lcd_Show_String(60,100,200,100,32,str);
}


void write_MQTT(clock_Class *pclock, char *Clock_Info)
{
	char str[5];
	POINT_COLOR = GRAY;
	Lcd_Show_Image(198,107,clock_bmp); 
	sscanf(Clock_Info, "@0X01: SW0: ,;switch0,%x; TH: ,;sethour,%d; TM: ,;setmin,%d;",&pclock->switch0,&pclock->clock_Time.hour, &pclock->clock_Time.min);
                  
    sprintf(str, "%02d:%02d", pclock->clock_Time.hour, pclock->clock_Time.min) ;
	Lcd_Show_String(190,140,200,100,18,str);
	if(pclock->switch0 == 1)
	{	POINT_COLOR = YELLOW;
	   Lcd_Show_Image(20,120,light_bmp);
	}
	else if(pclock->switch0 == 0)
	{	
		POINT_COLOR = GRAY;
	   Lcd_Show_Image(20,120,light_bmp);
	}
}

void write_Weather(Ip_City *pCity,day_Weather *weather_Info,  char* Weather_Info)
{
	char str[40]="";
//	sscanf((const char*)Weather_Info,"@0x04: C:%s TD: %s TW: %s TDW: %s TNW: %s TDT: %s TNT: %s ND: %s NW: %s NDW: %s NNW: %s NDT: %s DNT: %s AD: %s AW: %s ADW: %s ANW: %s ADT: %s ANT: %s ",
//	pCity->city,
//	weather_Info->today_Weather.date,
//	weather_Info->today_Weather.week,
//	weather_Info->today_Weather.dayweather,
//	weather_Info->today_Weather.nightweather,
//	weather_Info->today_Weather.daytemp,
//	weather_Info->today_Weather.nighttemp,
//	//:::::::::::::::::::::::::::::::::::::::::::	
//	weather_Info->next_Weather.date,
//	weather_Info->next_Weather.week,
//	weather_Info->next_Weather.dayweather,
//	weather_Info->next_Weather.nightweather,
//	weather_Info->next_Weather.daytemp,
//	weather_Info->next_Weather.nighttemp,
//	//:::::::::::::::::::::::::::::::::::::::::::	
//	weather_Info->after_Weather.date,
//	weather_Info->after_Weather.week,
//	weather_Info->after_Weather.dayweather,
//	weather_Info->after_Weather.nightweather,
//	weather_Info->after_Weather.daytemp,
//	weather_Info->after_Weather.nighttemp
//   );


sscanf((const char*)Weather_Info,"@0x04: C:%s TD: %s TW: %d TDW: %s TNW: %s TDT: %s TNT: %s", 
	pCity->city,
	weather_Info->today_Weather.date,
	weather_Info->today_Weather.week,
	weather_Info->today_Weather.dayweather,
	weather_Info->today_Weather.nightweather,
	weather_Info->today_Weather.daytemp,
	weather_Info->today_Weather.nighttemp);

	sscanf((const char*)Weather_Info,"@0x04: ND: %s NW: %s NDW: %s NNW: %s NDT: %s DNT: %s",
			weather_Info->next_Weather.date,
			weather_Info->next_Weather.week,
			weather_Info->next_Weather.dayweather,
			weather_Info->next_Weather.nightweather,
			weather_Info->next_Weather.daytemp,
			weather_Info->next_Weather.nighttemp
	);
	sscanf((const char*)Weather_Info,"@0x04: AD: %s AW: %s ADW: %s ANW: %s ADT: %s ANT: %s",
			weather_Info->after_Weather.date,
			weather_Info->after_Weather.week,
			weather_Info->after_Weather.dayweather,
			weather_Info->after_Weather.nightweather,
			weather_Info->after_Weather.daytemp,
			weather_Info->after_Weather.nighttemp
	);



	 
	POINT_COLOR = WHITE;
	sprintf(str,"%s %s℃",weather_Info->today_Weather.dayweather,weather_Info->today_Weather.daytemp);
	chinese_Str(125,10, 18,str);
	sprintf(str,"%s %s℃",weather_Info->today_Weather.nightweather,weather_Info->today_Weather.nighttemp);
	chinese_Str(125,30, 18,str);
	sprintf(str,"%s%s℃",weather_Info->next_Weather.dayweather,weather_Info->next_Weather.daytemp);
	chinese_Str(2,200,18,str);
	Lcd_Draw_Line(122,205,122,235);
	sprintf(str,"%s%s℃",weather_Info->next_Weather.nightweather,weather_Info->next_Weather.nighttemp);
	chinese_Str(2,220,18,str);
	sprintf(str,"%s%s℃",weather_Info->after_Weather.dayweather,weather_Info->after_Weather.daytemp);
	chinese_Str(125,200, 18,str);
	sprintf(str,"%s%s℃",weather_Info->after_Weather.nightweather,weather_Info->after_Weather.nighttemp);
	chinese_Str(125,220, 18,str);	
	chinese_Str(0,4,18,pCity->city);	
 

                                     
	//POINT_COLOR = LIGHTBLUE; 
	//Lcd_Show_Image(69,3,cloud_bmp); 
	//Lcd_Show_Char(57,0,' ',24,0);


	POINT_COLOR = WHITE;
	
	
	Lcd_Show_String(62,100,200,100,24,weather_Info->today_Weather.date);

    
	if(weather_Info->today_Weather.week == "1")
		chinese_Str(210,80,18, "一");
	else if(weather_Info->today_Weather.week == "2")
		chinese_Str(210,80,18, "二");
	else if(weather_Info->today_Weather.week == "3")
		chinese_Str(210,80,18, "三");
	else if(weather_Info->today_Weather.week == "4")
		chinese_Str(210,80,18, "四");
	else if(weather_Info->today_Weather.week == "5")
		chinese_Str(210,80,18, "五");
	else if(weather_Info->today_Weather.week == "6")
		chinese_Str(210,80,18, "六");
	else 
		chinese_Str(210,80,18, "日");


}
//湿度
//温度
void write_Dht11_Info(Temper_Hum *ptemper, char *pDHT11)
{
	char str[13];
	sscanf((const char*)pDHT11, "@0x02: T: %s %s H: %s %s ",ptemper->temper,ptemper->temper1,ptemper->hum,ptemper->hum1);
	sprintf(str, "%s.%s℃",ptemper->temper,ptemper->temper1)  ;
	POINT_COLOR = WHITE ;
	chinese_Str(5,60,18,str);
	sprintf(str, "%s.%s",ptemper->hum,ptemper->hum1)  ;
	Lcd_Show_String(5,75,200,100,18,str);
	Lcd_Show_Char(48,75,'%',18,0);
	POINT_COLOR = BROWN;
    Lcd_Show_Image(10,30,home_bmp);
}
