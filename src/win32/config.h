#pragma once

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "osdcomm.h"

#define DRIVER_WIN32 1

#ifndef _WINDOWS_H
#ifdef _MSC_VER

// 4018: signed/unsigned mismatch
// 4244: conversion from XXX to YYY, possible loss of data
// 4761: integral size mismatch in argument; conversion supplied
#pragma warning(disable: 4018 4244 4761)

#define  strcasecmp  _strcmpi
#define  snprintf    _snprintf

#define S_ISDIR(a) ((a) & _S_IFDIR)

// Additional definitions needed by MSVC++ 9.0
#if _MSC_VER >= 1500
#define inline	__inline
#define fileno	_fileno
#else
#define write	_write
#endif

#define open	_open
#define lseek	_lseek
#define close	_close
#define fdopen	_fdopen
#define unlink	_unlink
#define strdup	_strdup

#ifndef PATH_MAX
#define PATH_MAX	260
#endif

#ifndef M_LN2
#define M_LN2	0.69314718055994530942
#endif

#endif
#endif

#endif	/* __CONFIG_H__ */
