#include "exit.h"
#include "led.h"
#include "delay.h"
#include "usart.h"
#include "clock.h"
#include "timer.h"
#include "Lcd_Driver.h"
#include "logo.h"
#include "string.h"


uint8_t state_Clock = 0;

//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK战舰STM32开发板
//外部中断 驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2012/9/3
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//////////////////////////////////////////////////////////////////////////////////   
//外部中断0服务程序
void EXTIX_Init(void)
{
 
 	EXTI_InitTypeDef EXTI_InitStructure;
 	NVIC_InitTypeDef NVIC_InitStructure;

    KEY_Init();	 //	按键端口初始化

  	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);	//使能复用功能时钟


   //GPIOA.0	  中断线以及中断初始化配置 上升沿触发 PA0  WK_UP
 	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource8); 
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_InitStructure.EXTI_Line=EXTI_Line8;
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
  	EXTI_Init(&EXTI_InitStructure);		//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器


  	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;			//使能按键WK_UP所在的外部中断通道
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;	//抢占优先级2， 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;					//子优先级3
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
  	NVIC_Init(&NVIC_InitStructure); 
   
   //GPIOA.0	  中断线以及中断初始化配置 上升沿触发 PA0  WK_UP
 	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource12); 
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_InitStructure.EXTI_Line=EXTI_Line12;
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
  	EXTI_Init(&EXTI_InitStructure);		//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器


  	NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;			//使能按键WK_UP所在的外部中断通道
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;	//抢占优先级2， 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;					//子优先级3
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
  	NVIC_Init(&NVIC_InitStructure); 


 
}


void EXTI9_5_IRQHandler(void)
{
	delay_ms(15);//消抖       
	if(EXTI_GetITStatus(EXTI_Line8) != RESET)
     {        
		TIM_ITConfig(TIM3,TIM_IT_Update,DISABLE );
		 POINT_COLOR = GRAY;
		 Lcd_Show_Image(198,107,clock_bmp); 
		PAout(11) =0;
		state_Clock=1  ;
     }
          
	EXTI_ClearITPendingBit(EXTI_Line8); 
}


  
void EXTI15_10_IRQHandler(void)
{
	delay_ms(20);//消抖       
	if(EXTI_GetITStatus(EXTI_Line12) != RESET)
     {    
	delay_ms(20);//消抖  		 
	if(EXTI_GetITStatus(EXTI_Line12) != RESET)
     {        
		 if(weather_Clock.switch0==0)
			 printf("sw0:K\r\n");
		 else 
			 printf("sw0:G\r\n");
     }
	}   
	EXTI_ClearITPendingBit(EXTI_Line12); 
}

