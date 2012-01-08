/**
 * @file ui_node_image.c
 * @brief The <code>pic</code> behaviour allow to draw an image or a part of an image
 * into the GUI. It provide some layout properties. We can use it like an active node
 * (mouse in/out/click...) but in this case, it is better to use nodes with a semantics (like button,
 * or checkbox).
 * @code
 * image aircraft_return
 * {
 *	src	ui/buttons_small
 *	pos	"550 410"
 *	texl	"0 32"
 *	texh	"16 48"
 *	[..]
 * }
 * @endcode
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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_render.h"
#include "ui_node_image.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"

#define EXTRADATA_TYPE imageExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

/**
 * @brief Handled after the end of the load of the node from the script (all data and/or child are set)
 */
static void UI_ImageNodeLoaded (uiNode_t *node)
{
	/* update the size when its possible */
	if (Vector2Empty(node->size)) {
		if (EXTRADATA(node).texl[0] != 0 || EXTRADATA(node).texh[0]) {
			node->size[0] = EXTRADATA(node).texh[0] - EXTRADATA(node).texl[0];
			node->size[1] = EXTRADATA(node).texh[1] - EXTRADATA(node).texl[1];
		} else if (node->image) {
			const image_t *image = UI_LoadImage(node->image);
			if (image) {
				node->size[0] = image->width;
				node->size[1] = image->height;
			}
		}
	}
#ifdef DEBUG
	if (Vector2Empty(node->size)) {
		if (node->onClick || node->onRightClick || node->onMouseEnter || node->onMouseLeave || node->onWheelUp || node->onWheelDown || node->onWheel || node->onMiddleClick) {
			Com_DPrintf(DEBUG_CLIENT, "Node '%s' is an active image without size\n", UI_GetPath(node));
		}
	}
#endif
}

/**
 * @brief Get position of a inner box inside an outer box according to align param.
 * @todo Generic function, move it outside.
 * @param[in] outerBoxPos Position of the top-left corner of the outer box.
 * @param[in] outerBoxSize Size of the outer box.
 * @param[in] innerBoxSize Size of the inner box.
 * @param align Alignment of the inner box inside the outer box.
 * @param[out] innerBoxPos Position of the top-left corner of the inner box.
 */
static void UI_ImageAlignBoxInBox (vec2_t outerBoxPos, vec2_t outerBoxSize, vec2_t innerBoxSize, align_t align, vec2_t innerBoxPos)
{
	switch (align % 3) {
	case 0:	/* left */
		innerBoxPos[0] = outerBoxPos[0];
		break;
	case 1:	/* middle */
		innerBoxPos[0] = outerBoxPos[0] + (outerBoxSize[0] * 0.5) - (innerBoxSize[0] * 0.5);
		break;
	case 2:	/* right */
		innerBoxPos[0] = outerBoxPos[0] + outerBoxSize[0] - innerBoxSize[0];
		break;
	}
	switch (align / 3) {
	case 0:	/* top */
		innerBoxPos[1] = outerBoxPos[1];
		break;
	case 1:	/* middle */
		innerBoxPos[1] = outerBoxPos[1] + (outerBoxSize[1] * 0.5) - (innerBoxSize[1] * 0.5);
		break;
	case 2:	/* bottom */
		innerBoxPos[1] = outerBoxPos[1] + outerBoxSize[1] - innerBoxSize[1];
		break;
	default:
		innerBoxPos[1] = outerBoxPos[1];
		Com_Error(ERR_FATAL, "UI_ImageAlignBoxInBox: Align %d not supported\n", align);
	}
}

/**
 * @brief Draws the image node
 * @param[in] node The UI node to draw
 */
void UI_ImageNodeDraw (uiNode_t *node)
{
	vec2_t size;
	vec2_t nodepos;
	const image_t *image;
	vec2_t imagepos;
	vec2_t nodesize;

	const char* imageName = UI_GetReferenceString(node, node->image);
	if (Q_strnull(imageName))
		return;

	image = UI_LoadImage(imageName);
	if (!image)
		return;

	/* mouse darken effect */
	/** @todo convert all pic using mousefx into button.
	 * @todo delete mousefx
	 */
#if 0
	if (node->mousefx && node->state) {
		vec4_t color;
		VectorScale(node->color, 0.8, color);
		color[3] = node->color[3];
		R_Color(color);
	}
#endif

	UI_GetNodeAbsPos(node, nodepos);
	Vector2Copy(node->size, nodesize);
	nodesize[0] -= node->padding + node->padding;
	if (nodesize[0] < 0)
		nodesize[0] = 0;
	nodesize[1] -= node->padding + node->padding;
	if (nodesize[1] < 0)
		nodesize[1] = 0;

	/** @todo code is duplicated in the ekg node code */
	if (node->size[0] && !node->size[1]) {
		const float scale = image->width / node->size[0];
		Vector2Set(size, node->size[0], image->height / scale);
	} else if (node->size[1] && !node->size[0]) {
		const float scale = image->height / node->size[1];
		Vector2Set(size, image->width / scale, node->size[1]);
	} else {
		Vector2Copy(nodesize, size);

		if (EXTRADATA(node).preventRatio) {
			/* maximize the image into the bounding box */
			const float ratio = (float) image->width / (float) image->height;
			if (size[1] * ratio > size[0]) {
				Vector2Set(size, size[0], size[0] / ratio);
			} else {
				Vector2Set(size, size[1] * ratio, size[1]);
			}
		}
	}

	UI_ImageAlignBoxInBox(nodepos, nodesize, size, node->contentAlign, imagepos);
	UI_DrawNormImage(qfalse, imagepos[0] + node->padding, imagepos[1] + node->padding, size[0], size[1],
			EXTRADATA(node).texh[0], EXTRADATA(node).texh[1],
			EXTRADATA(node).texl[0], EXTRADATA(node).texl[1], image);

	/** @todo convert all pic using mousefx into button.
	 * @todo delete mousefx
	 */
#if 0
	if (node->mousefx && node->state) {
		R_Color(NULL);
	}
#endif
}

void UI_RegisterImageNode (uiBehaviour_t* behaviour)
{
	/** @todo rename it according to the function name when its possible */
	behaviour->name = "image";
	behaviour->draw = UI_ImageNodeDraw;
	behaviour->loaded = UI_ImageNodeLoaded;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Do not change the image ratio. The image will be proportionally stretched. */
	UI_RegisterExtradataNodeProperty(behaviour, "preventratio", V_BOOL, imageExtraData_t, preventRatio);
	/* Now this property do nothing. But we use it like a tag, to remember nodes we should convert into button...
	 * @todo delete it when its possible (use more button instead of image)
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "mousefx", V_BOOL, imageExtraData_t, mousefx);

	/* Texture high. Optional. Define the higher corner of the texture we want to display. Used with texl to crop the image. */
	UI_RegisterExtradataNodeProperty(behaviour, "texh", V_POS, imageExtraData_t, texh);
	/* Texture low. Optional. Define the lower corner of the texture we want to display. Used with texh to crop the image. */
	UI_RegisterExtradataNodeProperty(behaviour, "texl", V_POS, imageExtraData_t, texl);

	/* Source of the image */
	UI_RegisterNodeProperty(behaviour, "src", V_CVAR_OR_STRING, uiNode_t, image);
}
