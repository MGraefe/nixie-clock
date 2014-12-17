// dcf77.c
// Nixie clock program
// Copyright (c) 2014 Marius Graefe


#include <stdint.h>
#include "time.h"

static struct time_t *g_time = 0;

static uint8_t g_buffer[8];
static uint32_t g_last_flank = 0;
static uint8_t g_last = 0;
static uint8_t g_bit_index = -1;
static uint8_t g_parity = 0;
static uint8_t minute_tens, minute_ones, hour_tens, hour_ones, day_tens, day_ones, month_ones, month_tens;


void dcf77_init(struct time_t *time)
{
	g_time = time;
}


void dcf77_scrap()
{
	g_bit_index = -1;
}


void add_bit(uint8_t *val, uint8_t bit, uint8_t field_length)
{
	*val >>= 1;
	*val &= (bit << (field_length - 1));
}


void decode()
{
	g_time->seconds = 58;
	g_time->minutes = minute_tens * 10 + minute_ones;
	g_time->hours = hour_tens * 10 + hour_ones;
	g_time->day = day_tens * 10 + day_ones;
	g_time->month = month_tens * 10 + month_ones;
}


void dcf77_add_bit(uint8_t bit)
{
	if(g_bit_index == -1)
		return;

	//set / erase bit
	if(bit)
		g_buffer[g_bit_index << 3] |= 1 << (g_bit_index & 7);
	else
		g_buffer[g_bit_index << 3] &= ~(1 << (g_bit_index & 7));
	
	g_parity = g_parity != bit;
	
	if(g_bit_index >= 21 && g_bit_index <= 24)
		add_bit(&minute_ones, bit, 4);
	else if(g_bit_index >= 25 && g_bit_index <= 27)
		add_bit(&minute_tens, bit, 3);
	else if(g_bit_index >= 29 && g_bit_index <= 32)
		add_bit(&hour_ones, bit, 4);
	else if(g_bit_index == 33 || g_bit_index == 34)
		add_bit(&hour_tens, bit, 2);
	else if(g_bit_index >= 36 && g_bit_index <= 39)
		add_bit(&day_ones, bit, 4);
	else if(g_bit_index == 40 || g_bit_index == 41)
		add_bit(&day_tens, bit, 2);
	else if(g_bit_index >= 45 && g_bit_index <= 48)
		add_bit(&month_ones, bit, 4);
	else if(g_bit_index == 49)
		add_bit(&month_tens, bit, 1);
	
	if(g_bit_index == 28 || g_bit_index == 35 || g_bit_index == 58)
	{
		if(bit != g_parity)
			dcf77_scrap();
		else
			g_parity = 0;
	}

	if(g_bit_index >= 58)
	{
		decode();
		dcf77_scrap();
	}
	
	g_bit_index++;
}


void on_rising_flank(uint32_t time)
{
	if(g_bit_index != -1)
	{
		uint32_t time_low = time - g_last_flank;
		if(time_low > 7 && time_low < 13)
			dcf77_add_bit(0);
		else if(time_low > 17 && time_low < 23)
			dcf77_add_bit(1);
		else
			dcf77_scrap();
	}
	g_last_flank = time;
}


void on_falling_flank(uint32_t time)
{
	uint32_t time_high = time - g_last_flank;
	if(time_high > 190 && time_high < 210)
		g_bit_index = 0;
	g_last_flank = time;
}


void dcf77_update(uint8_t signal, uint32_t time)
{
	if(!g_last && signal)
		on_rising_flank(time);
	else if(g_last && !signal)
		on_falling_flank(time);
	
	g_last = signal;
}


#ifdef _TEST
int main()
{
	time_t time_obj = 0;
	dcf77_init(&time_obj);
	
	uint32_t time = 23;
	
	uint8_t bits[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		1,
		//minute:
		1, 0, 0, 1, //9
		1, 1, 0,    //5
		0,          //parity
		//hour:
		0, 0, 0, 1, //8
		1, 0,       //1
		0,			//parity
		//day
		0, 1, 0, 0, //2
		1, 0,       //1
		//day of week
		1, 0, 0,    //1
		//month
		1, 1, 1, 0, //7
		1, 0,       //1
		//year
		0, 0, 1, 0,  //4
		1, 0, 0, 0,  //1
		1,           //parity
	};
	
	while(true)
	{
		for(int i = 0; i < sizeof(bits); i++)
		{
			on_falling_flank(time);
			time += bits[i] ? 20 : 10;
			on_rising_flank(time);
			time += bits[i] ? 98 : 99;
		}
		time += 101;
	}
}
#endif
