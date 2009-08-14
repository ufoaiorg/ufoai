/**
 * @file r_misc.h
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

#ifndef R_MISC_H
#define R_MISC_H

void R_Transform(const vec3_t transform, const vec3_t rotate, const vec3_t scale);
void R_PopMatrix(void);
void R_PushMatrix(void);
void R_InitMiscTexture(void);
void R_ScreenShot_f(void);
void R_ScreenShot(int x, int y, int width, int height, const char *filename, const char *ext);

#endif
