// custom stdbool.h to for 1-byte bool types
#ifndef __CST_STDBOOL_H__
#define __CST_STDBOOL_H__

#ifndef __cplusplus	// C++ already has the bool-type

// the MS VC++ 6 compiler uses a one-byte-type (unsigned char, to be exact), so I'll reproduce this here
typedef unsigned char	bool;

#define false	0x00
#define true	0x01

#endif // ! __cplusplus

#endif // ! __CST_STDBOOL_H__
