#include "test.h"

/*
24 - Short envelope loops

Envelope loops should include both of the loop points. Each instrument in this
test should play differently: first with no envelope, then a stuttering effect,
rapid left/right pan shift, and finally an arpeggio effect.
*/

TEST(test_storlek_24_short_envelope_loops)
{
	compare_mixer_data(
		"data/storlek_24.it",
		"data/storlek_24.data");
}
END_TEST
