/**
 * @file ui_tooltip.c
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

#include "../cl_shared.h"
#include "../input/cl_keys.h"
#include "node/ui_node_window.h"
#include "ui_tooltip.h"
#include "ui_nodes.h"
#include "ui_parse.h"
#include "ui_render.h"
#include "ui_input.h"

static const vec4_t tooltipBG = { 0.0f, 0.0f, 0.0f, 0.7f };
static const vec4_t tooltipColor = { 0.0f, 0.8f, 0.0f, 1.0f };

/**
 * @brief Generic tooltip function
 */
int UI_DrawTooltip (const char *string, int x, int y, int maxWidth)
{
	const char *font = "f_small";
	int height = 0, width = 0;

	if (Q_strnull(string) || !font)
		return 0;

	R_FontTextSize(font, string, maxWidth, LONGLINES_WRAP, &width, &height, NULL, NULL);

	if (!width)
		return 0;

	x += 5;
	y += 5;

	if (x + width + 3 > VID_NORM_WIDTH)
		x -= width + 10;

	if (y + height + 3 > VID_NORM_HEIGHT)
		y -= height + 10;

	UI_DrawFill(x - 1, y - 1, width + 4, height + 4, tooltipBG);
	R_Color(tooltipColor);
	UI_DrawString(font, ALIGN_UL, x + 1, y + 1, x + 1, maxWidth, 0, string, 0, 0, NULL, qfalse, LONGLINES_WRAP);
	R_Color(NULL);

	return width;
}

/**
 * @brief Wrapper for UI tooltips
 */
void UI_Tooltip (uiNode_t *node, int x, int y)
{
	const char *string;
	const char *key = NULL;
	const char *tooltip = NULL;
	static const int maxWidth = 200;

	/* check values */
	if (node->key)
		key = Key_KeynumToString(node->key->key);
	if (node->tooltip)
		tooltip = UI_GetReferenceString(node, node->tooltip);

	/* normalize */
	if (tooltip && tooltip[0] == '\0')
		tooltip = NULL;
	if (key && key[0] == '\0')
		key = NULL;

	/* create tooltip */
	if (key && tooltip) {
		char buf[MAX_VAR];
		Com_sprintf(buf, sizeof(buf), _("Key: %s"), key);
		string = va("%s\n%s", tooltip, buf);
	} else if (tooltip) {
		string = tooltip;
	} else if (key) {
		string = va(_("Key: %s"), key);
	} else {
		return;
	}

	UI_DrawTooltip(string, x, y, maxWidth);
}
