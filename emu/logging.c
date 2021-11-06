#include <stdio.h>	// for vsnprintf()
#include <stdarg.h>

#include "../stdtype.h"
#include "logging.h"

#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif

#define LOGBUF_SIZE	0x100

// formatted logging
void emu_logf(DEV_LOGGER* logger, UINT8 level, const char* format, ...)
{
	va_list arg_list;
	int retVal;
	char buffer[LOGBUF_SIZE];
	
	if (logger->func == NULL)
		return;
	
	va_start(arg_list, format);
	retVal = _vsnprintf(buffer, LOGBUF_SIZE, format, arg_list);
	va_end(arg_list);
	if (retVal < 0 || retVal >= LOGBUF_SIZE)
		buffer[LOGBUF_SIZE - 1] = '\0';	// for older MSVC version and glibc <= 2.0.6
	
	logger->func(logger->param, logger->source, level, buffer);
	return;
}
