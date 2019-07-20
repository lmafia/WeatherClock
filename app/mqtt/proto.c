#include "proto.h"
#include "ringbuf.h"

/*
typedef struct
{
	U8 *buf;							// 解析后的数据指针
	U16 bufSize;						// 缓存区大小
	U16 dataLen;						// 已解析数据长度
	U8 isEsc;							// [转义码：0x7D]标志位
	U8 isBegin;							// [起始码：0x7E]标志位
	PROTO_PARSE_CALLBACK* callback;		// 回调函数

}	PROTO_PARSER;
*/

// 解析初始化：协议解析结构体初始化
// PROTO_Init(&proto, NULL, bufOut, maxBufLen);
//===================================================================================================================
I8 ICACHE_FLASH_ATTR PROTO_Init(PROTO_PARSER *parser, PROTO_PARSE_CALLBACK *completeCallback, U8 *buf, U16 bufSize)
{
    parser->buf = buf;						// 解析后的报文指针(指向局部数组变量的首地址)
    parser->bufSize = bufSize;				// 缓存空间大小
    parser->dataLen = 0;					// 当前解析数据长度
    parser->callback = completeCallback;	// 回调函数
    parser->isEsc = 0;						// [前缀码：0x7D]标志位
    return 0;
}
//===================================================================================================================


// 解析数据包的一个字节(特殊码需特殊处理)
//=================================================================================================
I8 ICACHE_FLASH_ATTR PROTO_ParseByte(PROTO_PARSER *parser, U8 value)
{	
	switch(value)
	{
		case 0x7D:							// [前缀码：0x7D]
			parser->isEsc = 1;				// 前缀码标志位 = 1
			break;
		
		case 0x7E:							// 开始码[0x7E]
			parser->dataLen = 0;			// 已解析数据长度 = 0
			parser->isEsc = 0;				// 不是[转义码]
			parser->isBegin = 1;			// 开始标志 = 1
			break;
		
		case 0x7F:							// 结束码[0x7F]
			if (parser->callback != NULL)	// 判断有无回调函数
				parser->callback();			// 执行回调函数
			parser->isBegin = 0;			// 开始标志 = 0

			return 0;						// 函数结束，返回0
			break;
		
		// 非特殊值(除[0x7D][0x7E][0x7F]外)
		//------------------------------------------------------------------------
		default:
			if(parser->isBegin == 0) break;	// 直到找到[开始码]，才会开始解析队列
			

			if(parser->isEsc)				// 如果上一字节是[前缀码：0x7D]
			{
				value ^= 0x20;				// 当前字节^=0x20(注：a^b^b == a)
				parser->isEsc = 0;
			}
				

			if(parser->dataLen < parser->bufSize)		// 解析数量 < 解析缓存区大小
				parser->buf[parser->dataLen++] = value;	// 将【解析后的数据】存放到【解析缓存区】
				
			break;
	}

    return -1;
}
//=================================================================================================


//=======================================================================
I8 ICACHE_FLASH_ATTR PROTO_Parse(PROTO_PARSER *parser, U8 *buf, U16 len)
{
    while(len--)
        PROTO_ParseByte(parser, *buf++);

    return 0;
}
//=======================================================================

// 解析队列中的数据，解析的报文指针赋值给[参数2：bufOut]【遇到[0x7F]结束码则返回0】
//=====================================================================================
I16 ICACHE_FLASH_ATTR PROTO_ParseRb(RINGBUF* rb, U8 *bufOut, U16* len, U16 maxBufLen)
{
	U8 c;

	PROTO_PARSER proto;		// 定义队列解析结构体

	PROTO_Init(&proto, NULL, bufOut, maxBufLen);	// 初始化队列解析结构体

	// 循环解析，直到遇见【Ox7F】结束码
	//-----------------------------------------------------------------
	while(RINGBUF_Get(rb, &c) == 0)
	{
		if(PROTO_ParseByte(&proto, c) == 0)		// 解析队列的一个字节
		{
			// 遇到【0x7F】结束码，进入这里
			//------------------------------------------------
			*len = proto.dataLen;		// 解析后的报文长度
			return 0;					// 跳出循环，函数结束
		}
	}
	return -1;		// 解析失败
}
//=====================================================================================


//=======================================================================
I16 ICACHE_FLASH_ATTR PROTO_Add(U8 *buf, const U8 *packet, I16 bufSize)
{
    U16 i = 2;
    U16 len = *(U16*) packet;

    if (bufSize < 1) return -1;

    *buf++ = 0x7E;
    bufSize--;

    while (len--) {
        switch (*packet) {
        case 0x7D:
        case 0x7E:
        case 0x7F:
            if (bufSize < 2) return -1;
            *buf++ = 0x7D;
            *buf++ = *packet++ ^ 0x20;
            i += 2;
            bufSize -= 2;
            break;
        default:
            if (bufSize < 1) return -1;
            *buf++ = *packet++;
            i++;
            bufSize--;
            break;
        }
    }

    if (bufSize < 1) return -1;
    *buf++ = 0x7F;

    return i;
}
//=======================================================================


// 将报文依次写入队列写指针(p_w)所指向的位置，添加【起始码】【结束码】【前缀码】，并返回写入的字节数
//---------------------------------------------------------------------------------------------------------
// 注：队列中【起始码】=[0x7E]、【结束码】=[0x7F]、【前缀码】=[Ox7D]。	报文中的[0x7D][0x7E][0x7F]，需要避讳特殊码(通过异或方式)
//===============================================================================================================================
I16 ICACHE_FLASH_ATTR PROTO_AddRb(RINGBUF *rb, const U8 *packet, I16 len)
{
    U16 i = 2;

    if(RINGBUF_Put(rb,0x7E)==-1) return -1;	// 向当前队列写指针指向处写入【起始码：0x7E】

    while (len--)	// 循环[len]次(报文所有字节)
    {
       switch (*packet)	// 获取当前数据包的一个字节
       {
       case 0x7D:		// 判断数据 ?= 【0x7D】/【0x7E】/【0x7F】
       case 0x7E:
       case 0x7F:

        	// 如果数据==[0x7D]||[0x7E]||[0x7F]，都在此数据前写入[0x7D]【因为[0x7E]==起始码、[0x7F]==结束码】
        	//-----------------------------------------------------------------------------------------------------------
        	if(RINGBUF_Put(rb, 0x7D) == -1) return -1;				// 在此数据前写入[0x7D]

        	if(RINGBUF_Put(rb, *packet++^0x20) == -1) return -1;	// 【0x7D/0x7E/0x7F】^=0x20，写入队列(注：a^b^b == a)

            i += 2;		// 写入队列的字节数+2

            break;

        // 数据包当前数据不是特殊码，则正常写入
        //------------------------------------------------------------------------------
       default:
    	   if(RINGBUF_Put(rb, *packet++) == -1) return -1;		// 写入数据包指针对应值

    	   i++;		// 写入队列的字节数+1

    	   break;
       }
	}

    if(RINGBUF_Put(rb, 0x7F) == -1) return -1;	// 向当前队列写指针指向处写入[结束码：0x7F]

    return i;	// 返回写入数量(包括起始码、结束码)
}
//===============================================================================================================================
