/**
 * @file ui_node_model.c
 * @brief This node allow to include a 3D-model into the GUI.
 * It provide a way to create composite models, check
 * [[How to script UI#How to create a composite model]]. We call it "main model"
 * when a model is a child node of a non model node, and "submodel" when the node
 * is a child node of a model node.
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

#include "../ui_main.h"
#include "../ui_internal.h"
#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_input.h"
#include "../ui_internal.h"
#include "ui_node_model.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_mesh.h"
#include "../../renderer/r_mesh_anim.h"
#include "../../renderer/r_model.h"

#define EXTRADATA_TYPE modelExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

#define ROTATE_SPEED	0.5
#define MAX_OLDREFVALUE MAX_VAR

static const uiBehaviour_t const *localBehaviour;

/**
 * @brief Returns pointer to UI model
 * @param[in] modelName model id from script files
 * @return uiModel_t pointer
 */
uiModel_t *UI_GetUIModel (const char *modelName)
{
	int i;

	for (i = 0; i < ui_global.numModels; i++) {
		uiModel_t *m = &ui_global.models[i];
		if (Q_streq(m->id, modelName))
			return m;
	}
	return NULL;
}

static void UI_ListUIModels_f (void)
{
	int i;

	/* search for UI models with same name */
	Com_Printf("UI models: %i\n", ui_global.numModels);
	for (i = 0; i < ui_global.numModels; i++) {
		const uiModel_t *m = &ui_global.models[i];
		const char *need = m->next != NULL ? m->next->id : "none";
		Com_Printf("id: %s\n...model: %s\n...need: %s\n\n", m->id, m->model, need);
	}
}

static void UI_ModelNodeDraw (uiNode_t *node)
{
	const char* ref = UI_GetReferenceString(node, EXTRADATA(node).model);
	char source[MAX_VAR];
	if (Q_strnull(ref))
		source[0] = '\0';
	else
		Q_strncpyz(source, ref, sizeof(source));
	UI_DrawModelNode(node, source);
}

static vec3_t nullVector = {0, 0, 0};

/**
 * @brief Set the Model info view (angle,origin,scale) according to the node definition
 * @todo the param "model" is not used !?
 */
static inline void UI_InitModelInfoView (uiNode_t *node, modelInfo_t *mi, uiModel_t *model)
{
	vec3_t nodeorigin;
	UI_GetNodeAbsPos(node, nodeorigin);
	nodeorigin[0] += node->size[0] / 2 + EXTRADATA(node).origin[0];
	nodeorigin[1] += node->size[1] / 2 + EXTRADATA(node).origin[1];
	nodeorigin[2] = EXTRADATA(node).origin[2];

	VectorCopy(EXTRADATA(node).scale, mi->scale);
	VectorCopy(EXTRADATA(node).angles, mi->angles);
	VectorCopy(nodeorigin, mi->origin);

	VectorCopy(nullVector, mi->center);
}

/**
 * @brief Draw a model using UI model definition
 */
static void UI_DrawModelNodeWithUIModel (uiNode_t *node, const char *source, modelInfo_t *mi, uiModel_t *model)
{
	qboolean autoScaleComputed = qfalse;
	vec3_t autoScale;
	vec3_t autoCenter;

	while (model) {
		/* no animation */
		mi->frame = 0;
		mi->oldframe = 0;
		mi->backlerp = 0;

		assert(model->model);
		mi->model = R_FindModel(model->model);
		if (!mi->model) {
			model = model->next;
			continue;
		}

		mi->skin = model->skin;
		mi->name = model->model;

		/* set mi pointers to model */
		mi->origin = model->origin;
		mi->angles = model->angles;
		mi->center = model->center;
		mi->color = model->color;
		mi->scale = model->scale;

		if (model->tag && model->parent) {
			/* tag and parent defined */
			uiModel_t *parentModel;
			modelInfo_t pmi;
			vec3_t pmiorigin;
			animState_t *as;
			/* place this model part on an already existing model tag */
			parentModel = UI_GetUIModel(model->parent);
			if (!parentModel) {
				Com_Printf("UI Model: Could not get the model '%s'\n", model->parent);
				break;
			}
			pmi.model = R_FindModel(parentModel->model);
			if (!pmi.model) {
				Com_Printf("UI Model: Could not get the model '%s'\n", parentModel->model);
				break;
			}

			pmi.name = parentModel->model;

			pmi.origin = pmiorigin;
			pmi.angles = parentModel->angles;
			pmi.scale = parentModel->scale;
			pmi.center = parentModel->center;
			pmi.color = parentModel->color;

			pmi.origin[0] = parentModel->origin[0] + mi->origin[0];
			pmi.origin[1] = parentModel->origin[1] + mi->origin[1];
			pmi.origin[2] = parentModel->origin[2];
			/* don't count window offset twice for tagged models */
			mi->origin[0] -= node->root->pos[0];
			mi->origin[1] -= node->root->pos[1];

			/* autoscale? */
			if (EXTRADATA(node).autoscale) {
				if (!autoScaleComputed)
					Sys_Error("Wrong order of model nodes - the tag and parent model node must be after the base model node");
				pmi.scale = autoScale;
				pmi.center = autoCenter;
			}

			as = &parentModel->animState;
			if (!as)
				Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", pmi.name, parentModel->anim);
			pmi.frame = as->frame;
			pmi.oldframe = as->oldframe;
			pmi.backlerp = as->backlerp;

			R_DrawModelDirect(mi, &pmi, model->tag);
		} else {
			/* no tag and no parent means - base model or single model */
			const char *ref;
			UI_InitModelInfoView(node, mi, model);
			Vector4Copy(node->color, mi->color);

			/* compute the scale and center for the first model.
			 * it think its the bigger of composite models.
			 * All next elements use the same result
			 */
			if (EXTRADATA(node).autoscale) {
				if (!autoScaleComputed) {
					vec2_t size;
					size[0] = node->size[0] - node->padding;
					size[1] = node->size[1] - node->padding;
					R_ModelAutoScale(size, mi, autoScale, autoCenter);
					autoScaleComputed = qtrue;
				} else {
					mi->scale = autoScale;
					mi->center = autoCenter;
				}
			}

			/* get the animation given by node properties */
			if (EXTRADATA(node).animation && *EXTRADATA(node).animation) {
				ref = UI_GetReferenceString(node, EXTRADATA(node).animation);
			/* otherwise use the standard animation from UI model definition */
			} else
				ref = model->anim;

			/* only base models have animations */
			if (ref && *ref) {
				animState_t *as = &model->animState;
				const char *anim = R_AnimGetName(as, mi->model);
				/* initial animation or animation change */
				if (anim == NULL || !Q_streq(anim, ref))
					R_AnimChange(as, mi->model, ref);
				else
					R_AnimRun(as, mi->model, cls.frametime * 1000);

				mi->frame = as->frame;
				mi->oldframe = as->oldframe;
				mi->backlerp = as->backlerp;
			}
			R_DrawModelDirect(mi, NULL, NULL);
		}

		/* next */
		model = model->next;
	}
}

/**
 * @todo need to merge UI model case, and the common case (looks to be a copy-pasted code)
 */
void UI_DrawModelNode (uiNode_t *node, const char *source)
{
	modelInfo_t mi;
	uiModel_t *model;
	vec3_t nodeorigin;
	vec2_t screenPos;
	vec3_t autoScale;
	vec3_t autoCenter;

	assert(UI_NodeInstanceOf(node, "model"));			/**< We use model extradata */

	if (!source || source[0] == '\0')
		return;

	model = UI_GetUIModel(source);
	/* direct model name - no UI model definition */
	if (!model) {
		/* prevent the searching for a model def in the next frame */
		mi.model = R_FindModel(source);
		mi.name = source;
		if (!mi.model) {
			Com_Printf("Could not find model '%s'\n", source);
			return;
		}
	}

	/* compute the absolute origin ('origin' property is relative to the node center) */
	UI_GetNodeScreenPos(node, screenPos);
	UI_GetNodeAbsPos(node, nodeorigin);
	R_CleanupDepthBuffer(nodeorigin[0], nodeorigin[1], node->size[0], node->size[1]);
	if (EXTRADATA(node).clipOverflow) {
		R_PushClipRect(screenPos[0], screenPos[1], node->size[0], node->size[1]);
	}
	nodeorigin[0] += node->size[0] / 2 + EXTRADATA(node).origin[0];
	nodeorigin[1] += node->size[1] / 2 + EXTRADATA(node).origin[1];
	nodeorigin[2] = EXTRADATA(node).origin[2];

	VectorMA(EXTRADATA(node).angles, cls.frametime, EXTRADATA(node).omega, EXTRADATA(node).angles);
	mi.origin = nodeorigin;
	mi.angles = EXTRADATA(node).angles;
	mi.scale = EXTRADATA(node).scale;
	mi.center = nullVector;
	mi.color = node->color;
	mi.mesh = 0;

	/* special case to draw models with UI model */
	if (model) {
		UI_DrawModelNodeWithUIModel(node, source, &mi, model);
		if (EXTRADATA(node).clipOverflow)
			R_PopClipRect();
		return;
	}

	/* if the node is linked to a parent, the parent will display it */
	if (EXTRADATA(node).tag) {
		if (EXTRADATA(node).clipOverflow)
			R_PopClipRect();
		return;
	}

	/* autoscale? */
	if (EXTRADATA(node).autoscale) {
		const vec2_t size = {node->size[0] - node->padding, node->size[1] - node->padding};
		R_ModelAutoScale(size, &mi, autoScale, autoCenter);
	}

	/* no animation */
	mi.frame = 0;
	mi.oldframe = 0;
	mi.backlerp = 0;

	/* get skin */
	if (EXTRADATA(node).skin && *EXTRADATA(node).skin)
		mi.skin = atoi(UI_GetReferenceString(node, EXTRADATA(node).skin));
	else
		mi.skin = 0;

	/* do animations */
	if (EXTRADATA(node).animation && *EXTRADATA(node).animation) {
		animState_t *as;
		const char *ref;
		ref = UI_GetReferenceString(node, EXTRADATA(node).animation);

		/* check whether the cvar value changed */
		if (strncmp(EXTRADATA(node).oldRefValue, source, MAX_OLDREFVALUE)) {
			Q_strncpyz(EXTRADATA(node).oldRefValue, source, MAX_OLDREFVALUE);
			/* model has changed but mem is already reserved in pool */
			if (EXTRADATA(node).animationState) {
				Mem_Free(EXTRADATA(node).animationState);
				EXTRADATA(node).animationState = NULL;
			}
		}
		if (!EXTRADATA(node).animationState) {
			as = (animState_t *) Mem_PoolAlloc(sizeof(*as), cl_genericPool, 0);
			if (!as)
				Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", mi.name, ref);
			R_AnimChange(as, mi.model, ref);
			EXTRADATA(node).animationState = as;
		} else {
			const char *anim;
			/* change anim if needed */
			as = EXTRADATA(node).animationState;
			if (!as)
				Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", mi.name, ref);
			anim = R_AnimGetName(as, mi.model);
			if (anim && !Q_streq(anim, ref))
				R_AnimChange(as, mi.model, ref);
			R_AnimRun(as, mi.model, cls.frametime * 1000);
		}

		mi.frame = as->frame;
		mi.oldframe = as->oldframe;
		mi.backlerp = as->backlerp;
	}

	/* draw the main model on the node */
	R_DrawModelDirect(&mi, NULL, NULL);

	/* draw all children */
	if (node->firstChild) {
		uiNode_t *child;
		modelInfo_t pmi = mi;
		for (child = node->firstChild; child; child = child->next) {
			const char *tag;
			char childSource[MAX_VAR];
			const char* childRef;

			/* skip non "model" nodes */
			if (child->behaviour != node->behaviour)
				continue;

			/* skip invisible child */
			if (child->invis || !UI_CheckVisibility(child))
				continue;

			OBJZERO(mi);
			mi.angles = EXTRADATA(child).angles;
			mi.scale = EXTRADATA(child).scale;
			mi.center = nullVector;
			mi.origin = EXTRADATA(child).origin;
			mi.color = pmi.color;

			/* get the anchor name to link the model into the parent */
			tag = EXTRADATA(child).tag;

			/* init model name */
			childRef = UI_GetReferenceString(child, EXTRADATA(child).model);
			if (Q_strnull(childRef))
				childSource[0] = '\0';
			else
				Q_strncpyz(childSource, childRef, sizeof(childSource));
			mi.model = R_FindModel(childSource);
			mi.name = childSource;

			/* init skin */
			if (EXTRADATA(child).skin && *EXTRADATA(child).skin)
				mi.skin = atoi(UI_GetReferenceString(child, EXTRADATA(child).skin));
			else
				mi.skin = 0;

			R_DrawModelDirect(&mi, &pmi, tag);
		}
	}

	if (EXTRADATA(node).clipOverflow)
		R_PopClipRect();
}

static int oldMousePosX = 0;
static int oldMousePosY = 0;

static void UI_ModelNodeCapturedMouseMove (uiNode_t *node, int x, int y)
{
	float *rotateAngles = EXTRADATA(node).angles;

	/* rotate a model */
	rotateAngles[YAW] -= ROTATE_SPEED * (x - oldMousePosX);
	rotateAngles[ROLL] += ROTATE_SPEED * (y - oldMousePosY);

	/* clamp the angles */
	while (rotateAngles[YAW] > 360.0)
		rotateAngles[YAW] -= 360.0;
	while (rotateAngles[YAW] < 0.0)
		rotateAngles[YAW] += 360.0;

	if (rotateAngles[ROLL] < 0.0)
		rotateAngles[ROLL] = 0.0;
	else if (rotateAngles[ROLL] > 180.0)
		rotateAngles[ROLL] = 180.0;

	oldMousePosX = x;
	oldMousePosY = y;
}

static void UI_ModelNodeMouseDown (uiNode_t *node, int x, int y, int button)
{
	if (button != K_MOUSE1)
		return;
	if (!EXTRADATA(node).rotateWithMouse)
		return;
	UI_SetMouseCapture(node);
	oldMousePosX = x;
	oldMousePosY = y;
}

static void UI_ModelNodeMouseUp (uiNode_t *node, int x, int y, int button)
{
	if (button != K_MOUSE1)
		return;
	if (UI_GetMouseCapture() != node)
		return;
	UI_MouseRelease();
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void UI_ModelNodeLoading (uiNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
	VectorSet(EXTRADATA(node).scale, 1, 1, 1);
	EXTRADATA(node).clipOverflow = qtrue;
}

/**
 * @brief Call to update a cloned node
 */
static void UI_ModelNodeClone (const uiNode_t *source, uiNode_t *clone)
{
	localBehaviour->super->clone(source, clone);
	if (!clone->dynamic)
		EXTRADATA(clone).oldRefValue = UI_AllocStaticString("", MAX_OLDREFVALUE);
}

static void UI_ModelNodeNew (uiNode_t *node)
{
	EXTRADATA(node).oldRefValue = (char*) Mem_PoolAlloc(MAX_OLDREFVALUE, ui_dynPool, 0);
	EXTRADATA(node).oldRefValue[0] = '\0';
}

static void UI_ModelNodeDelete (uiNode_t *node)
{
	Mem_Free(EXTRADATA(node).oldRefValue);
	EXTRADATA(node).oldRefValue = NULL;
}

static void UI_ModelNodeLoaded (uiNode_t *node)
{
	/* a tag without but not a submodel */
	if (EXTRADATA(node).tag != NULL && node->behaviour != node->parent->behaviour) {
		Com_Printf("UI_ModelNodeLoaded: '%s' use a tag but is not a submodel. Tag removed.\n", UI_GetPath(node));
		EXTRADATA(node).tag = NULL;
	}

	if (EXTRADATA(node).oldRefValue == NULL)
		EXTRADATA(node).oldRefValue = UI_AllocStaticString("", MAX_OLDREFVALUE);

	/* no tag but no size */
	if (EXTRADATA(node).tag == NULL && (node->size[0] == 0 || node->size[1] == 0)) {
		Com_Printf("UI_ModelNodeLoaded: Please set a pos and size to the node '%s'. Note: 'origin' is a relative value to the center of the node\n", UI_GetPath(node));
	}
}

void UI_RegisterModelNode (uiBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "model";
	behaviour->drawItselfChild = qtrue;
	behaviour->draw = UI_ModelNodeDraw;
	behaviour->mouseDown = UI_ModelNodeMouseDown;
	behaviour->mouseUp = UI_ModelNodeMouseUp;
	behaviour->loading = UI_ModelNodeLoading;
	behaviour->loaded = UI_ModelNodeLoaded;
	behaviour->clone = UI_ModelNodeClone;
	behaviour->newNode = UI_ModelNodeNew;
	behaviour->deleteNode = UI_ModelNodeDelete;
	behaviour->capturedMouseMove = UI_ModelNodeCapturedMouseMove;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Both. Name of the animation for the model */
	UI_RegisterExtradataNodeProperty(behaviour, "anim", V_CVAR_OR_STRING, modelExtraData_t, animation);
	/* Main model only. Point of view. */
	UI_RegisterExtradataNodeProperty(behaviour, "angles", V_VECTOR, modelExtraData_t, angles);
	/* Main model only. Position of the model relative to the center of the node. */
	UI_RegisterExtradataNodeProperty(behaviour, "origin", V_VECTOR, modelExtraData_t, origin);
	/* Main model only. Rotation vector of the model. */
	UI_RegisterExtradataNodeProperty(behaviour, "omega", V_VECTOR, modelExtraData_t, omega);
	/* Both. Scale the model */
	UI_RegisterExtradataNodeProperty(behaviour, "scale", V_VECTOR, modelExtraData_t, scale);
	/* Submodel only. A tag name to link the model to the parent model. */
	UI_RegisterExtradataNodeProperty(behaviour, "tag", V_CVAR_OR_STRING, modelExtraData_t, tag);
	/* Main model only. Auto compute the "better" scale for the model. The function dont work
	 * very well at the moment because it dont check the angle and no more submodel bounding box.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "autoscale", V_BOOL, modelExtraData_t, autoscale);
	/* Main model only. Allow to change the POV of the model with the mouse (only for main model) */
	UI_RegisterExtradataNodeProperty(behaviour, "rotatewithmouse", V_BOOL, modelExtraData_t, rotateWithMouse);
	/* Main model only. Clip the model with the node rect */
	UI_RegisterExtradataNodeProperty(behaviour, "clipoverflow", V_BOOL, modelExtraData_t, clipOverflow);
	/* Source of the model. The path to the model, relative to <code>base/models</code> */
	UI_RegisterExtradataNodeProperty(behaviour, "src", V_CVAR_OR_STRING, modelExtraData_t, model);
	/* Both. Name of the skin for the model. */
	UI_RegisterExtradataNodeProperty(behaviour, "skin", V_CVAR_OR_STRING, modelExtraData_t, skin);

	Cmd_AddCommand("uimodelslist", UI_ListUIModels_f, NULL);
}
