#ifndef __PLAYER_LOGGING_H__
#define __PLAYER_LOGGING_H__

#ifdef _DEBUG
#include <stdio.h>

#if defined(_MSC_VER) && _MSC_VER < 1400
#define debug	printf
#else
#define debug(...) fprintf(stderr, __VA_ARGS__)
#endif

#else

#if defined(_MSC_VER) && _MSC_VER < 1400
// MS VC6 doesn't support the variadic macro syntax
#define debug	
#else
#define debug(...) {}
#endif

#endif

#endif	// __PLAYER_LOGGING_H__
