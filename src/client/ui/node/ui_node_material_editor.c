/**
 * @file ui_node_material_editor.c
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
#include "../ui_main.h"
#include "../ui_data.h"
#include "../ui_windows.h"
#include "../ui_nodes.h"
#include "../ui_behaviour.h"
#include "../ui_actions.h"
#include "../ui_render.h"
#include "../ui_parse.h"
#include "ui_node_abstractnode.h"
#include "ui_node_abstractscrollable.h"
#include "../../cl_video.h"
#include "../../renderer/r_image.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_model.h"
#include "ui_node_material_editor.h"

#define EXTRADATA(node) UI_EXTRADATA(node, abstractScrollableExtraData_t)

/*#define ANYIMAGES*/
/** @todo Replace magic number 64 by some script definition */
#define IMAGE_WIDTH 64

typedef struct materialDescription_s {
	const char *name;
	const int stageFlag;
} materialDescription_t;

static const materialDescription_t materialDescriptions[] = {
	{"texture", STAGE_TEXTURE},
	{"envmap", STAGE_ENVMAP},
	{"blend", STAGE_BLEND},
	{"color", STAGE_COLOR},
	{"pulse", STAGE_PULSE},
	{"stretch", STAGE_STRETCH},
	{"rotate", STAGE_ROTATE},
	{"scroll.s", STAGE_SCROLL_S},
	{"scroll.t", STAGE_SCROLL_T},
	{"scale.s", STAGE_SCALE_S},
	{"scale.t", STAGE_SCALE_T},
	{"terrain", STAGE_TERRAIN},
	{"tape", STAGE_TAPE},
	{"lightmap", STAGE_LIGHTMAP},
	{"anim", STAGE_ANIM},
	{"dirtmap", STAGE_DIRTMAP},

	{NULL, 0}
};

static materialStage_t *UI_MaterialEditorGetStage (material_t *material, int stageIndex)
{
	materialStage_t *materialStage = material->stages;
	while (stageIndex-- > 0) {
		if (materialStage)
			materialStage = materialStage->next;
		else
			break;
	}
	return materialStage;
}

/**
 * @brief return the number of images we can display
 */
static int UI_MaterialEditorNodeGetImageCount (uiNode_t *node)
{
	int i;
	int cnt = 0;

	for (i = 0; i < r_numImages; i++) {
#ifndef ANYIMAGES
		const image_t *image = R_GetImageAtIndex(i);
		/* filter */
		if (image->type != it_world)
			continue;

		if (strstr(image->name, "tex_common"))
			continue;
#endif
		cnt++;
	}
	return cnt;
}

/**
 * @brief Update the scrollable view
 */
static void UI_MaterialEditorNodeUpdateView (uiNode_t *node, qboolean reset)
{
	const int imageCount = UI_MaterialEditorNodeGetImageCount(node);
	const int imagesPerLine = (node->size[0] - node->padding) / (IMAGE_WIDTH + node->padding);
	const int imagesPerColumn = (node->size[1] - node->padding) / (IMAGE_WIDTH + node->padding);

	/* update view */
	if (imagesPerLine > 0 && imagesPerColumn > 0) {
		const int pos = reset ? 0 : -1;
		UI_AbstractScrollableNodeSetY(node, pos, imagesPerColumn, imageCount / imagesPerLine);
	} else
		UI_AbstractScrollableNodeSetY(node, 0, 0, 0);
}

/**
 * @param node The node to draw
 */
static void UI_MaterialEditorNodeDraw (uiNode_t *node)
{
	int i;
	vec2_t pos;
	int cnt = 0;
	int cntView = 0;
	const int imagesPerLine = (node->size[0] - node->padding) / (IMAGE_WIDTH + node->padding);

	if (UI_AbstractScrollableNodeIsSizeChange(node))
		UI_MaterialEditorNodeUpdateView(node, qfalse);

	/* width too small to display anything */
	if (imagesPerLine <= 0)
		return;

	UI_GetNodeAbsPos(node, pos);

	/* display images */
	for (i = 0; i < r_numImages; i++) {
		image_t *image = R_GetImageAtIndex(i);
		vec2_t imagepos;

#ifndef ANYIMAGES
		/* filter */
		if (image->type != it_world)
			continue;

		if (strstr(image->name, "tex_common"))
			continue;
#endif

		/* skip images before the scroll position */
		if (cnt / imagesPerLine < EXTRADATA(node).scrollY.viewPos) {
			cnt++;
			continue;
		}

		/** @todo do it incremental. Don't need all this math */
		imagepos[0] = pos[0] + node->padding + (cntView % imagesPerLine) * (IMAGE_WIDTH + node->padding);
		imagepos[1] = pos[1] + node->padding + (cntView / imagesPerLine) * (IMAGE_WIDTH + node->padding);

		/* vertical overflow */
		if (imagepos[1] + IMAGE_WIDTH + node->padding >= pos[1] + node->size[1])
			break;

		if (i == node->num) {
#define MARGIN 3
			R_DrawRect(imagepos[0] - MARGIN, imagepos[1] - MARGIN, IMAGE_WIDTH + MARGIN * 2, IMAGE_WIDTH + MARGIN * 2, node->selectedColor, 2, 0xFFFF);
#undef MARGIN
		}

		UI_DrawNormImage(qfalse, imagepos[0], imagepos[1], IMAGE_WIDTH, IMAGE_WIDTH, 0, 0, 0, 0, image);

		cnt++;
		cntView++;
	}
}

/**
 * @brief Return index of the image (r_images[i]) else NULL
 */
static int UI_MaterialEditorNodeGetImageAtPosition (uiNode_t *node, int x, int y)
{
	int i;
	vec2_t pos;
	int cnt = 0;
	int cntView = 0;
	const int imagesPerLine = (node->size[0] - node->padding) / (IMAGE_WIDTH + node->padding);
	const int imagesPerColumn = (node->size[1] - node->padding) / (IMAGE_WIDTH + node->padding);
	int columnRequested;
	int lineRequested;

	UI_NodeAbsoluteToRelativePos(node, &x, &y);

	/* have we click between 2 images? */
	if (((x % (IMAGE_WIDTH + node->padding)) < node->padding)
		|| ((y % (IMAGE_WIDTH + node->padding)) < node->padding))
		return -1;

	/* get column and line of the image */
	columnRequested = x / (IMAGE_WIDTH + node->padding);
	lineRequested = y / (IMAGE_WIDTH + node->padding);

	/* have we click outside? */
	if (columnRequested >= imagesPerLine || lineRequested >= imagesPerColumn)
		return -1;

	UI_GetNodeAbsPos(node, pos);

	/* check images */
	for (i = 0; i < r_numImages; i++) {
#ifndef ANYIMAGES
		/* filter */
		image_t *image = R_GetImageAtIndex(i);
		if (image->type != it_world)
			continue;

		if (strstr(image->name, "tex_common"))
			continue;
#endif

		/* skip images before the scroll position */
		if (cnt / imagesPerLine < EXTRADATA(node).scrollY.viewPos) {
			cnt++;
			continue;
		}

		if (cntView % imagesPerLine == columnRequested && cntView / imagesPerLine == lineRequested)
			return i;

		/* vertical overflow */
		if (cntView / imagesPerLine > lineRequested)
			break;

		cnt++;
		cntView++;
	}

	return -1;
}

static void UI_MaterialEditorStagesToName (const materialStage_t *stage, char *buf, size_t size)
{
	const materialDescription_t *md = materialDescriptions;

	while (md->name) {
		if (stage->flags & md->stageFlag)
			Q_strcat(buf, va("%s ", md->name), size);
		md++;
	}
}

/**
 * Updates the material editor node for a given image and a given material stage
 * @param image The image to load into the material editor
 * @param materialStage The material stage to display
 */
static void UI_MaterialEditorUpdate (image_t *image, materialStage_t *materialStage)
{
	linkedList_t *materialStagesList = NULL;

	if (image->normalmap == NULL)
		UI_ExecuteConfunc("hideshaders true 0 0 0 0");
	else
		UI_ExecuteConfunc("hideshaders false %f %f %f %f", image->material.bump,
				image->material.hardness, image->material.parallax, image->material.specular);

	if (image->normalmap == NULL)
		Cvar_Set("me_imagename", image->name);
	else
		Cvar_Set("me_imagename", va("%s (nm)", image->name));

	if (!image->material.num_stages) {
		UI_ExecuteConfunc("hidestages true");
	} else {
		int i;
		if (materialStage) {
			const char *stageType = Cvar_GetString("me_stagetype");
			if (stageType[0] == '\0')
				stageType = "stretch";
			UI_ExecuteConfunc("hidestages false %s", stageType);
		} else
			Cvar_Set("me_stage_id", "-1");
		for (i = 0; i < image->material.num_stages; i++) {
			const materialStage_t *stage = UI_MaterialEditorGetStage(&image->material, i);
			char stageName[MAX_VAR] = "stage ";
			if (stage == materialStage) {
				UI_ExecuteConfunc("updatestagevalues %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
						stage->rotate.hz, stage->rotate.deg,
						stage->stretch.hz, stage->stretch.dhz, stage->stretch.amp, stage->stretch.damp,
						stage->pulse.hz, stage->pulse.dhz,
						stage->scroll.ds, stage->scroll.dt, stage->scroll.s, stage->scroll.t,
						stage->scale.s, stage->scale.t);
			}
			UI_MaterialEditorStagesToName(stage, stageName, sizeof(stageName) - 1);
			LIST_AddString(&materialStagesList, stageName);
		}
	}
	UI_RegisterLinkedListText(TEXT_MATERIAL_STAGES, materialStagesList);
}

/**
 * Converts a stage name into the stage flag
 * @param stageName The name to search the flag for
 * @return -1 if no flag was not found for the given name
 */
static int UI_MaterialEditorNameToStage (const char *stageName)
{
	const materialDescription_t *md = materialDescriptions;

	while (md->name) {
		if (!strncmp(md->name, stageName, strlen(md->name)))
			return md->stageFlag;
		md++;
	}
	return -1;
}

static void UI_MaterialEditorMouseDown (uiNode_t *node, int x, int y, int button)
{
	int id;
	if (button != K_MOUSE1)
		return;

	id = UI_MaterialEditorNodeGetImageAtPosition(node, x, y);
	if (id == -1)
		return;

	/** @note here we use "num" to cache the selected image id. We can reuse it on the script with "<num>" */
	/* have we selected a new image? */
	if (node->num != id) {
		image_t *image = R_GetImageAtIndex(id);
		UI_MaterialEditorUpdate(image, NULL);

		node->num = id;

		if (node->onChange)
			UI_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief Called when we push a window with this node
 */
static void UI_MaterialEditorNodeInit (uiNode_t *node, linkedList_t *params)
{
	node->num = -1;
	UI_MaterialEditorNodeUpdateView(node, qtrue);
}

/**
 * @brief Called when the user wheel the mouse over the node
 */
static qboolean UI_MaterialEditorNodeWheel (uiNode_t *node, int deltaX, int deltaY)
{
	qboolean down = deltaY > 0;
	const int diff = (down) ? 1 : -1;
	if (deltaY == 0)
		return qfalse;
	return UI_AbstractScrollableNodeScrollY(node, diff);
}

static void UI_MaterialEditorStart_f (void)
{
	/* material editor only makes sense in battlescape mode */
#ifndef ANYIMAGES
	if (cls.state != ca_active) {
		Com_Printf("Material editor is only usable in battlescape mode\n");
		UI_PopWindow(qfalse);
		return;
	}
#endif
}

static const value_t materialValues[] = {
	{"bump", V_FLOAT, offsetof(material_t, bump), 0},
	{"parallax", V_FLOAT, offsetof(material_t, parallax), 0},
	{"specular", V_FLOAT, offsetof(material_t, specular), 0},
	{"hardness", V_FLOAT, offsetof(material_t, hardness), 0},

	{NULL, V_NULL, 0, 0},
};


static const value_t materialStageValues[] = {
	{"rotate.hz", V_FLOAT, offsetof(materialStage_t, rotate.deg), 0},
	{"rotate.deg", V_FLOAT, offsetof(materialStage_t, rotate.hz), 0},
	{"stretch.hz", V_FLOAT, offsetof(materialStage_t, stretch.hz), 0},
	{"stretch.dhz", V_FLOAT, offsetof(materialStage_t, stretch.dhz), 0},
	{"stretch.amp", V_FLOAT, offsetof(materialStage_t, stretch.amp), 0},
	{"stretch.damp", V_FLOAT, offsetof(materialStage_t, stretch.damp), 0},
	{"pulse.hz", V_FLOAT, offsetof(materialStage_t, pulse.hz), 0},
	{"pulse.dhz", V_FLOAT, offsetof(materialStage_t, pulse.dhz), 0},
	{"scroll.s", V_FLOAT, offsetof(materialStage_t, scroll.s), 0},
	{"scroll.t", V_FLOAT, offsetof(materialStage_t, scroll.t), 0},
	{"scroll.ds", V_FLOAT, offsetof(materialStage_t, scroll.ds), 0},
	{"scroll.dt", V_FLOAT, offsetof(materialStage_t, scroll.dt), 0},
	{"scale.s", V_FLOAT, offsetof(materialStage_t, scale.s), 0},
	{"scale.t", V_FLOAT, offsetof(materialStage_t, scale.t), 0},
	{"terrain.floor", V_FLOAT, offsetof(materialStage_t, terrain.floor), 0},
	{"terrain.ceil", V_FLOAT, offsetof(materialStage_t, terrain.ceil), 0},
	{"tape.floor", V_FLOAT, offsetof(materialStage_t, tape.floor), 0},
	{"tape.ceil", V_FLOAT, offsetof(materialStage_t, tape.ceil), 0},
	{"tape.center", V_FLOAT, offsetof(materialStage_t, tape.center), 0},
	{"anim.frames", V_INT, offsetof(materialStage_t, anim.num_frames), 0},
	{"anim.dframe", V_INT, offsetof(materialStage_t, anim.dframe), 0},
	{"anim.dtime", V_FLOAT, offsetof(materialStage_t, anim.dtime), 0},
	{"anim.fps", V_FLOAT, offsetof(materialStage_t, anim.fps), 0},
	{"dirt.intensity", V_FLOAT, offsetof(materialStage_t, dirt.intensity), 0},
	{"blend.src", V_INT, offsetof(materialStage_t, blend.src), 0},
	{"blend.dest", V_INT, offsetof(materialStage_t, blend.dest), 0},

	{NULL, V_NULL, 0, 0},
};

static void UI_MaterialEditorChangeValue_f (void)
{
	image_t *image;
	int id, stageType;
	const char *var, *value;
	size_t bytes;

	if (Cmd_Argc() < 5) {
		Com_Printf("Usage: %s <image index> <stage index> <variable> <value>\n", Cmd_Argv(0));
		return;
	}

	id = atoi(Cmd_Argv(1));
	if (id < 0 || id >= r_numImages) {
		Com_Printf("Given image index (%i) is out of bounds\n", id);
		return;
	}

	var = Cmd_Argv(3);
	value = Cmd_Argv(4);

	image = R_GetImageAtIndex(id);

	stageType = UI_MaterialEditorNameToStage(var);
	if (stageType == -1) {
		const value_t *val = UI_FindPropertyByName(materialValues, var);
		if (!val) {
			Com_Printf("Could not find material variable for '%s'\n", var);
			return;
		}
		Com_ParseValue(&image->material, value, val->type, val->ofs, val->size, &bytes);
	} else {
		materialStage_t *stage;
		int stageID;
		const value_t *val = UI_FindPropertyByName(materialStageValues, var);

		if (!val) {
			Com_Printf("Could not find material stage variable for '%s'\n", var);
			return;
		}

		stageID = atoi(Cmd_Argv(2));
		if (stageID < 0 || stageID >= image->material.num_stages) {
			Com_Printf("Given stage index (%i) is out of bounds\n", stageID);
			return;
		}

		stage = UI_MaterialEditorGetStage(&image->material, stageID);
		assert(stage);

		stage->flags |= stageType;

		Com_ParseValue(stage, value, val->type, val->ofs, val->size, &bytes);

		/* a texture or envmap means render it */
		if (stage->flags & (STAGE_TEXTURE | STAGE_ENVMAP))
			stage->flags |= STAGE_RENDER;

		if (stage->flags & (STAGE_TAPE | STAGE_TERRAIN | STAGE_DIRTMAP))
			stage->flags |= STAGE_LIGHTING;
	}

	R_ModReloadSurfacesArrays();
}

static void UI_MaterialEditorSelectStage_f (void)
{
	image_t *image;
	int id, stageID;
	materialStage_t *materialStage;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <image index> <stage index>\n", Cmd_Argv(0));
		return;
	}

	id = atoi(Cmd_Argv(1));
	if (id < 0 || id >= r_numImages) {
		Com_Printf("Given image index (%i) is out of bounds\n", id);
		return;
	}

	image = R_GetImageAtIndex(id);

	stageID = atoi(Cmd_Argv(2));
	if (stageID < 0 || stageID >= image->material.num_stages) {
		Com_Printf("Given stage index (%i) is out of bounds\n", stageID);
		return;
	}

	materialStage = UI_MaterialEditorGetStage(&image->material, stageID);
	UI_MaterialEditorUpdate(image, materialStage);
}

static void UI_MaterialEditorRemoveStage_f (void)
{
	image_t *image;
	int id, stageID;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <image index> <stage index>\n", Cmd_Argv(0));
		return;
	}

	id = atoi(Cmd_Argv(1));
	if (id < 0 || id >= r_numImages) {
		Com_Printf("Given image index (%i) is out of bounds\n", id);
		return;
	}

	image = R_GetImageAtIndex(id);

	stageID = atoi(Cmd_Argv(2));
	if (stageID < 0 || stageID >= image->material.num_stages) {
		Com_Printf("Given stage index (%i) is out of bounds\n", stageID);
		return;
	}

	if (stageID == 0) {
		materialStage_t *s = image->material.stages;
		image->material.stages = s->next;
		Mem_Free(s);
	} else {
		materialStage_t *sParent = UI_MaterialEditorGetStage(&image->material, stageID - 1);
		materialStage_t *s = sParent->next;
		sParent->next = s->next;
		Mem_Free(s);
	}

	image->material.num_stages--;

	UI_MaterialEditorUpdate(image, NULL);
}

static void UI_MaterialEditorNewStage_f (void)
{
	material_t *m;
	materialStage_t *s;
	int id;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <image index>\n", Cmd_Argv(0));
		return;
	}

	id = atoi(Cmd_Argv(1));
	if (id < 0 || id >= r_numImages) {
		Com_Printf("Given image index (%i) is out of bounds\n", id);
		return;
	}

	m = &R_GetImageAtIndex(id)->material;
	s = (materialStage_t *)Mem_PoolAlloc(sizeof(*s), vid_imagePool, 0);
	m->num_stages++;

	/* append the stage to the chain */
	if (!m->stages)
		m->stages = s;
	else {
		materialStage_t *ss = m->stages;
		while (ss->next)
			ss = ss->next;
		ss->next = s;
	}

	UI_MaterialEditorUpdate(R_GetImageAtIndex(id), s);
}

void UI_RegisterMaterialEditorNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "material_editor";
	behaviour->extends = "abstractscrollable";
	behaviour->draw = UI_MaterialEditorNodeDraw;
	behaviour->windowOpened = UI_MaterialEditorNodeInit;
	behaviour->mouseDown = UI_MaterialEditorMouseDown;
	behaviour->scroll = UI_MaterialEditorNodeWheel;

	/** @todo convert it to ui functions */
	Cmd_AddCommand("ui_materialeditor_removestage", UI_MaterialEditorRemoveStage_f, "Removes the selected material stage");
	Cmd_AddCommand("ui_materialeditor_newstage", UI_MaterialEditorNewStage_f, "Creates a new material stage for the current selected material");
	Cmd_AddCommand("ui_materialeditor_selectstage", UI_MaterialEditorSelectStage_f, "Select a given material stage");
	Cmd_AddCommand("ui_materialeditor_changevalue", UI_MaterialEditorChangeValue_f, "Initializes the material editor window");
	Cmd_AddCommand("ui_materialeditor", UI_MaterialEditorStart_f, "Initializes the material editor window");
}
