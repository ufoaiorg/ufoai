/**
 * @file e_event_addedict.c
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
#include "e_event_addedict.h"


/**
 * @brief Draw the bounding boxes for the server side edicts
 */
static qboolean CL_AddEdictFunc (le_t *le, entity_t *ent)
{
	ent->flags = RF_BOX;
	VectorSet(ent->color, 1, 1, 1);
	ent->alpha = 1.0;
	VectorCopy(le->mins, ent->mins);
	VectorCopy(le->maxs, ent->maxs);
	R_EntitySetOrigin(ent, le->origin);
	return qtrue;
}

/**
 * @brief Adds server side edicts to the client for displaying them
 * @sa EV_ADD_EDICT
 * @sa CL_EntAppear
 */
void CL_AddEdict (const eventRegister_t *self, struct dbuffer * msg)
{
	le_t *le;
	int entnum;
	entity_type_t type;
	vec3_t mins, maxs;

	NET_ReadFormat(msg, self->formatString, &entnum, &type, &mins, &maxs);

	/* use an offset to ensure that we don't conflict with any other solid edict that is already spawned */
	le = LE_Get(entnum + MAX_EDICTS);
	if (!le) {
		le = LE_Add(entnum + MAX_EDICTS);
	} else {
		Com_DPrintf(DEBUG_CLIENT, "CL_AddEdict: Entity appearing already visible... overwriting the old one\n");
		le->inuse = qtrue;
	}

	VectorCopy(mins, le->mins);
	VectorCopy(maxs, le->maxs);
	le->addFunc = CL_AddEdictFunc;
	le->type = type;

	Com_DPrintf(DEBUG_CLIENT, "CL_AddEdict: entnum: %i - type: %i\n", entnum, type);
}
