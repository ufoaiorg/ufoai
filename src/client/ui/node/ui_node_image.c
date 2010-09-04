/**
 * @file ui_node_image.c
 * @brief The <code>pic</code> behaviour allow to draw an image or a part of an image
 * into the GUI. It provide some layout properties. We can use it like an active node
 * (mouse in/out/click...) but in this case, it is better to use nodes with a semantics (like button,
 * or checkbox).
 * @code
 * pic aircraft_return
 * {
 *	image	ui/buttons_small
 *	pos	"550 410"
 *	texl	"0 32"
 *	texh	"16 48"
 *	[..]
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

#include "../ui_nodes.h"
#include "../ui_parse.h"
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
 * @todo Center image, or use textalign property
 */
void UI_ImageNodeDraw (uiNode_t *node)
{
	vec2_t size;
	vec2_t nodepos;
	const image_t *image;

	const char* imageName = UI_GetReferenceString(node, node->image);
	if (!imageName || imageName[0] == '\0')
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

	/** @todo code is duplicated in the ekg node code */
	if (node->size[0] && !node->size[1]) {
		const float scale = image->width / node->size[0];
		Vector2Set(size, node->size[0], image->height / scale);
	} else if (node->size[1] && !node->size[0]) {
		const float scale = image->height / node->size[1];
		Vector2Set(size, image->width / scale, node->size[1]);
	} else {
		if (EXTRADATA(node).preventRatio) {
			/* maximize the image into the bounding box */
			const float ratio = (float) image->width / (float) image->height;
			if (node->size[1] * ratio > node->size[0]) {
				Vector2Set(size, node->size[0], node->size[0] / ratio);
			} else {
				Vector2Set(size, node->size[1] * ratio, node->size[1]);
			}
		} else {
			Vector2Copy(node->size, size);
		}
	}
	UI_DrawNormImage(nodepos[0], nodepos[1], size[0], size[1],
		EXTRADATA(node).texh[0], EXTRADATA(node).texh[1], EXTRADATA(node).texl[0], EXTRADATA(node).texl[1], image);

	/** @todo convert all pic using mousefx into button.
	 * @todo delete mousefx
	 */
#if 0
	if (node->mousefx && node->state) {
		R_Color(NULL);
	}
#endif
}

static const value_t properties[] = {
	/* Do not change the image ratio. The image will be proportionally stretched. */
	{"preventratio", V_BOOL, UI_EXTRADATA_OFFSETOF(imageExtraData_t, preventRatio), MEMBER_SIZEOF(imageExtraData_t, preventRatio)},
	/* Now this property do nothing. But we use it like a tag, to remember nodes we should convert into button...
	 * @todo delete it when its possible (use more button instead of image)
	 */
	{"mousefx", V_BOOL, UI_EXTRADATA_OFFSETOF(imageExtraData_t, mousefx), MEMBER_SIZEOF(imageExtraData_t, mousefx)},

	/* Texture high. Optional. Define the higher corner of the texture we want to display. Used with texl to crop the image. */
	{"texh", V_POS, UI_EXTRADATA_OFFSETOF(imageExtraData_t, texh), MEMBER_SIZEOF(imageExtraData_t, texh)},
	/* Texture low. Optional. Define the lower corner of the texture we want to display. Used with texh to crop the image. */
	{"texl", V_POS, UI_EXTRADATA_OFFSETOF(imageExtraData_t, texl), MEMBER_SIZEOF(imageExtraData_t, texl)},

	/* Source of the image */
	{"src", V_CVAR_OR_STRING, offsetof(uiNode_t, image), 0},

	{NULL, V_NULL, 0, 0}
};

void UI_RegisterImageNode (uiBehaviour_t* behaviour)
{
	/** @todo rename it according to the function name when its possible */
	behaviour->name = "image";
	behaviour->draw = UI_ImageNodeDraw;
	behaviour->loaded = UI_ImageNodeLoaded;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
