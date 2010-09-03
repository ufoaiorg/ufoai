/**
 * @file ui_icon.c
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

#include "ui_main.h"
#include "ui_internal.h"
#include "ui_parse.h"
#include "ui_icon.h"
#include "ui_render.h"

/**
 * @brief normal, hovered, disabled, highlighted status are store in the same texture 256x256.
 * Each use a row of 64 pixels
 */
#define TILE_HEIGHT 64

const value_t mn_iconProperties[] = {
	{"size", V_POS, offsetof(uiIcon_t, size), MEMBER_SIZEOF(uiIcon_t, size)},
	{"single", V_BOOL, offsetof(uiIcon_t, single), 0},
	{"blend", V_BOOL, offsetof(uiIcon_t, blend), 0},
	{"pack64", V_BOOL, offsetof(uiIcon_t, pack64), 0},

	{"texl", V_POS, offsetof(uiIcon_t, pos[ICON_STATUS_NORMAL]), MEMBER_SIZEOF(uiIcon_t, pos[ICON_STATUS_NORMAL])},
	{"hoveredtexl", V_POS, offsetof(uiIcon_t, pos[ICON_STATUS_HOVER]), MEMBER_SIZEOF(uiIcon_t, pos[ICON_STATUS_HOVER])},
	{"disabledtexl", V_POS, offsetof(uiIcon_t, pos[ICON_STATUS_DISABLED]), MEMBER_SIZEOF(uiIcon_t, pos[ICON_STATUS_DISABLED])},
	{"clickedtexl", V_POS, offsetof(uiIcon_t, pos[ICON_STATUS_CLICKED]), MEMBER_SIZEOF(uiIcon_t, pos[ICON_STATUS_CLICKED])},

	{"image", V_REF_OF_STRING, offsetof(uiIcon_t, image[ICON_STATUS_NORMAL]), 0},
	{"hoveredimage", V_REF_OF_STRING, offsetof(uiIcon_t, image[ICON_STATUS_HOVER]), 0},
	{"disabledimage", V_REF_OF_STRING, offsetof(uiIcon_t, image[ICON_STATUS_DISABLED]), 0},
	{"clickedimage", V_REF_OF_STRING, offsetof(uiIcon_t, image[ICON_STATUS_CLICKED]), 0},

	{"color", V_COLOR, offsetof(uiIcon_t, color[ICON_STATUS_NORMAL]), MEMBER_SIZEOF(uiIcon_t, color[ICON_STATUS_NORMAL])},
	{"hoveredcolor", V_COLOR, offsetof(uiIcon_t, color[ICON_STATUS_HOVER]), MEMBER_SIZEOF(uiIcon_t, color[ICON_STATUS_HOVER])},
	{"disabledcolor", V_COLOR, offsetof(uiIcon_t, color[ICON_STATUS_DISABLED]), MEMBER_SIZEOF(uiIcon_t, color[ICON_STATUS_DISABLED])},
	{"clickedcolor", V_COLOR, offsetof(uiIcon_t, color[ICON_STATUS_CLICKED]), MEMBER_SIZEOF(uiIcon_t, color[ICON_STATUS_CLICKED])},

	{NULL, V_NULL, 0, 0}
};

/**
 * @brief Search a file name inside pics/icons/ according to the icon name
 * If it exists, generate a "single" icon using the size of the image
 * @param name Name of the icon
 * @return An icon, else NULL
 */
static uiIcon_t* UI_AutoGenerateIcon (const char* name)
{
	uiIcon_t* icon = NULL;
	const char* suffix[ICON_STATUS_MAX] = {"", "_hovered", "_disabled", "_clicked"};
	int i;

	const char *picName = va("icons/%s", name);
	const image_t *pic = UI_LoadImage(picName);
	if (pic == NULL)
		return NULL;

	icon = UI_AllocStaticIcon(name);
	icon->image[ICON_STATUS_NORMAL] = UI_AllocStaticString(picName, 0);
	icon->size[0] = pic->width;
	icon->size[1] = pic->height;
	for (i = 1; i < ICON_STATUS_MAX; i++) {
		picName = va("icons/%s%s", name, suffix[i]);
		pic = UI_LoadImage(picName);
		if (pic != NULL)
			icon->image[i] = UI_AllocStaticString(picName, 0);
	}
	return icon;
}

#ifdef DEBUG
/**
 * @brief Check if an icon name exists
 * @param[in] name Name of the icon
 * @return True if the icon exists
 * @note not very fast; if we use it often we should improve the search
 */
static qboolean UI_IconExists (const char* name)
{
	int i;
	for (i = 0; i < ui_global.numIcons; i++) {
		if (strncmp(name, ui_global.icons[i].name, MEMBER_SIZEOF(uiIcon_t, name)) != 0)
			continue;
		return qtrue;
	}
	return qfalse;
}
#endif

/**
 * @brief Return an icon by is name
 * @param[in] name Name of the icon
 * @return The requested icon, else NULL
 * @note not very fast; if we use it often we should improve the search
 */
uiIcon_t* UI_GetIconByName (const char* name)
{
	int i;
	for (i = 0; i < ui_global.numIcons; i++) {
		if (strncmp(name, ui_global.icons[i].name, MEMBER_SIZEOF(uiIcon_t, name)) != 0)
			continue;
		return &ui_global.icons[i];
	}
	return UI_AutoGenerateIcon(name);
}

/**
 * @brief Allocate an icon to the UI static memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] name Name of the icon
 * @todo Assert out when we are not in parsing/loading stage
 */
uiIcon_t* UI_AllocStaticIcon (const char* name)
{
	uiIcon_t* result;
	/** @todo understand why we must hide this assert in release build with mingw */
#ifdef DEBUG
	assert(!UI_IconExists(name));
#endif
	if (ui_global.numIcons >= UI_MAX_ICONS)
		Com_Error(ERR_FATAL, "UI_AllocStaticIcon: UI_MAX_ICONS hit");

	result = &ui_global.icons[ui_global.numIcons];
	ui_global.numIcons++;

	memset(result, 0, sizeof(*result));
	Q_strncpyz(result->name, name, sizeof(result->name));
	return result;
}

/**
 * @param[in] status The state of the icon node
 * @param[in] icon Context icon
 * @param[in] posX Absolute X position of the top-left corner
 * @param[in] posY Absolute Y position of the top-left corner
 * @param[in] sizeX Width of the bounded box
 * @param[in] sizeY Height of the bounded box
 * @todo use named const for status
 */
void UI_DrawIconInBox (const uiIcon_t* icon, uiIconStatus_t status, int posX, int posY, int sizeX, int sizeY)
{
	int texX;
	int texY;
	const char* image;

	/** @todo Add warning */
	if (status >= ICON_STATUS_MAX)
		return;

	/** @todo merge all this cases */
	if (icon->single || icon->blend) {
		texX = icon->pos[ICON_STATUS_NORMAL][0];
		texY = icon->pos[ICON_STATUS_NORMAL][1];
		image = icon->image[ICON_STATUS_NORMAL];
	} else if (icon->pack64) {
		texX = icon->pos[ICON_STATUS_NORMAL][0];
		texY = icon->pos[ICON_STATUS_NORMAL][1] + (TILE_HEIGHT * status);
		image = icon->image[ICON_STATUS_NORMAL];
	} else {
		texX = icon->pos[status][0];
		texY = icon->pos[status][1];
		image = icon->image[status];
		if (!image) {
			texX = icon->pos[ICON_STATUS_NORMAL][0];
			texY = icon->pos[ICON_STATUS_NORMAL][1];
			image = icon->image[ICON_STATUS_NORMAL];
		}
	}
	if (!image)
		return;

	posX += (sizeX - icon->size[0]) / 2;
	posY += (sizeY - icon->size[1]) / 2;

	if (icon->blend) {
		const vec_t *color;
		color = icon->color[status];
		R_Color(color);
	}

	UI_DrawNormImageByName(posX, posY, icon->size[0], icon->size[1],
		texX + icon->size[0], texY + icon->size[1], texX, texY, image);
	if (icon->blend)
		R_Color(NULL);
}
