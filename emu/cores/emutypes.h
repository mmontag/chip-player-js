#ifndef __EMUTYPES_H__
#define __EMUTYPES_H__

#ifndef _STDINT_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <stdtype.h>

typedef UINT8 uint8_t;
typedef INT8 int8_t;
typedef UINT16 uint16_t;
typedef INT16 int16_t;
typedef UINT32 uint32_t;
typedef INT32 int32_t;
#endif

#endif

#endif	// __EMUTYPES_H__
