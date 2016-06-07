#ifndef LIBXMP_PAULA_H
#define LIBXMP_PAULA_H

/* 131072 to 0, 2048 entries */
#define MINIMUM_INTERVAL 16
#define BLEP_SCALE 17
#define BLEP_SIZE 2048
#define MAX_BLEPS (BLEP_SIZE / MINIMUM_INTERVAL)

/* the structure that holds data of bleps */
struct blep_state {
	int level;
	int age;
};

struct paula_data {
	/* the instantenous value of Paula output */
	int16 global_output_level;

	/* count of simultaneous bleps to keep track of */
	unsigned int active_bleps;

	/* place to keep our bleps in. MAX_BLEPS should be
	 * defined as a BLEP_SIZE / MINIMUM_EVENT_INTERVAL.
	 * For Paula, minimum event interval could be even 1, but it makes
	 * sense to limit it to some higher value such as 16. */
	struct blep_state blepstate[MAX_BLEPS];
};

#endif /* !LIBXMP_PAULA_H */
