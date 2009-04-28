/**
 * @file m_node_material_editor.c
 * @brief Material editor related code
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../../client.h"
#include "../m_menus.h"
#include "../m_nodes.h"
#include "../m_render.h"
#include "m_node_abstractnode.h"
#include "../../cl_video.h"
#include "../../renderer/r_image.h"
#include "../../renderer/r_model.h"
#include "m_node_material_editor.h"

#define IMAGE_WIDTH 64
/**
 * @todo Scrolling support for images
 * @todo Replace magic number 64 by some script definition
 * @todo Clicking one of the images will open a floating dialog where you can change some material values
 * @param node The node to draw
 */
static void MN_MaterialEditorNodeDraw (menuNode_t *node)
{
	int i;
	vec2_t nodepos;
	int cnt = 0;

	MN_GetNodeAbsPos(node, nodepos);

	R_Color(node->color);

	for (i = 0; i < r_numImages; i++) {
		image_t *image = &r_images[i];
		if (image->type == it_world) {
			if (node->u.abstractscrollbar.pos <= cnt) {
				const int x = nodepos[0] + cnt * (IMAGE_WIDTH + node->padding);
				if (x < -IMAGE_WIDTH)
					continue;
				if (x > viddef.virtualWidth)
					break;
				MN_DrawNormImage(x, nodepos[1] + node->padding, IMAGE_WIDTH, IMAGE_WIDTH,
						node->texh[0], node->texh[1], node->texl[0], node->texl[1], ALIGN_UL, image);
			}
			cnt++;
		}
	}

	R_Color(NULL);
}

static image_t *MN_MaterialEditorNodeGetImageAtPosition (menuNode_t *node, int x, int y)
{
	int i;
	vec2_t nodepos;
	int cnt = 0;

	MN_GetNodeAbsPos(node, nodepos);

	for (i = 0; i < r_numImages; i++) {
		image_t *image = &r_images[i];
		if (image->type == it_world) {
			const int imageX = nodepos[0] + cnt * (IMAGE_WIDTH + node->padding);
			if (x >= imageX && x <= imageX + IMAGE_WIDTH)
				return image;
			cnt++;
		}
	}
	return NULL;
}

static void MN_EditorNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	image_t *image;
	if (button != K_MOUSE1)
		return;

	image = MN_MaterialEditorNodeGetImageAtPosition(node, mousePosX, mousePosY);
	if (image) {
		Com_Printf("image: '%s'\n", image->name);
		image->material.flags = STAGE_RENDER;
		image->material.bump = 3.0;
		image->material.hardness = 3.0;
		image->material.parallax = 3.0;
		image->material.specular = 3.0;
		R_ModReloadSurfacesArrays();
	}
}

static void MN_MaterialEditorStart_f (void)
{
	/* material editor only makes sense in battlescape mode */
	if (cls.state != ca_active) {
		MN_PopMenu(qfalse);
		return;
	}
}

void MN_RegisterMaterialEditorNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "material_editor";
	behaviour->draw = MN_MaterialEditorNodeDraw;
	behaviour->mouseDown = MN_EditorNodeMouseDown;

	Cmd_AddCommand("mn_materialeditor", MN_MaterialEditorStart_f, "Initializes the material editor menu");
}
