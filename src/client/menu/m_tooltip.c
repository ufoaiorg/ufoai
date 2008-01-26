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

static const vec4_t tooltipBG = { 0.0f, 0.0f, 0.0f, 0.7f };
static const vec4_t tooltipColor = { 0.0f, 0.8f, 0.0f, 1.0f };

/**
 * @brief Generic tooltip function
 * FIXME: R_FontLength can change the string - which very very very bad for reference values and item names
 */
int MN_DrawTooltip (const char *font, char *string, int x, int y, int maxWidth, int maxHeight)
{
	int height = 0, width = 0;
	int lines = 5;

	R_FontLength(font, string, &width, &height);
	if (!width)
		return 0;

	/* maybe there is no maxWidth given */
	if (maxWidth < width)
		maxWidth = width;

	x += 5;
	y += 5;
	if (x + maxWidth +3 > VID_NORM_WIDTH)
		x -= (maxWidth + 10);
	R_DrawFill(x - 1, y - 1, maxWidth + 4, height, 0, tooltipBG);
	R_ColorBlend(tooltipColor);
	R_FontDrawString(font, 0, x + 1, y + 1, x + 1, y + 1, maxWidth, maxHeight, height, string, lines, 0, NULL, qfalse);
	R_ColorBlend(NULL);
	return width;
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
		char buf[256]; /* FIXME: see FIXME in MN_DrawTooltip */
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
