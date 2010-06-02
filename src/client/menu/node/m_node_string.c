/**
 * @file m_node_string.c
 * @brief A string can be a normal string, or a cvar string.
 * A string prefixed with a "_" is the auto translated.
 * @code
 * string team_members
 * {
 * 	string	"_Team Members:"
 * 	pos	"480 486"
 * 	size "200 30"
 * }
 * string team_hired
 * {
 * 	string	"*cvar mn_hired"
 * 	pos	"480 508"
 * 	size "200 30"
 * }
 * @endcode
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

#include "../m_nodes.h"
#include "../m_font.h"
#include "../m_parse.h"
#include "../m_tooltip.h"
#include "../m_render.h"
#include "m_node_string.h"
#include "m_node_abstractnode.h"

#define EXTRADATA_TYPE stringExtraData_t
#define EXTRADATA(node) MN_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) MN_EXTRADATACONST(node, EXTRADATA_TYPE)

static void MN_StringNodeDraw (menuNode_t *node)
{
	vec2_t nodepos;
	const char *font = MN_GetFontFromNode(node);
	const char* ref = MN_GetReferenceString(node, node->text);
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	vec_t *color;

	if (!ref)
		return;
	MN_GetNodeAbsPos(node, nodepos);

	if (node->disabled)
		color = disabledColor;
	else
		color = node->color;

	R_Color(color);
	if (node->size[0] == 0)
		MN_DrawString(font, node->textalign, nodepos[0], nodepos[1], nodepos[0], nodepos[1], node->size[0], 0, 0, ref, 0, 0, NULL, qfalse, 0);
	else
		MN_DrawStringInBox(font, node->textalign, nodepos[0] + node->padding, nodepos[1] + node->padding, node->size[0] - node->padding - node->padding, node->size[1] - node->padding - node->padding, ref, EXTRADATA(node).longlines);
	R_Color(NULL);
}

/**
 * @brief Custom tooltip of string node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
static void MN_StringNodeDrawTooltip (menuNode_t *node, int x, int y)
{
	if (node->tooltip) {
		MN_Tooltip(node, x, y);
	} else {
		const char *font = MN_GetFontFromNode(node);
		const char* text = MN_GetReferenceString(node, node->text);
		qboolean isTruncated;
		if (!text)
			return;

		R_FontTextSize(font, text, node->size[0] - node->padding - node->padding, EXTRADATA(node).longlines, NULL, NULL, NULL, &isTruncated);
		if (isTruncated) {
			const int tooltipWidth = 250;
			static char tooltiptext[MAX_VAR * 4];
			tooltiptext[0] = '\0';
			Q_strcat(tooltiptext, text, sizeof(tooltiptext));
			MN_DrawTooltip(tooltiptext, x, y, tooltipWidth, 0);
		}
	}
}

static void MN_StringNodeLoading (menuNode_t *node)
{
	node->padding = 3;
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
	EXTRADATA(node).longlines = LONGLINES_PRETTYCHOP;
}

static const value_t properties[] = {
	/* What to do with text lines longer than node width. Default is to wordwrap them to make multiple lines.
	 * It can be LONGLINES_WRAP, LONGLINES_CHOP, LONGLINES_PRETTYCHOP
	 */
	{"longlines", V_INT, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, longlines), MEMBER_SIZEOF(EXTRADATA_TYPE, longlines)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterStringNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "string";
	behaviour->properties = properties;
	behaviour->draw = MN_StringNodeDraw;
	behaviour->drawTooltip = MN_StringNodeDrawTooltip;
	behaviour->loading = MN_StringNodeLoading;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
