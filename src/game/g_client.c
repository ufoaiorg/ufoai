/**
 * @file g_client.c
 * @brief Main part of the game logic.
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

#include "g_local.h"

static chrScoreMission_t scoreMission[MAX_EDICTS];
static int scoreMissionNum = 0;

/**
 * @brief Generates the player bit mask for a given team
 * @param[in] team The team to create the player bit mask for
 * @note E.g. multiplayer team play can have more than one human player on the
 * same team.
 */
unsigned int G_TeamToPM (int team)
{
	const player_t *p;
	unsigned int playerMask, i;

	playerMask = 0;

	/* don't handle the ai players, here */
	for (i = 0, p = game.players; i < game.sv_maxplayersperteam; i++, p++)
		if (p->inuse && team == p->pers.team)
			playerMask |= (1 << i);

	return playerMask;
}

/**
 * @brief Converts vis mask to player mask
 * @param[in] vis_mask The visibility bit mask (contains the team numbers) that
 * is converted to a player mask
 * @return Returns a playermask for all the teams of the connected players that
 * are marked in the given @c vis_mask.
 */
unsigned int G_VisToPM (unsigned int vis_mask)
{
	const player_t *p;
	unsigned int playerMask, i;

	playerMask = 0;

	/* don't handle the ai players, here */
	for (i = 0, p = game.players; i < game.sv_maxplayersperteam; i++, p++)
		if (p->inuse && (vis_mask & (1 << p->pers.team)))
			playerMask |= (1 << i);

	return playerMask;
}

/**
 * Send messages to human players
 * @param player A player (AI players are ignored here)
 */
void G_PlayerPrintf (const player_t *player, int printLevel, const char *fmt, ...)
{
	va_list ap;

	/* there is no client for an AI controlled player on the server where we
	 * could send the message to */
	if (player->pers.ai)
		return;

	va_start(ap, fmt);
	gi.PlayerPrintf(player, printLevel, fmt, ap);
	va_end(ap);
}

/**
 * @brief Network function to update the time units (TUs) for each team-member.
 * @param[in] team The index of the team to update the values for.
 * @sa G_SendStats
 */
void G_GiveTimeUnits (int team)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent) && ent->team == team) {
			ent->state &= ~STATE_DAZED;
			ent->TU = GET_TU(ent->chr.score.skills[ABILITY_SPEED]);
			G_SendStats(ent);
		}
}

void G_SendState (unsigned int playerMask, const edict_t *ent)
{
	gi.AddEvent(playerMask & G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state);

	gi.AddEvent(playerMask & ~G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state & STATE_PUBLIC);
}

/**
 * Send a particle spawn event to the client
 * @param player_mask The clients that should see the particle
 * @param ent The particle to spawn
 */
static void G_SendParticle (unsigned int playerMask, edict_t *ent)
{
	gi.AddEvent(playerMask, EV_SPAWN_PARTICLE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->spawnflags);
	gi.WriteString(ent->particle);
}

static void G_EdictAppear (unsigned int playerMask, edict_t *ent)
{
	gi.AddEvent(playerMask, EV_ENT_APPEAR);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->type);
	gi.WriteGPos(ent->pos);
}

/**
 * @brief Send the appear or perish event the the affected clients
 * @param[in] player_mask These are the affected players or clients
 * In case of e.g. teamplay there can be more than one client affected - thus
 * this is a player mask
 * @param[in] appear Is this event about an appearing actor (or a perishing one)
 * @param[in] check The edict we are talking about (that appears or perishes)
 * @param[in] ent The edict that was responsible for letting the check edict appear
 * or perish. Might be @c NULL.
 * @sa CL_ActorAppear
 */
void G_AppearPerishEvent (unsigned int playerMask, qboolean appear, edict_t *check, edict_t *ent)
{
	/* test for pointless player mask */
	if (!playerMask)
		return;

	if (appear) {
		/* appear */
		switch (check->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			/* parsed in CL_ActorAppear */
			gi.AddEvent(playerMask, EV_ACTOR_APPEAR);
			gi.WriteShort(check->number);
			gi.WriteShort(ent && ent->number > 0 ? ent->number : SKIP_LOCAL_ENTITY);
			gi.WriteByte(check->team);
			gi.WriteByte(check->chr.teamDef ? check->chr.teamDef->idx : NONE);
			gi.WriteByte(check->chr.gender);
			gi.WriteByte(check->pnum);
			gi.WriteGPos(check->pos);
			gi.WriteByte(check->dir);
			if (RIGHT(check)) {
				gi.WriteShort(RIGHT(check)->item.t->idx);
			} else {
				gi.WriteShort(NONE);
			}

			if (LEFT(check)) {
				gi.WriteShort(LEFT(check)->item.t->idx);
			} else {
				gi.WriteShort(NONE);
			}

			gi.WriteShort(check->body);
			gi.WriteShort(check->head);
			gi.WriteByte(check->chr.skin);
			gi.WriteShort(check->state & STATE_PUBLIC);
			gi.WriteByte(check->fieldSize);
			gi.WriteByte(GET_TU(check->chr.score.skills[ABILITY_SPEED]));
			gi.WriteByte(min(MAX_SKILL, GET_MORALE(check->chr.score.skills[ABILITY_MIND])));
			gi.WriteShort(check->chr.maxHP);

			if (playerMask & G_TeamToPM(check->team)) {
				gi.AddEvent(playerMask & G_TeamToPM(check->team), EV_ACTOR_STATECHANGE);
				gi.WriteShort(check->number);
				gi.WriteShort(check->state);
			}
			G_SendInventory(G_TeamToPM(check->team) & playerMask, check);
			break;

		case ET_ITEM:
			G_EdictAppear(playerMask, check);
			G_SendInventory(playerMask, check);
			break;

		case ET_PARTICLE:
			G_EdictAppear(playerMask, check);
			G_SendParticle(playerMask, check);
			break;

		default:
			if (G_IsVisibleOnBattlefield(check))
				gi.error("Missing edict type %i in G_AppearPerishEvent", check->type);
			break;
		}
	} else if (G_IsVisibleOnBattlefield(check)) {
		/* disappear */
		gi.AddEvent(playerMask, EV_ENT_PERISH);
		gi.WriteShort(check->number);
	}
}

/**
 * @brief This function sends all the actors to the client that are not visible
 * initially - this is needed because an actor can e.g. produce sounds that are
 * send over the net. And the client can only handle them if he knows the
 * @c le_t (local entity) already
 * @note Call this for the first @c G_CheckVis call for every new
 * actor or player
 * @sa G_CheckVis
 * @sa CL_ActorAdd
 */
void G_SendInvisible (player_t* player)
{
	const int team = player->pers.team;

	assert(team != TEAM_NO_ACTIVE);
	if (level.num_alive[team]) {
		int i;
		edict_t* ent;
		/* check visibility */
		for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
			if (ent->inuse && ent->team != team && G_IsActor(ent)) {
				/* not visible for this team - so add the le only */
				if (!G_IsVisibleForTeam(ent, team)) {
					/* parsed in CL_ActorAdd */
					gi.AddEvent(G_PlayerToPM(player), EV_ACTOR_ADD);
					gi.WriteShort(ent->number);
					gi.WriteByte(ent->team);
					gi.WriteByte(ent->chr.teamDef ? ent->chr.teamDef->idx : NONE);
					gi.WriteByte(ent->chr.gender);
					gi.WriteByte(ent->pnum);
					gi.WriteGPos(ent->pos);
					gi.WriteShort(ent->state & STATE_PUBLIC);
					gi.WriteByte(ent->fieldSize);
				}
			}
		}
	}
}

/**
 * @brief Returns the current active team to the server
 * @par If this has the value @c TEAM_NO_ACTIVE there is either no
 * living actor for any player left, or the round wasn't started yet.
 */
int G_GetActiveTeam (void)
{
	return level.activeTeam;
}

/**
 * @brief Checks whether the the requested action is possible
 * @param[in] player Which player (human player) is trying to do the action
 * @param[in] ent Which of his units is trying to do the action.
 * @param[in] TU The time units to check against the ones ent has.
 * @param[in] quiet Don't print the console message if quiet is true.
 * the action with
 */
qboolean G_ActionCheck (player_t *player, edict_t *ent, int TU, qboolean quiet)
{
	int msglevel;

	/* don't check for a player - but maybe a server action */
	if (!player)
		return qtrue;

	/* a generic tester if an action could be possible */
	if (level.activeTeam != player->pers.team) {
		G_PlayerPrintf(player, PRINT_HUD, _("Can't perform action - this isn't your round!\n"));
		return qfalse;
	}

	msglevel = quiet ? PRINT_NONE : PRINT_HUD;

	if (!ent || !ent->inuse) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - object not present!\n"));
		return qfalse;
	}

	if (ent->type != ET_ACTOR && ent->type != ET_ACTOR2x2) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - not an actor!\n"));
		return qfalse;
	}

	if (G_IsStunned(ent)) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - actor is stunned!\n"));
		return qfalse;
	}

	if (G_IsDead(ent)) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - actor is dead!\n"));
		return qfalse;
	}

	if (ent->team != player->pers.team) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - not on same team!\n"));
		return qfalse;
	}

	if (ent->pnum != player->num) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - no control over allied actors!\n"));
		return qfalse;
	}

	if (TU > ent->TU) {
		return qfalse;
	}

	/* could be possible */
	return qtrue;
}

/**
 * @brief Sends the actual actor turn event over the netchannel
 */
static void G_ClientTurn (player_t * player, int num, byte dv)
{
	const int dir = getDVdir(dv);
	edict_t *ent = g_edicts + num;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, TU_TURN, NOISY))
		return;

	/* check if we're already facing that direction */
	if (ent->dir == dir)
		return;

	/* do the turn */
	G_ActorDoTurn(ent, dir);
	ent->TU -= TU_TURN;

	/* send the turn */
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_TURN);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->dir);

	/* send the new TUs */
	G_SendStats(ent);

	/* end the event */
	gi.EndEvents();
}

/**
 * @brief Changes the state of a player/soldier.
 * @param[in,out] player The player who controlls the actor
 * @param[in] num The index of the edict in the global g_edicts array
 * @param[in] reqState The bit-map of the requested state change
 * @param[in] checkaction only activate the events - network stuff is handled in the calling function
 * don't even use the G_ActionCheck function
 * @note Use checkaction true only for e.g. spawning values
 */
void G_ClientStateChange (player_t * player, int num, int reqState, qboolean checkaction)
{
	edict_t *ent;

	ent = g_edicts + num;

	/* Check if any action is possible. */
	if (checkaction && !G_ActionCheck(player, ent, 0, NOISY))
		return;

	if (!reqState)
		return;

	switch (reqState) {
	case STATE_CROUCHED: /* Toggle between crouch/stand. */
		/* Check if player has enough TUs (TU_CROUCH TUs for crouch/uncrouch). */
		if (!checkaction || G_ActionCheck(player, ent, TU_CROUCH, NOISY)) {
			ent->state ^= STATE_CROUCHED;
			ent->TU -= TU_CROUCH;
			/* Link it. */
			if (ent->state & STATE_CROUCHED)
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_CROUCH);
			else
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND);
			gi.LinkEdict(ent);
		}
		break;
	case ~STATE_REACTION: /* Request to turn off reaction fire. */
		if ((ent->state & STATE_REACTION_MANY) || (ent->state & STATE_REACTION_ONCE)) {
			if (ent->state & STATE_SHAKEN) {
				G_PlayerPrintf(player, PRINT_CONSOLE, _("Currently shaken, won't let their guard down.\n"));
			} else {
				/* Turn off reaction fire. */
				ent->state &= ~STATE_REACTION;

				if (player->pers.ai && checkaction)
					gi.error("AI reaction fire is server side only");
			}
		}
		break;
	case STATE_REACTION_MANY: /* Request to turn on multi-reaction fire mode. */
		/* Disable reaction fire. */
		ent->state &= ~STATE_REACTION;

		if (player->pers.ai && checkaction)
			gi.error("AI reaction fire is server side only");

		/* Enable multi reaction fire. */
		ent->state |= STATE_REACTION_MANY;
		break;
	case STATE_REACTION_ONCE: /* Request to turn on single-reaction fire mode. */
		/* Disable reaction fire. */
		ent->state &= ~STATE_REACTION;

		if (player->pers.ai && checkaction)
			gi.error("AI reaction fire is server side only");

		/* Turn on single reaction fire. */
		ent->state |= STATE_REACTION_ONCE;
		break;
	default:
		gi.dprintf("G_ClientStateChange: unknown request %i, ignoring\n", reqState);
		return;
	}

	/* Only activate the events - network stuff is handled in the calling function */
	if (!checkaction)
		return;

	/* Send the state change. */
	G_SendState(G_VisToPM(ent->visflags), ent);

	/* Check if the player appears/perishes, seen from other teams. */
	G_CheckVis(ent, qtrue);

	/* Calc new vis for this player. */
	G_CheckVisTeam(ent->team, NULL, qfalse, ent);

	/* Send the new TUs. */
	G_SendStats(ent);

	/* End the event. */
	gi.EndEvents();
}

/**
 * @brief Returns true if actor can reload weapon
 * @sa AI_ActorThink
 */
qboolean G_ClientCanReload (player_t *player, int entnum, shoot_types_t st)
{
	edict_t *ent;
	invList_t *ic;
	int hand;
	objDef_t *weapon;
	int container;

	ent = g_edicts + entnum;

	/* search for clips and select the one that is available easily */
	hand = st == ST_RIGHT_RELOAD ? gi.csi->idRight : gi.csi->idLeft;

	if (ent->i.c[hand]) {
		weapon = ent->i.c[hand]->item.t;
	} else if (hand == gi.csi->idLeft && ent->i.c[gi.csi->idRight]->item.t->holdTwoHanded) {
		/* Check for two-handed weapon */
		hand = gi.csi->idRight;
		weapon = ent->i.c[hand]->item.t;
	} else
		return qfalse;

	assert(weapon);

	for (container = 0; container < gi.csi->numIDs; container++)
		for (ic = ent->i.c[container]; ic; ic = ic->next)
			if (INVSH_LoadableInWeapon(ic->item.t, weapon))
				return qtrue;
	return qfalse;
}

/**
 * @brief Retrieve or collect weapon from any linked container for the actor
 * @note This function will also collect items from floor containers when the actor
 * is standing on a given point.
 * @sa AI_ActorThink
 */
void G_ClientGetWeaponFromInventory (player_t *player, int entnum, qboolean quiet)
{
	edict_t *ent;
	invList_t *ic;
	invList_t *icFinal;
	invDef_t *hand;
	int tu;
	int container;
	invDef_t *bestContainer;

	ent = g_edicts + entnum;
	/* e.g. bloodspiders are not allowed to carry or collect weapons */
	if (!ent->chr.teamDef->weapons)
		return;

	/* search for weapons and select the one that is available easily */
	tu = 100;
	hand = INVDEF(gi.csi->idRight);
	bestContainer = NULL;
	icFinal = NULL;

	for (container = 0; container < gi.csi->numIDs; container++) {
		if (gi.csi->ids[container].out < tu) {
			/* Once we've found at least one clip, there's no point
			 * searching other containers if it would take longer
			 * to retrieve the ammo from them than the one
			 * we've already found. */
			for (ic = ent->i.c[container]; ic; ic = ic->next) {
				assert(ic->item.t);
				if (ic->item.t->weapon && (ic->item.a > 0 || !ic->item.t->reload)) {
					icFinal = ic;
					bestContainer = INVDEF(container);
					tu = bestContainer->out;
					break;
				}
			}
		}
	}

	/* send request */
	if (bestContainer)
		G_ActorInvMove(entnum, bestContainer, icFinal, hand, 0, 0, qtrue, quiet);
}

/**
 * @brief This function 'uses' the edict. E.g. it opens the door when the player wants it to open
 * @sa PA_USE_DOOR
 * @param[in] player The player is trying to activate the door
 * @param[in,out] actor The actor the player is using to activate the entity
 * @param[in,out] edict The entity that is to be used
 * @todo Do we have to change the trigger position here, too? I don't think this is really needed.
 * @sa CL_ActorUseDoor
 * @sa G_UseEdict
 */
qboolean G_ClientUseEdict (player_t *player, edict_t *actor, edict_t *edict)
{
	/* check whether the actor has sufficient TUs to 'use' this edicts */
	if (!G_ActionCheck(player, actor, edict->TU, qfalse))
		return qfalse;

	if (!G_UseEdict(edict))
		return qfalse;

	/* using a group of edicts only costs TUs once (for the master) */
	actor->TU -= edict->TU;
	/* send the new TUs */
	G_SendStats(actor);

	gi.EndEvents();

	return qtrue;
}

/**
 * @brief The client sent us a message that he did something. We now execute the related fucntion(s) adn notify him if neccessary.
 * @param[in] player The player to execute the action for (the actor belongs to this player)
 * @note a client action will also send the server side edict number to determine the actor
 */
int G_ClientAction (player_t * player)
{
	player_action_t action;
	int num;
	pos3_t pos;
	int i;
	int firemode;
	int from, fx, fy, to, tx, ty;
	int hand, fdIdx, objIdx;
	int resType, resState, resValue;
	edict_t *ent;

	/* read the header */
	action = gi.ReadByte();
	num = gi.ReadShort();

	if (num < 0 || num >= MAX_EDICTS) {
		gi.dprintf("Invalid edict num %i\n", num);
		return action;
	}

	switch (action) {
	case PA_NULL:
		/* do nothing on a null action */
		break;

	case PA_TURN:
		gi.ReadFormat(pa_format[PA_TURN], &i);
		G_ClientTurn(player, num, (byte) i);
		break;

	case PA_MOVE:
		gi.ReadFormat(pa_format[PA_MOVE], &pos);
		G_ClientMove(player, player->pers.team, num, pos, qtrue, NOISY);
		break;

	case PA_STATE:
		gi.ReadFormat(pa_format[PA_STATE], &i);
		G_ClientStateChange(player, num, i, qtrue);
		break;

	case PA_SHOOT:
		gi.ReadFormat(pa_format[PA_SHOOT], &pos, &i, &firemode, &from);
		(void) G_ClientShoot(player, num, pos, i, firemode, NULL, qtrue, from);
		break;

	case PA_INVMOVE:
		gi.ReadFormat(pa_format[PA_INVMOVE], &from, &fx, &fy, &to, &tx, &ty);

		ent = g_edicts + num;

		/* if something was thrown, the floor must be updated even if the actor that is trying to pick
		 * the item up hasn't moved at all */
		G_GetFloorItems(ent);

		if (from < 0 || from >= gi.csi->numIDs || to < 0 || to >= gi.csi->numIDs) {
			gi.dprintf("G_ClientAction: PA_INVMOVE Container index out of range. (from: %i, to: %i)\n", from, to);
		} else {
			invDef_t *fromPtr = INVDEF(from);
			invDef_t *toPtr = INVDEF(to);
			invList_t *fromItem = Com_SearchInInventory(&ent->i, fromPtr, fx, fy);
			if (!fromItem)
				gi.error("Could not find item in inventory of ent %i (type %i) at %i:%i",
						ent->number, ent->type, fx, fy);
			G_ActorInvMove(num, fromPtr, fromItem, toPtr, tx, ty, qtrue, NOISY);
		}
		break;

	case PA_USE_DOOR: {
		edict_t *door;
		edict_t *actor = (g_edicts + num);

		/* read the door the client wants to open */
		gi.ReadFormat(pa_format[PA_USE_DOOR], &i);

		/* get the door edict */
		door = g_edicts + i;

		if (actor->clientAction == door) {
			/* check whether it's part of an edict group but not the master */
			if (door->flags & FL_GROUPSLAVE)
				door = door->groupMaster;

			G_ClientUseEdict(player, actor, door);
		} else
			Com_DPrintf(DEBUG_GAME, "client_action and ent differ: %i - %i\n", actor->clientAction->number,
					door->number);
	}
		break;

	case PA_REACT_SELECT:
		gi.ReadFormat(pa_format[PA_REACT_SELECT], &hand, &fdIdx, &objIdx);
		Com_DPrintf(DEBUG_GAME, "G_ClientAction: entnum:%i hand:%i fd:%i obj:%i\n", num, hand, fdIdx, objIdx);
		ent = g_edicts + num;
		ent->chr.RFmode.hand = hand;
		ent->chr.RFmode.fmIdx = fdIdx;
		ent->chr.RFmode.weapon = &gi.csi->ods[objIdx];
		break;

	case PA_RESERVE_STATE:
		gi.ReadFormat(pa_format[PA_RESERVE_STATE], &resType, &resState, &resValue);

		if (resValue < 0)
			gi.error("G_ClientAction: No sane value received for resValue!  (resType=%i resState=%i resValue=%i)\n",
					resType, resState, resValue);

		ent = g_edicts + num;
		switch (resType) {
		case RES_REACTION:
			ent->chr.reservedTus.reserveReaction = resState;
			ent->chr.reservedTus.reaction = resValue;
			break;
		default:
			gi.error("G_ClientAction: Unknown reservation type (on the server-side)!\n");
		}
		break;

	default:
		gi.error("G_ClientAction: Unknown action!\n");
	}
	return action;
}

/**
 * @brief Sets the teamnum var for this match
 * @param[in] player Pointer to connected player
 * @todo Check whether there are enough free spawnpoints in all cases
 */
static void G_GetTeam (player_t * player)
{
	player_t *p;
	int i, j;
	int playersInGame = 0;

	/* number of currently connected players (no ai players) */
	for (j = 0, p = game.players; j < game.sv_maxplayersperteam; j++, p++)
		if (p->inuse)
			playersInGame++;

	/* player has already a team */
	if (player->pers.team > 0) {
		Com_DPrintf(DEBUG_GAME, "You are already on team %i\n", player->pers.team);
		return;
	}

	/* randomly assign a teamnumber in deathmatch games */
	if (playersInGame <= 1 && sv_maxclients->integer > 1 && !sv_teamplay->integer) {
		int spawnCheck[MAX_TEAMS];
		int spawnSpots = 0;
		int randomSpot;
		/* skip civilian teams */
		for (i = TEAM_PHALANX; i < MAX_TEAMS; i++) {
			spawnCheck[i] = 0;
			/* check whether there are spawnpoints for this team */
			if (level.num_spawnpoints[i])
				spawnCheck[spawnSpots++] = i;
		}
		/* we need at least 2 different team spawnpoints for multiplayer */
		if (spawnSpots <= 1) {
			Com_DPrintf(DEBUG_GAME, "G_GetTeam: Not enough spawn spots in map!\n");
			player->pers.team = TEAM_NO_ACTIVE;
			return;
		}
		/* assign random valid team number */
		randomSpot = (int) (frand() * (spawnSpots - 1) + 0.5);
		player->pers.team = spawnCheck[randomSpot];
		gi.dprintf("You have been randomly assigned to team %i\n", player->pers.team);
		return;
	}

	/* find a team */
	if (sv_maxclients->integer == 1)
		player->pers.team = TEAM_PHALANX;
	else if (sv_teamplay->integer) {
		/* set the team specified in the userinfo */
		gi.dprintf("Get a team for teamplay for %s\n", player->pers.netname);
		i = atoi(Info_ValueForKey(player->pers.userinfo, "cl_teamnum"));
		/* civilians are at team zero */
		if (i > TEAM_CIVILIAN && sv_maxteams->integer >= i) {
			player->pers.team = i;
			gi.BroadcastPrintf(PRINT_CHAT, "serverconsole: %s has chosen team %i\n", player->pers.netname, i);
		} else {
			gi.dprintf("Team %i is not valid - choose a team between 1 and %i\n", i, sv_maxteams->integer);
			player->pers.team = TEAM_DEFAULT;
		}
	} else {
		qboolean teamAvailable;
		/* search team */
		gi.dprintf("Getting a multiplayer team for %s\n", player->pers.netname);
		for (i = TEAM_CIVILIAN + 1; i < MAX_TEAMS; i++) {
			if (level.num_spawnpoints[i]) {
				teamAvailable = qtrue;
				/* check if team is in use (only human controlled players) */
				for (j = 0, p = game.players; j < game.sv_maxplayersperteam; j++, p++)
					if (p->inuse && p->pers.team == i) {
						Com_DPrintf(DEBUG_GAME, "Team %i is already in use\n", i);
						/* team already in use */
						teamAvailable = qfalse;
						break;
					}
				if (teamAvailable)
					break;
			}
		}

		/* set the team */
		if (i < MAX_TEAMS) {
			/* remove ai player */
			for (j = 0, p = game.players + game.sv_maxplayersperteam; j < game.sv_maxplayersperteam; j++, p++)
				if (p->inuse && p->pers.team == i) {
					gi.BroadcastPrintf(PRINT_CONSOLE, "Removing ai player...");
					p->inuse = qfalse;
					break;
				}
			Com_DPrintf(DEBUG_GAME, "Assigning %s to Team %i\n", player->pers.netname, i);
			player->pers.team = i;
		} else {
			gi.dprintf("No free team - disconnecting '%s'\n", player->pers.netname);
			G_ClientDisconnect(player);
		}
	}
}

/**
 * @brief Returns the assigned team number of the player
 */
int G_ClientGetTeamNum (const player_t * player)
{
	assert(player);
	return player->pers.team;
}

/**
 * @brief Returns the preferred team number for the player
 */
int G_ClientGetTeamNumPref (const player_t * player)
{
	assert(player);
	return atoi(Info_ValueForKey(player->pers.userinfo, "cl_teamnum"));
}

/**
 * @brief Tests if all teams are connected for a multiplayer match and if so,
 * randomly assigns the first turn to a team.
 * @sa SVCmd_StartGame_f
 */
static void G_ClientTeamAssign (const player_t * player)
{
	int i, j, teamCount = 1;
	int playerCount = 0;
	int knownTeams[MAX_TEAMS];
	player_t *p;
	knownTeams[0] = player->pers.team;

	/* return with no action if activeTeam already assigned or if in single-player mode */
	if (G_MatchIsRunning() || sv_maxclients->integer == 1)
		return;

	/* count number of currently connected unique teams and players (human controlled players only) */
	for (i = 0, p = game.players; i < game.sv_maxplayersperteam; i++, p++) {
		if (p->inuse && p->pers.team > 0) {
			playerCount++;
			for (j = 0; j < teamCount; j++) {
				if (p->pers.team == knownTeams[j])
					break;
			}
			if (j == teamCount)
				knownTeams[teamCount++] = p->pers.team;
		}
	}

	Com_DPrintf(DEBUG_GAME, "G_ClientTeamAssign: Players in game: %i, Unique teams in game: %i\n", playerCount,
			teamCount);

	/* if all teams/players have joined the game, randomly assign which team gets the first turn */
	if ((sv_teamplay->integer && teamCount >= sv_maxteams->integer) || playerCount >= sv_maxclients->integer) {
		char buffer[MAX_VAR] = "";
		G_PrintStats(va("Starting new game: %s with %i teams", level.mapname, teamCount));
		level.activeTeam = knownTeams[(int) (frand() * (teamCount - 1) + 0.5)];
		for (i = 0, p = game.players; i < game.sv_maxplayersperteam; i++, p++) {
			if (p->inuse) {
				if (p->pers.team == level.activeTeam) {
					Q_strcat(buffer, p->pers.netname, sizeof(buffer));
					Q_strcat(buffer, " ", sizeof(buffer));
				} else
					/* all the others are set to waiting */
					p->ready = qtrue;
				if (p->pers.team)
					G_PrintStats(va("Team %i: %s", p->pers.team, p->pers.netname));
			}
		}
		G_PrintStats(va("Team %i got the first round", level.activeTeam));
		gi.BroadcastPrintf(PRINT_CONSOLE, _("Team %i (%s) will get the first turn.\n"), level.activeTeam, buffer);
	}
}

/**
 * @brief Find valid actor spawn fields for this player.
 * @note Already used spawn-point are not found because ent->type is changed in G_ClientTeamInfo.
 * @param[in] player The player to spawn the actors for.
 * @param[in] spawnType The type of spawn-point so search for (ET_ACTORSPAWN or ET_ACTOR2x2SPAWN)
 * @return A pointer to a found spawn point or NULL if nothing was found or on error.
 */
static edict_t *G_ClientGetFreeSpawnPoint (const player_t * player, int spawnType)
{
	int i;
	edict_t *ent;

	/* Abort for non-spawnpoints */
	assert(spawnType == ET_ACTORSPAWN || spawnType == ET_ACTOR2x2SPAWN);

	if (level.randomSpawn) {
		edict_t *list[MAX_EDICTS];
		int count = 0;
		for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
			if (ent->type == spawnType && player->pers.team == ent->team)
				list[count++] = ent;

		if (count)
			return list[rand() % count];
	} else {
		for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
			if (ent->type == spawnType && player->pers.team == ent->team)
				return ent;
	}

	return NULL;
}

/**
 * @brief Call this if you want to skip some actor netchannel data
 * @note The fieldsize is not skipped
 * @sa G_ClientTeamInfo
 */
static void G_ClientSkipActorInfo (void)
{
	int k, j;

	gi.ReadShort(); /* ucn */
	for (k = 0; k < 4; k++)
		gi.ReadString(); /* name, path, body, head */
	gi.ReadByte(); /* skin */

	gi.ReadShort(); /* HP */
	gi.ReadShort(); /* maxHP */
	gi.ReadByte(); /* teamDef->idx */
	gi.ReadByte(); /* gender */
	gi.ReadByte(); /* STUN */
	gi.ReadByte(); /* morale */

	/** Scores @sa inv_shared.h:chrScoreGlobal_t */
	for (k = 0; k < SKILL_NUM_TYPES + 1; k++)
		gi.ReadLong(); /* score.experience */
	for (k = 0; k < SKILL_NUM_TYPES; k++)
		gi.ReadByte(); /* score.skills */
	for (k = 0; k < SKILL_NUM_TYPES + 1; k++)
		gi.ReadByte(); /* score.initialSkills */
	for (k = 0; k < KILLED_NUM_TYPES; k++)
		gi.ReadShort(); /* score.kills */
	for (k = 0; k < KILLED_NUM_TYPES; k++)
		gi.ReadShort(); /* score.stuns */
	gi.ReadShort(); /* score.assigned missions */

	gi.ReadShort(); /* reservedTus.reserveReaction */

	/* skip inventory */
	j = gi.ReadShort();
	for (k = 0; k < j; k++)
		gi.ReadByte(); /* inventory */
}

/**
 * @brief The client lets the server spawn the actors for a given player by sending their information (models, inventory, etc..) over the network.
 * @param[in] player The player to spawn the actors for.
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa clc_teaminfo
 */
void G_ClientTeamInfo (player_t * player)
{
	int i, k;
	int x, y;
	item_t item;
	const int length = gi.ReadByte(); /* Get the actor amount that the client sent. */

	/* find a team */
	G_GetTeam(player);

	for (i = 0; i < length; i++) {
		/* Search for a spawn point for each entry the client sent
		 * Don't allow spawning of soldiers if:
		 * + the player is not in a valid team
		 * +++ Multiplayer
		 * + the game is already running (activeTeam != -1)
		 * + the sv_maxsoldiersperplayer limit is hit (e.g. the assembled team is bigger than the allowed number of soldiers)
		 * + the team already hit the max allowed amount of soldiers */
		if (player->pers.team != TEAM_NO_ACTIVE && (sv_maxclients->integer == 1 || (!G_MatchIsRunning() && i
				< sv_maxsoldiersperplayer->integer && level.num_spawned[player->pers.team]
				< sv_maxsoldiersperteam->integer))) {
			/* Here the client tells the server the information for the spawned actor(s). */
			edict_t *ent;

			/* Receive fieldsize and get matching spawnpoint. */
			const int dummyFieldSize = gi.ReadByte();
			switch (dummyFieldSize) {
			case ACTOR_SIZE_NORMAL:
				/* Find valid actor spawn fields for this player. */
				ent = G_ClientGetFreeSpawnPoint(player, ET_ACTORSPAWN);
				if (ent) {
					ent->type = ET_ACTOR;
				}
				break;
			case ACTOR_SIZE_2x2:
				/* Find valid actor spawn fields for this player. */
				ent = G_ClientGetFreeSpawnPoint(player, ET_ACTOR2x2SPAWN);
				if (ent) {
					ent->type = ET_ACTOR2x2;
					ent->morale = 100;
				}
				break;
			default:
				gi.error("G_ClientTeamInfo: unknown fieldSize for actor edict (actorSize: %i, actor num: %i)\n",
						dummyFieldSize, i);
			}

			if (!ent) {
				Com_DPrintf(DEBUG_GAME,
						"G_ClientTeamInfo: Could not spawn actor because no useable spawn-point is available (%i)\n",
						dummyFieldSize);
				G_ClientSkipActorInfo();
				continue;
			}

			level.num_alive[ent->team]++;
			level.num_spawned[ent->team]++;
			ent->pnum = player->num;

			ent->chr.fieldSize = dummyFieldSize;
			ent->fieldSize = ent->chr.fieldSize;

			Com_DPrintf(DEBUG_GAME, "Player: %i - team %i - size: %i\n", player->num, ent->team, ent->fieldSize);

			gi.LinkEdict(ent);

			/* model */
			ent->chr.ucn = gi.ReadShort();
			Q_strncpyz(ent->chr.name, gi.ReadString(), sizeof(ent->chr.name));
			Q_strncpyz(ent->chr.path, gi.ReadString(), sizeof(ent->chr.path));
			Q_strncpyz(ent->chr.body, gi.ReadString(), sizeof(ent->chr.body));
			Q_strncpyz(ent->chr.head, gi.ReadString(), sizeof(ent->chr.head));
			ent->chr.skin = gi.ReadByte();

			Com_DPrintf(DEBUG_GAME, "G_ClientTeamInfo: name: %s, path: %s, body: %s, head: %s, skin: %i\n",
					ent->chr.name, ent->chr.path, ent->chr.body, ent->chr.head, ent->chr.skin);

			ent->chr.HP = gi.ReadShort();
			ent->chr.minHP = ent->chr.HP;
			ent->chr.maxHP = gi.ReadShort();
			ent->chr.teamDef = &gi.csi->teamDef[gi.ReadByte()];

			ent->chr.gender = gi.ReadByte();
			ent->chr.STUN = gi.ReadByte();
			ent->chr.morale = gi.ReadByte();

			/** Scores @sa G_ClientSkipActorInfo */
			for (k = 0; k < SKILL_NUM_TYPES + 1; k++) /* new attributes */
				ent->chr.score.experience[k] = gi.ReadLong();
			for (k = 0; k < SKILL_NUM_TYPES; k++) /* new attributes */
				ent->chr.score.skills[k] = gi.ReadByte();
			for (k = 0; k < SKILL_NUM_TYPES + 1; k++)
				ent->chr.score.initialSkills[k] = gi.ReadByte();
			for (k = 0; k < KILLED_NUM_TYPES; k++)
				ent->chr.score.kills[k] = gi.ReadShort();
			for (k = 0; k < KILLED_NUM_TYPES; k++)
				ent->chr.score.stuns[k] = gi.ReadShort();
			ent->chr.score.assignedMissions = gi.ReadShort();

			/* Read user-defined reaction-state. */
			ent->chr.reservedTus.reserveReaction = gi.ReadShort();

			/* Mission Scores */
			memset(&scoreMission[scoreMissionNum], 0, sizeof(scoreMission[scoreMissionNum]));
			ent->chr.scoreMission = &scoreMission[scoreMissionNum];
			scoreMissionNum++;

			/* inventory */
			{
				int nr = gi.ReadShort() / INV_INVENTORY_BYTES;

				for (; nr-- > 0;) {
					invDef_t *container;
					G_ReadItem(&item, &container, &x, &y);
					Com_DPrintf(DEBUG_GAME, "G_ClientTeamInfo: t=%i:a=%i:m=%i (x=%i:y=%i)\n", (item.t ? item.t->idx
							: NONE), item.a, (item.m ? item.m->idx : NONE), x, y);

					if (container) {
						Com_AddToInventory(&ent->i, item, container, x, y, 1);
						Com_DPrintf(DEBUG_GAME, "G_ClientTeamInfo: (container: %i - idArmour: %i) <- Added %s.\n",
								container->id, gi.csi->idArmour, ent->i.c[container->id]->item.t->id);
					}
				}
			}

			/* set models */
			/** @todo is this copy needed? - wouldn't it be enough to use the inventory from character_t? */
			ent->chr.inv = ent->i;
			ent->body = gi.ModelIndex(CHRSH_CharGetBody(&ent->chr));
			ent->head = gi.ModelIndex(CHRSH_CharGetHead(&ent->chr));

			/* set initial vital statistics */
			ent->HP = ent->chr.HP;
			ent->morale = ent->chr.morale;

			/** @todo for now, heal fully upon entering mission */
			ent->morale = GET_MORALE(ent->chr.score.skills[ABILITY_MIND]);

			ent->reaction_minhit = 30; /** @todo allow later changes from GUI */
		} else {
			/* just do nothing with the info */
			gi.ReadByte(); /* fieldSize */
			G_ClientSkipActorInfo();
		}
	}
	G_ClientTeamAssign(player);
}

/**
 * @brief Send brush models for entities like func_breakable and func_door and triggers
 * with their mins and maxs bounding boxes to the client and let him know about them.
 * There are also entities that are announced here, but fully handled clientside - like
 * func_rotating.
 * @sa CL_AddBrushModel
 * @sa EV_ADD_BRUSH_MODEL
 * @param[in] team The team the edicts are send to
 */
static void G_ClientSendEdictsAndBrushModels (const player_t *player)
{
	int i;
	edict_t *ent;

	/* make SOLID_BSP edicts visible to the client */
	for (i = 1, ent = g_edicts + 1; i < globals.num_edicts; ent++, i++) {
		if (!ent->inuse)
			continue;

		/* brush models that have a type - not the world - keep in
		 * mind that there are several world edicts in the list in case of
		 * a map assembly */
		switch (ent->solid) {
		case SOLID_BSP:
			/* skip the world(s) in case of map assembly */
			if (ent->type) {
				gi.AddEvent(G_PlayerToPM(player), EV_ADD_BRUSH_MODEL);
				gi.WriteShort(ent->type);
				gi.WriteShort(ent->number);
				gi.WriteShort(ent->modelindex);
				/* strip the higher bits - only send levelflags */
				gi.WriteByte(ent->spawnflags & 0xFF);
				gi.WritePos(ent->origin);
				gi.WritePos(ent->angles);
				gi.WriteShort(ent->speed);
				gi.WriteByte(ent->angle);
				ent->visflags |= ~ent->visflags;
			}
			break;

			/* send trigger entities to the client to display them (needs mins, maxs set) */
		case SOLID_TRIGGER:
			G_SendTriggerBoundingBoxes(G_PlayerToPM(player), ent);
			break;

		case SOLID_NOT:
		case SOLID_BBOX:
			break;
		}
	}
}

/**
 * @brief This functions starts the client
 * @sa G_ClientSpawn
 * @sa CL_StartGame
 */
void G_ClientBegin (player_t* player)
{
	/* this doesn't belong here, but it works */
	if (!level.routed) {
		level.routed = qtrue;
		G_CompleteRecalcRouting();
	}

	/** @todo This should be a client side error */
	if (!G_PlayerToPM(player)) {
		gi.BroadcastPrintf(PRINT_CONSOLE, "%s tried to join - but server is full\n", player->pers.netname);
		return;
	}

	player->began = qtrue;

	level.numplayers++;
	gi.ConfigString(CS_PLAYERCOUNT, "%i", level.numplayers);

	/* spawn camera (starts client rendering) */
	gi.AddEvent(G_PlayerToPM(player), EV_START | EVENT_INSTANTLY);
	gi.WriteByte(sv_teamplay->integer);

	/* ensure that the start event is send */
	gi.EndEvents();

	/* set the net name */
	gi.ConfigString(CS_PLAYERNAMES + player->num, "%s", player->pers.netname);

	/* inform all clients */
	gi.BroadcastPrintf(PRINT_CONSOLE, "%s has joined team %i\n", player->pers.netname, player->pers.team);
}

/**
 * @brief Sets the team, init the TU and sends the player stats.
 * @return Returns qtrue if player spawns, otherwise qfalse.
 * @sa G_SendPlayerStats
 * @sa G_GetTeam
 * @sa G_GiveTimeUnits
 * @sa G_ClientBegin
 * @sa CL_Reset
 */
qboolean G_ClientSpawn (player_t * player)
{
	edict_t *ent;
	int i;

	if (player->spawned) {
		gi.BroadcastPrintf(PRINT_CONSOLE, "%s already spawned.\n", player->pers.netname);
		G_ClientDisconnect(player);
		return qfalse;
	}

	/** @todo Check player->pers.team here */
	if (!G_MatchIsRunning()) {
		/* activate round if in single-player */
		if (sv_maxclients->integer == 1) {
			level.activeTeam = player->pers.team;
		} else {
			/* return since not all multiplayer teams have joined */
			/* (G_ClientTeamAssign sets level.activeTeam once all teams have joined) */
			return qfalse;
		}
	}

	player->spawned = qtrue;

	/* do all the init events here... */
	/* reset the data */
	gi.AddEvent(G_PlayerToPM(player), EV_RESET | EVENT_INSTANTLY);
	gi.WriteByte(player->pers.team);
	gi.WriteByte(level.activeTeam);

	/* show visible actors and add invisible actor */
	G_ClearVisFlags(player->pers.team);
	G_CheckVisPlayer(player, qfalse);
	G_SendInvisible(player);

	/** Set initial state of reaction fire to previously stored state for all team-members.
	 * This (and the second loop below) defines the default reaction-mode.
	 * @sa CL_GenerateCharacter for the initial default value.
	 * @sa G_SpawnAIPlayer */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && ent->team == player->pers.team && G_IsActor(ent)) {
			Com_DPrintf(DEBUG_GAME, "G_ClientSpawn: Setting default reaction-mode to %i (%s - %s).\n",
					ent->chr.reservedTus.reserveReaction, player->pers.netname, ent->chr.name);
			G_ClientStateChange(player, i, ent->chr.reservedTus.reserveReaction, qfalse);
		}

	/* submit stats */
	G_SendPlayerStats(player);

	/* send things like doors and breakables */
	G_ClientSendEdictsAndBrushModels(player);

	/* give time units */
	G_GiveTimeUnits(player->pers.team);

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && ent->team == player->pers.team && G_IsActor(ent)) {
			gi.AddEvent(player->pers.team, EV_ACTOR_STATECHANGE);
			gi.WriteShort(ent->number);
			gi.WriteShort(ent->state);
		}

	gi.AddEvent(G_PlayerToPM(player), EV_START_DONE);
	/* ensure that the last event is send, too */
	gi.EndEvents();

	/* inform all clients */
	gi.BroadcastPrintf(PRINT_CONSOLE, "%s has taken control over team %i.\n", player->pers.netname, player->pers.team);
	return qtrue;
}

/**
 * @brief called whenever the player updates a userinfo variable.
 * @note The game can override any of the settings in place (forcing skins or names, etc) before copying it off.
 */
void G_ClientUserinfoChanged (player_t * player, char *userinfo)
{
	const char *s;

	/* check for malformed or illegal info strings */
	if (!Info_Validate(userinfo))
		Q_strncpyz(userinfo, "\\cl_name\\badinfo", sizeof(userinfo));

	/* set name */
	s = Info_ValueForKey(userinfo, "cl_name");
	Q_strncpyz(player->pers.netname, s, sizeof(player->pers.netname));

	Q_strncpyz(player->pers.userinfo, userinfo, sizeof(player->pers.userinfo));

	s = Info_ValueForKey(userinfo, "cl_autostand");
	player->autostand = atoi(s);

	/* send the updated config string */
	gi.ConfigString(CS_PLAYERNAMES + player->num, "%s", player->pers.netname);
}

/**
 * @brief Checks whether the connection is valid or invalid and set some
 * user info keys.
 * @param[in,out] player The player that is trying to connect to the game
 * @param[in,out] userinfo The userinfo data that is checked and changed
 * @sa G_ClientDisconnect
 * @sa CL_ConnectionlessPacket
 * @todo Check if the teamnum preference has already reached maxsoldiers
 * and reject connection if so
 * @return @c false if the connection is refused, @c true otherwise
 */
qboolean G_ClientConnect (player_t * player, char *userinfo)
{
	const char *value;

	value = Info_ValueForKey(userinfo, "ip");

	Com_Printf("%s is trying to connect... (%s)\n", player->pers.netname, value);

	/* check to see if they are on the banned IP list */
	if (SV_FilterPacket(value)) {
		Info_SetValueForKey(userinfo, "rejmsg", REJ_BANNED);
		return qfalse;
	}

	value = Info_ValueForKey(userinfo, "password");
	if (password->string[0] != '\0' && strcmp(password->string, "none") && strcmp(password->string, value)) {
		Info_SetValueForKey(userinfo, "rejmsg", REJ_PASSWORD_REQUIRED_OR_INCORRECT);
		return qfalse;
	}

	/* fix for fast reconnects after a disconnect */
	if (player->inuse) {
		gi.BroadcastPrintf(PRINT_CONSOLE, "%s already in use.\n", player->pers.netname);
		G_ClientDisconnect(player);
	}

	/* reset persistent data */
	memset(&player->pers, 0, sizeof(player->pers));
	G_ClientUserinfoChanged(player, userinfo);

	gi.BroadcastPrintf(PRINT_CHAT, "%s is connecting...\n", player->pers.netname);
	return qtrue;
}

/**
 * @sa G_ClientConnect
 */
void G_ClientDisconnect (player_t * player)
{
#if 0
	edict_t *ent;
#endif

	/* only if the player already sent his began */
	if (player->began) {
		level.numplayers--;
		gi.ConfigString(CS_PLAYERCOUNT, "%i", level.numplayers);

		if (level.activeTeam == player->pers.team)
			G_ClientEndRound(player, NOISY);

		/* if no more players are connected - stop the server */
		G_MatchEndCheck();
	}

#if 0
	/* now let's remove all the edicts that belongs to this player */
	for (; ent < &g_edicts[globals.num_edicts]; ent++)
		if (ent->pnum == player->num && ent->inuse)
			if (G_IsLivingActor(ent))
				G_ActorDie(ent, STATE_DEAD, NULL);
	G_MatchEndCheck();
#endif

	player->began = qfalse;
	player->spawned = qfalse;
	player->ready = qfalse;

	gi.BroadcastPrintf(PRINT_CHAT, "%s disconnected.\n", player->pers.netname);
}

/**
 * @brief Called after every player has joined
 */
void G_ResetClientData (void)
{
	scoreMissionNum = 0;
	memset(scoreMission, 0, sizeof(scoreMission));
}
