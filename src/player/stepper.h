#ifndef __XMP_STEPPER_H
#define __XMP_STEPPER_H

#define STEPPER_SIZE 4

struct stepper {
	int values[STEPPER_SIZE];
	int size;
	int index;
};

void reset_stepper(struct stepper *);
void update_stepper(struct stepper *);
int get_stepper(struct stepper *);
void set_stepper(struct stepper *, int, ...);

#endif
