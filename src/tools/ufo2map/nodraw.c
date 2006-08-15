/**
 * @file nodraw.c
 * @brief
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


#include "qbsp.h"

vec3_t draw_mins, draw_maxs;
qboolean	drawflag;

void Draw_ClearWindow (void)
{
}

/*============================================================ */

#define	GLSERV_PORT	25001


void GLS_BeginScene (void)
{
}

void GLS_Winding (winding_t *w, int code)
{
}

void GLS_EndScene (void)
{
}
