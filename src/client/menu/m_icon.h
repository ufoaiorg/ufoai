/**
 * @file m_icon.h
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CLIENT_MENU_M_ICON_H
#define CLIENT_MENU_M_ICON_H

#define MAX_MENUICONS 128

typedef struct menuIcon_s {
	char name[MAX_VAR];
	char* image;
	vec2_t pos;
	vec2_t size;
	qboolean single;
	qboolean blend;
	vec4_t normalColor;
	vec4_t selectedColor;
	vec4_t disabledColor;
	vec4_t clickColor;
} menuIcon_t;

typedef enum {
	ICON_STATUS_NORMAL = 0,
	ICON_STATUS_HOVER = 1,
	ICON_STATUS_DISABLED = 2,
	ICON_STATUS_CLICKED = 3,

	ICON_STATUS_MAX
} iconStatus_t;

extern const value_t mn_iconProperties[];

menuIcon_t* MN_GetIconByName(const char* name);
menuIcon_t* MN_AllocStaticIcon(const char* name) __attribute__ ((warn_unused_result));
void MN_DrawIconInBox(const menuIcon_t* icon, iconStatus_t status, int posX, int posY, int sizeX, int sizeY);

#endif
