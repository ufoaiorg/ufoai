/**
 * @file win_local.h
 * @brief Win32-specific UFO header file
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

#ifndef WIN_LOCAL_H
#define WIN_LOCAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#ifndef DWORD_PTR
# ifndef _WIN64
#  define DWORD_PTR DWORD
# endif
#endif

extern HINSTANCE global_hInstance;

#include "../system.h"
void Sys_BacktraceInit(void);
void Sys_BacktraceShutdown(void);

#endif /* WIN_LOCAL_H */
