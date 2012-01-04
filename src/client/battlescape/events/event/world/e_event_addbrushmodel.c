/**
 * @file e_event_addbrushmodel.c
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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "e_event_addbrushmodel.h"
#include "../../../../ui/ui_main.h"

/**
 * @brief Register local entities for SOLID_BSP models like func_breakable or func_door
 * @note func_breakable, func_door
 * @sa G_SendEdictsAndBrushModels
 * @sa EV_ADD_BRUSH_MODEL
 * @sa CL_SpawnParseEntitystring
 */
void CL_AddBrushModel (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t *le;
	int entnum, modelnum1, levelflags, speed, dir;
	entity_type_t type;
	const cBspModel_t *model;
	int angle;
	vec3_t origin, angles;

	NET_ReadFormat(msg, self->formatString, &type, &entnum, &modelnum1, &levelflags, &origin, &angles, &speed, &angle, &dir);

	if (type != ET_BREAKABLE && type != ET_DOOR && type != ET_ROTATING && type != ET_DOOR_SLIDING && type != ET_TRIGGER_RESCUE && type != ET_TRIGGER_NEXTMAP)
		Com_Error(ERR_DROP, "Invalid le announced via EV_ADD_BRUSH_MODEL type: %i\n", type);
	else if (modelnum1 > MAX_MODELS || modelnum1 < 1)
		Com_Error(ERR_DROP, "Invalid le modelnum1 announced via EV_ADD_BRUSH_MODEL\n");

	/* check if the ent is already visible */
	le = LE_Get(entnum);
	if (le)
		Com_Error(ERR_DROP, "le announced a second time - le for entnum %i (type: %i) already exists (via EV_ADD_BRUSH_MODEL)\n", entnum, type);

	le = LE_Add(entnum);
	assert(le);

	le->rotationSpeed = speed;
	le->slidingSpeed = speed;
	le->angle = angle;
	le->dir = dir;
	le->type = type;
	le->modelnum1 = modelnum1;
	le->levelflags = levelflags;
	le->addFunc = LE_BrushModelAction;
	LE_SetThink(le, LET_BrushModel);
	/* The origin and angles are REQUIRED for doors to work! */
	VectorCopy(origin, le->origin);
	/* store the initial position - needed for sliding doors */
	VectorCopy(le->origin, le->oldOrigin);
	VectorCopy(angles, le->angles);

	Com_sprintf(le->inlineModelName, sizeof(le->inlineModelName), "*%i", le->modelnum1);
	model = LE_GetClipModel(le);
	le->model1 = R_FindModel(le->inlineModelName);
	if (!le->model1)
		Com_Error(ERR_DROP, "CL_AddBrushModel: Could not register inline model %i", le->modelnum1);

	/* Transfer model mins and maxs to entity */
	VectorCopy(model->mins, le->mins);
	VectorCopy(model->maxs, le->maxs);
	VectorSubtract(le->maxs, le->mins, le->size);
	VecToPos(le->origin, le->pos);

	/* to allow tracing against this le */
	if (!LE_IsNotSolid(le)) {
		/* This is to help the entity collision code out */
		/* Copy entity origin and angles to model*/
		CM_SetInlineModelOrientation(cl.mapTiles, le->inlineModelName, le->origin, le->angles);

		le->contents = CONTENTS_SOLID;

		CL_RecalcRouting(le);
	}

	if (le->type == ET_TRIGGER_RESCUE) {
		UI_ExecuteConfunc("enable_rescuezone");
	}
}
