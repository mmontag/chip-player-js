#include <stdarg.h>
#include "stepper.h"

/* Stepper */

void reset_stepper(struct stepper *stepper)
{
	stepper->values[0] = 0;
	stepper->size = 1;
	stepper->index = 0;
}

void update_stepper(struct stepper *stepper)
{
	stepper->index++;
	stepper->index %= stepper->size;
}

int get_stepper(struct stepper *stepper)
{
	return stepper->values[stepper->index];
}

void set_stepper(struct stepper *stepper, int num, ...)
{
	va_list ap;
	int i;

	va_start(ap, num);

	if (num > STEPPER_SIZE)
		num = STEPPER_SIZE;

	stepper->size = num;
	stepper->index = 0;

	for (i = 0; i < num; i++) {
		stepper->values[i] = va_arg(ap, int);
	}

	va_end(ap);
}

