/**
 * @file qal_osx.h
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

*/


#ifndef __QAL_OSX_H__
#define __QAL_OSX_H__


#ifndef __MAXOSX__
#error "You should not be including this file on this platform"
#endif


#include <OpenAL/al.h>
#include <OpenAL/alc.h>

#define GPA(a)			dlsym(oalState.hInstOpenAL, a);

typedef struct {
	HINSTANCE	hInstOpenAL;

	ALCdevice	*device;
	ALCcontext	*context;
} oalState_t;

extern oalState_t	oalState;

#endif	/* __QAL_OSX_H__ */
