/**
* \file
*		Ring Buffer library
*/

#include "ringbuf.h"


/**
* \brief init a RINGBUF object
* \param r pointer to a RINGBUF object
* \param buf pointer to a byte array
* \param size size of buf
* \return 0 if successfull, otherwise failed
*/

// 队列参数初始化：设置[原/读/写指针]、[装填数量]、[队列尺寸]
//==============================================================================
I16 ICACHE_FLASH_ATTR RINGBUF_Init(RINGBUF *r, U8* buf, I32 size)
{
	if(r == NULL || buf == NULL || size < 2) return -1;
	
	r->p_o = r->p_r = r->p_w = buf;	// [原/读/写指针]指向相同位置(队列首地址)
	r->fill_cnt = 0;
	r->size = size;
	
	return 0;
}
//==============================================================================

/**
* \brief put a character into ring buffer
* \param r pointer to a ringbuf object
* \param c character to be put
* \return 0 if successfull, otherwise failed
*/

// 向【当前写指针指向处】写入【参数2】
//==================================================================================================================================
I16 ICACHE_FLASH_ATTR RINGBUF_Put(RINGBUF *r, U8 c)
{
	if(r->fill_cnt>=r->size)return -1;		// 判断是否队列已满						// ring buffer is full, this should be atomic operation
	
	r->fill_cnt++;							// 装填数量++							// increase filled slots count, this should be atomic operation

	*r->p_w++ = c;							// 向【当前写指针指向处】写入【参数2】	// put character into buffer
	
	if(r->p_w >= r->p_o + r->size)			// 判断写指针是否超出队列				// rollback if write pointer go pass
		r->p_w = r->p_o;					// 写指针回到队列首指针					// the physical boundary
	
	return 0;
}
//==================================================================================================================================


/**
* \brief get a character from ring buffer
* \param r pointer to a ringbuf object
* \param c read character
* \return 0 if successfull, otherwise failed
*/

// 从【当前读指针指向处】读出数据，赋值给【参数2】指向的变量
//=======================================================================================================================================
I16 ICACHE_FLASH_ATTR RINGBUF_Get(RINGBUF *r, U8* c)
{
	if(r->fill_cnt<=0)return -1;			// 队列出错							// ring buffer is empty, this should be atomic operation
	
	r->fill_cnt--;							// 队列装填数量--					// decrease filled slots count

	*c = *r->p_r++;							// 获取[当前读指针指向处]的值		// get the character out
	
	if(r->p_r >= r->p_o + r->size)			// 判断读指针是否超出队列			// rollback if write pointer go pass
		r->p_r = r->p_o;					// 读指针回到队列起始				// the physical boundary
	
	return 0;
}
//=======================================================================================================================================
