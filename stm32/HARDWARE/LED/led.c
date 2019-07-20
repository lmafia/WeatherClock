#include "led.h"
#include "delay.h"

void KEY_Init(void)
{
 
 GPIO_InitTypeDef  GPIO_InitStructure;
 	
 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	
	
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8|GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14;			
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 		
	
 GPIO_Init(GPIOB, &GPIO_InitStructure);					
										
}
 
u8 KEY_Scan(u8 mode)
{	 
	static u8 key_up=1;//按键按松开标志
	if(mode)key_up=1;  //支持连按		  
	if(key_up&&(KEY1==1))
	{
		delay_ms(2);//去抖动 
		key_up=1;
		if(KEY1==1)return KEY1_PRES;
	
	}else if(KEY1==0)key_up=1; 	    
 	return 0;// 无按键按下
}