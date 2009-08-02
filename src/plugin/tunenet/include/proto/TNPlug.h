#ifndef PROTO_TNPLUG_H
#define PROTO_TNPLUG_H

/*
**	$Id$
**	Includes Release 50.1
**
**	Prototype/inline/pragma header file combo
**
**	(C) Copyright 2003-2005 Amiga, Inc.
**	    All Rights Reserved
*/

#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif

#ifndef lib_TNPlug
#include "../libraries/TNPlug.h"
#endif

/****************************************************************************/

#ifndef __NOLIBBASE__
 #ifndef __USE_BASETYPE__
  extern struct Library * TNPlugBase;
 #else
  extern struct TNPlugLibBase * TNPlugBase;
 #endif /* __USE_BASETYPE__ */
#endif /* __NOLIBBASE__ */

/****************************************************************************/

#ifdef __amigaos4__
 #include "../interfaces/TNPlug.h"
 #ifdef __USE_INLINE__
  #include "../inline4/TNPlug.h"
 #endif /* __USE_INLINE__ */
 #ifndef CLIB_TNPLUG_PROTOS_H
  #define CLIB_TNPLUG_PROTOS_H 1
 #endif /* CLIB_TNPLUG_PROTOS_H */
 #ifndef __NOGLOBALIFACE__
  extern struct TNPlugIFace *ITNPlug;
 #endif /* __NOGLOBALIFACE__ */
#else /* __amigaos4__ */
 #ifndef CLIB_TNPLUG_PROTOS_H
  #include "../clib/TNPlug_protos.h"
 #endif /* CLIB_TNPLUG_PROTOS_H */
 #if defined(__GNUC__)
  #ifndef __PPC__
   #include "../inline/TNPlug.h"
  #else
   #include "../ppcinline/TNPlug.h"
  #endif /* __PPC__ */
 #elif defined(__VBCC__)
  #ifndef __PPC__
   #include "../inline/TNPlug_protos.h"
  #endif /* __PPC__ */
 #else
  #include "../pragmas/TNPlug_pragmas.h"
 #endif /* __GNUC__ */
#endif /* __amigaos4__ */

/****************************************************************************/

#endif /* PROTO_TNPLUG_H */
