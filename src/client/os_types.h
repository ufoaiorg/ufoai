/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

Changes:
15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.
	Modified inclusion guard.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: #ifdef jail to whip a few platforms into the UNIX ideal.
 last mod: $Id$

 ********************************************************************/
#ifndef CLIENT_OS_TYPES_H
#define CLIENT_OS_TYPES_H

/* make it easy on the folks that want to compile the libs with a
   different malloc than stdlib */
#define _ogg_malloc  malloc
#define _ogg_calloc  calloc
#define _ogg_realloc realloc
#define _ogg_free    free

#ifdef _WIN32

#ifndef __GNUC__
/* MSVC/Borland */
typedef __int64 ogg_int64_t;
typedef __int32 ogg_int32_t;
typedef unsigned __int32 ogg_uint32_t;
typedef __int16 ogg_int16_t;
typedef unsigned __int16 ogg_uint16_t;
#elif defined __MINGW32__
#include <stdint.h>
typedef int16_t ogg_int16_t;
typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;
typedef uint64_t ogg_uint64_t;
#else
/* Cygwin */
#include <_G_config.h>
typedef _G_int64_t ogg_int64_t;
typedef _G_int32_t ogg_int32_t;
typedef _G_uint32_t ogg_uint32_t;
typedef _G_int16_t ogg_int16_t;
typedef _G_uint16_t ogg_uint16_t;
#endif

#elif defined(__MACOS__)

#include <sys/types.h>
typedef SInt16 ogg_int16_t;
typedef UInt16 ogg_uint16_t;
typedef SInt32 ogg_int32_t;
typedef UInt32 ogg_uint32_t;
typedef SInt64 ogg_int64_t;

#elif defined(__MACOSX__)		/* MacOS X Framework build */

#include <sys/types.h>
typedef int16_t ogg_int16_t;
typedef u_int16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef u_int32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;

#elif defined(__BEOS__)

/* Be */
#include <inttypes.h>
typedef int16_t ogg_int16_t;
typedef u_int16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef u_int32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;

#elif defined (__EMX__)

/* OS/2 GCC */
typedef short ogg_int16_t;
typedef unsigned short ogg_uint16_t;
typedef int ogg_int32_t;
typedef unsigned int ogg_uint32_t;
typedef long long ogg_int64_t;

#elif defined (DJGPP)

/* DJGPP */
typedef short ogg_int16_t;
typedef int ogg_int32_t;
typedef unsigned int ogg_uint32_t;
typedef long long ogg_int64_t;

#elif defined(R5900)

/* PS2 EE */
typedef long ogg_int64_t;
typedef int ogg_int32_t;
typedef unsigned ogg_uint32_t;
typedef short ogg_int16_t;

#else

#include <sys/types.h>
#include <ogg/config_types.h>

#endif

#endif /* CLIENT_OS_TYPES_H */
