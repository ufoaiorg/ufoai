/**
 * @file
 * @brief A string can be a normal string, or a cvar string.
 * A string prefixed with a "_" or a string in the form "*msgid:some_msgid" is auto translated.
 * @code
 * string team_members
 * {
 * 	string	"_Team Members:"
 * 	pos	"480 486"
 * 	size "200 30"
 * }
 * string team_hired
 * {
 * 	string	"*cvar:foobar"
 * 	pos	"480 508"
 * 	size "200 30"
 * }
 * string team_hired_msgid
 * {
 * 	string	"*msgid:some_msgid"
 * 	pos	"480 508"
 * 	size "200 30"
 * }
 * @endcode
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../ui_nodes.h"
#include "../ui_font.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_tooltip.h"
#include "../ui_render.h"
#include "ui_node_string.h"
#include "ui_node_abstractnode.h"

#define EXTRADATA_TYPE stringExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

void uiStringNode::draw (uiNode_t* node)
{
	vec2_t nodepos;
	const char* font = UI_GetFontFromNode(node);
	const char* ref = UI_GetReferenceString(node, node->text);
	vec_t* color;

	if (!ref)
		return;
	UI_GetNodeAbsPos(node, nodepos);

	if (node->disabled)
		color = node->disabledColor;
	else
		color = node->color;

	R_Color(color);
	if (node->box.size[0] == 0)
		UI_DrawString(font, (align_t)node->contentAlign, nodepos[0], nodepos[1], nodepos[0], node->box.size[0], 0, ref);
	else
		UI_DrawStringInBox(font, (align_t)node->contentAlign, nodepos[0] + node->padding, nodepos[1] + node->padding, node->box.size[0] - node->padding - node->padding, node->box.size[1] - node->padding - node->padding, ref, (longlines_t) EXTRADATA(node).longlines);
	R_Color(nullptr);
}

/**
 * @brief Custom tooltip of string node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x,y Position of the mouse
 */
void uiStringNode::drawTooltip (const uiNode_t* node, int x, int y) const
{
	if (node->tooltip) {
		UI_Tooltip(node, x, y);
		return;
	}
	const char* font = UI_GetFontFromNode(node);
	const char* text = UI_GetReferenceString(node, node->text);
	bool isTruncated;
	if (!text)
		return;

	const int maxWidth = node->box.size[0] - node->padding - node->padding;
	const longlines_t longLines = (longlines_t)EXTRADATACONST(node).longlines;
	R_FontTextSize(font, text, maxWidth, longLines, nullptr, nullptr, nullptr, &isTruncated);
	if (!isTruncated)
		return;

	const int tooltipWidth = 250;
	static char tooltiptext[256];
	tooltiptext[0] = '\0';
	Q_strcat(tooltiptext, sizeof(tooltiptext), "%s", text);
	UI_DrawTooltip(tooltiptext, x, y, tooltipWidth);
}

void uiStringNode::onLoading (uiNode_t* node)
{
	node->padding = 3;
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
	Vector4Set(node->disabledColor, 0.5, 0.5, 0.5, 1.0);
	EXTRADATA(node).longlines = LONGLINES_PRETTYCHOP;
}

void UI_RegisterStringNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "string";
	behaviour->manager = UINodePtr(new uiStringNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* What to do with text lines longer than node width. Default is to wordwrap them to make multiple lines.
	 * It can be LONGLINES_WRAP, LONGLINES_CHOP, LONGLINES_PRETTYCHOP
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "longlines", V_INT, EXTRADATA_TYPE, longlines);
}
