/**
 * @file cl_ugv.c
 * @brief Unmanned ground vehicle related routines.
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

#include "cl_ugv.h"
#include "cl_actor.h"
#include "../cl_team.h"

/**
 * @brief Updates the UGV cvars for the given "character".
 *
 * The models and stats that are displayed in the menu are stored in cvars.
 * These cvars are updated here when you select another character.
 *
 * @param chr Pointer to character_t (may not be null)
 * @sa CL_ActorCvars
 * @sa CL_ActorSelect
 */
void CL_UGVCvars (const character_t *chr)
{
	assert(chr);

	Cvar_ForceSet("mn_name", chr->name);
	Cvar_ForceSet("mn_body", CHRSH_CharGetBody(chr));
	Cvar_ForceSet("mn_head", CHRSH_CharGetHead(chr));
	Cvar_ForceSet("mn_skin", va("%i", chr->skin));
	Cvar_ForceSet("mn_skinname", CL_GetTeamSkinName(chr->skin));

	Cvar_Set("mn_chrmis", va("%i", chr->score.assignedMissions));
	Cvar_Set("mn_chrkillalien", va("%i", chr->score.kills[KILLED_ENEMIES]));
	Cvar_Set("mn_chrkillcivilian", va("%i", chr->score.kills[KILLED_CIVILIANS]));
	Cvar_Set("mn_chrkillteam", va("%i", chr->score.kills[KILLED_TEAM]));
	Cvar_Set("mn_chrrank_img", "");
	Cvar_Set("mn_chrrank", "");
	Cvar_Set("mn_chrrank_img", "");

	Cvar_Set("mn_vpwr", va("%i", chr->score.skills[ABILITY_POWER]));
	Cvar_Set("mn_vspd", va("%i", chr->score.skills[ABILITY_SPEED]));
	Cvar_Set("mn_vacc", va("%i", chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_vmnd", "0");
	Cvar_Set("mn_vcls", va("%i", chr->score.skills[SKILL_CLOSE]));
	Cvar_Set("mn_vhvy", va("%i", chr->score.skills[SKILL_HEAVY]));
	Cvar_Set("mn_vass", va("%i", chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set("mn_vsnp", va("%i", chr->score.skills[SKILL_SNIPER]));
	Cvar_Set("mn_vexp", va("%i", chr->score.skills[SKILL_EXPLOSIVE]));

	Cvar_Set("mn_tpwr", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_POWER]), chr->score.skills[ABILITY_POWER]));
	Cvar_Set("mn_tspd", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_SPEED]), chr->score.skills[ABILITY_SPEED]));
	Cvar_Set("mn_tacc", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[ABILITY_ACCURACY]), chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_tmnd", va("%s (0)", CL_ActorGetSkillString(chr->score.skills[ABILITY_MIND])));
	Cvar_Set("mn_tcls", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_CLOSE]), chr->score.skills[SKILL_CLOSE]));
	Cvar_Set("mn_thvy", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_HEAVY]), chr->score.skills[SKILL_HEAVY]));
	Cvar_Set("mn_tass", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_ASSAULT]), chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set("mn_tsnp", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_SNIPER]), chr->score.skills[SKILL_SNIPER]));
	Cvar_Set("mn_texp", va("%s (%i)", CL_ActorGetSkillString(chr->score.skills[SKILL_EXPLOSIVE]), chr->score.skills[SKILL_EXPLOSIVE]));
}

/**
 * @brief Adds an UGV to the render entities.
 * @param[in] le The local entity the UGV should be created from
 * @param[in] ent
 * @sa CL_AddActor
 */
qboolean CL_AddUGV (le_t * le, entity_t * ent)
{
	entity_t add;

	if (!LE_IsDead(le)) {
		/* add weapon */
		if (le->left != NONE) {
			memset(&add, 0, sizeof(add));

			add.model = cls.modelPool[le->left];

			add.tagent = R_GetFreeEntity() + 2 + (le->right != NONE);
			add.tagname = "tag_lweapon";
			add.lighting = &le->lighting; /* values from the ugv */

			R_AddEntity(&add);
		}

		/* add weapon */
		if (le->right != NONE) {
			memset(&add, 0, sizeof(add));

			add.alpha = le->alpha;
			add.model = cls.modelPool[le->right];

			add.tagent = R_GetFreeEntity() + 2;
			add.tagname = "tag_rweapon";
			add.lighting = &le->lighting; /* values from the ugv */

			R_AddEntity(&add);
		}
	}

	/* add head */
	memset(&add, 0, sizeof(add));

	add.alpha = le->alpha;
	add.model = le->model2;
	add.skinnum = le->skinnum;

	/** @todo */
	add.tagent = R_GetFreeEntity() + 1;
	add.tagname = "tag_head";
	add.lighting = &le->lighting; /* values from the ugv */

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
	}

	return qtrue;
}

