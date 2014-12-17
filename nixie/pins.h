// pins.h
// Nixie clock program
// Copyright (c) 2014 Marius Graefe


#ifndef PINS_H_
#define PINS_H_

#include <stdint.h>

enum inout_e
{
	PIN_IN = 0,
	PIN_OUT
};

//Port mappings
#define PB 0
#define PC 1
#define PD 2

struct pin_t
{
	uint8_t port;
	uint8_t pin;
};


void set_pin(const struct pin_t *pin, uint8_t value);
void set_pin_inout(const struct pin_t *pin, enum inout_e value);
uint8_t read_pin(const struct pin_t *pin);

#endif /* PINS_H_ */