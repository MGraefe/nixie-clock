// pins.c
// Nixie clock program
// Copyright (c) 2014 Marius Graefe


#include <avr/io.h>

#include "pins.h"

void set_pin(const struct pin_t *pin, uint8_t value)
{
	uint8_t mask = 1 << pin->pin;
	volatile uint8_t *port;

	switch(pin->port)
	{
		case PB: port = &PORTB; break;
		case PC: port = &PORTC; break;
		case PD: port = &PORTD; break;
		default: port = 0;
	};

	*port = value ? *port | mask : *port & (~mask);
}


void set_pin_inout(const struct pin_t *pin, enum inout_e value)
{
	uint8_t mask = 1 << pin->pin;
	volatile uint8_t *port;

	switch(pin->port)
	{
		case PB: port = &DDRB; break;
		case PC: port = &DDRC; break;
		case PD: port = &DDRD; break;
		default: port = 0;
	}

	*port = value ? *port | mask : *port & (~mask);
}


uint8_t read_pin(const struct pin_t *pin)
{
	uint8_t mask = 1 << pin->pin;
	switch(pin->port)
	{
		case PB: return PINB & mask;
		case PC: return PINC & mask;
		case PD: return PIND & mask;
		default: return 0;
	}
}
