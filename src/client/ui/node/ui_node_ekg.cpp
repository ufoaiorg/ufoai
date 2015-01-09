/**
 * @file
 * @brief Health and morale ekg images for actors
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
#include "ui_node_ekg.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"

#define EXTRADATA_TYPE ekgExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

void uiEkgNode::draw (uiNode_t* node)
{
	vec2_t nodepos;
	const image_t* image;

	const char* imageName = UI_GetReferenceString(node, EXTRADATA(node).super.source);
	if (Q_strnull(imageName))
		return;

	UI_GetNodeAbsPos(node, nodepos);

	image = UI_LoadWrappedImage(imageName);
	if (image) {
		vec2_t size;
		const int ekgHeight = node->box.size[1];
		const int ekgWidth = image->width;
		/* we have different ekg parts in each ekg image... */
		const int ekgImageParts = image->height / node->box.size[1];
		const int ekgMaxIndex = ekgImageParts - 1;
		/* we change the index of the image part in 20s steps */
		/** @todo this magic number should be replaced with a sane calculation of the value */
		const int ekgDivide = 20;
		/* If we are in the range of (ekgMaxValue + ekgDivide, ekgMaxValue) we are using the first image */
		const int ekgMaxValue = ekgDivide * ekgMaxIndex;
		int ekgValue;
		float current;

		/** @todo these cvars should come from the script */
		/* ekg_morale and ekg_hp are the node names */
		if (node->name[0] == 'm')
			current = Cvar_GetValue("mn_morale") / EXTRADATA(node).scaleCvarValue;
		else
			current = Cvar_GetValue("mn_hp") / EXTRADATA(node).scaleCvarValue;

		ekgValue = std::min((int)current, ekgMaxValue);

		EXTRADATA(node).super.texl[1] = (ekgMaxIndex - (int)(ekgValue / ekgDivide)) * ekgHeight;
		EXTRADATA(node).super.texh[1] = EXTRADATA(node).super.texl[1] + ekgHeight;
		EXTRADATA(node).super.texl[0] = -(int) (EXTRADATA(node).scrollSpeed * CL_Milliseconds()) % ekgWidth;
		EXTRADATA(node).super.texh[0] = EXTRADATA(node).super.texl[0] + node->box.size[0];
		/** @todo code is duplicated in the image node code */
		if (node->box.size[0] && !node->box.size[1]) {
			const float scale = image->width / node->box.size[0];
			Vector2Set(size, node->box.size[0], image->height / scale);
		} else if (node->box.size[1] && !node->box.size[0]) {
			const float scale = image->height / node->box.size[1];
			Vector2Set(size, image->width / scale, node->box.size[1]);
		} else {
			if (EXTRADATA(node).super.preventRatio) {
				/* maximize the image into the bounding box */
				const float ratio = (float) image->width / (float) image->height;
				if (node->box.size[1] * ratio > node->box.size[0]) {
					Vector2Set(size, node->box.size[0], node->box.size[0] / ratio);
				} else {
					Vector2Set(size, node->box.size[1] * ratio, node->box.size[1]);
				}
			} else {
				Vector2Copy(node->box.size, size);
			}
		}
		UI_DrawNormImage(false, nodepos[0], nodepos[1], size[0], size[1],
				EXTRADATA(node).super.texh[0], EXTRADATA(node).super.texh[1], EXTRADATA(node).super.texl[0], EXTRADATA(node).super.texl[1], image);
	}
}

/**
 * @brief Called at the begin of the load from script
 */
void uiEkgNode::onLoading (uiNode_t* node)
{
	EXTRADATA(node).scaleCvarValue = 1.0f;
	EXTRADATA(node).scrollSpeed = 0.07f;
}

void UI_RegisterEKGNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "ekg";
	behaviour->extends = "image";
	behaviour->manager = UINodePtr(new uiEkgNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* The speed that the wrap texture is scrollend with */
	UI_RegisterExtradataNodeProperty(behaviour, "scrollspeed", V_FLOAT, ekgExtraData_t, scrollSpeed);
	/* @todo Need documentation */
	UI_RegisterExtradataNodeProperty(behaviour, "scale", V_FLOAT, ekgExtraData_t, scaleCvarValue);
}
