/**
 * @file cl_ugv.c
 * @brief Unmanned ground vehicle related routines.
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

#include "../client.h"
#include "cl_actor.h"

/**
 * @brief Adds an UGV to the render entities.
 * @param[in] le The local entity the UGV should be created from
 * @param[out] ent
 * @sa CL_AddActor
 */
qboolean CL_AddUGV (le_t * le, entity_t * ent)
{
	entity_t add;

	if (!LE_IsDead(le)) {
		/* add weapon */
		if (le->left != NONE) {
			OBJZERO(add);

			add.model = cls.modelPool[le->left];

			add.tagent = R_GetFreeEntity() + 2 + (le->right != NONE);
			add.tagname = "tag_lweapon";

			R_AddEntity(&add);
		}

		/* add weapon */
		if (le->right != NONE) {
			OBJZERO(add);

			add.alpha = le->alpha;
			add.model = cls.modelPool[le->right];

			add.tagent = R_GetFreeEntity() + 2;
			add.tagname = "tag_rweapon";

			R_AddEntity(&add);
		}
	}

	/* add head */
	OBJZERO(add);

	add.alpha = le->alpha;
	add.model = le->model2;
	add.skinnum = le->skinnum;

	/** @todo */
	add.tagent = R_GetFreeEntity() + 1;
	add.tagname = "tag_head";

	R_AddEntity(&add);

	/* add actor special effects */
	ent->flags |= RF_SHADOW;
	ent->flags |= RF_ACTOR;

	if (!LE_IsDead(le)) {
		if (le->selected)
			ent->flags |= RF_SELECTED;
		if (le->team == cls.team) {
			if (le->pnum == cl.pnum)
				ent->flags |= RF_MEMBER;
			if (le->pnum != cl.pnum)
				ent->flags |= RF_ALLIED;
		}
		if (le->team == TEAM_CIVILIAN)
			ent->flags |= RF_NEUTRAL;
	}

	return qtrue;
}
