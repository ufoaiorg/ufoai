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
	{"texl", V_POS, offsetof(menuIcon_t, pos), MEMBER_SIZEOF(menuIcon_t, pos)},
	{"size", V_POS, offsetof(menuIcon_t, size), MEMBER_SIZEOF(menuIcon_t, size)},
	{"image", V_REF_OF_STRING, offsetof(menuIcon_t, image), 0},
	{"single", V_BOOL, offsetof(menuIcon_t, single), 0},
	{"blend", V_BOOL, offsetof(menuIcon_t, blend), 0},
	{"normalcolor", V_COLOR, offsetof(menuIcon_t, normalColor), MEMBER_SIZEOF(menuIcon_t, normalColor)},
	{"selectedcolor", V_COLOR, offsetof(menuIcon_t, selectedColor), MEMBER_SIZEOF(menuIcon_t, selectedColor)},
	{"disabledcolor", V_COLOR, offsetof(menuIcon_t, disabledColor), MEMBER_SIZEOF(menuIcon_t, disabledColor)},
	{"clickColor", V_COLOR, offsetof(menuIcon_t, clickColor), MEMBER_SIZEOF(menuIcon_t, clickColor)},
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

	const char *picName = va("icons/%s", name);
	const image_t *pic = MN_LoadImage(picName);
	if (pic == NULL)
		return NULL;

	icon = MN_AllocStaticIcon(name);
	icon->single = qtrue;
	icon->image = MN_AllocStaticString(picName, 0);
	icon->size[0] = pic->width;
	icon->size[1] = pic->height;
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
	assert(MN_IconExists(name) == NULL);
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
	const int texX = icon->pos[0];
	int texY;
	assert(icon->image != NULL);

	if (icon->single || icon->blend)
		texY = icon->pos[1];
	else
		/** @todo use an enum for status and document what the expected image format is */
		texY = icon->pos[1] + (TILE_HEIGHT * status);

	posX += (sizeX - icon->size[0]) / 2;
	posY += (sizeY - icon->size[1]) / 2;

	if (icon->blend) {
		const vec_t *color;
		switch (status) {
		case ICON_STATUS_NORMAL:
			color = icon->normalColor;
			break;
		case ICON_STATUS_HOVER:
			color = icon->selectedColor;
			break;
		case ICON_STATUS_DISABLED:
			color = icon->disabledColor;
			break;
		case ICON_STATUS_CLICKED:
			color = icon->clickColor;
			break;
		default:
			return;
		}
		R_Color(color);
	}

	MN_DrawNormImageByName(posX, posY, icon->size[0], icon->size[1],
		texX + icon->size[0], texY + icon->size[1], texX, texY, icon->image);
	if (icon->blend)
		R_Color(NULL);
}
