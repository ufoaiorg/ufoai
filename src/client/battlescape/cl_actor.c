/**
 * @file cl_actor.c
 * @brief Actor related routines.
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
#include "../cgame/cl_game.h"
#include "cl_hud.h"
#include "cl_parse.h"
#include "cl_particle.h"
#include "cl_view.h"
#include "../cl_screen.h"
#include "../ui/ui_main.h"
#include "../ui/node/ui_node_container.h"
#include "../renderer/r_entity.h"
#include "../../common/routing.h"
#include "../../common/grid.h"

/** @brief Confirm actions in tactical mode - valid values are 0, 1 and 2 */
static cvar_t *confirm_actions;
/** @brief Player preference: should the server make guys stand for long walks, to save TU. */
static cvar_t *cl_autostand;
static cvar_t *cl_showactors;
static cvar_t *cl_reactionleftover;

/* public */
le_t *selActor;
pos3_t truePos; /**< The cell at the current worldlevel under the mouse cursor. */
pos3_t mousePos; /**< The cell that an actor will move to when directed to move. */

/**
 * @brief If you want to change the z level of targeting and shooting,
 * use this value. Negative and positive offsets are possible
 * @sa CL_ActorTargetAlign_f
 * @sa G_ClientShoot
 * @sa G_ShootGrenade
 * @sa G_ShootSingle
 */
static int mousePosTargettingAlign = 0;

static le_t *mouseActor;
static le_t *interactEntity;
static pos3_t mouseLastPos;

/**
 * @brief Writes player action with its data.
 * @param[in] playerAction Type of action.
 * @param[in] entnum The server side edict number of the actor
 */
void MSG_Write_PA (player_action_t playerAction, int entnum, ...)
{
	va_list ap;
	struct dbuffer *msg = new_dbuffer();

	va_start(ap, entnum);
	NET_WriteFormat(msg, "bbs", clc_action, playerAction, entnum);
	NET_vWriteFormat(msg, pa_format[playerAction], ap);
	va_end(ap);
	NET_WriteMsg(cls.netStream, msg);
}

/*
==============================================================
ACTOR MENU UPDATING
==============================================================
*/

/**
 * @brief Return the skill string for the given skill level
 * @return skill string
 * @param[in] skill a skill value between 0 and MAX_SKILL
 */
const char *CL_ActorGetSkillString (const int skill)
{
	const int skillLevel = skill * 10 / MAX_SKILL;
#ifdef DEBUG
	if (skill > MAX_SKILL) {
		Com_Printf("CL_GetSkillString: Skill is bigger than max allowed skill value (%i/%i).\n", skill, MAX_SKILL);
	}
#endif
	switch (skillLevel) {
	case 0:
		return _("Poor");
	case 1:
		return _("Mediocre");
	case 2:
		return _("Average");
	case 3:
		return _("Competent");
	case 4:
		return _("Proficient");
	case 5:
		return _("Very Good");
	case 6:
		return _("Highly Proficient");
	case 7:
		return _("Excellent");
	case 8:
		return _("Outstanding");
	case 9:
		return _("Impressive");
	case 10:
		return _("Superhuman");
	default:
		Com_Printf("CL_GetSkillString: Unknown skill: %i (index: %i).\n", skill, skillLevel);
		return "";
	}
}

void CL_ActorSetFireDef (le_t *actor, const fireDef_t *fd)
{
	if (actor->fd != fd)
		mousePosTargettingAlign = 0;
	actor->fd = fd;
}

/**
 * @brief Decide how the actor will walk, taking into account autostanding.
 * @param[in] le Pointer to an actor for which we set the moving mode.
 * @param[in] length The distance to move: units are TU required assuming actor is standing.
 */
int CL_ActorMoveMode (const le_t *le, int length)
{
	assert(le);
	if (LE_IsCrouched(le)) {
		if (cl_autostand->integer) { /* Is the player using autostand? */
			if (SHOULD_USE_AUTOSTAND(length)) {
				return WALKTYPE_AUTOSTAND_BEING_USED;
			} else {
				return WALKTYPE_AUTOSTAND_BUT_NOT_FAR_ENOUGH;
			}
		} else {
			return WALKTYPE_CROUCH_WALKING;
		}
	} else {
		return WALKTYPE_WALKING;
	}
}

/**
 * @brief Returns the number of the actor in the teamlist.
 * @param[in] le The actor to search.
 * @return The number of the actor in the teamlist. Or @c -1 if the given entity is not in the team list.
 */
static int CL_ActorGetNumber (const le_t * le)
{
	int actorIdx;

	assert(le);

	for (actorIdx = 0; actorIdx < cl.numTeamList; actorIdx++) {
		if (cl.teamList[actorIdx] == le)
			return actorIdx;
	}
	return -1;
}

/**
 * @brief Returns the character information for an actor in the teamlist.
 * @param[in] le The actor to search.
 * @return A pointer to a character struct.
 */
character_t *CL_ActorGetChr (const le_t * le)
{
	int i;
	const chrList_t *chrList = &cl.chrList;

	for (i = 0; i < chrList->num; i++) {
		character_t *chr = chrList->chr[i];
		if (chr->ucn == le->ucn)
			return chr;
	}

	return NULL;
}

/**
 * @brief Sets reactionfire firemode for given actor.
 * @param[out] chr Pointer to an actor for which RF is being set.
 * @param[in] hand Store the given hand.
 * @param[in] fireModeIndex Store the given firemode for this hand.
 * @param[in] weapon Pointer to weapon in the hand.
 */
void CL_ActorSetRFMode (character_t *chr, actorHands_t hand, int fireModeIndex, const objDef_t *weapon)
{
	chr->RFmode.hand = hand;
	chr->RFmode.fmIdx = fireModeIndex;
	chr->RFmode.weapon = weapon;
}

/**
 * @brief Sets shoot firemode for given actor.
 * @param[out] chr Pointer to an actor for which shoot is being set.
 * @param[in] hand Store the given hand.
 * @param[in] fireModeIndex Store the given firemode for this hand.
 * @param[in] weapon Pointer to weapon in the hand.
 */
void CL_ActorSetShotSettings (character_t *chr, actorHands_t hand, int fireModeIndex, const objDef_t *weapon)
{
	chr->reservedTus.shotSettings.hand = hand;
	chr->reservedTus.shotSettings.fmIdx = fireModeIndex;
	chr->reservedTus.shotSettings.weapon = weapon;
}

/**
 * @brief Returns the amount of reserved TUs for a certain type.
 * @param[in] le The actor to check.
 * @param[in] type The type to check. Use RES_ALL_ACTIVE to get all reserved TUs that are not "active" (e.g. RF is skipped if disabled). RES_ALL returns ALL of them, no matter what. See reservation_types_t for a list of options.
 * @return The reserved TUs for the given type.
 * @return -1 on error.
 */
int CL_ActorReservedTUs (const le_t * le, const reservation_types_t type)
{
	character_t *chr;
	int reservedReaction, reservedCrouch, reservedShot;

	if (!le)
		return -1;

	chr = CL_ActorGetChr(le);
	if (!chr) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorReservedTUs: No character found for le.\n");
		return -1;
	}

	reservedReaction = max(0, chr->reservedTus.reaction);
	reservedCrouch = max(0, chr->reservedTus.crouch);
	reservedShot = max(0, chr->reservedTus.shot);

	switch (type) {
	case RES_ALL:
		/* A summary of ALL TUs that are reserved. */
		return reservedReaction + reservedCrouch + reservedShot;
	case RES_ALL_ACTIVE: {
		/* A summary of ALL TUs that are reserved depending on their "status". */
		/* Only use reaction-value if we are have RF activated. */
		if ((le->state & STATE_REACTION))
			return reservedReaction + reservedShot + reservedCrouch;
		else
			return reservedShot + reservedCrouch;
	}
	case RES_REACTION:
		return reservedReaction;
	case RES_CROUCH:
		return reservedCrouch;
	case RES_SHOT:
		return reservedShot;
	default:
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorReservedTUs: Bad type given: %i\n", type);
		return -1;
	}
}

/**
 * @brief Returns the amount of usable (overall-reserved) TUs for an actor.
 * @param[in] le The actor to check.
 * @return The remaining/usable TUs for this actor
 * @return -1 on error (this includes bad [very large] numbers stored in the struct).
 */
int CL_ActorUsableTUs (const le_t * le)
{
	if (!le)
		return -1;

	return le->TU - CL_ActorReservedTUs(le, RES_ALL_ACTIVE);
}

/**
 * @brief Replace the reserved TUs for a certain type.
 * @param[in] le The actor to change it for.
 * @param[in] type The reservation type to be changed (i.e be replaced).
 * @param[in] tus How many TUs to set.
 */
void CL_ActorReserveTUs (const le_t * le, const reservation_types_t type, const int tus)
{
	character_t *chr;

	assert(type != RES_REACTION);

	if (!le || tus < 0)
		return;

	chr = CL_ActorGetChr(le);
	if (chr) {
		chrReservations_t res = chr->reservedTus;

		if (type == RES_CROUCH)
			res.crouch = tus;
		else if (type == RES_SHOT)
			res.shot = tus;

		MSG_Write_PA(PA_RESERVE_STATE, le->entnum, res.shot, res.crouch);
	}
}

/*
==============================================================
ACTOR SELECTION AND TEAM LIST
==============================================================
*/

/**
 * @brief Adds the actor to the team list.
 * @sa CL_ActorAppear
 * @sa CL_ActorRemoveFromTeamList
 * @param le Pointer to local entity struct
 */
void CL_ActorAddToTeamList (le_t * le)
{
	int actorIdx;
	const size_t size = lengthof(cl.teamList);

	/* test team */
	if (!le || le->team != cls.team || le->pnum != cl.pnum || LE_IsDead(le))
		return;

	/* check list for that actor */
	actorIdx = CL_ActorGetNumber(le);

	/* add it */
	if (actorIdx == -1) {
		/* check list length */
		if (cl.numTeamList >= size) {
			Com_Printf("Too many actors on the teamlist!\n");
			return;
		}
		cl.teamList[cl.numTeamList] = le;
		UI_ExecuteConfunc("hudenable %i", cl.numTeamList);
		cl.numTeamList++;
		if (cl.numTeamList == 1)
			CL_ActorSelectList(0);
	} else {
		UI_ExecuteConfunc("hudenable %i", actorIdx);
	}
}

void CL_ActorCleanup (le_t *le)
{
	cls.i.DestroyInventory(&cls.i, &le->i);
}

/**
 * @brief Removes an actor (from your team) from the team list.
 * @sa CL_ActorStateChange
 * @sa CL_ActorAddToTeamList
 * @param[in,out] le Pointer to local entity struct of the actor of your team
 */
void CL_ActorRemoveFromTeamList (le_t * le)
{
	int i;

	if (!le)
		return;

	for (i = 0; i < cl.numTeamList; i++) {
		if (cl.teamList[i] == le) {
			if (!LE_IsStunned(le)) {
				CL_ActorCleanup(le);
				/* remove from list */
				cl.teamList[i] = NULL;
			} else {
				/** @todo why the heck is that needed? the inventory was already dropped to floor. */
				le->left = le->right = le->extension = le->headgear = NONE;
				cls.i.DestroyInventory(&cls.i, &le->i);
			}

			/* disable hud button */
			UI_ExecuteConfunc("huddisable %i", i);

			break;
		}
	}

	/* check selection */
	if (le->selected) {
		for (i = 0; i < cl.numTeamList; i++) {
			le_t *tl = cl.teamList[i];
			if (tl && CL_ActorSelect(tl))
				break;
		}

		if (i == cl.numTeamList)
			CL_ActorSelect(NULL);
	}
}

/**
 * @brief Selects an actor.
 * @param le Pointer to local entity struct. If this is @c NULL the ui_inventory that is linked from the actors
 * @sa CL_UGVCvars
 * @sa CL_ActorCvars
 */
qboolean CL_ActorSelect (le_t * le)
{
	int actorIdx;
	character_t* chr;

	/* test team */
	if (!le) {
		if (selActor)
			selActor->selected = qfalse;
		selActor = NULL;
		ui_inventory = NULL;
		return qfalse;
	}

	if (le->pnum != cl.pnum || LE_IsDead(le) || !le->inuse)
		return qfalse;

	if (le->selected) {
		mousePosTargettingAlign = 0;
		return qtrue;
	}

	if (selActor)
		selActor->selected = qfalse;

	mousePosTargettingAlign = 0;
	selActor = le;
	selActor->selected = qtrue;
	ui_inventory = &selActor->i;

	if (le->state & RF_IRGOGGLESSHOT)
		refdef.rendererFlags |= RDF_IRGOGGLES;
	else
		refdef.rendererFlags &= ~RDF_IRGOGGLES;

	if (le->clientAction != NULL)
		UI_ExecuteConfunc("enable_clientaction");
	else
		UI_ExecuteConfunc("disable_clientaction");

	actorIdx = CL_ActorGetNumber(le);
	if (actorIdx == -1)
		return qfalse;

	/* console commands, update cvars */
	Cvar_ForceSet("cl_selected", va("%i", actorIdx));

	chr = CL_ActorGetChr(le);
	if (!chr)
		Com_Error(ERR_DROP, "No character given for local entity!");

	CL_UpdateCharacterValues(chr, "mn_");

	CL_ActorConditionalMoveCalc(le);

	return qtrue;
}

/**
 * @brief Selects an actor from a list.
 *
 * This function is used to select an actor from the lists that are
 * used in equipment and team assemble screens
 *
 * @param num The index value from the list of actors
 *
 * @sa CL_ActorSelect
 * @return qtrue if selection was possible otherwise qfalse
 */
qboolean CL_ActorSelectList (int num)
{
	le_t *le;

	/* check if actor exists */
	if (num >= cl.numTeamList || num < 0)
		return qfalse;

	/* select actor */
	le = cl.teamList[num];
	if (!le || !CL_ActorSelect(le))
		return qfalse;

	/* center view (if wanted) */
	LE_CenterView(le);
	Cvar_SetValue("cl_worldlevel", le->pos[2]);

	return qtrue;
}

/**
 * @brief selects the next actor
 */
qboolean CL_ActorSelectNext (void)
{
	int selIndex = -1;
	const int num = cl.numTeamList;
	int i;

	/* find index of currently selected actor */
	for (i = 0; i < num; i++) {
		const le_t *le = cl.teamList[i];
		if (le && le->selected && le->inuse && !LE_IsDead(le)) {
			selIndex = i;
			break;
		}
	}
	if (selIndex < 0)
		return qfalse;			/* no one selected? */

	/* cycle round */
	i = selIndex;
	while (qtrue) {
		i = (i + 1) % num;
		if (i == selIndex)
			break;
		if (CL_ActorSelectList(i))
			return qtrue;
	}
	return qfalse;
}

/**
 * @brief selects the previous actor
 */
qboolean CL_ActorSelectPrev (void)
{
	int selIndex = -1;
	const int num = cl.numTeamList;
	int i;

	/* find index of currently selected actor */
	for (i = 0; i < num; i++) {
		const le_t *le = cl.teamList[i];
		if (le && le->selected && le->inuse && !LE_IsDead(le)) {
			selIndex = i;
			break;
		}
	}
	if (selIndex < 0)
		return qfalse;			/* no one selected? */

	/* cycle round */
	i = selIndex;
	while (qtrue) {
	/*	i = (i - 1) % num; */
		i--; if (i < 0) i = num - 1;
		if (i == selIndex)
			break;
		if (CL_ActorSelectList(i))
			return qtrue;
	}
	return qfalse;
}


/*
==============================================================
ACTOR MOVEMENT AND SHOOTING
==============================================================
*/

/**
 * @brief A list of locations that cannot be moved to.
 * @note Pointer to le->pos or edict->pos followed by le->fieldSize or edict->fieldSize
 * @see CL_BuildForbiddenList
 */
static pos_t *forbiddenList[MAX_FORBIDDENLIST];
/**
 * @brief Current length of fb_list.
 * @note all byte pointers in the fb_list list (pos + fieldSize)
 * @see fb_list
 */
static int forbiddenListLength;

/**
 * @brief Builds a list of locations that cannot be moved to (client side).
 * @sa G_MoveCalc
 * @sa G_BuildForbiddenList <- server side
 * @sa Grid_CheckForbidden
 * @note This is used for pathfinding.
 * It is a list of where the selected unit can not move to because others are standing there already.
 */
static void CL_BuildForbiddenList (void)
{
	le_t *le = NULL;

	forbiddenListLength = 0;

	while ((le = LE_GetNextInUse(le))) {
		if (le->invis)
			continue;
		/* Dead ugv will stop walking, too. */
		if (le->type == ET_ACTOR2x2 || (!LE_IsStunned(le) && LE_IsLivingAndVisibleActor(le))) {
			forbiddenList[forbiddenListLength++] = le->pos;
			forbiddenList[forbiddenListLength++] = (byte*)&le->fieldSize;
		}
	}

#ifdef PARANOID
	if (forbiddenListLength > MAX_FORBIDDENLIST)
		Com_Error(ERR_DROP, "CL_BuildForbiddenList: list too long!");
#endif
}

#ifdef DEBUG
/**
 * @brief Draws a marker for all blocked map-positions.
 * @note currently uses basically the same code as CL_BuildForbiddenList
 * @note usage in console: "debug_drawblocked"
 * @todo currently the particles stay a _very_ long time ... so everybody has to stand still in order for the display to be correct.
 * @sa CL_BuildForbiddenList
 */
static void CL_DisplayBlockedPaths_f (void)
{
	le_t *le = NULL;
	int j;
	ptl_t *ptl;
	vec3_t s;

	while ((le = LE_GetNextInUse(le))) {
		switch (le->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			/* draw blocking cursor at le->pos */
			if (!LE_IsDead(le))
				Grid_PosToVec(cl.mapData->map, le->fieldSize, le->pos, s);
			break;
		case ET_DOOR:
		case ET_BREAKABLE:
		case ET_ROTATING:
			VectorCopy(le->origin, s);
			break;
		default:
			continue;
		}

		ptl = CL_ParticleSpawn("blocked_field", 0, s, NULL, NULL);
		ptl->rounds = 2;
		ptl->roundsCnt = 2;
		ptl->life = 10000;
		ptl->t = 0;
		if (le->fieldSize == ACTOR_SIZE_2x2) {
			/* If this actor blocks 4 fields draw them as well. */
			for (j = 0; j < 3; j++) {
				ptl_t *ptl2 = CL_ParticleSpawn("blocked_field", 0, s, NULL, NULL);
				ptl2->rounds = ptl->rounds;
				ptl2->roundsCnt = ptl->roundsCnt;
				ptl2->life = ptl->life;
				ptl2->t = ptl->t;
			}
		}
	}
}
#endif

/**
 * @brief Recalculate forbidden list, available moves and actor's move length
 * for the current selected actor.
 * @note An attempt to do this with le->TU to save time ended up with the first actor not being able to move at gamestart.
 * @todo seems like this function is called *before* the TUs are set
 */
void CL_ActorConditionalMoveCalc (le_t *le)
{
	CL_BuildForbiddenList();
	if (le && le->selected) {
		const byte crouchingState = LE_IsCrouched(le) ? 1 : 0;
		Grid_MoveCalc(cl.mapData->map, le->fieldSize, &cl.pathMap, le->pos, crouchingState, MAX_ROUTE, forbiddenList, forbiddenListLength);
		CL_ActorResetMoveLength(le);
	}
}

/**
 * @brief Checks that an action is valid.
 * @param[in] le Pointer to actor for which we check an action.
 * @return qtrue if action is valid.
 */
int CL_ActorCheckAction (const le_t *le)
{
	if (!le)
		return qfalse;

	/* already moving */
	if (le->pathLength)
		return qfalse;

	if (cls.team != cl.actTeam) {
		HUD_DisplayMessage(_("It is not your turn!"));
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Get the real move length (depends on crouch-state of the current actor).
 * @note The part of the line that is not reachable in this turn (i.e. not enough
 * @note TUs left) will be drawn differently.
 * @param[in] to The position in the map to calculate the move-length for.
 * @param[in] le Pointer to actor for which we calculate move lenght.
 * @return The amount of TUs that are needed to walk to the given grid position
 */
static byte CL_ActorMoveLength (const le_t *le, const pos3_t to)
{
	const byte crouchingState = LE_IsCrouched(le) ? 1 : 0;
	const byte length = Grid_MoveLength(&cl.pathMap, to, crouchingState, qfalse);
	return length;
}

/**
 * @brief Recalculates the currently selected Actor's move length.
 * @param[in,out] le Pointer to actor for which we reset move lenght.
 */
void CL_ActorResetMoveLength (le_t *le)
{
	le->actorMoveLength = CL_ActorMoveLength(le, mousePos);
}

/**
 * @brief Draws the way to walk when confirm actions is activated.
 * @param[in] to The location we draw the line to (starting with the location of selActor)
 * @return qtrue if everything went ok, otherwise qfalse.
 * @sa CL_MaximumMove (similar algo.)
 * @sa CL_AddTargetingBox
 */
static qboolean CL_ActorTraceMove (const pos3_t to)
{
	byte length;
	vec3_t vec, oldVec;
	pos3_t pos;
	int dvec;
	byte crouchingState;

	if (!selActor)
		return qfalse;

	length = CL_ActorMoveLength(selActor, to);
	if (!length || length >= ROUTING_NOT_REACHABLE)
		return qfalse;

	crouchingState = LE_IsCrouched(selActor) ? 1 : 0;

	Grid_PosToVec(cl.mapData->map, selActor->fieldSize, to, oldVec);
	VectorCopy(to, pos);

	Com_DPrintf(DEBUG_PATHING, "Starting pos: (%i, %i, %i).\n", pos[0], pos[1], pos[2]);

	while ((dvec = Grid_MoveNext(&cl.pathMap, pos, crouchingState)) != ROUTING_UNREACHABLE) {
		length = CL_ActorMoveLength(selActor, pos);
		PosSubDV(pos, crouchingState, dvec); /* We are going backwards to the origin. */
		Com_DPrintf(DEBUG_PATHING, "Next pos: (%i, %i, %i, %i) [%i].\n", pos[0], pos[1], pos[2], crouchingState, dvec);
		Grid_PosToVec(cl.mapData->map, selActor->fieldSize, pos, vec);
		if (length > CL_ActorUsableTUs(selActor))
			CL_ParticleSpawn("longRangeTracer", 0, vec, oldVec, NULL);
		else if (crouchingState)
			CL_ParticleSpawn("crawlTracer", 0, vec, oldVec, NULL);
		else
			CL_ParticleSpawn("moveTracer", 0, vec, oldVec, NULL);
		VectorCopy(vec, oldVec);
	}
	return qtrue;
}

/**
 * @brief Return the last position we can walk to with a defined amount of TUs.
 * @param[in] to The location we want to reach.
 * @param[in] le Pointer to an actor for which we check maximum move.
 * @param[in,out] pos The location we can reach with the given amount of TUs.
 * @sa CL_TraceMove (similar algo.)
 */
static void CL_ActorMaximumMove (const pos3_t to, const le_t *le, pos3_t pos)
{
	int dvec;
	byte crouchingState = le && LE_IsCrouched(le) ? 1 : 0;
	const int tus = CL_ActorUsableTUs(le);
	const byte length = CL_ActorMoveLength(le, to);
	if (!length || length >= ROUTING_NOT_REACHABLE)
		return;

	VectorCopy(to, pos);

	while ((dvec = Grid_MoveNext(&cl.pathMap, pos, crouchingState)) != ROUTING_UNREACHABLE) {
		const byte length2 = CL_ActorMoveLength(le, pos);
		if (length2 <= tus)
			return;
		PosSubDV(pos, crouchingState, dvec); /* We are going backwards to the origin. */
	}
}

void CL_ActorSetMode (le_t *actor, actorModes_t actorMode)
{
	actor->actorMode = actorMode;
}

/**
 * @brief Starts moving actor.
 * @param[in] le
 * @param[in] to
 * @sa CL_ActorActionMouse
 * @sa CL_ActorSelectMouse
 */
void CL_ActorStartMove (le_t * le, const pos3_t to)
{
	byte length;
	pos3_t toReal;

	if (IN_GetMouseSpace() != MS_WORLD)
		return;

	if (!CL_ActorCheckAction(le))
		return;

	length = CL_ActorMoveLength(le, to);

	if (!length || length >= ROUTING_NOT_REACHABLE) {
		/* move not valid, don't even care to send */
		return;
	}

	/* Get the last position we can walk to with the usable TUs. */
	CL_ActorMaximumMove(to, le, toReal);

	/* Get the cost of the new position just in case. */
	length = CL_ActorMoveLength(le, toReal);

	if (CL_ActorUsableTUs(le) < length) {
		/* We do not have enough _usable_ TUs to move so don't even try to send. */
		/* This includes a check for reserved TUs (which isn't done on the server!) */
		return;
	}

	/* change mode to move now */
	CL_ActorSetMode(le, M_MOVE);

	/* move seems to be possible; send request to server */
	MSG_Write_PA(PA_MOVE, le->entnum, toReal);
}


/**
 * @brief Shoot with actor.
 * @param[in] le Who is shooting
 * @param[in] at Position you are targeting to
 */
void CL_ActorShoot (const le_t * le, const pos3_t at)
{
	int type;

	if (IN_GetMouseSpace() != MS_WORLD)
		return;

	if (!CL_ActorCheckAction(le))
		return;

	if (IS_MODE_FIRE_RIGHT(le->actorMode)) {
		type = ST_RIGHT;
	} else if (IS_MODE_FIRE_LEFT(le->actorMode)) {
		type = ST_LEFT;
	} else if (IS_MODE_FIRE_HEADGEAR(le->actorMode)) {
		type = ST_HEADGEAR;
	} else
		return;

	MSG_Write_PA(PA_SHOOT, le->entnum, at, type, le->currentSelectedFiremode, mousePosTargettingAlign);
}

/**
 * @brief Searches the clip with the least TU usage to put it into the weapon
 * @param invList The inventory list that can be used outside of this function for the found ammo
 * @param inv The inventory to do the search in
 * @param weapon The weapon to reload
 * @return @c NONE if no container was found, the container id otherwise.
 */
int CL_ActorGetContainerForReload (invList_t **invList, const inventory_t *inv, const objDef_t *weapon)
{
	containerIndex_t container;
	int tu = 100;
	containerIndex_t bestContainer = NONE;

	/* also search the linked ground floor tile (temp container) */
	for (container = 0; container < csi.numIDs; container++) {
		if (INVDEF(container)->out < tu) {
			invList_t *ic;
			/* Once we've found at least one clip, there's no point
			 * searching other containers if it would take longer
			 * to retrieve the ammo from them than the one
			 * we've already found. */
			for (ic = inv->c[container]; ic; ic = ic->next) {
				const objDef_t *od = ic->item.t;
				if (INVSH_LoadableInWeapon(od, weapon) && GAME_ItemIsUseable(od)) {
					tu = INVDEF(container)->out;
					bestContainer = container;
					*invList = ic;
					break;
				}
			}
		}
	}
	return bestContainer;
}

/**
 * @brief Reload weapon with actor.
 * @param[in,out] le The actor to reload the weapon for
 * @param[in] containerID The container to reload
 * @sa CL_ActorCheckAction
 */
void CL_ActorReload (le_t *le, containerIndex_t containerID)
{
	inventory_t *inv;
	invList_t *ic;
	const objDef_t *weapon;
	containerIndex_t bestContainer;

	if (!CL_ActorCheckAction(le))
		return;

	/* check weapon */
	inv = &le->i;

	if (inv->c[containerID]) {
		weapon = inv->c[containerID]->item.t;
	} else if (containerID == csi.idLeft && inv->c[csi.idRight]->item.t->holdTwoHanded) {
		/* Check for two-handed weapon */
		containerID = csi.idRight;
		weapon = inv->c[containerID]->item.t;
	} else
		/* otherwise we could use weapon uninitialized */
		return;

	if (!weapon)
		return;

	/* return if the weapon is not reloadable */
	if (!weapon->reload)
		return;

	if (!GAME_ItemIsUseable(weapon)) {
		HUD_DisplayMessage(_("You cannot reload this unknown item.\n"));
		return;
	}

	bestContainer = CL_ActorGetContainerForReload(&ic, inv, weapon);
	/* send request */
	if (bestContainer != NONE) {
		int x, y;

		INVSH_GetFirstShapePosition(ic, &x, &y);
		x += ic->x;
		y += ic->y;

		CL_ActorInvMove(le, bestContainer, x, y, containerID, 0, 0);
	}
}

/**
 * @brief Sends an inventory move event to the server
 * @param le The le that is doing the inventory move (an actor)
 * @param fromContainer The container to fetch the item from
 * @param fromX The x position in the container to get the item from
 * @param fromY The y position in the container to get the item from
 * @param toContainer The container to store the item in
 * @param toX The x position in the container to move the item to
 * @param toY The y position in the container to move the item to
 */
void CL_ActorInvMove (const le_t *le, containerIndex_t fromContainer, int fromX, int fromY, containerIndex_t toContainer, int toX, int toY)
{
	const invDef_t *fromPtr = INVDEF(fromContainer);

	assert(CL_BattlescapeRunning());
	assert(le);
	assert(LE_IsActor(le));

	if (INVSH_SearchInInventory(&le->i, fromPtr, fromX, fromY) != NULL)
		MSG_Write_PA(PA_INVMOVE, le->entnum, fromContainer, fromX, fromY, toContainer, toX, toY);
}

/**
 * @brief Uses the current selected entity in the battlescape. Can e.g. open the selected door.
 * @sa G_ClientUseEdict
 */
static void CL_ActorUse (const le_t *le)
{
	if (!CL_ActorCheckAction(le))
		return;

	assert(le->clientAction);

	MSG_Write_PA(PA_USE, le->entnum, le->clientAction->entnum);
	Com_DPrintf(DEBUG_CLIENT, "CL_ActorUse: Use door number: %i (actor %i).\n", le->clientAction->entnum, le->entnum);
}

/**
 * @brief Hud callback to use the current selected entity
 */
static void CL_ActorUse_f (void)
{
	le_t *actor = selActor;

	if (!CL_ActorCheckAction(actor))
		return;

	/* no client action */
	if (actor->clientAction == NULL) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorUse_f: No client_action set for actor with entnum %i.\n", actor->entnum);
		return;
	}

	if (LE_IsDoor(actor->clientAction)) {
		/* Check if we should even try to send this command (no TUs left or). */
		if (CL_ActorUsableTUs(actor) >= TU_DOOR_ACTION)
			CL_ActorUse(actor);
	}
}

/**
 * @brief Checks whether we are in fire mode or node
 * @param mode The actor mode
 * @return @c true if we are in fire mode, @c false otherwise
 */
qboolean CL_ActorFireModeActivated (const actorModes_t mode)
{
	return IS_MODE_FIRE_RIGHT(mode) || IS_MODE_FIRE_LEFT(mode) || IS_MODE_FIRE_HEADGEAR(mode);
}

/**
 * @brief Turns the actor around without moving
 */
void CL_ActorTurnMouse (void)
{
	vec3_t directionVector;
	dvec_t dvec;

	if (IN_GetMouseSpace() != MS_WORLD)
		return;

	if (!CL_ActorCheckAction(selActor))
		return;

	if (CL_ActorUsableTUs(selActor) < TU_TURN) {
		/* Cannot turn because of not enough usable TUs. */
		return;
	}

	/* check for fire-modes, and cancel them */
	if (CL_ActorFireModeActivated(selActor->actorMode)) {
		CL_ActorActionMouse();
		return; /* and return without turning */
	}

	/* calculate dvec */
	VectorSubtract(mousePos, selActor->pos, directionVector);
	dvec = AngleToDV((int) (atan2(directionVector[1], directionVector[0]) * todeg));

	/* send message to server */
	MSG_Write_PA(PA_TURN, selActor->entnum, dvec);
}

/**
 * @brief Stands or crouches actor.
 */
static void CL_ActorStandCrouch_f (void)
{
	if (!CL_ActorCheckAction(selActor))
		return;

	/* Check if we should even try to send this command (no TUs left or). */
	if (CL_ActorUsableTUs(selActor) >= TU_CROUCH || CL_ActorReservedTUs(selActor, RES_CROUCH) >= TU_CROUCH) {
		/* send a request to toggle crouch to the server */
		MSG_Write_PA(PA_STATE, selActor->entnum, STATE_CROUCHED);
	}
}

/**
 * @brief Toggles the headgear for the current selected player
 */
static void CL_ActorUseHeadgear_f (void)
{
	invList_t* headgear;
	const int tmpMouseSpace = IN_GetMouseSpace();

	/* this can be executed by a click on a hud button
	 * but we need MS_WORLD mouse space to let the shooting
	 * function work */
	IN_SetMouseSpace(MS_WORLD);

	if (!CL_ActorCheckAction(selActor))
		return;

	headgear = HEADGEAR(selActor);
	if (!headgear)
		return;

	CL_ActorSetMode(selActor, M_FIRE_HEADGEAR);
	/** @todo make this a variable somewhere? */
	selActor->currentSelectedFiremode = 0;
	CL_ActorShoot(selActor, selActor->pos);
	CL_ActorSetMode(selActor, M_MOVE);

	/* restore old mouse space */
	IN_SetMouseSpace(tmpMouseSpace);
}

/*
==============================================================
MOUSE INPUT
==============================================================
*/

/**
 * @brief handle select or action clicking in either move mode
 * @sa CL_ActorSelectMouse
 * @sa CL_ActorActionMouse
 */
static void CL_ActorMoveMouse (void)
{
	/* Don't display the cursor if it's above the currently selected level.
	 * The 2nd part of the if is an attempt to display it anyway when we eg. climb a hill.
	 * But there are too many situations inside buildings that match the criteria (eg. actorclip around chair).
	 * So disabled for now.*/
	if (mousePos[2] > cl_worldlevel->integer/* && !RT_AllCellsBelowAreFilled(cl.mapData->map, fieldSize, pos)*/)
		return;

	if (selActor->actorMode == M_PEND_MOVE) {
		if (VectorCompare(mousePos, selActor->mousePendPos)) {
			/* Pending move and clicked the same spot (i.e. 2 clicks on the same place) */
			CL_ActorStartMove(selActor, mousePos);
		} else {
			/* Clicked different spot. */
			VectorCopy(mousePos, selActor->mousePendPos);
		}
	} else {
		/* either we want to confirm every move, or it's not our round and we prepare the
		 * movement for the next round */
		if (confirm_actions->integer || cls.team != cl.actTeam) {
			/* Set our mode to pending move. */
			VectorCopy(mousePos, selActor->mousePendPos);

			CL_ActorSetMode(selActor, M_PEND_MOVE);
		} else {
			/* Just move there */
			CL_ActorStartMove(selActor, mousePos);
		}
	}
}

/**
 * @brief Selects an actor using the mouse.
 * @sa CL_ActorStartMove
 */
void CL_ActorSelectMouse (void)
{
	if (IN_GetMouseSpace() != MS_WORLD || !selActor)
		return;

	switch (selActor->actorMode) {
	case M_MOVE:
	case M_PEND_MOVE:
		/* Try and select another team member */
		if (mouseActor && !mouseActor->selected && CL_ActorSelect(mouseActor)) {
			/* Succeeded so go back into move mode. */
			CL_ActorSetMode(selActor, M_MOVE);
		} else if (interactEntity) {
			CL_ActorUse(selActor);
		} else {
			CL_ActorMoveMouse();
		}
		break;
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		if (VectorCompare(mousePos, selActor->mousePendPos)) {
			/* Pending shot and clicked the same spot (i.e. 2 clicks on the same place) */
			CL_ActorShoot(selActor, mousePos);

			/* We switch back to aiming mode. */
			if (selActor->actorMode == M_PEND_FIRE_R)
				CL_ActorSetMode(selActor, M_FIRE_R);
			else
				CL_ActorSetMode(selActor, M_FIRE_L);
		} else {
			/* Clicked different spot. */
			VectorCopy(mousePos, selActor->mousePendPos);
		}
		break;
	case M_FIRE_R:
		if (mouseActor && mouseActor->selected)
			break;

		/* We either switch to "pending" fire-mode or fire the gun. */
		if (confirm_actions->integer == 1) {
			CL_ActorSetMode(selActor, M_PEND_FIRE_R);
			VectorCopy(mousePos, selActor->mousePendPos);
		} else {
			CL_ActorShoot(selActor, mousePos);
		}
		break;
	case M_FIRE_L:
		if (mouseActor && mouseActor->selected)
			break;

		/* We either switch to "pending" fire-mode or fire the gun. */
		if (confirm_actions->integer == 1) {
			CL_ActorSetMode(selActor, M_PEND_FIRE_L);
			VectorCopy(mousePos, selActor->mousePendPos);
		} else {
			CL_ActorShoot(selActor, mousePos);
		}
		break;
	default:
		break;
	}
}


/**
 * @brief initiates action with mouse.
 * @sa CL_ActionDown
 * @sa CL_ActorStartMove
 */
void CL_ActorActionMouse (void)
{
	if (!selActor || IN_GetMouseSpace() != MS_WORLD)
		return;

	if (CL_ActorFireModeActivated(selActor->actorMode)) {
		CL_ActorSetMode(selActor, M_MOVE);
	}
}

/*
==============================================================
MOUSE SCANNING
==============================================================
*/

/**
 * @brief Battlescape cursor positioning.
 * @note Sets global var mouseActor to current selected le
 * @sa IN_Parse
 */
qboolean CL_ActorMouseTrace (void)
{
	int restingLevel;
	float cur[2], frustumSlope[2];
	const float projectionDistance = 2048.0f;
	float nDotP2minusP1;
	vec3_t forward, right, up, stop;
	vec3_t from, end;
	vec3_t mapNormal, P3, P2minusP1;
	vec3_t pA, pB, pC;
	pos3_t testPos;
	le_t *interactLe;

	/* get cursor position as a -1 to +1 range for projection */
	cur[0] = (mousePosX * viddef.rx - viddef.viewWidth * 0.5 - viddef.x) / (viddef.viewWidth * 0.5);
	cur[1] = (mousePosY * viddef.ry - viddef.viewHeight * 0.5 - viddef.y) / (viddef.viewHeight * 0.5);

	/* get trace vectors */
	VectorCopy(cl.cam.camorg, from);
	VectorCopy(cl.cam.axis[0], forward);
	VectorCopy(cl.cam.axis[1], right);
	VectorCopy(cl.cam.axis[2], up);

	if (cl_isometric->integer)
		frustumSlope[0] = 10.0 * refdef.fieldOfViewX;
	else
		frustumSlope[0] = tan(refdef.fieldOfViewX * (M_PI / 360.0)) * projectionDistance;
	frustumSlope[1] = frustumSlope[0] * ((float)viddef.viewHeight / (float)viddef.viewWidth);

	/* transform cursor position into perspective space */
	VectorMA(from, projectionDistance, forward, stop);
	VectorMA(stop, cur[0] * frustumSlope[0], right, stop);
	VectorMA(stop, cur[1] * -frustumSlope[1], up, stop);

	/* in isometric mode the camera position has to be calculated from the cursor position so that the trace goes in the right direction */
	if (cl_isometric->integer)
		VectorMA(stop, -projectionDistance * 2, forward, from);

	/* set stop point to the intersection of the trace line with the desired plane */
	/* description of maths used:
	 *   The equation for the plane can be written:
	 *     mapNormal dot (end - P3) = 0
	 *     where mapNormal is the vector normal to the plane,
	 *         P3 is any point on the plane and
	 *         end is the point where the line intersects the plane
	 *   All points on the line can be calculated using:
	 *     P1 + u*(P2 - P1)
	 *     where P1 and P2 are points that define the line and
	 *           u is some scalar
	 *   The intersection of the line and plane occurs when:
	 *     mapNormal dot (P1 + u*(P2 - P1)) == mapNormal dot P3
	 *   The intersection therefore occurs when:
	 *     u = (mapNormal dot (P3 - P1))/(mapNormal dot (P2 - P1))
	 * Note: in the code below from & stop represent P1 and P2 respectively
	 */
	VectorSet(P3, 0., 0., cl_worldlevel->integer * UNIT_HEIGHT + CURSOR_OFFSET);
	VectorSet(mapNormal, 0., 0., 1.);
	VectorSubtract(stop, from, P2minusP1);
	nDotP2minusP1 = DotProduct(mapNormal, P2minusP1);

	/* calculate intersection directly if angle is not parallel to the map plane */
	if (nDotP2minusP1 > 0.01 || nDotP2minusP1 < -0.01) {
		float u;
		vec3_t dir, P3minusP1;

		VectorSubtract(P3, from, P3minusP1);
		u = DotProduct(mapNormal, P3minusP1) / nDotP2minusP1;
		VectorScale(P2minusP1, (vec_t)u, dir);
		VectorAdd(from, dir, end);
	} else { /* otherwise do a full trace */
		CM_EntTestLineDM(cl.mapTiles, from, stop, end, TL_FLAG_ACTORCLIP, cl.leInlineModelList);
	}

	VecToPos(end, testPos);

	/* hack to prevent cursor from getting stuck on the top of an invisible
	 * playerclip surface (in most cases anyway) */
	PosToVec(testPos, pA);
	/* ensure that the cursor is in the world, if this is not done, the tracer box is
	 * drawn in the void on the first level and the menu key bindings might get executed
	 * this could result in different problems like the zooming issue (where you can't zoom
	 * in again, because in_zoomout->state is not reseted). */
	if (CL_OutsideMap(pA, MAP_SIZE_OFFSET))
		return qfalse;

	VectorCopy(pA, pB);
	pA[2] += UNIT_HEIGHT;
	pB[2] -= UNIT_HEIGHT;
	/** @todo Shouldn't we check the return value of CM_TestLineDM here - maybe
	 * we don't have to do the second Grid_Fall call at all and can safe a lot
	 * of traces */
	restingLevel = Grid_Fall(cl.mapData->map, ACTOR_GET_FIELDSIZE(selActor), testPos);
	CM_EntTestLineDM(cl.mapTiles, pA, pB, pC, TL_FLAG_ACTORCLIP, cl.leInlineModelList);
	VecToPos(pC, testPos);
	restingLevel = min(restingLevel, Grid_Fall(cl.mapData->map, ACTOR_GET_FIELDSIZE(selActor), testPos));

	/* if grid below intersection level, start a trace from the intersection */
	if (restingLevel < cl_worldlevel->integer) {
		VectorCopy(end, from);
		from[2] -= CURSOR_OFFSET;
		CM_EntTestLineDM(cl.mapTiles, from, stop, end, TL_FLAG_ACTORCLIP, cl.leInlineModelList);
		VecToPos(end, testPos);
		restingLevel = Grid_Fall(cl.mapData->map, ACTOR_GET_FIELDSIZE(selActor), testPos);
	}

	/* test if the selected grid is out of the world */
	if (restingLevel < 0 || restingLevel >= PATHFINDING_HEIGHT)
		return qfalse;

	/* Set truePos- test pos is under the cursor. */
	VectorCopy(testPos, truePos);
	truePos[2] = cl_worldlevel->integer;

	/* Set mousePos to the position that the actor will move to. */
	testPos[2] = restingLevel;
	VectorCopy(testPos, mousePos);

	interactLe = CL_BattlescapeSearchAtGridPos(mousePos, qfalse, selActor);
	if (interactLe != NULL && LE_IsActor(interactLe)) {
		mouseActor = interactLe;
		interactEntity = NULL;
	} else if (selActor != NULL && selActor->clientAction == interactLe) {
		interactEntity = interactLe;
		mouseActor = NULL;
	} else {
		interactEntity = NULL;
		mouseActor = NULL;
	}

	if (interactEntity != NULL) {
		SCR_ChangeCursor(2);
	} else {
		SCR_ChangeCursor(1);
	}

	/* calculate move length */
	if (selActor && !VectorCompare(mousePos, mouseLastPos)) {
		VectorCopy(mousePos, mouseLastPos);
		CL_ActorResetMoveLength(selActor);
	}

	return qtrue;
}

/*
==============================================================
ACTOR GRAPHICS
==============================================================
*/

/**
 * @brief Checks whether a weapon should be added to the entity's hand
 * @param[in] objID The item id that the actor is holding in his hand (@c le->left or @c le->right)
 * @return true if the weapon is a valid item and false if it's a dummy item or the actor has nothing
 * in the given hand
 */
static inline qboolean CL_AddActorWeapon (int objID)
{
	if (objID != NONE) {
		const objDef_t *od = INVSH_GetItemByIDX(objID);
		if (od->isVirtual)
			return qfalse;
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief Adds an actor to the render entities with all it's models and items.
 * @param[in] le The local entity to get the values from
 * @param[in] ent The body entity used in the renderer
 * @sa CL_AddUGV
 * @sa LE_AddToScene
 * @sa CL_ActorAppear
 * @note Called via addfunc for each local entity in every frame
 */
qboolean CL_AddActor (le_t * le, entity_t * ent)
{
	entity_t add;

	if (!cl_showactors->integer)
		return qfalse;

	if (LE_IsStunned(le)) {
		if (!le->ptl)
			le->ptl = CL_ParticleSpawn("stunnedactor", 0, le->origin, NULL, NULL);
	} else if (!LE_IsDead(le)) {
		/* add the weapons to the actor's hands */
		const qboolean addLeftHandWeapon = CL_AddActorWeapon(le->left);
		const qboolean addRightHandWeapon = CL_AddActorWeapon(le->right);
		/* add left hand weapon */
		if (addLeftHandWeapon) {
			OBJZERO(add);

			add.model = cls.modelPool[le->left];
			if (!add.model)
				Com_Error(ERR_DROP, "Actor model for left hand weapon wasn't found!");

			/* point to the body ent which will be added last */
			add.tagent = R_GetFreeEntity() + 2 + addRightHandWeapon;
			add.tagname = "tag_lweapon";

			R_AddEntity(&add);
		}

		/* add right hand weapon */
		if (addRightHandWeapon) {
			OBJZERO(add);

			add.alpha = le->alpha;
			add.model = cls.modelPool[le->right];
			if (!add.model)
				Com_Error(ERR_DROP, "Actor model for right hand weapon wasn't found!");

			/* point to the body ent which will be added last */
			add.tagent = R_GetFreeEntity() + 2;
			add.tagname = "tag_rweapon";

			R_AddEntity(&add);
		}
		if (le->ptl) {
			CL_ParticleFree(le->ptl);
			le->ptl = NULL;
		}
	} else {
		if (le->ptl) {
			CL_ParticleFree(le->ptl);
			le->ptl = NULL;
		}
	}

	/* add head */
	OBJZERO(add);

	add.alpha = le->alpha;
	add.model = le->model2;
	if (!add.model)
		Com_Error(ERR_DROP, "Actor model wasn't found!");
	add.skinnum = le->skinnum;

	/* point to the body ent which will be added last */
	add.tagent = R_GetFreeEntity() + 1;
	add.tagname = "tag_head";

	if (le->team != cls.team)
		add.flags |= RF_IRGOGGLES;

	R_AddEntity(&add);

	/** Add actor special effects.
	 * Only draw blood if the actor is dead or (if stunned) was damaged more than half its maximum HPs. */
	/** @todo Better value for this?	*/
	if (LE_IsStunned(le) && le->HP <= le->maxHP / 2)
		ent->flags |= RF_BLOOD;
	else if (LE_IsDead(le))
		ent->flags |= RF_BLOOD;
	else
		ent->flags |= RF_SHADOW;

	ent->flags |= RF_ACTOR;
	/* actors are highlighted if some other actor uses ir goggles */
	if (le->team != cls.team)
		ent->flags |= RF_IRGOGGLES;

	if (!LE_IsDead(le) && !LE_IsStunned(le)) {
		if (le->selected)
			ent->flags |= RF_SELECTED;
		if (le->team == cls.team) {
			if (le->pnum == cl.pnum)
				ent->flags |= RF_MEMBER;
			if (le->pnum != cl.pnum)
				ent->flags |= RF_ALLIED;
		} else {
			ent->flags |= RF_OPPONENT;
		}
		if (le->team == TEAM_CIVILIAN)
			ent->flags |= RF_NEUTRAL;
	}

	if (ent->flags & RF_BLOOD) {
		const char *deathTextureName;
		assert(le->teamDef != NULL);
		deathTextureName = le->teamDef->deathTextureName;
		ent->texture = R_FindImage(deathTextureName, it_effect);
	}

	return qtrue;
}

/*
==============================================================
TARGETING GRAPHICS
==============================================================
*/

/**
 * @brief Show weapon radius
 * @param[in] center The center of the circle
 * @param[in] radius The radius of the damage circle
 */
static void CL_TargetingRadius (const vec3_t center, const float radius)
{
	ptl_t *particle = CL_ParticleSpawn("circle", 0, center, NULL, NULL);
	if (particle != NULL)
		particle->size[0] = radius;
}

/**
 * @brief Draws line to target.
 * @param[in] fromPos The (grid-) position of the aiming actor.
 * @param[in] fromActorSize The size of the aiming actor (1 for 1x1 or 2 for 2x2).
 * @param[in] toPos The (grid-) position of the target.
 * @sa CL_TargetingGrenade
 * @sa CL_AddTargeting
 * @sa CL_Trace
 * @sa G_ShootSingle
 */
static void CL_TargetingStraight (const pos3_t fromPos, actorSizeEnum_t fromActorSize, const pos3_t toPos)
{
	vec3_t start, end;
	vec3_t dir, mid, temp;
	trace_t tr;
	qboolean crossNo;
	le_t *target = NULL;
	actorSizeEnum_t toActorSize;

	if (!selActor || !selActor->fd)
		return;

	/* search for an actor at target */
	target = CL_BattlescapeSearchAtGridPos(toPos, qtrue, NULL);

	/* Determine the target's size. */
	toActorSize = target
		? target->fieldSize
		: ACTOR_SIZE_NORMAL;

	Grid_PosToVec(cl.mapData->map, fromActorSize, fromPos, start);
	Grid_PosToVec(cl.mapData->map, toActorSize, toPos, end);
	if (mousePosTargettingAlign)
		end[2] -= mousePosTargettingAlign;

	/* calculate direction */
	VectorSubtract(end, start, dir);
	VectorNormalize(dir);

	/* calculate 'out of range point' if there is one */
	if (VectorDistSqr(start, end) > selActor->fd->range * selActor->fd->range) {
		VectorMA(start, selActor->fd->range, dir, mid);
		crossNo = qtrue;
	} else {
		VectorCopy(end, mid);
		crossNo = qfalse;
	}

	VectorMA(start, UNIT_SIZE * 1.4, dir, temp);
	/* switch up to top level, this is needed to make sure our trace doesn't go through ceilings ... */
	/** @todo is this really needed for straight targetting? - for grenades, yes, but not for straight no?
	 * cl_worldlevel->integer should be enough here */
	tr = CL_Trace(start, temp, vec3_origin, vec3_origin, selActor, NULL, MASK_SHOT, cl.mapMaxLevel - 1);
	if (tr.le && (tr.le->team == cls.team || LE_IsCivilian(tr.le)) && LE_IsCrouched(tr.le))
		VectorMA(start, UNIT_SIZE * 1.4, dir, temp);
	else
		VectorCopy(start, temp);

	/** @todo is this really needed for straight targetting? - for grenades, yes, but not for straight no?
	 * cl_worldlevel->integer should be enough here */
	tr = CL_Trace(temp, mid, vec3_origin, vec3_origin, selActor, target, MASK_SHOT, cl.mapMaxLevel - 1);

	if (tr.fraction < 1.0) {
		const float d = VectorDist(temp, mid);
		VectorMA(start, tr.fraction * d, dir, mid);
		crossNo = qtrue;
	}

	/* spawn particles */
	CL_ParticleSpawn("inRangeTracer", 0, start, mid, NULL);
	if (crossNo) {
		CL_ParticleSpawn("longRangeTracer", 0, mid, end, NULL);
		CL_ParticleSpawn("cross_no", 0, end, NULL, NULL);
	} else {
		CL_ParticleSpawn("cross", 0, end, NULL, NULL);
	}
}


#define GRENADE_PARTITIONS	20

/**
 * @brief Shows targeting for a grenade.
 * @param[in] fromPos The (grid-) position of the aiming actor.
 * @param[in] fromActorSize The size of the aiming actor (1 for 1x1 or 2 for 2x2).
 * @param[in] toPos The (grid-) position of the target (mousePos or mousePendPos).
 * @sa CL_TargetingStraight
 */
static void CL_TargetingGrenade (const pos3_t fromPos, actorSizeEnum_t fromActorSize, const pos3_t toPos)
{
	vec3_t from, at, cross;
	float vz, dt;
	vec3_t v0, ds, next;
	trace_t tr;
	qboolean obstructed = qfalse;
	int i;
	le_t *target = NULL;
	actorSizeEnum_t toActorSize;

	if (!selActor || !selActor->fd || Vector2Compare(fromPos, toPos))
		return;

	/* search for an actor at target */
	target = CL_BattlescapeSearchAtGridPos(toPos, qtrue, NULL);

	/* Determine the target's size. */
	toActorSize = target
		? target->fieldSize
		: ACTOR_SIZE_NORMAL;

	/* get vectors, paint cross */
	Grid_PosToVec(cl.mapData->map, fromActorSize, fromPos, from);
	Grid_PosToVec(cl.mapData->map, toActorSize, toPos, at);
	from[2] += selActor->fd->shotOrg[1];

	/* prefer to aim grenades at the ground */
	at[2] -= GROUND_DELTA;
	if (mousePosTargettingAlign)
		at[2] -= mousePosTargettingAlign;
	VectorCopy(at, cross);

	/* calculate parabola */
	dt = Com_GrenadeTarget(from, at, selActor->fd->range, selActor->fd->launched, selActor->fd->rolled, v0);
	if (!dt) {
		CL_ParticleSpawn("cross_no", 0, cross, NULL, NULL);
		return;
	}

	dt /= GRENADE_PARTITIONS;
	VectorSubtract(at, from, ds);
	VectorScale(ds, 1.0 / GRENADE_PARTITIONS, ds);
	ds[2] = 0;

	/* paint */
	vz = v0[2];

	for (i = 0; i < GRENADE_PARTITIONS; i++) {
		VectorAdd(from, ds, next);
		next[2] += dt * (vz - 0.5 * GRAVITY * dt);
		vz -= GRAVITY * dt;
		VectorScale(v0, (i + 1.0) / GRENADE_PARTITIONS, at);

		/* trace for obstacles. Switch up to top level, to make sure our trace
		 * doesn't go through ceilings ... */
		tr = CL_Trace(from, next, vec3_origin, vec3_origin, selActor, target, MASK_SHOT, cl.mapMaxLevel - 1);

		/* something was hit */
		if (tr.fraction < 1.0) {
			obstructed = qtrue;
		}

		/* draw particles */
		/** @todo character strength should be used here, too
		 * the stronger the character, the further the throw */
		if (obstructed || VectorLength(at) > selActor->fd->range)
			CL_ParticleSpawn("longRangeTracer", 0, from, next, NULL);
		else
			CL_ParticleSpawn("inRangeTracer", 0, from, next, NULL);
		VectorCopy(next, from);
	}
	/* draw targeting cross */
	if (obstructed || VectorLength(at) > selActor->fd->range)
		CL_ParticleSpawn("cross_no", 0, cross, NULL, NULL);
	else
		CL_ParticleSpawn("cross", 0, cross, NULL, NULL);

	if (selActor->fd->splrad > 0.0) {
		Grid_PosToVec(cl.mapData->map, toActorSize, toPos, at);
		CL_TargetingRadius(at, selActor->fd->splrad);
	}
}

/**
 * @brief field marker box
 * @sa ModelOffset
 */
static const vec3_t boxSize = { BOX_DELTA_WIDTH, BOX_DELTA_LENGTH, BOX_DELTA_HEIGHT };
#define BoxSize(i,source,target) (target[0]=i*source[0]+((i-1)*UNIT_SIZE),target[1]=i*source[1]+((i-1)*UNIT_SIZE),target[2]=source[2])
#define BoxOffset(i, target) (target[0]=(i-1)*(UNIT_SIZE+BOX_DELTA_WIDTH), target[1]=(i-1)*(UNIT_SIZE+BOX_DELTA_LENGTH), target[2]=0)

/**
 * @brief create a targeting box at the given position
 * @sa CL_ParseClientinfo
 * @sa CL_TraceMove
 */
static void CL_AddTargetingBox (pos3_t pos, qboolean pendBox)
{
	entity_t ent;
	vec3_t realBoxSize;
	vec3_t cursorOffset;

	if (!cl_showactors->integer)
		return;

	OBJZERO(ent);
	ent.flags = RF_BOX;

	Grid_PosToVec(cl.mapData->map, ACTOR_GET_FIELDSIZE(selActor), pos, ent.origin);

	/* Paint the green box if move is possible ...
	 * OR paint a dark blue one if move is impossible or the
	 * soldier does not have enough TimeUnits left. */
	if (selActor && selActor->actorMoveLength < ROUTING_NOT_REACHABLE
	 && selActor->actorMoveLength <= CL_ActorUsableTUs(selActor))
		VectorSet(ent.color, 0, 1, 0); /* Green */
	else
		VectorSet(ent.color, 0.6, 0.68, 1); /* Light Blue */

	VectorAdd(ent.origin, boxSize, ent.oldorigin);

	/* color */
	if (mouseActor && !mouseActor->selected) {
		ent.alpha = 0.6 + 0.2 * sin((float) cl.time / 80);
		/* Paint the box red if the soldiers under the cursor is
		 * not in our team and no civilian either. */
		if (mouseActor->team != cls.team) {
			switch (mouseActor->team) {
			case TEAM_CIVILIAN:
				/* Civilians are yellow */
				VectorSet(ent.color, 1, 1, 0); /* Yellow */
				break;
			default:
				if (LE_IsAlien(mouseActor)) {
					if (GAME_TeamIsKnown(mouseActor->teamDef))
						UI_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, _(mouseActor->teamDef->name));
					else
						UI_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, _("Unknown alien race"));
				} else {
					/* multiplayer names */
					/* see CL_ParseClientinfo */
					UI_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, CL_PlayerGetName(mouseActor->pnum));
				}
				/* Aliens (and players not in our team [multiplayer]) are red */
				VectorSet(ent.color, 1, 0, 0); /* Red */
				break;
			}
		} else {
			/* coop multiplayer games */
			if (mouseActor->pnum != cl.pnum) {
				UI_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, CL_PlayerGetName(mouseActor->pnum));
			} else {
				/* we know the names of our own actors */
				character_t* chr = CL_ActorGetChr(mouseActor);
				assert(chr);
				UI_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, chr->name);
			}
			/* Paint a light blue box if on our team */
			VectorSet(ent.color, 0.2, 0.3, 1); /* Light Blue */
		}

		if (selActor) {
			BoxOffset(selActor->fieldSize, cursorOffset);
			VectorAdd(ent.oldorigin, cursorOffset, ent.oldorigin);
			VectorAdd(ent.origin, cursorOffset, ent.origin);
			BoxSize(selActor->fieldSize, boxSize, realBoxSize);
			VectorSubtract(ent.origin, realBoxSize, ent.origin);
		}
	} else {
		if (selActor) {
			BoxOffset(selActor->fieldSize, cursorOffset);
			VectorAdd(ent.oldorigin, cursorOffset, ent.oldorigin);
			VectorAdd(ent.origin, cursorOffset, ent.origin);

			BoxSize(selActor->fieldSize, boxSize, realBoxSize);
			VectorSubtract(ent.origin, realBoxSize, ent.origin);
		} else {
			VectorSubtract(ent.origin, boxSize, ent.origin);
		}
		ent.alpha = 0.3;
	}

	/* if pendBox is true then ignore all the previous color considerations and use cyan */
	if (pendBox) {
		VectorSet(ent.color, 0, 1, 1); /* Cyan */
		ent.alpha = 0.15;
	}

	/* add it */
	R_AddEntity(&ent);
}

/**
 * @brief Targets to the ground when holding the assigned button
 * @sa mousePosTargettingAlign
 */
void CL_ActorTargetAlign_f (void)
{
	int align = GROUND_DELTA;
	static int currentPos = 0;

	/* no firedef selected */
	if (!selActor || !selActor->fd)
		return;
	if (!CL_ActorFireModeActivated(selActor->actorMode))
		return;

	/* user defined height align */
	if (Cmd_Argc() == 2) {
		align = atoi(Cmd_Argv(1));
	} else {
		switch (currentPos) {
		case 0:
			if (selActor->fd->gravity)
				align = -align;
			currentPos = 1; /* next level */
			break;
		case 1:
			/* only allow to align to lower z level if the actor is
			 * standing at a higher z-level */
			if (selActor->fd->gravity)
				align = -(2 * align);
			else
				align = -align;
			currentPos = 2;
			break;
		case 2:
			/* the static var is not reseted on weaponswitch or actorswitch */
			if (selActor->fd->gravity) {
				align = 0;
				currentPos = 0; /* next level */
			} else {
				align = -(2 * align);
				currentPos = 3; /* next level */
			}
			break;
		case 3:
			align = 0;
			currentPos = 0; /* back to start */
			break;
		}
	}
	mousePosTargettingAlign = align;
}

/**
 * @brief Adds a target cursor when we render the world.
 * @sa CL_TargetingStraight
 * @sa CL_TargetingGrenade
 * @sa CL_AddTargetingBox
 * @sa CL_TraceMove
 * @sa CL_ViewRender
 * Draws the tracer (red, yellow, green box) on the grid
 */
void CL_AddTargeting (void)
{
	if (IN_GetMouseSpace() != MS_WORLD || !selActor)
		return;

	switch (selActor->actorMode) {
	case M_MOVE:
	case M_PEND_MOVE:
		/* Don't display the cursor if it's above the currently selected level.
		 * The 2nd part of the if is an attempt to display it anyway when we eg. climb a hill.
		 * But there are too many situations inside buildings that match the criteria (eg. actorclip around chair).
		 * So disabled for now.*/
		if (mousePos[2] > cl_worldlevel->integer/* && !RT_AllCellsBelowAreFilled(cl.mapData->map, fieldSize, pos)*/)
			return;

		/* Display Move-cursor. */
		CL_AddTargetingBox(mousePos, qfalse);

		if (selActor->actorMode == M_PEND_MOVE) {
			/* Also display a box for the pending move if we have one. */
			CL_AddTargetingBox(selActor->mousePendPos, qtrue);
			if (!CL_ActorTraceMove(selActor->mousePendPos))
				CL_ActorSetMode(selActor, M_MOVE);
		}
		break;
	case M_FIRE_R:
	case M_FIRE_L:
		if (!selActor->fd)
			return;

		if (!selActor->fd->gravity)
			CL_TargetingStraight(selActor->pos, selActor->fieldSize, mousePos);
		else
			CL_TargetingGrenade(selActor->pos, selActor->fieldSize, mousePos);
		break;
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		if (!selActor->fd)
			return;

		/* Draw cursor at mousepointer */
		CL_AddTargetingBox(mousePos, qfalse);

		/* Draw (pending) Cursor at target */
		CL_AddTargetingBox(selActor->mousePendPos, qtrue);

		if (!selActor->fd->gravity)
			CL_TargetingStraight(selActor->pos, selActor->fieldSize, selActor->mousePendPos);
		else
			CL_TargetingGrenade(selActor->pos, selActor->fieldSize, selActor->mousePendPos);
		break;
	default:
		break;
	}
}

static const vec3_t boxShift = { PLAYER_WIDTH, PLAYER_WIDTH, UNIT_HEIGHT / 2 - DIST_EPSILON };

/**
 * @brief create a targeting box at the given position
 * @sa CL_ParseClientinfo
 */
static qboolean CL_AddPathingBox (pos3_t pos, qboolean addUnreachableCells)
{
	entity_t ent;
	int height; /* The total opening size */
	int base; /* The floor relative to this cell */

	const byte crouchingState = LE_IsCrouched(selActor) ? 1 : 0;
	const int TUneed = Grid_MoveLength(&cl.pathMap, pos, crouchingState, qfalse);
	const int TUhave = CL_ActorUsableTUs(selActor);
	if (!addUnreachableCells && TUhave < TUneed)
		return qfalse;

	OBJZERO(ent);
	ent.flags = RF_PATH;

	Grid_PosToVec(cl.mapData->map, ACTOR_GET_FIELDSIZE(selActor), pos, ent.origin);
	VectorSubtract(ent.origin, boxShift, ent.origin);

	base = Grid_Floor(cl.mapData->map, ACTOR_GET_FIELDSIZE(selActor), pos);

	/* Paint the box green if it is reachable,
	 * yellow if it can be entered but is too far,
	 * or red if it cannot be entered ever. */
	if (base < -QuantToModel(PATHFINDING_MAX_FALL)) {
		VectorSet(ent.color, 0.0, 0.0, 0.0); /* Can't enter - black */
	} else {
		/* Can reach - green
		 * Passable but unreachable - yellow
		 * Not passable - red */
		VectorSet(ent.color, (TUneed > TUhave), (TUneed != ROUTING_NOT_REACHABLE), 0);
	}

	/* Set the box height to the ceiling value of the cell. */
	height = 2 + min(TUneed * (UNIT_HEIGHT - 2) / ROUTING_NOT_REACHABLE, 16);
	ent.oldorigin[2] = height;
	ent.oldorigin[0] = TUneed;
	ent.oldorigin[1] = TUhave;

	ent.alpha = 0.25;

	/* add it */
	R_AddEntity(&ent);
	return qtrue;
}

/**
 * @brief Adds a pathing marker to the current floor when we render the world.
 * @sa CL_ViewRender
 * Draws the tracer (red, yellow, green box) on the grid
 */
void CL_AddPathing (void)
{
	pos3_t pos;

	if (selActor == NULL) {
		return;
	}

	pos[2] = cl_worldlevel->integer;
	for (pos[1] = max(mousePos[1] - 8, 0); pos[1] <= min(mousePos[1] + 8, PATHFINDING_WIDTH - 1); pos[1]++) {
		for (pos[0] = max(mousePos[0] - 8, 0); pos[0] <= min(mousePos[0] + 8, PATHFINDING_WIDTH - 1); pos[0]++) {
			CL_AddPathingBox(pos, qtrue);
		}
	}
}

/**
 * @brief Adds an actor pathing marker to the current floor when we render the world.
 * @sa CL_ViewRender
 * Draws the tracer (red, yellow, green box) on the grid
 */
void CL_AddActorPathing (void)
{
	int x, y;
	pos3_t pos;
	int i = 0;

	if (selActor == NULL) {
		return;
	}

	pos[2] = cl_worldlevel->integer;
	for (y = 0; y <= PATHFINDING_WIDTH; y++) {
		for (x = 0; x <= PATHFINDING_WIDTH; x++) {
			pos[0] = (pos_t)x;
			pos[1] = (pos_t)y;
			i += CL_AddPathingBox(pos, qfalse);
			if (i > 1024)
				return;
		}
	}
}

/**
 * @brief Plays various sounds on actor action.
 * @param[in] le The actor
 * @param[in] soundType Type of action (among actorSound_t) for which we need a sound.
 */
void CL_ActorPlaySound (const le_t *le, actorSound_t soundType)
{
	const char *actorSound = Com_GetActorSound(le->teamDef, le->gender, soundType);
	if (actorSound) {
		if (S_LoadAndPlaySample(actorSound, le->origin, SOUND_ATTN_IDLE, SND_VOLUME_DEFAULT)) {
			Com_DPrintf(DEBUG_SOUND|DEBUG_CLIENT, "CL_PlayActorSound: ActorSound: '%s'\n", actorSound);
		}
	}
}

/**
 * @brief create an arrow between from and to with the specified color ratios
 */
static void CL_AddArrow (vec3_t from, vec3_t to, float red, float green, float blue)
{
	entity_t ent;

	/* Com_Printf("Adding arrow (%f, %f, %f) to (%f, %f, %f).\n", from[0], from[1], from[2], to[0], to[1], to[2]); */

	OBJZERO(ent);
	ent.flags = RF_ARROW;
	VectorCopy(from, ent.origin);
	VectorCopy(to, ent.oldorigin);
	VectorSet(ent.color, red, green, blue);

	ent.alpha = 0.25;

	/* add it */
	R_AddEntity(&ent);
}

/**
 * @brief Useful for debugging pathfinding
 */
void CL_DisplayFloorArrows (void)
{
	vec3_t base, start;

	Grid_PosToVec(cl.mapData->map, ACTOR_GET_FIELDSIZE(selActor), truePos, base);
	VectorCopy(base, start);
	base[2] -= QUANT;
	start[2] += QUANT;
	CL_AddArrow(base, start, 0.0, 0.0, 0.0);
}

/**
 * @brief Useful for debugging pathfinding
 */
void CL_DisplayObstructionArrows (void)
{
	vec3_t base, start;

	Grid_PosToVec(cl.mapData->map, ACTOR_GET_FIELDSIZE(selActor), truePos, base);
	VectorCopy(base, start);
	CL_AddArrow(base, start, 0.0, 0.0, 0.0);
}

#ifdef DEBUG
/**
 * @brief Triggers @c Grid_MoveMark in every direction at the current truePos.
 */
static void CL_DumpMoveMark_f (void)
{
	const byte crouchingState = selActor ? (LE_IsCrouched(selActor) ? 1 : 0) : 0;
	const int temp = developer->integer;

	if (!selActor)
		return;

	developer->integer |= DEBUG_PATHING;

	CL_BuildForbiddenList();
	Grid_MoveCalc(cl.mapData->map, ACTOR_GET_FIELDSIZE(selActor), &cl.pathMap, truePos, crouchingState, MAX_ROUTE, forbiddenList, forbiddenListLength);

	developer->integer ^= DEBUG_PATHING;

	CL_ActorConditionalMoveCalc(selActor);
	developer->integer = temp;
}

/**
 * @brief Shows a table of the TUs that would be used by the current actor to move
 * relative to its current location
 */
static void CL_DumpTUs_f (void)
{
	int x, y, crouchingState;
	pos3_t pos, loc;

	if (!selActor)
		return;

	crouchingState = LE_IsCrouched(selActor) ? 1 : 0;
	VectorCopy(selActor->pos, pos);

	Com_Printf("TUs around (%i, %i, %i).\n", pos[0], pos[1], pos[2]);

	for (y = max(0, pos[1] - 8); y <= min(PATHFINDING_WIDTH, pos[1] + 8); y++) {
		for (x = max(0, pos[0] - 8); x <= min(PATHFINDING_WIDTH, pos[0] + 8); x++) {
			VectorSet(loc, x, y, pos[2]);
			Com_Printf("%3i ", Grid_MoveLength(&cl.pathMap, loc, crouchingState, qfalse));
		}
		Com_Printf("\n");
	}
	Com_Printf("TUs at (%i, %i, %i) = %i\n", pos[0], pos[1], pos[2], Grid_MoveLength(&cl.pathMap, pos, crouchingState, qfalse));
}

/**
 * @brief display pathfinding info to the console. Also useful to
 * directly use the debugger on some vital pathfinding functions.
 * Will probably be removed for the release.
 */
static void CL_DebugPathDisplay (actorSizeEnum_t actorSize, int x, int y, int z)
{
	Com_Printf("data at cursor XYZ(%i, %i, %i) Floor(%i) Ceiling(%i)\n", x, y, z,
		RT_FLOOR(cl.mapData->map, actorSize, x, y, z),
		RT_CEILING(cl.mapData->map, actorSize, x, y, z) );
	Com_Printf("connections ortho: (PX=%i, NX=%i, PY=%i, NY=%i))\n",
		RT_CONN_PX(cl.mapData->map, actorSize, x, y, z),		/* dir = 0 */
		RT_CONN_NX(cl.mapData->map, actorSize, x, y, z),		/* 1 */
		RT_CONN_PY(cl.mapData->map, actorSize, x, y, z),		/* 2 */
		RT_CONN_NY(cl.mapData->map, actorSize, x, y, z) );	/* 3 */
	Com_Printf("connections diago: (PX_PY=%i, NX_NY=%i, NX_PY=%i, PX_NY=%i))\n",
		RT_CONN_PX_PY(cl.mapData->map, actorSize, x, y, z),	/* dir = 4 */
		RT_CONN_NX_NY(cl.mapData->map, actorSize, x, y, z),	/* 5 */
		RT_CONN_NX_PY(cl.mapData->map, actorSize, x, y, z),	/* 6 */
		RT_CONN_PX_NY(cl.mapData->map, actorSize, x, y, z) );	/* 7 */
	Com_Printf("stepup ortho: (PX=%i, NX=%i, PY=%i, NY=%i))\n",
		RT_STEPUP_PX(cl.mapData->map, actorSize, x, y, z),		/* dir = 0 */
		RT_STEPUP_NX(cl.mapData->map, actorSize, x, y, z),		/* 1 */
		RT_STEPUP_PY(cl.mapData->map, actorSize, x, y, z),		/* 2 */
		RT_STEPUP_NY(cl.mapData->map, actorSize, x, y, z) );		/* 3 */
}

static void CL_DebugPath_f (void)
{
	const actorSizeEnum_t actorSize = ACTOR_SIZE_NORMAL;
	const pos_t x = mousePos[0];
	const pos_t y = mousePos[1];
	const pos_t z = mousePos[2];
	int dir;
	dir = 3;

	if (IN_GetMouseSpace() != MS_WORLD)
		return;

	RT_DebugSpecial(cl.mapTiles, cl.mapData->map, actorSize, x, y, dir, cl.leInlineModelList);

#if 0
	{
	pos3_t boxmin = {118,119,0};
	pos3_t boxmax = {130,122,1};
	Grid_RecalcBoxRouting(cl.mapTiles, cl.mapData->map, boxmin, boxmax, cl.leInlineModelList);
	}
#endif

	CL_DebugPathDisplay(actorSize, x, y, z);

}
#endif


/**
 * @brief Switch to the next living soldier
 */
static void CL_ActorNext_f (void)
{
	if (CL_BattlescapeRunning()) {
		CL_ActorSelectNext();
	}
}

/**
 * @brief Switch to the previous living soldier
 */
static void CL_ActorPrev_f (void)
{
	if (CL_BattlescapeRunning()) {
		CL_ActorSelectPrev();
	}
}

/**
 * @brief Selects a soldier while we are on battlescape
 */
static void CL_ActorSelect_f (void)
{
	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	/* check whether we are connected (tactical mission) */
	if (CL_BattlescapeRunning()) {
		const int num = atoi(Cmd_Argv(1));
		CL_ActorSelectList(num);
	}
}

/**
 * @brief Update the skin of the current soldier
 */
static void CL_ActorUpdate_f (void)
{
	const int num = cl_selected->integer;

	/* We are in the base or multiplayer inventory */
	if (num < chrDisplayList.num) {
		const character_t *chr = chrDisplayList.chr[num];
		CL_UpdateCharacterValues(chr, "mn_");
	}
}

/**
 * @sa G_ActorVis
 * @param[in] le The local entity to do the check for
 * @param[in] check The local entity to check the visibility for
 * @return @c true if the given edict is visible from the given world coordinate, @c false otherwise.
 */
static qboolean CL_ActorVis (const le_t *le, const le_t *check)
{
	vec3_t test, dir;
	float delta;
	int i;
	vec3_t from;

	VectorCopy(le->origin, from);

	/* start on eye height */
	VectorCopy(check->origin, test);
	if (LE_IsDead(check)) {
		test[2] += PLAYER_DEAD;
		delta = 0;
	} else if (LE_IsCrouched(check)) {
		test[2] += PLAYER_CROUCH - 2;
		delta = (PLAYER_CROUCH - PLAYER_MIN) / 2 - 2;
	} else {
		test[2] += PLAYER_STAND;
		delta = (PLAYER_STAND - PLAYER_MIN) / 2 - 2;
	}

	/* side shifting -> better checks */
	dir[0] = from[1] - check->origin[1];
	dir[1] = check->origin[0] - from[0];
	dir[2] = 0;
	VectorNormalize(dir);
	VectorMA(test, -7, dir, test);

	/* do 3 tests */
	for (i = 0; i < 3; i++) {
		const trace_t tr = CL_Trace(from, test, vec3_origin, vec3_origin, le, NULL, MASK_SOLID, cl_worldlevel->integer);
		/* trace didn't reach the target - something was hit before */
		if (tr.fraction < 1.0) {
			/* look further down or stop */
			if (!delta)
				return qfalse;
			VectorMA(test, 7, dir, test);
			test[2] -= delta;
			continue;
		}

		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Cycles between visible (to selected actor) aliens.
 * @sa CL_NextAlien_f
 */
static void CL_NextAlienVisibleFromActor_f (void)
{
	static int lastAlien = 0;
	int i;

	if (!selActor)
		return;

	if (lastAlien >= cl.numLEs)
		lastAlien = 0;

	i = lastAlien;
	do {
		const le_t *le;
		if (++i >= cl.numLEs)
			i = 0;
		le = &cl.LEs[i];
		if (le->inuse && LE_IsLivingAndVisibleActor(le) && le->team != cls.team
		 && !LE_IsCivilian(le)) {
			if (CL_ActorVis(selActor, le)) {
				lastAlien = i;
				CL_ViewCenterAtGridPosition(le->pos);
				CL_ParticleSpawn("fadeTracer", 0, selActor->origin, le->origin, NULL);
				return;
			}
		}
	} while (i != lastAlien);
}

/**
 * @brief Cycles between visible aliens
 * @sa CL_NextAlienVisibleFromActor_f
 */
static void CL_NextAlien_f (void)
{
	int lastAlien;
	int i;

	if (cl.numLEs <= 0)
		return;

	lastAlien = max(0, min(cl.numLEs - 1, Cvar_GetValue("ui_lastalien")));

	i = lastAlien;
	do {
		const le_t *le;
		if (++i >= cl.numLEs)
			i = 0;
		le = &cl.LEs[i];
		if (le->inuse && LE_IsLivingAndVisibleActor(le) && le->team != cls.team
		 && le->team != TEAM_CIVILIAN) {
			lastAlien = i;
			CL_ViewCenterAtGridPosition(le->pos);
			Cvar_SetValue("ui_lastalien", lastAlien);
			return;
		}
	} while (i != lastAlien);
}

/**
 * @brief Cycles between visible aliens in reverse direction
 * @sa CL_NextAlienVisibleFromActor_f
 */
static void CL_PrevAlien_f (void)
{
	int lastAlien;
	int i;

	if (cl.numLEs <= 0)
		return;

	lastAlien = max(0, min(cl.numLEs - 1, Cvar_GetValue("ui_lastalien")));

	i = lastAlien;
	do {
		const le_t *le;
		if (--i < 0)
			i = cl.numLEs - 1;
		le = &cl.LEs[i];
		if (le->inuse && LE_IsLivingAndVisibleActor(le) && le->team != cls.team
		 && le->team != TEAM_CIVILIAN) {
			lastAlien = i;
			CL_ViewCenterAtGridPosition(le->pos);
			Cvar_SetValue("ui_lastalien", lastAlien);
			return;
		}
	} while (i != lastAlien);
}

/**
 * Performs pending actions for the given actor
 * @param le The actor that should perform the pending actions
 */
static void CL_ActorConfirmAction (le_t *le)
{
	if (le->team != cl.actTeam)
		return;

	/* might be a friendly player controlled actor */
	if (le->pnum != cl.pnum)
		return;

	switch (le->actorMode) {
	case M_PEND_MOVE:
		CL_ActorStartMove(le, le->mousePendPos);
		break;
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		CL_ActorShoot(le, le->mousePendPos);
		break;
	default:
		break;
	}
}

/**
 * @brief Executes "pending" actions such as walking and firing.
 * @note Manually triggered by the player when hitting the "confirm" button.
 * @note When triggering this twice in 1000ms all pending actions are performed, otherwise only
 * the current selected actor is handled.
 */
static void CL_ActorConfirmAction_f (void)
{
	static int time = 0;

	if (time - cl.time < 1000) {
		le_t *le = NULL;
		while ((le = LE_GetNextInUse(le))) {
			if (LE_IsLivingActor(le) && !LE_IsStunned(le) && le->team == cls.team)
				CL_ActorConfirmAction(le);
		}
	} else {
		time = cl.time;
		if (!selActor)
			return;
		CL_ActorConfirmAction(selActor);
	}
}

void ACTOR_InitStartup (void)
{
	cl_reactionleftover = Cvar_Get("cl_reactionleftover", "0", CVAR_USERINFO | CVAR_ARCHIVE, "Minimum TUs left over by reaction fire.");
	cl_autostand = Cvar_Get("cl_autostand","1", CVAR_USERINFO | CVAR_ARCHIVE, "Prevent accidental wasting of TUs by allowing the actor to automatically stand up before starting long walks.");
	confirm_actions = Cvar_Get("confirm_actions", "0", CVAR_ARCHIVE, "Confirm all actions in tactical mode");
	cl_showactors = Cvar_Get("cl_showactors", "1", 0, "Show actors on the battlefield");
	Cmd_AddCommand("actor_next", CL_ActorNext_f, N_("Toggle to next living actor"));
	Cmd_AddCommand("actor_prev", CL_ActorPrev_f, N_("Toggle to previous living actor"));
	Cmd_AddCommand("actor_select", CL_ActorSelect_f, N_("Select an actor from list"));
	Cmd_AddCommand("actor_updatecurrent", CL_ActorUpdate_f, N_("Update an actor"));
	Cmd_AddCommand("actor_standcrouch", CL_ActorStandCrouch_f, N_("Toggle stand/crouch."));
	Cmd_AddCommand("actor_useheadgear", CL_ActorUseHeadgear_f, N_("Toggle the headgear"));
	Cmd_AddCommand("actor_use", CL_ActorUse_f, N_("Use"));
	Cmd_AddCommand("actor_confirmaction", CL_ActorConfirmAction_f, N_("Confirm the current action"));
	Cmd_AddCommand("actor_nextalien", CL_NextAlienVisibleFromActor_f, N_("Toggle to the next alien in sight of the selected actor."));

	Cmd_AddCommand("nextalien", CL_NextAlien_f, N_("Toggle camera to the next visible alien."));
	Cmd_AddCommand("prevalien", CL_PrevAlien_f, N_("Toggle camera to the previous visible alien."));

#ifdef DEBUG
	Cmd_AddCommand("debug_path", CL_DebugPath_f, "Display routing data for current mouse position.");
	Cmd_AddCommand("debug_drawblocked", CL_DisplayBlockedPaths_f, "Draw a marker for all blocked map-positions.");
	Cmd_AddCommand("debug_movemark", CL_DumpMoveMark_f, "Trigger Grid_MoveMark in every direction at the current truePos.");
	Cmd_AddCommand("debug_tus", CL_DumpTUs_f, "Show a table of the TUs that would be used by the current actor to move relative to his current location.");
	Cmd_AddCommand("debug_actorinvlist", NULL, "Show the inventory list of all actors.");
#endif /* DEBUG */
}
