// nixie.c
// Nixie clock program
// Copyright (c) 2014 Marius Gräfe


// ### Physical system-characteristics ###
// Targeted processor: Atmega328P-PU
// Clock with a 4-digit display (nixie tubes), 2 buttons and one LED.
// Output mapping can be derived below, each digit is outputted as a
// binary coded digit (BCD) to a nixie-driver (74141).
// Buttons are pulled to ground if pressed, floating if not.


// ### Logical system-characteristics ###
// Button 0 is used to switch between normal operation and settings mode
// State traversal is as following:
//     Normal op (display time/date)
//  -> Set hour (hour starts blinking)
//  -> Set minute (minutes start blinking)
//  -> Set day (date is displayed instead of time, day starts blinking)
//  -> Set month (date is displayed instead of time, month starts blinking)
//  -> Back to normal op

// If Button 1 is hold during normal operation the date is displayed instead 
// of the time.
// If button 1 is pressed during any settings mode the value that is currently
// modified is incremented by one.
// If button 1 is pressed and hold for more then one second during any
// settings mode the value that is currently modified is incremented 
// every 150 ms, starting after 1000 ms.

// If both buttons are pressed during power-on the system 
// enters a test mode  where all tubes display the same 
// digit which is inc/decrementable by with the two buttons.

// In all modes the LED is blinking once per second.


#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

enum inout_e
{
	PIN_IN = 0,
	PIN_OUT,
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


struct time_t
{
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};


enum modes_e
{
	MODE_NORMAL = 0,
	MODE_SET_HOURS,
	MODE_SET_MINUTES,
	MODE_SET_DAY,
	MODE_SET_MONTH,
	
	MODE_COUNT, //LAST ONE!
};


//Tube 4 (tens of hours) LSB -> MSB
const struct pin_t pins_d4[] = {
	{PD, 4}, //IC1-A
	{PD, 1}, //IC1-B
	{PD, 0}, //IC1-C
	{PD, 2}  //IC1-D
};

//Tube 3 (hours) LSB -> MSB
const struct pin_t pins_d3[] = {
	{PC, 3}, //IC2-A
	{PC, 1}, //IC2-B
	{PC, 0}, //IC2-C
	{PC, 2}  //IC2-D
};

//Tube 2 (tens of minutes) LSB -> MSB
struct pin_t pins_d2[] = {
	{PB, 4}, //IC3-A
	{PB, 2}, //IC3-B
	{PB, 1}, //IC3-C
	{PB, 3}  //IC3-D
};

//Tube 1 (minutes) LSB -> MSB
struct pin_t pins_d1[] = {
	{PB, 0}, //IC4-A
	{PD, 6}, //IC4-B
	{PD, 5}, //IC4-C
	{PD, 7}  //IC4-D
};


//Buttons
struct pin_t pins_but[] = {
	{PC, 4}, //BUTTON-0
	{PC, 5}  //BUTTON-1
};


//LEDs
struct pin_t pin_leds = { PD, 3 };

//Global time
struct time_t g_time = {1, 1, 13, 37, 0};

//Month lookup array
static const uint8_t month_days[12] =
//   J   F   M   A   M   J   J   A   S   O   N   D
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

//State of leds (for blinking etc)
uint8_t g_leds_state = 0;

//clock mode
enum modes_e g_mode = MODE_NORMAL;


void set_number(const struct pin_t *pins, uint8_t number)
{
	set_pin(&pins[0], number & 0b0001);
	set_pin(&pins[1], number & 0b0010);
	set_pin(&pins[2], number & 0b0100);
	set_pin(&pins[3], number & 0b1000);
}

void write_output(uint8_t a, uint8_t b)
{
	set_number(pins_d1, a > 99 ? 0xF : (a / 10));
	set_number(pins_d2, a > 99 ? 0xF : (a % 10));
	set_number(pins_d3, b > 99 ? 0xF : (b / 10));
	set_number(pins_d4, b > 99 ? 0xF : (b % 10));
}

uint8_t button_pressed(uint8_t index)
{
	return read_pin(&pins_but[index]) == 0;
}

void setup()
{
	cli(); //stop interrupts
	
	//Configure tube pins as PIN_OUTs
	for(uint8_t i = 0; i < 4; i++)
	{
		set_pin_inout(&pins_d1[i], PIN_OUT);
		set_pin_inout(&pins_d2[i], PIN_OUT);
		set_pin_inout(&pins_d3[i], PIN_OUT);
		set_pin_inout(&pins_d4[i], PIN_OUT);
	}

	//configure led pin as PIN_OUT
	set_pin_inout(&pin_leds, PIN_OUT);

	//configure button pins as PIN_IN
	set_pin_inout(&pins_but[0], PIN_IN);
	set_pin_inout(&pins_but[1], PIN_IN);

	//enable internal pullup resistors for buttons
	set_pin(&pins_but[0], 1);
	set_pin(&pins_but[1], 1);
	
	//set timer1 interrupt at 100Hz
	TCCR1A = 0; // set entire TCCR1A register to 0
	TCCR1B = 0; // same for TCCR1B
	TCNT1  = 0; //initialize counter value to 0
	// set compare match register for 100hz increments
	OCR1A = 19999; // = (16*10^6) / (100*8) - 1 (must be <65536)
	TCCR1B |= (1 << WGM12); // turn on CTC mode
	TCCR1B |= (1 << CS11); // Set CS11 bit for 8 prescaler
	TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt

	sei(); //enable interrupts
}


//Increment the second and adjust the other things appropriateley
void inc_time()
{
	g_time.seconds++;
	if(g_time.seconds > 59)
	{
		g_time.minutes++;
		g_time.seconds = 0;
	}
	if(g_time.minutes > 59)
	{
		g_time.hours++;
		g_time.minutes = 0;
	}
	if(g_time.hours > 23)
	{
		g_time.hours = 0;
		g_time.day++;
	}
	if(g_time.day > month_days[g_time.month-1])
	{
		g_time.month++;
		g_time.day = 1;
	}
	if(g_time.month > 12)
	{
		g_time.month = 1;
	}
}


uint8_t g_ticks_raw = 0;
uint32_t g_ticks = 0; //Resolution of 10 ms

//Timer interrupt function (called at 100 Hz)
ISR(TIMER1_COMPA_vect)
{
	g_ticks++;
	if(++g_ticks_raw > 99)
	{
		g_ticks_raw = 0;
		if(g_mode == MODE_NORMAL)
		inc_time();
		set_pin(&pin_leds, g_leds_state = !g_leds_state);
	}
}

uint32_t time_since(uint32_t time)
{
	return g_ticks - time;
}


uint8_t  g_last_buttons[2] = {0, 0};
uint8_t  g_blink_numbers = 0;
uint32_t g_inc_pressed_time = 0;
uint32_t g_last_inc_action = 0;
uint32_t g_last_blink_action = 0;
uint8_t  g_last_blink_state = 1;
uint32_t g_last_button_action[2] = {0, 0};
uint8_t  g_buttons[2] = {0, 0};

//Update button states, I implemented a kind of cooldown
//on button actions (high pass filter) to prevent rapid changes
//in case the button voltage changes rapidly
void update_button(uint8_t index)
{
	g_last_buttons[index] = g_buttons[index];
	
	if(button_pressed(index))
	{
		g_buttons[index] = 1;
		g_last_button_action[index] = g_ticks;
	}
	else if(time_since(g_last_button_action[index]) > 15)
	{
		g_buttons[index] = 0;
	}
}


void update_buttons(void)
{
	update_button(0);
	update_button(1);
}


void on_increment_pressed()
{
	//reset blinking
	g_last_blink_action = g_ticks;
	g_last_blink_state = 1;
	
	switch(g_mode)
	{
		case MODE_SET_HOURS:
		g_time.hours = (g_time.hours + 1) % 24;
		break;
		case MODE_SET_MINUTES:
		g_time.minutes = (g_time.minutes + 1) % 60;
		break;
		case MODE_SET_DAY:
		g_time.day = (g_time.day % 31) + 1;
		break;
		case MODE_SET_MONTH:
		g_time.month = (g_time.month % 12) + 1;
		break;
		default:
		break;
	}
}


//The main program (this function is called in an endless loop)
void main_program(void)
{
	update_buttons();
	
	//Menu button pressed? Switch menu mode
	if(g_buttons[0] && !g_last_buttons[0])
	{
		g_mode = (enum modes_e)((g_mode + 1) % MODE_COUNT);
		switch(g_mode)
		{
			case MODE_SET_HOURS:
			case MODE_SET_DAY:
				g_blink_numbers = 1;
				break;
			case MODE_SET_MINUTES:
			case MODE_SET_MONTH:
				g_blink_numbers = 2;
				break;
			default:
				g_blink_numbers = 0;
		};
		g_last_blink_action = g_ticks;
		g_last_blink_state = 1;
		g_last_inc_action = g_ticks;
		g_inc_pressed_time = g_ticks;
	}

	if(g_mode != MODE_NORMAL) //Inside Setup-mode?
	{
		//date/inc buttons pressed
		if(g_buttons[1])
		{
			if(!g_last_buttons[1])
			{
				on_increment_pressed();
				g_inc_pressed_time = g_ticks;
			}

			//Increment continously if pressed & hold
			if(time_since(g_inc_pressed_time) > 100 && time_since(g_last_inc_action) > 15)
			{
				g_last_inc_action = g_ticks;
				on_increment_pressed();
			}
		}
		
		//Are we currently setting time or date? display the appropriate output
		if(g_mode == MODE_SET_HOURS || g_mode == MODE_SET_MINUTES)
		{
			output[0] = g_time.hours;
			output[1] = g_time.minutes;
		}
		else
		{
			output[0] = g_time.day;
			output[1] = g_time.month;
		}
		
		// handle blinking of modifiable numbers
		if(g_blink_numbers != 0)
		{
			if(time_since(g_last_blink_action) > 50)
			{
				g_last_blink_action = g_ticks;
				g_last_blink_state = !g_last_blink_state;
			}
			// Going beyond single-digit range of the display
			// should turn the digit off
			if(g_last_blink_state == 0)
				output[g_blink_numbers-1] = 0xFF;
		}
	}
	else //normal operation (non-setup)
	{
		if(g_buttons[1])
		{
			output[0] = g_time.day;
			output[1] = g_time.month;
		}
		else
		{
			output[0] = g_time.hours;
			output[1] = g_time.minutes;
		}
	}
	
	write_output(output[0], output[1]);
}


// The test program (function is called in an endless loop)
// Used to display all digits
// Activated when both buttons are pressed during system startup
// Button 0 switches between continous running mode (where
// digits just get incremented every 250ms) and selective mode where
// Button 1 is used to increment the digits
void test_program(void)
{
	update_buttons();
	
	if(g_buttons[0] && !g_last_buttons[0])
		g_last_blink_state = 1 - g_last_blink_state;
	
	if(g_last_blink_state == 0)
	{
		if(g_buttons[1] && !g_last_buttons[1])
		{
			g_last_blink_action = g_ticks;
			g_blink_numbers = (g_blink_numbers + 1) % 10;
		}
	}
	else if(time_since(g_last_blink_action) > 25)
	{	
		g_last_blink_action = g_ticks;
		g_blink_numbers = (g_blink_numbers + 1) % 10;
	}
	
	set_number(pins_d1, g_blink_numbers);
	set_number(pins_d2, g_blink_numbers);
	set_number(pins_d3, g_blink_numbers);
	set_number(pins_d4, g_blink_numbers);
}


//Program entry point
int main()
{
	setup();

	//If both buttons are pressed on startup enter test mode
	if(button_pressed(0) && button_pressed(1))
	{
		while(1)
			test_program();
	}
	else
	{
		while(1)
			main_program();
	}
	
	return 0;
}