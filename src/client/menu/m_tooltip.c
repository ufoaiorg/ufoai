/**
 * @file m_tooltip.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../client.h"
#include "m_menu.h"
#include "m_tooltip.h"
#include "m_nodes.h"
#include "m_parse.h"
#include "../../renderer/r_local.h"
#include "../../renderer/r_font.h"

static const vec4_t tooltipBG = { 0.0f, 0.0f, 0.0f, 0.7f };
static const vec4_t tooltipColor = { 0.0f, 0.8f, 0.0f, 1.0f };
static fontCacheList_t cacheList;

/**
 * @brief Generic tooltip function
 * @todo R_FontLength can change the string - which very very very bad for reference values and item names.
 * @todo Check for max height as well? (multi-line tooltips)
 */
int MN_DrawTooltip (const char *font, char *string, int x, int y, int maxWidth, int maxHeight)
{
	int height = 0, width = 0;
	int lines = 5;
	int dx; /**< Delta-x position. Relative to original x position. */

	if (!string || !*string || !font)
		return 0;

	/* Get height of string. The width will be ignored (except for the check below). */
	R_FontLength(font, string, &width, &height);

	if (!width)
		return 0;

	x += 5;
	y += 5;
/*	if (x + maxWidth +3 > VID_NORM_WIDTH)
		x -= (maxWidth + 10);*/

	R_FontGenerateCacheList(font, 0, x + 1, y + 1, x + 1, y + 1, maxWidth, height, string, lines, 0, NULL, qfalse, &cacheList);

	if (x + cacheList.width + 3 > VID_NORM_WIDTH)
		dx = -(cacheList.width + 10);
	else
		dx = 0;

/** @todo
	if (y + cacheList.height + 3 > VID_NORM_HEIGHT)
		dy = -(cacheList.height + 10);
*/

	R_DrawFill(x - 1 + dx, y - 1, cacheList.width + 4, cacheList.height + 4, 0, tooltipBG);
	R_ColorBlend(tooltipColor);
	R_FontRenderCacheList(&cacheList, y + 1, maxWidth, maxHeight, dx, 0);

	R_ColorBlend(NULL);
	return cacheList.width;
}

/**
 * @brief Wrapper for menu tooltips
 */
void MN_Tooltip (menu_t *menu, menuNode_t *node, int x, int y)
{
	const char *tooltip;
	int width = 0;

	/* tooltips
	 * data[MN_DATA_NODE_TOOLTIP] is a char pointer to the tooltip text
	 * see value_t nps for more info */

	/* maybe not tooltip but a key entity? */
	if (node->data[MN_DATA_NODE_TOOLTIP]) {
		char buf[256]; /** @todo @sa MN_DrawTooltip */
		tooltip = MN_GetReferenceString(menu, node->data[MN_DATA_NODE_TOOLTIP]);
		Q_strncpyz(buf, tooltip, sizeof(buf));
		width = MN_DrawTooltip("f_small", buf, x, y, width, 0);
		y += 20;
	}
	if (node->key[0]) {
		if (node->key[0] == '*') {
			tooltip = MN_GetReferenceString(menu, node->key);
			if (tooltip)
				Com_sprintf(node->key, sizeof(node->key), _("Key: %s"), tooltip);
		}
		MN_DrawTooltip("f_verysmall", node->key, x, y, width,0);
	}
}

/**
 * @brief Generic notice function
 */
int MN_DrawNotice (int x, int y)
{
	const vec4_t noticeBG = { 1.0f, 0.0f, 0.0f, 0.2f };
	const vec4_t noticeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	int height = 0, width = 0;
	const int maxWidth = 320;
	const int maxHeight = 100;
	const char *font = "f_normal";
	int lines = 5;
	int dx; /**< Delta-x position. Relative to original x position. */

	/* Get height of string. The width will be ignored (except for the check below). */
	R_FontLength(font, cl.msgText, &width, &height);

	if (!width)
		return 0;

	R_FontGenerateCacheList(font, 0, x + 1, y + 1, x + 1, y + 1, maxWidth, height, cl.msgText, lines, 0, NULL, qfalse, &cacheList);

	if (x + cacheList.width + 3 > VID_NORM_WIDTH)
		dx = -(cacheList.width + 10);
	else
		dx = 0;

	R_DrawFill(x - 1 + dx, y - 1, cacheList.width + 4, cacheList.height + 4, 0, noticeBG);
	R_ColorBlend(noticeColor);
	R_FontRenderCacheList(&cacheList, y + 1, maxWidth, maxHeight, dx, 0);

	R_ColorBlend(NULL);
	return cacheList.width;
}
