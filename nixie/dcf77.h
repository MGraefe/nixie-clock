// dcf77.h
// Nixie clock program
// Copyright (c) 2014 Marius Graefe


#ifndef DCF77_H_
#define DCF77_H_

#include <stdint.h>

struct time_t;

void dcf77_init(struct time_t *time);
void dcf77_scrap();
void dcf77_add_bit(uint8_t bit);
void dcf77_on_rising_flank(uint32_t time);
void dcf77_on_falling_flank(uint32_t time);
void dcf77_update(uint8_t signal, uint32_t time);

#endif /* DCF77_H_ */