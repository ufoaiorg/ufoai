/**
 * @file m_node_model.c
 * @brief This node allow to include a 3D-model into the GUI.
 * It provide a way to create composite models, check
 * [[How to script menu#How to create a composite model]]. We call it "main model"
 * when a model is a child node of a non model node, and "submodel" when the node
 * is a child node of a model node.
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "../m_main.h"
#include "../m_internal.h"
#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_input.h"
#include "m_node_model.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_mesh.h"
#include "../../renderer/r_mesh_anim.h"

#define EXTRADATA(node) (node->u.model)

#define ROTATE_SPEED	0.5
#define MAX_OLDREFVALUE MAX_VAR

static const nodeBehaviour_t const *localBehaviour;

/**
 * @brief Returns pointer to menu model
 * @param[in] menuModel menu model id from script files
 * @return menuModel_t pointer
 */
menuModel_t *MN_GetMenuModel (const char *menuModel)
{
	int i;
	menuModel_t *m;

	for (i = 0; i < mn.numMenuModels; i++) {
		m = &mn.menuModels[i];
		if (!strncmp(m->id, menuModel, MAX_VAR))
			return m;
	}
	return NULL;
}

void MN_ListMenuModels_f (void)
{
	int i;

	/* search for menumodels with same name */
	Com_Printf("menu models: %i\n", mn.numMenuModels);
	for (i = 0; i < mn.numMenuModels; i++)
		Com_Printf("id: %s\n...model: %s\n...need: %s\n\n", mn.menuModels[i].id, mn.menuModels[i].model, mn.menuModels[i].need);
}


#ifdef DEBUG
/**
 * @brief This function allows you inline transforming of models.
 * @note Changes you made are lost on quit
 */
static void MN_SetModelTransform_f (void)
{
	menuNode_t *node;
	menuModel_t *model;
	const char *command, *nodeOrMenuID, *menuModel;
	float x, y ,z;
	vec3_t value;
	const nodeBehaviour_t *modelBehaviour = MN_GetNodeBehaviour("model");

	/* not initialized yet - commandline? */
	if (mn.windowStackPos <= 0)
		return;

	if (Cmd_Argc() < 5) {
		Com_Printf("Usage: %s [<model> <menu>] | [<node>] <x> <y> <z>\n", Cmd_Argv(0));
		Com_Printf("<model> <menu> is needed for menumodel definitions\n");
		Com_Printf("<node> is needed for 'normal' models\n");
		return;
	}

	if (Cmd_Argc() == 5) {
		command = Cmd_Argv(0);
		menuModel = NULL;
		nodeOrMenuID = Cmd_Argv(1);
		x = atof(Cmd_Argv(2));
		y = atof(Cmd_Argv(3));
		z = atof(Cmd_Argv(4));
	} else {
		command = Cmd_Argv(0);
		menuModel = Cmd_Argv(1);
		nodeOrMenuID = Cmd_Argv(2);
		x = atof(Cmd_Argv(3));
		y = atof(Cmd_Argv(4));
		z = atof(Cmd_Argv(5));
	}

	VectorSet(value, x, y, z);

	if (menuModel) {
		model = MN_GetMenuModel(menuModel);
		if (!model) {
			Com_Printf("MN_SetModelTransform_f: model \"%s\" wasn't found\n", menuModel);
			return;
		}

		if (model->menuTransformCnt) {
#if 0	/** @todo model is no more linked to menu; this code need an update */
			const menuNode_t *menu = MN_GetActiveMenu();
			int i;

			for (i = 0; i < model->menuTransformCnt; i++) {
				if (menu == model->menuTransform[i].menuPtr) {
					if (!strcmp(command, "debug_mnscale")) {
						VectorCopy(value, model->menuTransform[i].scale);
					} else if (!strcmp(command, "debug_mnangles")) {
						VectorCopy(value, model->menuTransform[i].angles);
					} else if (!strcmp(command, "debug_mnorigin")) {
						VectorCopy(value, model->menuTransform[i].origin);
					}
					break;
				}
			}
#endif
		} else {
			Com_Printf("MN_SetModelTransform_f: no entry in menumodel '%s' for menu '%s'\n", menuModel, nodeOrMenuID);
			return;
		}
	} else {
		/* search the node */
		node = MN_GetNode(MN_GetActiveMenu(), nodeOrMenuID);
		if (!node) {
			/* didn't find node -> "kill" action and print error */
			Com_Printf("MN_SetModelTransform_f: node \"%s\" doesn't exist\n", nodeOrMenuID);
			return;
		} else if (node->behaviour != modelBehaviour) {
			Com_Printf("MN_SetModelTransform_f: node \"%s\" isn't a model node\n", nodeOrMenuID);
			return;
		}

		if (!strcmp(command, "debug_mnscale")) {
			VectorCopy(value, EXTRADATA(node).scale);
		} else if (!strcmp(command, "debug_mnangles")) {
			VectorCopy(value, EXTRADATA(node).angles);
		} else if (!strcmp(command, "debug_mnorigin")) {
			VectorCopy(value, EXTRADATA(node).origin);
		}
	}
}
#endif

static void MN_ModelNodeDraw (menuNode_t *node)
{
	const char* ref = MN_GetReferenceString(node, EXTRADATA(node).model);
	char source[MAX_VAR];
	if (ref == NULL || ref[0] == '\0')
		source[0] = '\0';
	else
		Q_strncpyz(source, ref, sizeof(source));
	MN_DrawModelNode(node, source);
}

static vec3_t nullVector = {0, 0, 0};

/**
 * @brief Set the Model info view (angle,origin,scale) according to the node definition
 */
static inline void MN_InitModelInfoView (menuNode_t *node, modelInfo_t *mi, menuModel_t *menuModel)
{
	qboolean isInitialised = qfalse;
	vec3_t nodeorigin;
	MN_GetNodeAbsPos(node, nodeorigin);
	nodeorigin[0] += node->size[0] / 2 + EXTRADATA(node).origin[0];
	nodeorigin[1] += node->size[1] / 2 + EXTRADATA(node).origin[1];
	nodeorigin[2] = EXTRADATA(node).origin[2];

	if (menuModel->menuTransformCnt) {
		int i;
		/* search a menuTransforme */
		for (i = 0; i < menuModel->menuTransformCnt; i++) {
			if (EXTRADATA(node).viewName != NULL) {
				/** @todo improve the test when its possible */
				if (!strcmp(EXTRADATA(node).viewName, menuModel->menuTransform[i].menuID))
					break;
			}
		}

		/* menuTransforme found */
		if (i != menuModel->menuTransformCnt) {
			/* Use menu scale if defined. */
			if (menuModel->menuTransform[i].useScale) {
				VectorCopy(menuModel->menuTransform[i].scale, mi->scale);
			} else {
				VectorCopy(EXTRADATA(node).scale, mi->scale);
			}

			/* Use menu angles if defined. */
			if (menuModel->menuTransform[i].useAngles) {
				VectorCopy(menuModel->menuTransform[i].angles, mi->angles);
			} else {
				VectorCopy(EXTRADATA(node).angles, mi->angles);
			}

			/* Use menu origin if defined. */
			if (menuModel->menuTransform[i].useOrigin) {
				VectorAdd(nodeorigin, menuModel->menuTransform[i].origin, mi->origin);
			} else {
				VectorCopy(nodeorigin, mi->origin);
			}
			isInitialised = qtrue;
		}
		mi->angles[0] += EXTRADATA(node).angles[0];
		mi->angles[1] += EXTRADATA(node).angles[1];
		mi->angles[2] += EXTRADATA(node).angles[2];
	}

	if (!isInitialised) {
		VectorCopy(EXTRADATA(node).scale, mi->scale);
		VectorCopy(EXTRADATA(node).angles, mi->angles);
		VectorCopy(nodeorigin, mi->origin);
	}

	VectorCopy(nullVector, mi->center);
}

/**
 * @brief Draw a model using "menu model" definition
 */
static void MN_DrawModelNodeWithMenuModel (menuNode_t *node, const char *source, modelInfo_t *mi, menuModel_t *menuModel)
{
	qboolean autoScaleComputed = qfalse;
	vec3_t autoScale;
	vec3_t autoCenter;

	while (menuModel) {
		/* no animation */
		mi->frame = 0;
		mi->oldframe = 0;
		mi->backlerp = 0;

		assert(menuModel->model);
		mi->model = R_RegisterModelShort(menuModel->model);
		if (!mi->model) {
			menuModel = menuModel->next;
			continue;
		}

		mi->skin = menuModel->skin;
		mi->name = menuModel->model;

		/* set mi pointers to menuModel */
		mi->origin = menuModel->origin;
		mi->angles = menuModel->angles;
		mi->center = menuModel->center;
		mi->color = menuModel->color;
		mi->scale = menuModel->scale;

		if (menuModel->tag && menuModel->parent) {
			/* tag and parent defined */
			menuModel_t *menuModelParent;
			modelInfo_t pmi;
			vec3_t pmiorigin;
			animState_t *as;
			/* place this menumodel part on an already existing menumodel tag */
			menuModelParent = MN_GetMenuModel(menuModel->parent);
			if (!menuModelParent) {
				Com_Printf("Menumodel: Could not get the menuModel '%s'\n", menuModel->parent);
				break;
			}
			pmi.model = R_RegisterModelShort(menuModelParent->model);
			if (!pmi.model) {
				Com_Printf("Menumodel: Could not get the model '%s'\n", menuModelParent->model);
				break;
			}

			pmi.name = menuModelParent->model;

			pmi.origin = pmiorigin;
			pmi.angles = menuModelParent->angles;
			pmi.scale = menuModelParent->scale;
			pmi.center = menuModelParent->center;
			pmi.color = menuModelParent->color;

			pmi.origin[0] = menuModelParent->origin[0] + mi->origin[0];
			pmi.origin[1] = menuModelParent->origin[1] + mi->origin[1];
			pmi.origin[2] = menuModelParent->origin[2];
			/* don't count menuoffset twice for tagged models */
			mi->origin[0] -= node->root->pos[0];
			mi->origin[1] -= node->root->pos[1];

			/* autoscale? */
			if (EXTRADATA(node).autoscale) {
				if (!autoScaleComputed)
					Sys_Error("Wrong order of model nodes - the tag and parent model node must be after the base model node");
				pmi.scale = autoScale;
				pmi.center = autoCenter;
			}

			as = &menuModelParent->animState;
			if (!as)
				Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", pmi.name, menuModelParent->anim);
			pmi.frame = as->frame;
			pmi.oldframe = as->oldframe;
			pmi.backlerp = as->backlerp;

			R_DrawModelDirect(mi, &pmi, menuModel->tag);
		} else {
			/* no tag and no parent means - base model or single model */
			const char *ref;
			MN_InitModelInfoView(node, mi, menuModel);
			Vector4Copy(node->color, mi->color);

			/* compute the scale and center for the first model.
			 * it think its the bigger of composite models.
			 * All next elements use the same result
			 */
			if (EXTRADATA(node).autoscale) {
				if (!autoScaleComputed) {
					R_ModelAutoScale(node->size, mi, autoScale, autoCenter);
					autoScaleComputed = qtrue;
				} else {
					mi->scale = autoScale;
					mi->center = autoCenter;
				}
			}

			/* get the animation given by menu node properties */
			if (EXTRADATA(node).animation && *(char *) EXTRADATA(node).animation) {
				ref = MN_GetReferenceString(node, EXTRADATA(node).animation);
			/* otherwise use the standard animation from modelmenu definition */
			} else
				ref = menuModel->anim;

			/* only base models have animations */
			if (ref && *ref) {
				animState_t *as = &menuModel->animState;
				const char *anim = R_AnimGetName(as, mi->model);
				/* initial animation or animation change */
				if (!anim || (anim && strncmp(anim, ref, MAX_VAR)))
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
		menuModel = menuModel->next;
	}

}

/**
 * @todo Menu models should inherite the node values from their parent
 * @todo need to merge menuModel case, and the common case (look to be a copy-pasted code)
 */
void MN_DrawModelNode (menuNode_t *node, const char *source)
{
	modelInfo_t mi;
	menuModel_t *menuModel;
	vec3_t nodeorigin;
	vec3_t autoScale;
	vec3_t autoCenter;

	assert(MN_NodeInstanceOf(node, "model"));			/**< We use model extradata */

	if (source[0] == '\0')
		return;

	menuModel = MN_GetMenuModel(source);
	/* direct model name - no menumodel definition */
	if (!menuModel) {
		/* prevent the searching for a menumodel def in the next frame */
		mi.model = R_RegisterModelShort(source);
		mi.name = source;
		if (!mi.model) {
			Com_Printf("Could not find model '%s'\n", source);
			return;
		}
	}

	/* compute the absolute origin ('origin' property is relative to the node center) */
	MN_GetNodeAbsPos(node, nodeorigin);
	R_CleanupDepthBuffer(nodeorigin[0], nodeorigin[1], node->size[0], node->size[1]);
	if (EXTRADATA(node).clipOverflow)
		R_BeginClipRect(nodeorigin[0], nodeorigin[1], node->size[0], node->size[1]);
	nodeorigin[0] += node->size[0] / 2 + EXTRADATA(node).origin[0];
	nodeorigin[1] += node->size[1] / 2 + EXTRADATA(node).origin[1];
	nodeorigin[2] = EXTRADATA(node).origin[2];

	mi.origin = nodeorigin;
	mi.angles = EXTRADATA(node).angles;
	mi.scale = EXTRADATA(node).scale;
	mi.center = nullVector;
	mi.color = node->color;
	mi.mesh = 0;

	/* special case to draw models with "menu model" */
	if (menuModel) {
		MN_DrawModelNodeWithMenuModel(node, source, &mi, menuModel);
		if (EXTRADATA(node).clipOverflow)
			R_EndClipRect();
		return;
	}

	/* autoscale? */
	if (EXTRADATA(node).autoscale)
		R_ModelAutoScale(node->size, &mi, autoScale, autoCenter);

	/* if the node is linked to a parent, the parent will display it */
	if (EXTRADATA(node).tag) {
		if (EXTRADATA(node).clipOverflow)
			R_EndClipRect();
		return;
	}

	/* no animation */
	mi.frame = 0;
	mi.oldframe = 0;
	mi.backlerp = 0;

	/* get skin */
	if (EXTRADATA(node).skin && *(char *) EXTRADATA(node).skin)
		mi.skin = atoi(MN_GetReferenceString(node, EXTRADATA(node).skin));
	else
		mi.skin = 0;

	/* do animations */
	if (EXTRADATA(node).animation && *(char *) EXTRADATA(node).animation) {
		animState_t *as;
		const char *ref;
		ref = MN_GetReferenceString(node, EXTRADATA(node).animation);

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
			if (anim && strcmp(anim, ref))
				R_AnimChange(as, mi.model, ref);
			R_AnimRun(as, mi.model, cls.frametime * 1000);
		}

		mi.frame = as->frame;
		mi.oldframe = as->oldframe;
		mi.backlerp = as->backlerp;
	}

	/* draw the main model on the node */
	R_DrawModelDirect(&mi, NULL, NULL);

	/* draw all childs */
	if (node->firstChild) {
		menuNode_t *child;
		modelInfo_t pmi = mi;
		for (child = node->firstChild; child; child = child->next) {
			const char *tag;
			char childSource[MAX_VAR];
			const char* childRef;

			/* skip non "model" nodes */
			if (child->behaviour != node->behaviour)
				continue;

			/* skip invisible child */
			if (child->invis || !MN_CheckVisibility(child))
				continue;

			memset(&mi, 0, sizeof(mi));
			mi.angles = EXTRADATA(child).angles;
			mi.scale = EXTRADATA(child).scale;
			mi.center = nullVector;
			mi.origin = EXTRADATA(child).origin;
			mi.color = pmi.color;

			/* get the anchor name to link the model into the parent */
			tag = EXTRADATA(child).tag;

			/* init model name */
			childRef = MN_GetReferenceString(child, EXTRADATA(child).model);
			if (childRef == NULL || childRef[0] == '\0')
				childSource[0] = '\0';
			else
				Q_strncpyz(childSource, childRef, sizeof(childSource));
			mi.model = R_RegisterModelShort(childSource);
			mi.name = childSource;

			/* init skin */
			if (EXTRADATA(child).skin && *(char *) EXTRADATA(child).skin)
				mi.skin = atoi(MN_GetReferenceString(child, EXTRADATA(child).skin));
			else
				mi.skin = 0;

			R_DrawModelDirect(&mi, &pmi, tag);
		}
	}

	if (EXTRADATA(node).clipOverflow)
		R_EndClipRect();
}

static int oldMousePosX = 0;
static int oldMousePosY = 0;

static void MN_ModelNodeCapturedMouseMove (menuNode_t *node, int x, int y)
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

static void MN_ModelNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	if (button != K_MOUSE1)
		return;
	if (!EXTRADATA(node).rotateWithMouse)
		return;
	MN_SetMouseCapture(node);
	oldMousePosX = x;
	oldMousePosY = y;
}

static void MN_ModelNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (button != K_MOUSE1)
		return;
	if (MN_GetMouseCapture() != node)
		return;
	MN_MouseRelease();
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void MN_ModelNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
	if (EXTRADATA(node).oldRefValue == NULL)
		EXTRADATA(node).oldRefValue = MN_AllocStaticString("", MAX_OLDREFVALUE);
	VectorSet(EXTRADATA(node).scale, 1, 1, 1);
	EXTRADATA(node).clipOverflow = qtrue;
}

/**
 * @brief Call to update a cloned node
 */
static void MN_ModelNodeClone (const menuNode_t *source, menuNode_t *clone)
{
	localBehaviour->super->clone(source, clone);
	EXTRADATA(clone).oldRefValue = MN_AllocStaticString("", MAX_OLDREFVALUE);
}

static void MN_ModelNodeLoaded (menuNode_t *node)
{
	/* a tag without but not a submodel */
	if (EXTRADATA(node).tag != NULL && node->behaviour != node->parent->behaviour) {
		Com_Printf("MN_ModelNodeLoaded: '%s' use a tag but is not a submodel. Tag removed.\n", MN_GetPath(node));
		EXTRADATA(node).tag = NULL;
	}

	/* no tag but no size */
	if (EXTRADATA(node).tag == NULL && (node->size[0] == 0 || node->size[1] == 0)) {
		Com_Printf("MN_ModelNodeLoaded: Please set a pos and size to the node '%s'. Note: 'origin' is a relative value to the center of the node\n", MN_GetPath(node));
	}
}

/** @brief valid properties for model */
static const value_t properties[] = {
	/* Both. Name of the animation for the model */
	{"anim", V_CVAR_OR_STRING, MN_EXTRADATA_OFFSETOF(modelExtraData_t, animation), 0},
	/* Main model only. Point of view. */
	{"angles", V_VECTOR, MN_EXTRADATA_OFFSETOF(modelExtraData_t, angles), MEMBER_SIZEOF(modelExtraData_t, angles)},
	/* Main model only. Position of the model relative to the center of the node. */
	{"origin", V_VECTOR, MN_EXTRADATA_OFFSETOF(modelExtraData_t, origin), MEMBER_SIZEOF(modelExtraData_t, origin)},
	/* Both. Scale the model */
	{"scale", V_VECTOR, MN_EXTRADATA_OFFSETOF(modelExtraData_t, scale), MEMBER_SIZEOF(modelExtraData_t, scale)},
	/* Submodel only. A tag name to link the model to the parent model. */
	{"tag", V_CVAR_OR_STRING, MN_EXTRADATA_OFFSETOF(modelExtraData_t, tag), 0},
	/* Main model only. A way to use a custom scripted POV into the model. Check <code>base/ufos/models.ufo</code> for the list of allowed string.
	 * For example "ufopedia", "aircraft", "aircraft_equip", "item". It is also called "menumodel", but have no more link with windows name (aka menu name).
	 * @todo use V_REF_OF_STRING when its possible ('viewName' is never a cvar)
	 */
	{"view", V_CVAR_OR_STRING, MN_EXTRADATA_OFFSETOF(modelExtraData_t, viewName), 0},
	/* Main model only. Auto compute the "better" scale for the model. The function dont work
	 * very well at the moment because it dont check the angle and no more submodel bounding box.
	 */
	{"autoscale", V_BOOL, MN_EXTRADATA_OFFSETOF(modelExtraData_t, autoscale), MEMBER_SIZEOF(modelExtraData_t, autoscale)},
	/* Main model only. Allow to change the POV of the model with the mouse (only for main model) */
	{"rotatewithmouse", V_BOOL, MN_EXTRADATA_OFFSETOF(modelExtraData_t, rotateWithMouse), MEMBER_SIZEOF(modelExtraData_t, rotateWithMouse)},
	/* Main model only. Clip the model with the node rect */
	{"clipoverflow", V_BOOL, MN_EXTRADATA_OFFSETOF(modelExtraData_t, clipOverflow), MEMBER_SIZEOF(modelExtraData_t, clipOverflow)},
	/* Both. Name of the model. The path to the model, relative to <code>base/models</code> */
	{"model", V_CVAR_OR_STRING, MN_EXTRADATA_OFFSETOF(modelExtraData_t, model), 0},
	/* Both. Name of the skin for the model. */
	{"skin", V_CVAR_OR_STRING, MN_EXTRADATA_OFFSETOF(modelExtraData_t, skin), 0},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterModelNode (nodeBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "model";
	behaviour->drawItselfChild = qtrue;
	behaviour->draw = MN_ModelNodeDraw;
	behaviour->mouseDown = MN_ModelNodeMouseDown;
	behaviour->mouseUp = MN_ModelNodeMouseUp;
	behaviour->loading = MN_ModelNodeLoading;
	behaviour->loaded = MN_ModelNodeLoaded;
	behaviour->clone = MN_ModelNodeClone;
	behaviour->capturedMouseMove = MN_ModelNodeCapturedMouseMove;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(modelExtraData_t);

#ifdef DEBUG
	Cmd_AddCommand("debug_mnscale", MN_SetModelTransform_f, "Transform model from command line.");
	Cmd_AddCommand("debug_mnangles", MN_SetModelTransform_f, "Transform model from command line.");
	Cmd_AddCommand("debug_mnorigin", MN_SetModelTransform_f, "Transform model from command line.");
#endif
	Cmd_AddCommand("menumodelslist", MN_ListMenuModels_f, NULL);
}
