/**
 * @file ui_sprite.h
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_UI_UI_SPRITE_H
#define CLIENT_UI_UI_SPRITE_H

#define UI_MAX_SPRITES 256

/**
 * @todo use a more generic name and move it to main ui
 */
typedef enum {
	SPRITE_STATUS_NORMAL = 0,  /**< ICON_STATUS_NORMAL */
	SPRITE_STATUS_HOVER = 1,   /**< ICON_STATUS_HOVER */
	SPRITE_STATUS_DISABLED = 2,/**< ICON_STATUS_DISABLED */
	SPRITE_STATUS_CLICKED = 3, /**< ICON_STATUS_CLICKED */

	SPRITE_STATUS_MAX          /**< ICON_STATUS_MAX */
} uiSpriteStatus_t;

typedef struct uiSprite_s {
	char name[MAX_VAR];
	vec2_t size;
	qboolean single;
	qboolean blend;
	qboolean pack64;

	vec4_t color[SPRITE_STATUS_MAX];
	char* image[SPRITE_STATUS_MAX];
	vec2_t pos[SPRITE_STATUS_MAX];
} uiSprite_t;

extern const value_t ui_spriteProperties[];

uiSprite_t* UI_GetSpriteByName(const char* name);
uiSprite_t* UI_AllocStaticSprite(const char* name) __attribute__ ((warn_unused_result));
void UI_DrawSpriteInBox(qboolean flip, const uiSprite_t* icon, uiSpriteStatus_t status, int posX, int posY, int sizeX, int sizeY);

#endif
