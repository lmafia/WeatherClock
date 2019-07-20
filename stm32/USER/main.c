
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "Lcd_Driver.h"
#include "string.h"
#include "clock.h"
#include "logo.h"
#include "stdlib.h"
#include "timer.h"
#include "exit.h"
char buf[300];

extern clock_Class weather_Clock;
 int main(void)
 { 

	SystemInit();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	weather_Clock.clock_Time.hour = 1;
	delay_init();	
	EXTIX_Init();	 //???????	  
	uart_init(74880);
	Lcd_Init();
	TIM3_Int_Init(4999,7199);
	 

	 
	Lcd_Clear(BACK_COLOR);
	POINT_COLOR = LIGHTBLUE;
	chinese_Str(80,10,18,"白天:   ");
	chinese_Str(80,30,18,"夜晚:   ");
	chinese_Str(2,180,18,"明天:   ");
	chinese_Str(125,180,18,"后天:   ");
	 chinese_Str(174,80,18, "星期    ");
//	Lcd_Show_String(100,0,100,100,32,"0:123456:789");
//	Lcd_Show_String(0,70,100,100,32,"0:123456:789");
	//chinese_Str(100,2,"=");
	//delay_ms(199);

	//chinese_Char(100,32,"ll");	 

	weather_Clock.state = IDLE;
while(1)
{

	if(USART_RX_STA&0x8000)
	{
	
		printf("%s",USART_RX_BUF) ;
		if(strncmp((const char*)USART_RX_BUF, "@",1) == 0)
			sscanf ((const char*)USART_RX_BUF, "@%x:",&weather_Clock.state);
			USART_RX_STA = 0;
             strcpy(buf,USART_RX_BUF)     ;
			memset((char *)USART_RX_BUF ,0,300) ;
	}
	
//状态机大爆发
	switch (weather_Clock.state)
    {
		case IDLE:
		if(weather_Clock.clock_Time.hour == weather_Clock.time.hour
			&&weather_Clock.clock_Time.min == weather_Clock.time.min && state_Clock==0)
			{TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE );
			POINT_COLOR = RED;
			Lcd_Show_Image(198,107,clock_bmp); 
			}
		else if(weather_Clock.clock_Time.hour != weather_Clock.time.hour
			|| weather_Clock.clock_Time.min != weather_Clock.time.min )
		{state_Clock=0;}
			break;
			

		
		case MQTT:
			printf("mqtt");
            write_MQTT(&weather_Clock,buf);
			weather_Clock.state = IDLE;
			break;

		case WEATHER:
			write_Weather(&weather_Clock.ip_City, &weather_Clock.weather, buf);
			weather_Clock.state = IDLE;
			printf("ww");
			break;
		
		case DHT11:
			write_Dht11_Info(&weather_Clock.temper_Hum, buf);
			weather_Clock.state = IDLE;
				printf("dd");
		    break;
		
		case SNTP:
			write_Time(&(weather_Clock.time), buf);
			weather_Clock.state = IDLE;
		printf("sntp");
			break;
		
		default:
			weather_Clock.state = IDLE;
			break;
    }
	
	
	
	
	
	
	
	
}
}
