/*
/* config.c
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
#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "mqtt.h"
#include "config.h"
#include "user_config.h"
#include "debug.h"


// Flash扇区
//=========================================================================
// 扇区【Ox79】：系统参数存放区_0
// 扇区【Ox7A】：系统参数存放区_1

// 扇区【Ox7C】：【0：系统参数存放在0x79扇区  !0：系统参数存放在0x7A扇区】
//=========================================================================


// 全局变量
//================================================================================================
SYSCFG sysCfg;			// 系统配置缓存

SAVE_FLAG saveFlag;		// 参数扇区标志	【0：系统参数存放在0x79扇区	!0：系统参数存放在0x7A扇区】
//================================================================================================


// 将更新后的系统参数烧录到Flash中
//==================================================================================================
void ICACHE_FLASH_ATTR CFG_Save()
{
	// 读Flash【0x7C】扇区，存放到【saveFlag】（读出之前的持有人标识）
	//--------------------------------------------------------------------------------------------
	 spi_flash_read((CFG_LOCATION+3)*SPI_FLASH_SEC_SIZE, (uint32 *)&saveFlag, sizeof(SAVE_FLAG));

	 //根据【参数扇区标志】，将系统参数烧录到对应扇区
	//---------------------------------------------------------------------------------------
	if (saveFlag.flag == 0)		//【0：系统参数在0x79扇区】
	{
		spi_flash_erase_sector(CFG_LOCATION + 1);					// 擦除0x7A扇区
		spi_flash_write((CFG_LOCATION + 1) * SPI_FLASH_SEC_SIZE,	// 系统参数写入0x7A扇区
						(uint32 *)&sysCfg, sizeof(SYSCFG));

		saveFlag.flag = 1;											// 参数扇区标志 = 1
		spi_flash_erase_sector(CFG_LOCATION + 3);					// 擦除0x7C扇区
		spi_flash_write((CFG_LOCATION + 3) * SPI_FLASH_SEC_SIZE,	// 参数扇区标志写入0x7C
						(uint32 *)&saveFlag, sizeof(SAVE_FLAG));
	}

	else //saveFlag.flag != 0	//【!0：系统参数在0x7A扇区】
	{
		spi_flash_erase_sector(CFG_LOCATION + 0);					// 擦除0x79扇区
		spi_flash_write((CFG_LOCATION + 0) * SPI_FLASH_SEC_SIZE,	// 系统参数写入0x79扇区
						(uint32 *)&sysCfg, sizeof(SYSCFG));

		saveFlag.flag = 0;											// 参数扇区标志 = 0
		spi_flash_erase_sector(CFG_LOCATION + 3);					// 擦除0x7C扇区
		spi_flash_write((CFG_LOCATION + 3) * SPI_FLASH_SEC_SIZE,	// 参数扇区标志写入0x7C
						(uint32 *)&saveFlag, sizeof(SAVE_FLAG));
	}
	//---------------------------------------------------------------------------------------
}
//==================================================================================================


// 加载/更新系统参数【WIFI参数、MQTT参数】(由持有人标识决定)
//===================================================================================================================================
void ICACHE_FLASH_ATTR CFG_Load()
{
	INFO("\r\nload ...\r\n");

	// 读Flash【0x7C】扇区，存放到【saveFlag】（读出之前的持有人标识）
	//----------------------------------------------------------------------------------------------
	spi_flash_read((CFG_LOCATION+3)*SPI_FLASH_SEC_SIZE,(uint32 *)&saveFlag, sizeof(SAVE_FLAG));


	//根据【参数扇区标志】，读取对应扇区的系统参数【0：系统参数在0x79扇区	!0：系统参数在0x7A扇区】
	//-------------------------------------------------------------------------------------------------------------------------
	if (saveFlag.flag == 0)
	{
		spi_flash_read((CFG_LOCATION+0)*SPI_FLASH_SEC_SIZE,	(uint32 *)&sysCfg, sizeof(SYSCFG));		// 读出系统参数(1区：0x79)
	}
	else //saveFlag.flag != 0
	{
		spi_flash_read((CFG_LOCATION+1)*SPI_FLASH_SEC_SIZE,	(uint32 *)&sysCfg, sizeof(SYSCFG));		// 读出系统参数(2区：0x7A)
	}


	// 只有在【持有人标识和之前不同】的情况下，才会更新系统参数（修改系统参数时，一定要记得修改持有人标识的值）
	//------------------------------------------------------------------------------------------------------------------------
	//if(sysCfg.cfg_holder != CFG_HOLDER)		// 持有人标识不同
	{
		os_memset(&sysCfg, 0x00, sizeof sysCfg);	// 参数扇区=0

		sysCfg.cfg_holder = CFG_HOLDER;		// 更新持有人标识

		os_strcpy(sysCfg.device_id, MQTT_CLIENT_ID);		// 【MQTT_CLIENT_ID】MQTT客户端标识符
		sysCfg.device_id[sizeof(sysCfg.device_id) - 1] = '\0';					// 最后添'\0'（防止字符串填满数组，指针溢出）

/*
		os_strncpy(sysCfg.sta_ssid, STA_SSID, sizeof(sysCfg.sta_ssid)-1);		// 【STA_SSID】WIFI名称
		os_strncpy(sysCfg.sta_pwd, STA_PASS, sizeof(sysCfg.sta_pwd)-1);			// 【STA_PASS】WIFI密码
		sysCfg.sta_type = STA_TYPE;												// 【STA_TYPE】WIFI类型
*/

		os_strncpy(sysCfg.mqtt_host, MQTT_HOST, sizeof(sysCfg.mqtt_host)-1);	// 【MQTT_HOST】MQTT服务端域名/IP地址
		sysCfg.mqtt_port = MQTT_PORT;											// 【MQTT_PORT】网络连接端口号
		os_strncpy(sysCfg.mqtt_user, MQTT_USER, sizeof(sysCfg.mqtt_user)-1);	// 【MQTT_USER】MQTT用户名
		os_strncpy(sysCfg.mqtt_pass, MQTT_PASS, sizeof(sysCfg.mqtt_pass)-1);	// 【MQTT_PASS】MQTT密码

		sysCfg.security = DEFAULT_SECURITY;		/* default non ssl */			// 【DEFAULT_SECURITY】默认安全等级(默认=0，不加密)

		sysCfg.mqtt_keepalive = MQTT_KEEPALIVE;		// 【MQTT_KEEPALIVE】保持连接时长(宏定义==120)

		INFO(" default configuration\r\n");

		CFG_Save();		// 将更新后的系统参数烧录到Flash中
	}
}
//===================================================================================================================================
