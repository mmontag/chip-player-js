#ifndef __PLAYER_LOGGING_H__
#define __PLAYER_LOGGING_H__

#ifdef _DEBUG
#include <stdio.h>

#define debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug(...) {}
#endif



#endif	// __PLAYER_LOGGING_H__
