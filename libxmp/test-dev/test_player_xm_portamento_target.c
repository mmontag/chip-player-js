#include "test.h"

/*
 Don't assume a default portamento target. Also check the OpenMPT
 Porta-Pickup.xm test. Real life case happens in "Unknown danger." sent
 by Georgy Lomsadze (UNDANGER.XM).
*/

TEST(test_player_xm_portamento_target)
{
	compare_mixer_data(
		"data/xm_portamento_target.xm",
		"data/xm_portamento_target.data");
}
END_TEST
