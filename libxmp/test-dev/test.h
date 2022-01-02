#include <xmp.h>
#include <math.h>

#if defined(_MSC_VER) || defined(__WATCOMC__)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "../src/common.h"
#include "../src/md5.h"

#define TMP_FILE ".test"

#define TEST_FUNC(x) int _test_func_##x(void)

#undef TEST
#define TEST(x) TEST_FUNC(x) {

#define END_TEST \
	return 0; }

#if defined(_WIN32) || defined(__riscos__)
/* exit() prevents the failure message from being printed! */
#define FAIL_MSG "**fail**\n"
#else
#define FAIL_MSG
#endif
#define fail_unless(x, y) do { \
	if (!(x)) { printf("at %s:%d: %s: " FAIL_MSG, __FILE__, __LINE__, y); exit(1); } \
} while (0)

static inline int is_big_endian() {
	uint16 w = 0x00ff;
	return (*(char *)&w == 0x00);
}

static inline double libxmp_round(double val)
{
	return (val >= 0.0)? floor(val + 0.5) : ceil(val - 0.5);
}

/* Get period from note */
static inline int note_to_period(int n)
{
        return (int)libxmp_round (13696.0 / pow(2, (double)n / 12));
}

#define PERIOD ((int)libxmp_round(1.0 * info.channel_info[0].period / 4096))

enum playback_action
{
	PLAY_END,
	PLAY_FRAMES,
};

struct playback_sequence
{
	enum playback_action action;
	int value;
	int result;
};

int map_channel(struct player_data *, int);
int play_frame(struct context_data *);


int compare_module(struct xmp_module *, FILE *);
void dump_module(struct xmp_module *, FILE *);
void compare_playback(const char *, const struct playback_sequence *, int, int, int);
int compare_md5(const unsigned char *, const char *);
int check_md5(const char *, const char *);
int check_randomness(int *, int, double);
void read_file_to_memory(const char *, void **, long *);
void compare_mixer_data(const char *, const char *);
void compare_mixer_data_loops(const char *, const char *, int);
void compare_mixer_data_no_rv(const char *, const char *);
void convert_endian(unsigned char *, int);
void create_simple_module(struct context_data *, int, int);
void set_order(struct context_data *, int, int);
void set_instrument_volume(struct context_data *, int, int, int);
void set_instrument_nna(struct context_data *, int, int, int, int, int);
void set_instrument_envelope(struct context_data *, int, int, int, int);
void set_instrument_envelope_sus(struct context_data *, int, int);
void set_instrument_fadeout(struct context_data *, int, int);
void set_period_type(struct context_data *, int);
void set_quirk(struct context_data *, int, int);
void reset_quirk(struct context_data *, int);
void new_event(struct context_data *, int, int, int, int, int, int, int, int, int, int);

#define declare_test(x) TEST_FUNC(x)
#include "all_tests.c"
#undef declare_test
