#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include "stm32f10x.h"
#include "stdlib.h"

//????
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE         	 0x001F  
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //??
#define BRRED 			 0XFC07 //???
#define GRAY  			 0X8430 //??
//GUI??

#define DARKBLUE      	 0X01CF	//???
#define LIGHTBLUE      	 0X7D7C	//???  
#define GRAYBLUE       	 0X5458 //???
//?????PANEL??? 
 
#define LIGHTGREEN     	 0X841F //???
//#define LIGHTGRAY        0XEF5B //???(PANNEL)
#define LGRAY 			 0XC618 //???(PANNEL),?????

#define LGRAYBLUE        0XA651 //????(?????)
#define LBBLUE           0X2B12 //????(???????)

#define WEIGHT 240
#define HEIGHT 240


#define CS_PIN GPIO_Pin_4

#define RS_PIN GPIO_Pin_5
#define WR_PIN GPIO_Pin_6

void Lcd_Init(void);
void Lcd_Show_Image(uint16_t x, uint16_t y,  unsigned char *p);
void Lcd_Show_String(uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t size,uint8_t *p);
void Lcd_Show_Num(uint16_t x,uint16_t y,u32 num,uint8_t len,uint8_t size,uint8_t mode);
uint32_t Lcd_Pow(uint8_t m,uint8_t n);
void Lcd_Show_Char(uint16_t x,uint16_t y,uint8_t num,uint8_t size,uint8_t mode);
void Lcd_Filled(uint16_t x_start,uint16_t y_start,uint16_t x_end,uint16_t y_end,uint16_t color);
void Lcd_Draw_Line(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);
void Lcd_Clear(uint16_t color)  ;
void Lcd_Draw_Point(uint16_t x,uint16_t y,uint16_t color);
void chinese_Char(uint16_t x, uint16_t  y, uint8_t size, unsigned char c[2]);
void chinese_Str(uint16_t x,uint16_t y,uint8_t size, char *str)	 ;
void showlogo(const unsigned char * p);
extern uint16_t POINT_COLOR ;
extern uint16_t BACK_COLOR  ;
#endif
