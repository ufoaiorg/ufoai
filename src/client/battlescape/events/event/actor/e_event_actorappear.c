/**
 * @file e_event_actorappear.c
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
#include "../../../cl_ugv.h"
#include "../../../cl_actor.h"
#include "../../../cl_hud.h"
#include "../../../../cgame/cl_game.h"
#include "../../../cl_particle.h"
#include "e_event_actorappear.h"
#include "../../../../../common/grid.h"

/**
 * @brief Decides if following events should be delayed
 */
int CL_ActorAppearTime (const eventRegister_t *self, struct dbuffer *msg, eventTiming_t *eventTiming)
{
	const int eventTime = eventTiming->nextTime;

	/* delay following events */
	if (cl.actTeam != cls.team)
		eventTiming->nextTime += 600;

	return eventTime;
}

/**
 * @brief draw a simple 'spotted' line from a spotter to the spotted
 */
static void CL_DrawLineOfSight (const le_t *watcher, const le_t *target)
{
	ptl_t *ptl;
	vec3_t eyes;

	if (!watcher || !target)
		return;

	/* start is the watchers origin */
	VectorCopy(watcher->origin, eyes);
	if (LE_IsCrouched(watcher))
		eyes[2] += EYE_HT_CROUCH;
	else
		eyes[2] += EYE_HT_STAND;

	ptl = CL_ParticleSpawn("fadeTracer", 0, eyes, target->origin, NULL);

	if (LE_IsCivilian(target))
		VectorSet(ptl->color, 0.2, 0.2, 1);
}

/**
 * @sa CL_ActorAddToTeamList
 * @sa G_AppearPerishEvent
 * @sa CL_ActorAdd
 * @note EV_ACTOR_APPEAR
 */
void CL_ActorAppear (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t *le, *leResponsible;
	int entnum, entnumResponsible, modelnum1, modelnum2;
	int teamDefID = -1;

	/* check if the actor is already visible */
	entnum = NET_ReadShort(msg);
	entnumResponsible = NET_ReadShort(msg);
	le = LE_Get(entnum);
	leResponsible = LE_Get(entnumResponsible);

	if (entnumResponsible != SKIP_LOCAL_ENTITY && !leResponsible)
		LE_NotFoundError(entnumResponsible);

	/* mission start - no actor is spawned yet - so create it */
	if (!le)
		le = LE_Add(entnum);

	/* Locking should be unnecessary if CL_CheckDefault filters this call, since this event starts and
	 * ends in this function only.  Adding lock/unlock just to be sure. */
	LE_Lock(le);

	/* maybe added via CL_ActorAdd before */
	le->invis = qfalse;

	/* get the info */
	NET_ReadFormat(msg, self->formatString,
			&le->team, &teamDefID, &le->gender, &le->ucn, &le->pnum, &le->pos,
			&le->angle, &le->right, &le->left,
			&modelnum1, &modelnum2, &le->skinnum,
			&le->state, &le->fieldSize,
			&le->maxTU, &le->maxMorale, &le->maxHP);

	if (teamDefID < 0 || teamDefID > csi.numTeamDefs)
		Com_Printf("CL_ActorAppear: Invalid teamDef index\n");
	else
		le->teamDef = &csi.teamDef[teamDefID];

	switch (le->fieldSize) {
	case ACTOR_SIZE_NORMAL:
		le->addFunc = CL_AddActor;
		le->type = ET_ACTOR;
		break;
	case ACTOR_SIZE_2x2:
		le->addFunc = CL_AddUGV;
		le->type = ET_ACTOR2x2;
		break;
	default:
		Com_Error(ERR_DROP, "Unknown fieldSize for le in CL_ActorAppear (EV_ACTOR_APPEAR)");
	}
	le->modelnum1 = modelnum1;
	le->modelnum2 = modelnum2;
	le->model1 = LE_GetDrawModel(modelnum1);
	le->model2 = LE_GetDrawModel(modelnum2);
	Grid_PosToVec(cl.mapData->map, le->fieldSize, le->pos, le->origin);
	le->angles[YAW] = directionAngles[le->angle];

	le->contents = CONTENTS_ACTOR;
	VectorCopy(player_mins, le->mins);
	if (LE_IsDead(le))
		VectorCopy(player_dead_maxs, le->maxs);
	else
		VectorCopy(player_maxs, le->maxs);

	LE_SetThink(le, LET_StartIdle);

	/* count spotted aliens (also stunned) */
	cl.numEnemiesSpotted = CL_CountVisibleEnemies();

	if (LE_IsLivingActor(le)) {
		if (cl.actTeam != cls.team) {
			/* center view (if wanted) */
			LE_CenterView(le);
		}

		/* draw line of sight */
		if (le->team != cls.team) {
			if (leResponsible)
				CL_DrawLineOfSight(leResponsible, le);

			/* message */
			if (le->team != TEAM_CIVILIAN) {
				if (GAME_TeamIsKnown(le->teamDef)) {
					char tmpbuf[128];
					Com_sprintf(tmpbuf, sizeof(tmpbuf), _("Enemy spotted: %s!"), _(le->teamDef->name));
					HUD_DisplayMessage(tmpbuf);
				} else
					HUD_DisplayMessage(_("Enemy spotted!\n"));
			} else
				HUD_DisplayMessage(_("Civilian spotted!\n"));

			/* update pathing as new actor could block path */
			CL_ActorConditionalMoveCalc(leResponsible ? leResponsible : selActor);
		}
	}

	/* add team members to the actor list */
	CL_ActorAddToTeamList(le);
	LE_Unlock(le);
}
