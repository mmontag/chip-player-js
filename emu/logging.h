#ifndef __EMU_LOGGING_H__
#define __EMU_LOGGING_H__

#include "../stdtype.h"
#include "../common_def.h"
#include "EmuStructs.h"


#ifndef LOGBUF_SIZE
#define LOGBUF_SIZE	0x100
#endif

typedef struct _device_logger
{
	DEVCB_LOG func;
	void* source;
	void* param;
} DEV_LOGGER;

INLINE void dev_logger_set(DEV_LOGGER* logger, void* source, DEVCB_LOG func, void* param)
{
	logger->func = func;
	logger->source = source;
	logger->param = param;
	return;
}

void emu_logf(DEV_LOGGER* logger, UINT8 level, const char* format, ...);

#endif	// __EMU_LOGGING_H__
