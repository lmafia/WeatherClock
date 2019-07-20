#ifndef _RING_BUF_H_
#define _RING_BUF_H_

#include <os_type.h>
#include <stdlib.h>
#include "typedef.h"

typedef struct{
	U8* p_o;				/**< Original pointer */		// 队列首指针
	U8* volatile p_r;		/**< Read pointer */			// 读指针
	U8* volatile p_w;		/**< Write pointer */			// 写指针
	volatile I32 fill_cnt;	/**< Number of filled slots */	// 装填数量
	I32 size;				/**< Buffer size */				// 队列大小（2048）
}RINGBUF;

I16 ICACHE_FLASH_ATTR RINGBUF_Init(RINGBUF *r, U8* buf, I32 size);
I16 ICACHE_FLASH_ATTR RINGBUF_Put(RINGBUF *r, U8 c);
I16 ICACHE_FLASH_ATTR RINGBUF_Get(RINGBUF *r, U8* c);
#endif
