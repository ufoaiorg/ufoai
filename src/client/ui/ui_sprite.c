/**
 * @file ui_sprite.c
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

#include "ui_main.h"
#include "ui_internal.h"
#include "ui_parse.h"
#include "ui_sprite.h"
#include "ui_render.h"

const value_t ui_spriteProperties[] = {
	{"size", V_POS, offsetof(uiSprite_t, size), MEMBER_SIZEOF(uiSprite_t, size)},
	{"single", V_BOOL, offsetof(uiSprite_t, single), 0},
	{"blend", V_BOOL, offsetof(uiSprite_t, blend), 0},
	{"pack64", V_BOOL, offsetof(uiSprite_t, pack64), 0},

	{"texl", V_POS, offsetof(uiSprite_t, pos[SPRITE_STATUS_NORMAL]), MEMBER_SIZEOF(uiSprite_t, pos[SPRITE_STATUS_NORMAL])},
	{"hoveredtexl", V_POS, offsetof(uiSprite_t, pos[SPRITE_STATUS_HOVER]), MEMBER_SIZEOF(uiSprite_t, pos[SPRITE_STATUS_HOVER])},
	{"disabledtexl", V_POS, offsetof(uiSprite_t, pos[SPRITE_STATUS_DISABLED]), MEMBER_SIZEOF(uiSprite_t, pos[SPRITE_STATUS_DISABLED])},
	{"clickedtexl", V_POS, offsetof(uiSprite_t, pos[SPRITE_STATUS_CLICKED]), MEMBER_SIZEOF(uiSprite_t, pos[SPRITE_STATUS_CLICKED])},

	{"image", V_REF_OF_STRING, offsetof(uiSprite_t, image[SPRITE_STATUS_NORMAL]), 0},
	{"hoveredimage", V_REF_OF_STRING, offsetof(uiSprite_t, image[SPRITE_STATUS_HOVER]), 0},
	{"disabledimage", V_REF_OF_STRING, offsetof(uiSprite_t, image[SPRITE_STATUS_DISABLED]), 0},
	{"clickedimage", V_REF_OF_STRING, offsetof(uiSprite_t, image[SPRITE_STATUS_CLICKED]), 0},

	{"color", V_COLOR, offsetof(uiSprite_t, color[SPRITE_STATUS_NORMAL]), MEMBER_SIZEOF(uiSprite_t, color[SPRITE_STATUS_NORMAL])},
	{"hoveredcolor", V_COLOR, offsetof(uiSprite_t, color[SPRITE_STATUS_HOVER]), MEMBER_SIZEOF(uiSprite_t, color[SPRITE_STATUS_HOVER])},
	{"disabledcolor", V_COLOR, offsetof(uiSprite_t, color[SPRITE_STATUS_DISABLED]), MEMBER_SIZEOF(uiSprite_t, color[SPRITE_STATUS_DISABLED])},
	{"clickedcolor", V_COLOR, offsetof(uiSprite_t, color[SPRITE_STATUS_CLICKED]), MEMBER_SIZEOF(uiSprite_t, color[SPRITE_STATUS_CLICKED])},

	{NULL, V_NULL, 0, 0}
};

/**
 * @brief Search a file name inside pics/ according to the sprite name
 * If it exists, generate a "single" sprite using the size of the image
 * @param name Name of the sprite
 * @return A sprite, else NULL
 */
static uiSprite_t* UI_AutoGenerateSprite (const char* name)
{
	uiSprite_t* sprite = NULL;
	const char* suffix[SPRITE_STATUS_MAX] = {"", "_hovered", "_disabled", "_clicked"};
	char basePicNameBuf[MAX_QPATH];
	const image_t *pic;
	int i;

	Q_strncpyz(basePicNameBuf, name, sizeof(basePicNameBuf));

	pic = UI_LoadImage(basePicNameBuf);
	if (pic == NULL)
		return NULL;

	sprite = UI_AllocStaticSprite(basePicNameBuf);
	sprite->image[SPRITE_STATUS_NORMAL] = UI_AllocStaticString(basePicNameBuf, 0);
	sprite->size[0] = pic->width;
	sprite->size[1] = pic->height;
	for (i = 1; i < SPRITE_STATUS_MAX; i++) {
		char picNameBuf[MAX_QPATH];
		Com_sprintf(picNameBuf, sizeof(picNameBuf), "%s%s", basePicNameBuf, suffix[i]);
		pic = UI_LoadImage(picNameBuf);
		if (pic != NULL)
			sprite->image[i] = UI_AllocStaticString(picNameBuf, 0);
	}
	return sprite;
}

/**
 * @brief Check if an sprite name exists
 * @param[in] name Name of the sprite
 * @return True if the sprite exists
 * @note not very fast; if we use it often we should improve the search
 */
static qboolean UI_SpriteExists (const char* name)
{
	int i;
	for (i = 0; i < ui_global.numSprites; i++) {
		if (strncmp(name, ui_global.sprites[i].name, MEMBER_SIZEOF(uiSprite_t, name)) != 0)
			continue;
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief Return an sprite by is name
 * @param[in] name Name of the sprite
 * @return The requested sprite, else NULL
 * @note not very fast; if we use it often we should improve the search
 */
uiSprite_t* UI_GetSpriteByName (const char* name)
{
	int i;
	for (i = 0; i < ui_global.numSprites; i++) {
		if (Q_streq(name, ui_global.sprites[i].name))
			return &ui_global.sprites[i];
	}
	return UI_AutoGenerateSprite(name);
}

/**
 * @brief Allocate an sprite to the UI static memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] name Name of the sprite
 * @todo Assert out when we are not in parsing/loading stage
 */
uiSprite_t* UI_AllocStaticSprite (const char* name)
{
	uiSprite_t* result;

	if (UI_SpriteExists(name))
		Com_Error(ERR_FATAL, "UI_AllocStaticSprite: \"%s\" sprite already allocated. Check your scripts.", name);

	if (ui_global.numSprites >= UI_MAX_SPRITES)
		Com_Error(ERR_FATAL, "UI_AllocStaticSprite: UI_MAX_SPRITES hit");

	result = &ui_global.sprites[ui_global.numSprites];
	ui_global.numSprites++;

	OBJZERO(*result);
	Q_strncpyz(result->name, name, sizeof(result->name));
	return result;
}

/**
 * @param[in] status The state of the sprite node
 * @param[in] sprite Context sprite
 * @param[in] posX Absolute X position of the top-left corner
 * @param[in] posY Absolute Y position of the top-left corner
 * @param[in] sizeX Width of the bounded box
 * @param[in] sizeY Height of the bounded box
 * @todo use named const for status
 */
void UI_DrawSpriteInBox (qboolean flip, const uiSprite_t* sprite, uiSpriteStatus_t status, int posX, int posY, int sizeX, int sizeY)
{
	int texX;
	int texY;
	const char* image;

	/** @todo Add warning */
	if (status >= SPRITE_STATUS_MAX)
		return;

	/** @todo merge all this cases */
	if (sprite->single || sprite->blend) {
		texX = sprite->pos[SPRITE_STATUS_NORMAL][0];
		texY = sprite->pos[SPRITE_STATUS_NORMAL][1];
		image = sprite->image[SPRITE_STATUS_NORMAL];
	} else if (sprite->pack64) {
		texX = sprite->pos[SPRITE_STATUS_NORMAL][0];
		texY = sprite->pos[SPRITE_STATUS_NORMAL][1] + (64 * status);
		image = sprite->image[SPRITE_STATUS_NORMAL];
	} else {
		texX = sprite->pos[status][0];
		texY = sprite->pos[status][1];
		image = sprite->image[status];
		if (!image) {
			if (texX == 0 && texY == 0) {
				texX = sprite->pos[SPRITE_STATUS_NORMAL][0];
				texY = sprite->pos[SPRITE_STATUS_NORMAL][1];
				image = sprite->image[SPRITE_STATUS_NORMAL];
			} else {
				image = sprite->image[SPRITE_STATUS_NORMAL];
			}
		}
	}
	if (!image)
		return;

	posX += (sizeX - sprite->size[0]) / 2;
	posY += (sizeY - sprite->size[1]) / 2;

	if (sprite->blend) {
		const vec_t *color;
		color = sprite->color[status];
		R_Color(color);
	}

	UI_DrawNormImageByName(flip, posX, posY, sprite->size[0], sprite->size[1],
		texX + sprite->size[0], texY + sprite->size[1], texX, texY, image);
	if (sprite->blend)
		R_Color(NULL);
}
