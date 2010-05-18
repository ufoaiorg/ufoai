/**
 * @file m_icon.c
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

#include "m_main.h"
#include "m_internal.h"
#include "m_parse.h"
#include "m_icon.h"
#include "m_render.h"

/**
 * @brief normal, hovered, disabled, highlighted status are store in the same texture 256x256.
 * Each use a row of 64 pixels
 */
#define TILE_HEIGHT 64

const value_t mn_iconProperties[] = {
	{"size", V_POS, offsetof(menuIcon_t, size), MEMBER_SIZEOF(menuIcon_t, size)},
	{"single", V_BOOL, offsetof(menuIcon_t, single), 0},
	{"blend", V_BOOL, offsetof(menuIcon_t, blend), 0},
	{"pack64", V_BOOL, offsetof(menuIcon_t, pack64), 0},

	{"texl", V_POS, offsetof(menuIcon_t, pos[ICON_STATUS_NORMAL]), MEMBER_SIZEOF(menuIcon_t, pos[ICON_STATUS_NORMAL])},
	{"selectedtexl", V_REF_OF_STRING, offsetof(menuIcon_t, pos[ICON_STATUS_HOVER]), MEMBER_SIZEOF(menuIcon_t, pos[ICON_STATUS_HOVER])},
	{"disabledtexl", V_REF_OF_STRING, offsetof(menuIcon_t, pos[ICON_STATUS_DISABLED]), MEMBER_SIZEOF(menuIcon_t, pos[ICON_STATUS_DISABLED])},
	{"clickedtexl", V_REF_OF_STRING, offsetof(menuIcon_t, pos[ICON_STATUS_CLICKED]), MEMBER_SIZEOF(menuIcon_t, pos[ICON_STATUS_CLICKED])},

	{"image", V_REF_OF_STRING, offsetof(menuIcon_t, image[ICON_STATUS_NORMAL]), 0},
	{"selectedimage", V_REF_OF_STRING, offsetof(menuIcon_t, image[ICON_STATUS_HOVER]), 0},
	{"disabledimage", V_REF_OF_STRING, offsetof(menuIcon_t, image[ICON_STATUS_DISABLED]), 0},
	{"clickedimage", V_REF_OF_STRING, offsetof(menuIcon_t, image[ICON_STATUS_CLICKED]), 0},

	{"color", V_COLOR, offsetof(menuIcon_t, color[ICON_STATUS_NORMAL]), MEMBER_SIZEOF(menuIcon_t, color[ICON_STATUS_NORMAL])},
	{"selectedcolor", V_COLOR, offsetof(menuIcon_t, color[ICON_STATUS_HOVER]), MEMBER_SIZEOF(menuIcon_t, color[ICON_STATUS_HOVER])},
	{"disabledcolor", V_COLOR, offsetof(menuIcon_t, color[ICON_STATUS_DISABLED]), MEMBER_SIZEOF(menuIcon_t, color[ICON_STATUS_DISABLED])},
	{"clickedcolor", V_COLOR, offsetof(menuIcon_t, color[ICON_STATUS_CLICKED]), MEMBER_SIZEOF(menuIcon_t, color[ICON_STATUS_CLICKED])},

	{NULL, V_NULL, 0, 0}
};

/**
 * @brief Search a file name inside pics/icons/ according to the icon name
 * If it exists, generate a "single" icon using the size of the image
 * @param name Name of the icon
 * @return An icon, else NULL
 */
static menuIcon_t* MN_AutoGenerateIcon (const char* name)
{
	menuIcon_t* icon = NULL;
	const char* suffix[ICON_STATUS_MAX] = {"", "_hover", "_disabled", "_clicked"};
	int i;

	const char *picName = va("icons/%s", name);
	const image_t *pic = MN_LoadImage(picName);
	if (pic == NULL)
		return NULL;

	icon = MN_AllocStaticIcon(name);
	icon->single = qtrue;
	icon->image[ICON_STATUS_NORMAL] = MN_AllocStaticString(picName, 0);
	icon->size[0] = pic->width;
	icon->size[1] = pic->height;
	for (i = 1; i < ICON_STATUS_MAX; i++) {
		picName = va("icons/%s%s", name, suffix[i]);
		pic = MN_LoadImage(picName);
		if (pic != NULL)
			icon->image[i] = MN_AllocStaticString(picName, 0);
	}
	return icon;
}

/**
 * @brief Check if an icon name exists
 * @param[in] name Name of the icon
 * @return True if the icon exists
 * @note not very fast; if we use it often we should improve the search
 */
static qboolean MN_IconExists (const char* name)
{
	int i;
	for (i = 0; i < mn.numIcons; i++) {
		if (strncmp(name, mn.icons[i].name, MEMBER_SIZEOF(menuIcon_t, name)) != 0)
			continue;
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief Return an icon by is name
 * @param[in] name Name of the icon
 * @return The requested icon, else NULL
 * @note not very fast; if we use it often we should improve the search
 */
menuIcon_t* MN_GetIconByName (const char* name)
{
	int i;
	for (i = 0; i < mn.numIcons; i++) {
		if (strncmp(name, mn.icons[i].name, MEMBER_SIZEOF(menuIcon_t, name)) != 0)
			continue;
		return &mn.icons[i];
	}
	return MN_AutoGenerateIcon(name);
}

/**
 * @brief Allocate an icon from the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] name Name of the icon
 * @todo Assert out when we are not in parsing/loading stage
 */
menuIcon_t* MN_AllocStaticIcon (const char* name)
{
	menuIcon_t* result;
	assert(!MN_IconExists(name));
	if (mn.numIcons >= MAX_MENUICONS)
		Com_Error(ERR_FATAL, "MN_AllocStaticIcon: MAX_MENUICONS hit");

	result = &mn.icons[mn.numIcons];
	mn.numIcons++;

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
void MN_DrawIconInBox (const menuIcon_t* icon, iconStatus_t status, int posX, int posY, int sizeX, int sizeY)
{
	int texX;
	int texY;
	const char* image;

	/** @todo Add warning */
	if (status < 0 && status >= ICON_STATUS_MAX)
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
		texY = icon->pos[status][1] + (TILE_HEIGHT * status);
		image = icon->image[status];
		if (!image)
			image = icon->image[0];
	}

	posX += (sizeX - icon->size[0]) / 2;
	posY += (sizeY - icon->size[1]) / 2;

	if (icon->blend) {
		const vec_t *color;
		color = icon->color[status];
		R_Color(color);
	}

	MN_DrawNormImageByName(posX, posY, icon->size[0], icon->size[1],
		texX + icon->size[0], texY + icon->size[1], texX, texY, image);
	if (icon->blend)
		R_Color(NULL);
}
