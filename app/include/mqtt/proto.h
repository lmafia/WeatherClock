/* 
 * File:   proto.h
 * Author: ThuHien
 *
 * Created on November 23, 2012, 8:57 AM
 */

#ifndef _PROTO_H_
#define	_PROTO_H_
#include <stdlib.h>
#include "typedef.h"
#include "ringbuf.h"

typedef void(PROTO_PARSE_CALLBACK)();


// 队列解析结构体
//---------------------------------------------------------------
typedef struct{
	U8 *buf;							// 解析后的报文指针
	U16 bufSize;						// 解析缓存区大小
	U16 dataLen;						// 已解析数据长度
	U8 isEsc;							// [前缀码：0x7D]标志位
	U8 isBegin;							// [起始码：0x7E]标志位
	PROTO_PARSE_CALLBACK* callback;		// 回调函数
}PROTO_PARSER;
//---------------------------------------------------------------


I8 ICACHE_FLASH_ATTR PROTO_Init(PROTO_PARSER *parser, PROTO_PARSE_CALLBACK *completeCallback, U8 *buf, U16 bufSize);
I8 ICACHE_FLASH_ATTR PROTO_Parse(PROTO_PARSER *parser, U8 *buf, U16 len);
I16 ICACHE_FLASH_ATTR PROTO_Add(U8 *buf, const U8 *packet, I16 bufSize);
I16 ICACHE_FLASH_ATTR PROTO_AddRb(RINGBUF *rb, const U8 *packet, I16 len);
I8 ICACHE_FLASH_ATTR PROTO_ParseByte(PROTO_PARSER *parser, U8 value);
I16 ICACHE_FLASH_ATTR PROTO_ParseRb(RINGBUF *rb, U8 *bufOut, U16* len, U16 maxBufLen);
#endif

