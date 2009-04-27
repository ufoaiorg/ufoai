/**
 * @file m_node_ekg.c
 * @brief Health and morale ekg images for actors
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

#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_render.h"
#include "m_node_ekg.h"
#include "m_node_abstractnode.h"

#include "../../client.h"

void MN_EKGNodeDraw (menuNode_t *node)
{
	vec2_t size;
	vec2_t nodepos;
	const image_t *image;

	const char* imageName = MN_GetReferenceString(node, node->image);
	if (!imageName || imageName[0] == '\0')
		return;

	MN_GetNodeAbsPos(node, nodepos);

	image = MN_LoadImage(imageName);
	if (image) {
		int pt;
		const int ekgHeight = node->size[1];
		const int ekgWidth = image->width;
		const int time = cl.time;
		/* the higher the letter - the higher the speed => morale scrolls faster than hp */
		const int scrollSpeed = node->name[0] - 'a';
		/* we have four different ekg parts in each ekg image... */
		const int ekgImageParts = 4;
		/* ... thus indices go from 0 up tp 3 */
		const int ekgMaxIndex = ekgImageParts - 1;
		/* we change the index of the image part in 20s steps */
		const int ekgDivide = 20;
		/* If we are in the range of (ekgMaxValue + ekgDivide, ekgMaxValue) we are using the first image */
		const int ekgMaxValue = ekgDivide * ekgMaxIndex;
		/* the current value that was fetched from the cvar and was clamped to the max value */
		int ekgValue;

		/** @sa GET_HP GET_MORALE macros */
		/* ekg_morale and ekg_hp are the node names */
		if (node->name[0] == 'm')
			/* morale uses another value scaling as the hp is using - so
			 * we have to adjust it a little bit here */
			pt = Cvar_GetInteger("mn_morale") / 2;
		else
			pt = Cvar_GetInteger("mn_hp");

		ekgValue = min(pt, ekgMaxValue);

		node->texl[1] = (ekgMaxIndex - (int) ekgValue / ekgDivide) * ekgHeight;
		node->texh[1] = node->texl[1] + ekgHeight;
		node->texl[0] = -(int) (0.01 * scrollSpeed * time) % ekgWidth;
		node->texh[0] = node->texl[0] + node->size[0];
		if (node->size[0] && !node->size[1]) {
			float scale;

			scale = image->width / node->size[0];
			Vector2Set(size, node->size[0], image->height / scale);
		} else if (node->size[1] && !node->size[0]) {
			float scale;

			scale = image->height / node->size[1];
			Vector2Set(size, image->width / scale, node->size[1]);
		} else {
			if (node->preventRatio) {
				/* maximize the image into the bounding box */
				float ratio;
				ratio = (float) image->width / (float) image->height;
				if (node->size[1] * ratio > node->size[0]) {
					Vector2Set(size, node->size[0], node->size[0] / ratio);
				} else {
					Vector2Set(size, node->size[1] * ratio, node->size[1]);
				}
			} else {
				Vector2Copy(node->size, size);
			}
		}
		MN_DrawNormImage(nodepos[0], nodepos[1], size[0], size[1],
			node->texh[0], node->texh[1], node->texl[0], node->texl[1], ALIGN_UL, image);
	}
}

void MN_RegisterEKGNode (nodeBehaviour_t* behaviour)
{
	behaviour->name = "ekg";
	behaviour->extends = "pic";
	behaviour->draw = MN_EKGNodeDraw;
}
