/**
 * @file
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
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_render.h"
#include "ui_node_tbar.h"
#include "ui_node_abstractvalue.h"
#include "ui_node_abstractnode.h"

#define EXTRADATA_TYPE tbarExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

#define TEXTURE_WIDTH 250.0

void uiTBarNode::draw (uiNode_t* node)
{
	/* dataImageOrModel is the texture name */
	float shx;
	vec2_t nodepos;
	const char* ref = UI_GetReferenceString(node, EXTRADATA(node).image);
	float pointWidth;
	float width;
	if (Q_strnull(ref))
		return;

	UI_GetNodeAbsPos(node, nodepos);

	pointWidth = TEXTURE_WIDTH / 100.0;	/* relative to the texture */

	{
		float ps;
		const float min = getMin(node);
		const float max = getMax(node);
		float value = getValue(node);
		/* clamp the value */
		if (value > max)
			value = max;
		if (value < min)
			value = min;
		ps = (value - min) / (max - min) * 100;
		shx = EXTRADATA(node).texl[0];	/* left gap to the texture */
		shx += round(ps * pointWidth); /* add size from 0..TEXTURE_WIDTH */
	}

	width = (shx * node->box.size[0]) / TEXTURE_WIDTH;

	UI_DrawNormImageByName(false, nodepos[0], nodepos[1], width, node->box.size[1],
		shx, EXTRADATA(node).texh[1], EXTRADATA(node).texl[0], EXTRADATA(node).texl[1], ref);
}

void UI_RegisterTBarNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "tbar";
	behaviour->extends = "abstractvalue";
	behaviour->manager = UINodePtr(new uiTBarNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Image to use. Each behaviour use it like they want.
	 * @todo use V_REF_OF_STRING when its possible ('image' is never a cvar)
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "image", V_CVAR_OR_STRING, EXTRADATA_TYPE, image);

	/* @todo Need documentation */
	UI_RegisterExtradataNodeProperty(behaviour, "texh", V_POS, EXTRADATA_TYPE, texh);
	/* @todo Need documentation */
	UI_RegisterExtradataNodeProperty(behaviour, "texl", V_POS, EXTRADATA_TYPE, texl);
}
