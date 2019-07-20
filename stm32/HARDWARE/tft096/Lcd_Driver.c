
#include "Lcd_Driver.h"
#include "delay.h"
#include "font.h"
#include "string.h"
#define CS_L GPIOA->BRR=CS_PIN;
#define CS_H GPIOA->BSRR=CS_PIN;

#define RS_L GPIOA->BRR=RS_PIN;
#define RS_H GPIOA->BSRR=RS_PIN;

#define WR_L GPIOA->BRR=WR_PIN;
#define WR_H GPIOA->BSRR=WR_PIN;
uint16_t POINT_COLOR = 0xFEFD  ;
uint16_t BACK_COLOR  = 0x0000 ;
#define DATAPORT GPIOB->ODR
#define Chinese_Lib_Len  40
void Lcd_Gpio_Init(void)
{

	GPIO_InitTypeDef  GPIO_InitStructure;
	      
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB|RCC_APB2Periph_AFIO ,ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	

}


//向液晶屏写一个8位指令
void Lcd_WriteCommand(uint8_t Command)
{
	CS_L;
	RS_L;
	DATAPORT=Command;
	WR_L;
	WR_H;
	RS_H;
	CS_H;
}

//向液晶屏写一个8位数据
void Lcd_WriteData(uint8_t Data)
{
	CS_L;
	RS_H;
	DATAPORT=Data;
	WR_L;
	WR_H;
	CS_H;
}
//向液晶屏写一个16位数据
void Lcd_WriteData16(uint16_t Data)
{

	 Lcd_WriteData(Data>>8); 	//写入高8位数据
	 Lcd_WriteData(Data); 			//写入低8位数据

}
//写寄存器 写指令加写数据
void Lcd_WriteReg(uint8_t Command,uint8_t Data)
{
    Lcd_WriteCommand(Command);
    Lcd_WriteData(Data);
}



//Lcd Init 
void Lcd_Init(void)
{	
	Lcd_Gpio_Init();


	Lcd_WriteCommand(0x11);
	delay_ms(10); //Delay 120ms
	//--------------------------------Display and color format setting-------------------

	Lcd_WriteCommand(0x36);
	Lcd_WriteData(0x00);
	Lcd_WriteCommand(0x3a);
	Lcd_WriteData(0x05);
	//--------------------------------ST7789S Frame rate setting-------------------------

	Lcd_WriteCommand(0xb2);
	Lcd_WriteData(0x0c);
	Lcd_WriteData(0x0c);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x33);
	Lcd_WriteData(0x33);
	Lcd_WriteCommand(0xb7);
	Lcd_WriteData(0x35);
	//---------------------------------ST7789S Power setting-----------------------------

	Lcd_WriteCommand(0xbb);
	Lcd_WriteData(0x35);
	Lcd_WriteCommand(0xc0);
	Lcd_WriteData(0x2c);
	Lcd_WriteCommand(0xc2);
	Lcd_WriteData(0x01);
	Lcd_WriteCommand(0xc3);
	Lcd_WriteData(0x13);
	Lcd_WriteCommand(0xc4);
	Lcd_WriteData(0x20);
	Lcd_WriteCommand(0xc6);
	Lcd_WriteData(0x0f);
	Lcd_WriteCommand(0xca);
	Lcd_WriteData(0x0f);
	Lcd_WriteCommand(0xc8);
	Lcd_WriteData(0x08);
	Lcd_WriteCommand(0x55);
	Lcd_WriteData(0x90);
	Lcd_WriteCommand(0xd0);
	Lcd_WriteData(0xa4);
	Lcd_WriteData(0xa1);
	//--------------------------------ST7789S gamma setting------------------------------
	Lcd_WriteCommand(0xe0);
	Lcd_WriteData(0x70);
	Lcd_WriteData(0x04);
	Lcd_WriteData(0x08);
	Lcd_WriteData(0x09);
	Lcd_WriteData(0x09);
	Lcd_WriteData(0x05);
	Lcd_WriteData(0x2a);
	Lcd_WriteData(0x33);
	Lcd_WriteData(0x41);
	Lcd_WriteData(0x07);
	Lcd_WriteData(0x13);
	Lcd_WriteData(0x13);
	Lcd_WriteData(0x29);
	Lcd_WriteData(0x2f);
	Lcd_WriteCommand(0xe1);
	Lcd_WriteData(0x70);
	Lcd_WriteData(0x03);
	Lcd_WriteData(0x09);
	Lcd_WriteData(0x0a);
	Lcd_WriteData(0x09);
	Lcd_WriteData(0x06);
	Lcd_WriteData(0x2b);
	Lcd_WriteData(0x34);
	Lcd_WriteData(0x41);
	Lcd_WriteData(0x07);
	Lcd_WriteData(0x12);
	Lcd_WriteData(0x14);
	Lcd_WriteData(0x28);
	Lcd_WriteData(0x2e);
	Lcd_WriteCommand(0x29);
}


/*************************************************
函数名：Lcd_Set_Region
功能：设置lcd显示区域，在此区域写点数据自动换行
入口参数：xy起点和终点
返回值：无
*************************************************/
void Lcd_Set_Region(uint16_t x_start,uint16_t y_start,uint16_t x_end,uint16_t y_end)
{		
	Lcd_WriteCommand(0x2a);
	Lcd_WriteData(0x00);
	Lcd_WriteData(x_start);
	Lcd_WriteData(0x00);
	Lcd_WriteData(x_end);

	Lcd_WriteCommand(0x2b);
	Lcd_WriteData(0x00);
	Lcd_WriteData(y_start);
	Lcd_WriteData(0x00);
	Lcd_WriteData(y_end);
	
	Lcd_WriteCommand(0x2c);

}

/*************************************************
函数名：Lcd_Set_XY
功能：设置lcd显示起始点
入口参数：xy坐标
返回值：无
*************************************************/
void Lcd_SetXY(uint16_t x,uint16_t y)
{
  	Lcd_Set_Region(x,y,x,y);
}

	
/*************************************************
函数名：Lcd_Draw_Point
功能：画一个点
入口参数：无
返回值：无
*************************************************/
void Lcd_Draw_Point(uint16_t x,uint16_t y,uint16_t color)
{
	Lcd_SetXY(x,y);
	Lcd_WriteCommand(0x2C);
	color = ~color;
	Lcd_WriteData16(color);

}    

/*************************************************
函数名：Lcd_Clear
功能：全屏清屏函数
入口参数：填充颜色COLOR
返回值：无
*************************************************/
void Lcd_Clear(uint16_t color)               
{	
   unsigned int i,m;
   color = ~color;
   Lcd_Set_Region(0,0,239,239);
   Lcd_WriteCommand(0x2C);
   for(i=0;i<240;i++)
    for(m=0;m<240;m++)
    {	
	  	Lcd_WriteData16(color);
    }   
}


//画线
//x_start,y_start:起点坐标
//x_end,y_end:终点坐标
void Lcd_Draw_Line(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
	uint16_t point = POINT_COLOR;
	uint16_t t;
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	delta_x=x_end-x_start; //计算坐标增量
	delta_y=y_end-y_start;
	uRow=x_start;
	uCol=y_start;
	if(delta_x>0)incx=1; //设置单步方向
	else if(delta_x==0)incx=0;//垂直线
	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if(delta_y==0)incy=0;//水平线
	else{incy=-1;delta_y=-delta_y;}
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴
	else distance=delta_y;
	for(t=0;t<=distance+1;t++ )//画线输出
	{
		Lcd_Draw_Point(uRow,uCol,point);//画点
		xerr+=delta_x ;
		yerr+=delta_y ;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}
}

/*************************************************
功能：填充指定区域
入口参数：填充颜色COLOR
返回值：无
*************************************************/
void Lcd_Filled(uint16_t x_start,uint16_t y_start,uint16_t x_end,uint16_t y_end,uint16_t color)               
{	
	uint16_t i;
	uint16_t j;
	color = ~color;
	Lcd_Set_Region(x_start,y_start, x_end, y_end);

	for(i=y_start; i<=y_end; i++)
	{
		for(j=x_start; j<=x_end; j++)
			Lcd_WriteData16(color);	//设置光标位置
	}
}
//在指定位置显示一个字符
//x,y:起始坐标
//num:要显示的字符:" "--->"~"
//size:字体大小 12/16/24
//mode:叠加方式(1)还是非叠加方式(0)
void Lcd_Show_Char(uint16_t x,uint16_t y,uint8_t num,uint8_t size,uint8_t mode)
{

	uint8_t temp,t1,t;
	uint16_t y0 = y;
	uint8_t csize;
	//uint8_t csize=(size/8+((size%8)?1:0))*(size/2);		//得到字体一个字符对应点阵集所占的字节数
	if(size == 18) csize =27;
	else if(size == 32) csize =64;
	else if(size == 24) csize = 36;
		
 	num=num-' ';//得到偏移后的值（ASCII字库是从空格开始取模，所以-' '就是对应字符的字库）
	for(t=0;t<csize;t++)
	{
		if(size==24)temp=asc2_2412[num][t]; 	 	//调用1206字体
		else if(size==32)temp=asc2_3321[num][t];	//调用2412字体
		else if(size==18)temp=asc2_1809[num][t];
		else return;								//没有的字库
		for(t1=0;t1<8;t1++)
		{
			if(temp&0x80)Lcd_Draw_Point(x,y,POINT_COLOR);
			else if(mode==0)Lcd_Draw_Point(x,y,BACK_COLOR);
			temp<<=1;
			y++;
			if(y>=HEIGHT)return;		//超区域了
			if((y-y0)==size)
			{
				y=y0;
				x++;
				if(x>=WEIGHT)return;	//超区域了
				break;
			}
		}
	}
}

//m^n函数
//返回值:m^n次方.
uint32_t Lcd_Pow(uint8_t m,uint8_t n)
{
	uint32_t result=1;
	while(n--)result*=m;
	return result;
}


//显示数字,高位为0,还是显示
//x,y:起点坐标
//num:数值(0~999999999);
//len:长度(即要显示的位数)
//size:字体大小
//mode:
//[7]:0,不填充;1,填充0.
//[6:1]:保留
//[0]:0,非叠加显示;1,叠加显示.
void Lcd_Show_Num(uint16_t x,uint16_t y,u32 num,uint8_t len,uint8_t size,uint8_t mode)
{
	uint8_t t,temp;
	uint8_t enshow=0;
	for(t=0;t<len;t++)
	{
		temp=(num/Lcd_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				if(mode&0X80)Lcd_Show_Char(x+(size/2)*t,y,'0',size,mode&0X01);
				else Lcd_Show_Char(x+(size/2)*t,y,' ',size,mode&0X01);
 				continue;
			}else enshow=1;

		}
	 	Lcd_Show_Char(x+(size/2)*t,y,temp+'0',size,mode&0X01);
	}
}
//显示字符串
//x,y:起点坐标
//width,height:区域大小
//size:字体大小
//*p:字符串起始地址
void Lcd_Show_String(uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t size,uint8_t *p)
{
	uint8_t x0=x;
	width+=x;
	height+=y;
    while((*p<='~')&&(*p>=' '))//判断是不是非法字符!
    {
        if(x>=width){x=x0;y+=size;}
        if(y>=height)break;//退出
        Lcd_Show_Char(x,y,*p,size,0);
        x+=size/2;
        p++;
    }
}

//===============================================
//函数名：Lcd_ShowImage //功能：显示图片 入口参数：图片名称
//============================================
void Lcd_Show_Image(uint16_t x, uint16_t y, unsigned char *p) 
{
int i, j;
	uint16_t y0;
	y0 = y;
	for(i=0;i<120;i++)
	{	
    uint8_t temp;
	temp = p[i];
	for(j=0;j<8;j++)
	  {
		if(temp&0x80)Lcd_Draw_Point(x,y,POINT_COLOR);
		else Lcd_Draw_Point(x,y,BACK_COLOR);
		temp<<=1;
		y++;
		if((y-y0)==30){y=y0;x++;break;}
	  }
	}	
}
void chinese_Char(uint16_t x, uint16_t  y, uint8_t size, unsigned char c[3])
{
	uint16_t i,j,k;
		uint16_t y0;
	uint8_t csize;
	uint16_t dcolor, bgcolor;
	dcolor = POINT_COLOR;
	dcolor = ~dcolor;
	bgcolor = BACK_COLOR;
	bgcolor = ~bgcolor;
    if(size==18) csize = 54;
	//else if(size == 24) csize = 72;
	y0 =y;
	//TFT_SetWindow(x,y,x+32-1, y+32-1);     //选择坐标位置
	//Lcd_Set_Region(x,y,31,31);
	for (k=0;k<Chinese_Lib_Len;k++) { //15标示自建汉字库中的个数，循环查询内码
	  if ((weather_Char[k].Chinese[0]==c[0])&&(weather_Char[k].Chinese[1]==c[1])&&(weather_Char[k].Chinese[2]==c[2])){ 
    	for(i=0;i<csize;i++) {
		  unsigned short m=weather_Char[k].char_Model[i];
		  for(j=0;j<8;j++) {
			if(m&0x80) {
				Lcd_Draw_Point(x,y,POINT_COLOR);
				}
			else {
			    Lcd_Draw_Point(x,y,BACK_COLOR);
				}
			m<<=1;
			y++;

			if((y-y0)==size)
			{
				y=y0;
				x++;
					
				break;
			}
			}    
		  }
		}  
	  }	
	}
 
	
void chinese_Str(uint16_t x,uint16_t y,uint8_t size, char *str)	 
{  
   while(*str)
   {

	 while((*str<='~')&&(*str>=' '))//判断是不是非法字符!
    {
        Lcd_Show_Char(x,y,*str,size,0);
        x+=size/2;
        str++;
    }
	chinese_Char(x,y,size,(unsigned char*)str);
	str++;
	x+=(size/4)+2;
   }
}

void showlogo(const unsigned char * p)
{

   unsigned int i,m;
    uint16_t val;
   Lcd_Set_Region(100,100,199,199);
   Lcd_WriteCommand(0x2C);
   for(i=0;i<100;i++)
    for(m=0;m<100;m++)
    {	
	  	val = *p;
		p++;
		val= (val<<8)|(*p) ;
		Lcd_WriteData16(val);
		p++;
    }   


}
	