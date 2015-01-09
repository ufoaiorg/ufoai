/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "e_event_actorappear.h"
#include "../../../cl_localentity.h"
#include "../../../cl_ugv.h"
#include "../../../cl_actor.h"
#include "../../../cl_hud.h"
#include "../../../../cgame/cl_game.h"
#include "../../../cl_particle.h"
#include "../../../../../common/grid.h"

/**
 * @brief Decides if following events should be delayed
 */
int CL_ActorAppearTime (const eventRegister_t* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	const int eventTime = eventTiming->nextTime;

	/* delay following events */
	if (!cls.isOurRound())
		eventTiming->nextTime += 600;

	return eventTime;
}

/**
 * @brief draw a simple 'spotted' line from a spotter to the spotted
 */
static void CL_DrawLineOfSight (const le_t* watcher, const le_t* target)
{
	if (!watcher || !target)
		return;

	/* start is the watchers origin */
	vec3_t eyes;
	VectorCopy(watcher->origin, eyes);
	if (LE_IsCrouched(watcher))
		eyes[2] += EYE_HT_CROUCH;
	else
		eyes[2] += EYE_HT_STAND;

	ptl_t* ptl = CL_ParticleSpawn("fadeTracer", 0, eyes, target->origin);
	if (ptl == nullptr)
		return;

	if (LE_IsCivilian(target))
		VectorSet(ptl->color, 0.2, 0.2, 1);
}

/**
 * @sa CL_ActorAddToTeamList
 * @sa G_AppearPerishEvent
 * @sa CL_ActorAdd
 * @note EV_ACTOR_APPEAR
 */
void CL_ActorAppear (const eventRegister_t* self, dbuffer* msg)
{

	/* check if the actor is already visible */
	const int entnum = NET_ReadShort(msg);
	const int entnumResponsible = NET_ReadShort(msg);
	le_t* le = LE_Get(entnum);
	le_t* leResponsible = LE_Get(entnumResponsible);

	if (entnumResponsible != SKIP_LOCAL_ENTITY && !leResponsible)
		LE_NotFoundError(entnumResponsible);

	/* mission start - no actor is spawned yet - so create it */
	if (!le)
		le = LE_Add(entnum);

	/* Locking should be unnecessary if CL_CheckDefault filters this call, since this event starts and
	 * ends in this function only.  Adding lock/unlock just to be sure. */
	LE_Lock(le);

	/* maybe added via CL_ActorAdd before */
	le->flags &= ~LE_INVISIBLE;

	/* get the info */
	int teamDefID = -1;
	int modelnum1, modelnum2;
	NET_ReadFormat(msg, self->formatString,
			&le->team, &teamDefID, &le->gender, &le->ucn, &le->pnum, &le->pos,
			&le->angle, &le->right, &le->left,
			&modelnum1, &modelnum2, &le->bodySkin, &le->headSkin,
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
	Grid_PosToVec(cl.mapData->routing, le->fieldSize, le->pos, le->origin);
	le->angles[YAW] = directionAngles[le->angle];

	if (LE_IsDead(le) && !LE_IsStunned(le))
		le->contents = CONTENTS_DEADACTOR;
	else
		le->contents = CONTENTS_ACTOR;
	le->aabb.setMins(player_mins);
	if (LE_IsDead(le))
		le->aabb.setMaxs(player_dead_maxs);
	else
		le->aabb.setMaxs(player_maxs);

	LE_SetThink(le, LET_StartIdle);

	/* count spotted aliens (also stunned) */
	cl.numEnemiesSpotted = CL_CountVisibleEnemies();
	Cvar_SetValue("mn_numaliensspotted", cl.numEnemiesSpotted);

	if (LE_IsLivingActor(le)) {
		if (!cls.isOurRound()) {
			/* center view (if wanted) */
			if (leResponsible) {
				CL_CheckCameraRoute(leResponsible->pos, le->pos);
			} else {
				LE_CenterView(le);
			}
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
					HUD_DisplayMessage(_("Unknown enemy spotted!"));
			} else
				HUD_DisplayMessage(_("Civilian spotted."));

			/* update pathing as new actor could block path */
			CL_ActorConditionalMoveCalc(leResponsible ? leResponsible : selActor);
		}
	}

	/* add team members to the actor list */
	CL_ActorAddToTeamList(le);
	LE_Unlock(le);
}
