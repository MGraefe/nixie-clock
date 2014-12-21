// types.h
// Nixie clock program
// Copyright (c) 2014 Marius Graefe

#ifndef TIME_H_
#define TIME_H_

#include <stdint.h>

struct time_t
{
	uint8_t month;
	uint8_t day;
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
};

#endif /* TIME_H_ */