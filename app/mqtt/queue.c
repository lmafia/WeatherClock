/* str_queue.c
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
#include "queue.h"

#include "user_interface.h"
#include "osapi.h"
#include "os_type.h"
#include "mem.h"
#include "proto.h"

// 消息队列初始化【队列可以存放一个/多个MQTT报文】
//=============================================================================
void ICACHE_FLASH_ATTR QUEUE_Init(QUEUE *queue, int bufferSize)
{
	queue->buf = (uint8_t*)os_zalloc(bufferSize);	// 申请队列内存(2048字节)

	// 原/读/写指针都指向队列缓存、装填数量=0、队列尺寸=2048
	//-----------------------------------------------------------------
	RINGBUF_Init(&queue->rb, queue->buf, bufferSize);	// 设置队列参数
}
//=============================================================================

// 将报文写入队列，并返回写入字节数(包括特殊码)
//=================================================================================
int32_t ICACHE_FLASH_ATTR QUEUE_Puts(QUEUE *queue, uint8_t* buffer, uint16_t len)
{
	return PROTO_AddRb(&queue->rb, buffer, len);
}
//=================================================================================


// 解析队列中的报文【参数2：报文解析后缓存指针 / 参数3：解析后的报文长度】
//===================================================================================================
int32_t ICACHE_FLASH_ATTR QUEUE_Gets(QUEUE *queue, uint8_t* buffer, uint16_t* len, uint16_t maxLen)
{
	return PROTO_ParseRb(&queue->rb, buffer, len, maxLen);
}
//===================================================================================================

// 判断队列是否为空
//==================================================
BOOL ICACHE_FLASH_ATTR QUEUE_IsEmpty(QUEUE *queue)
{
	if(queue->rb.fill_cnt<=0)	// 装填数 <= 0
		return TRUE;			// 返回：空
	return FALSE;				// 返回：非空
}
//==================================================
