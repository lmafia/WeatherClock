#ifndef FONT_H
#define FONT_H



#include "sys.h"
struct  chinese_Char
{
	unsigned char char_Model[54]; 
	char Chinese[3];

};

extern const unsigned char asc2_1809[95][27];

extern const unsigned char asc2_2412[95][36] ;

extern const unsigned char asc2_3321[95][64] ;
extern struct chinese_Char weather_Char[];

#endif