#include "lib/sensors.h"
#include "dev/motion-sensor.h"
#include <avr/interrupt.h>

const struct sensors_sensor motion_sensor;

static struct timer debouncetimer;

static int enabled = 0;

static int
status(int type)
{
	switch (type) {
	case SENSORS_ACTIVE:
	case SENSORS_READY:
		return enabled;
	}
	return 0;
}



static int
value(int type)
{
	char t;
	t = PINE & _BV(PE7);
	return t;
}

static int
configure(int type, int c)
{
	switch (type) {
	case SENSORS_ACTIVE:
		if (c) {
			if(!status(SENSORS_ACTIVE)) {
				
				DDRE &= ~_BV(PE7);
				PORTE &= ~_BV(PE7);
				EICRB  |= _BV(ISC70);
				EICRB &= ~_BV(ISC71);
				EIMSK |= _BV(INT7); 
				enabled = 1;
				timer_set(&debouncetimer, 0);

			}
		} else {
			EIMSK &= ~_BV(INT7); 
			enabled = 0;
		}
		return 1;
	}
	return 0;
}


ISR(INT7_vect)
{
	if(timer_expired(&debouncetimer)) {
		timer_set(&debouncetimer, CLOCK_SECOND/8);
		sensors_changed(&motion_sensor);
	}
}


SENSORS_SENSOR(motion_sensor, MOTION_SENSOR,
	       value, configure, status);
