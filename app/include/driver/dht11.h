
#ifndef __DHT11_H
#define __DHT11_H

// 头文件引用
//==================================================================================
#include "os_type.h"			// os_XXX
#include "osapi.h"  			// os_XXX、软件定时器
#include "c_types.h"			// 变量类型
#include "user_interface.h" 	// 系统接口、system_param_xxx接口、WIFI、RateContro
//==================================================================================


// 全局变量
//=====================================================
extern u8 DHT11_Data_Array[6];		// DHT11数据数组

extern u8 DHT11_Data_Char[2][10];	// DHT11数据字符串
//=====================================================


// 函数声明
//==================================================================================================
void ICACHE_FLASH_ATTR Dht11_delay_ms(u32 C_time);			// 毫秒延时函数

void ICACHE_FLASH_ATTR DHT11_Signal_Output(u8 Value_Vol);	// DHT11信号线(IO5)输出参数电平

void ICACHE_FLASH_ATTR DHT11_Signal_Input(void);			// DHT11信号线(IO5) 设为输入

u8 ICACHE_FLASH_ATTR DHT11_Start_Signal_JX(void);			// DHT11：输出起始信号－＞接收响应信号

u8 ICACHE_FLASH_ATTR DHT11_Read_Bit(void);					// 读取DHT11一位数据

u8 ICACHE_FLASH_ATTR DHT11_Read_Byte(void);				// 读取DHT11一个字节

u8 ICACHE_FLASH_ATTR DHT11_Read_Data_Complete(void);		// 完整的读取DHT11数据操作

void ICACHE_FLASH_ATTR DHT11_NUM_Char(void);				// DHT11数据值转成字符串
//==================================================================================================


#endif /* __DHT11_H */
