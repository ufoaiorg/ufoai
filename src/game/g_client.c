/**
 * @file g_client.c
 * @brief Main part of the game logic.
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

#include "g_local.h"

static chrScoreMission_t scoreMission[MAX_EDICTS];
static int scoreMissionNum = 0;

/**
 * @brief Iterate through the list of players
 * @param lastPlayer The player found in the previous iteration; if NULL, we start at the beginning
 */
player_t* G_PlayerGetNextHuman (player_t* lastPlayer)
{
	player_t* endOfPlayers = &game.players[game.sv_maxplayersperteam];
	player_t* player;

	if (!game.sv_maxplayersperteam)
		return NULL;

	if (!lastPlayer)
		return game.players;
	assert(lastPlayer >= game.players);
	assert(lastPlayer < endOfPlayers);

	player = lastPlayer;

	player++;
	if (player >= endOfPlayers)
		return NULL;
	else
		return player;
}

/**
 * @brief Iterate through the list of players
 * @param lastPlayer The player found in the previous iteration; if NULL, we start at the beginning
 */
player_t* G_PlayerGetNextAI (player_t* lastPlayer)
{
	player_t* endOfPlayers = &game.players[game.sv_maxplayersperteam * 2];
	player_t* player;

	if (!game.sv_maxplayersperteam)
		return NULL;

	if (!lastPlayer)
		return &game.players[game.sv_maxplayersperteam];
	assert(lastPlayer >= &game.players[game.sv_maxplayersperteam]);
	assert(lastPlayer < endOfPlayers);

	player = lastPlayer;

	player++;
	if (player >= endOfPlayers)
		return NULL;
	else
		return player;
}

/**
 * @brief Iterate through the list of players
 * @param lastPlayer The player found in the previous iteration; if NULL, we start at the beginning
 */
player_t* G_PlayerGetNextActiveHuman (player_t* lastPlayer)
{
	player_t* player = lastPlayer;

	while ((player = G_PlayerGetNextHuman(player))) {
		if (player->inuse)
			return player;
	}

	return NULL;
}

/**
 * @brief Iterate through the list of players
 * @param lastPlayer The player found in the previous iteration; if NULL, we start at the beginning
 */
player_t* G_PlayerGetNextActiveAI (player_t* lastPlayer)
{
	player_t* player = lastPlayer;

	while ((player = G_PlayerGetNextAI(player))) {
		if (player->inuse)
			return player;
	}

	return NULL;
}

/**
 * @brief Generates the player bit mask for a given team
 * @param[in] team The team to create the player bit mask for
 * @note E.g. multiplayer team play can have more than one human player on the
 * same team.
 */
unsigned int G_TeamToPM (int team)
{
	player_t *p;
	unsigned int playerMask;

	playerMask = 0;

	/* don't handle the ai players, here */
	p = NULL;
	while ((p = G_PlayerGetNextHuman(p))) {
		if (p->inuse && team == p->pers.team)
			playerMask |= G_PlayerToPM(p);
	}

	return playerMask;
}

/**
 * @brief Converts player mask to vis mask
 * @param[in] playerMask The player bit mask (contains the player numbers) that
 * is converted to a vis mask
 * @return Returns a vis mask for all the teams of the connected players that
 * are marked in the given @c playerMask.
 */
vismask_t G_PMToVis (unsigned int playerMask)
{
	player_t *p;
	vismask_t visMask;

	visMask = 0;

	/* don't handle the ai players, here */
	p = NULL;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		if (playerMask & G_PlayerToPM(p))
			visMask |= G_TeamToVisMask(p->pers.team);
	}

	return visMask;
}

/**
 * @brief Converts vis mask to player mask
 * @param[in] vis_mask The visibility bit mask (contains the team numbers) that
 * is converted to a player mask
 * @return Returns a playermask for all the teams of the connected players that
 * are marked in the given @c vis_mask.
 */
unsigned int G_VisToPM (vismask_t vis_mask)
{
	player_t *p;
	unsigned int playerMask;

	playerMask = 0;

	/* don't handle the ai players, here */
	p = NULL;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		if (vis_mask & G_TeamToVisMask(p->pers.team))
			playerMask |= G_PlayerToPM(p);
	}

	return playerMask;
}

/**
 * Send messages to human players
 * @param player A player (AI players are ignored here)
 * @param printLevel A numeric value to restrict and channel the printing (CONSOLE, HUD, CHAT...)
 * @param fmt A format string as in printf
 */
void G_ClientPrintf (const player_t *player, int printLevel, const char *fmt, ...)
{
	va_list ap;

	/* there is no client for an AI controlled player on the server where we
	 * could send the message to */
	if (G_IsAIPlayer(player))
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
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, team))) {
		G_ActorGiveTimeUnits(ent);
		G_SendStats(ent);
	}
}

/**
 * @brief Send the appear or perish event to the affected clients
 * @param[in] playerMask These are the affected players or clients
 * In case of e.g. teamplay there can be more than one client affected - thus
 * this is a player mask
 * @param[in] appear Is this event about an appearing actor (or a perishing one)
 * @param[in] check The edict we are talking about (that appears or perishes)
 * @param[in] ent The edict that was responsible for letting the check edict appear
 * or perish. Might be @c NULL.
 * @sa CL_ActorAppear
 */
void G_AppearPerishEvent (unsigned int playerMask, qboolean appear, edict_t *check, const edict_t *ent)
{
	vismask_t visMaskDiff;

	/* test for pointless player mask */
	if (!playerMask)
		return;

	visMaskDiff = G_PMToVis(playerMask);
	G_VisFlagsSwap(check, visMaskDiff);

	if (appear) {
		/* appear */
		switch (check->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			G_EventActorAppear(playerMask, check, ent);
			break;

		case ET_ITEM:
			G_EventEdictAppear(playerMask, check);
			G_SendInventory(playerMask, check);
			break;

		case ET_PARTICLE:
			G_EventEdictAppear(playerMask, check);
			G_EventSendParticle(playerMask, check);
			break;

		case ET_TRIGGER_RESCUE:
			G_EventAddBrushModel(playerMask, check);
			break;

		default:
			if (G_IsVisibleOnBattlefield(check))
				gi.Error("Missing edict type %i in G_AppearPerishEvent", check->type);
			break;
		}
	} else if (G_IsVisibleOnBattlefield(check)) {
		G_EventEdictPerish(playerMask, check);
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
void G_SendInvisible (const player_t* player)
{
	const int team = player->pers.team;

	assert(team != TEAM_NO_ACTIVE);
	if (level.num_alive[team]) {
		edict_t* ent = NULL;
		/* check visibility */
		while ((ent = G_EdictsGetNextActor(ent))) {
			if (ent->team != team) {
				/* not visible for this team - so add the le only */
				if (!G_IsVisibleForTeam(ent, team)) {
					G_EventActorAdd(G_PlayerToPM(player), ent);
				}
			}
		}
	}
}

/**
 * @brief Returns the current active team to the server
 * @par If this has the value @c TEAM_NO_ACTIVE there is either no
 * living actor for any player left, or the turn wasn't started yet.
 */
int G_GetActiveTeam (void)
{
	return level.activeTeam;
}

/**
 * @brief Checks whether the requested action is possible
 * @param[in] player Which player (human player) is trying to do the action
 * @param[in] ent Which of his units is trying to do the action.
 */
static qboolean G_ActionCheck (const player_t *player, edict_t *ent)
{
	/* don't check for a player - but maybe a server action */
	if (!player)
		return qtrue;

	if (!ent || !ent->inuse) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - object not present!\n"));
		return qfalse;
	}

	if (ent->type != ET_ACTOR && ent->type != ET_ACTOR2x2) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - not an actor!\n"));
		return qfalse;
	}

	if (G_IsStunned(ent)) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - actor is stunned!\n"));
		return qfalse;
	}

	if (G_IsDead(ent)) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - actor is dead!\n"));
		return qfalse;
	}

	if (ent->team != player->pers.team) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - not on same team!\n"));
		return qfalse;
	}

	if (ent->pnum != player->num) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - no control over allied actors!\n"));
		return qfalse;
	}

	/* could be possible */
	return qtrue;
}

/**
 * @brief Checks whether the requested action is possible for the current active team
 * @param[in] player Which player (human player) is trying to do the action
 * @param[in] ent Which of his units is trying to do the action.
 * @param[in] TU The time units to check against the ones ent has.
 * the action with
 */
qboolean G_ActionCheckForCurrentTeam (const player_t *player, edict_t *ent, int TU)
{
	/* don't check for a player - but maybe a server action */
	if (!player)
		return qtrue;

	/* a generic tester if an action could be possible */
	if (level.activeTeam != player->pers.team) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - it is not your turn!\n"));
		return qfalse;
	}

	if (TU > G_ActorUsableTUs(ent)) {
		return qfalse;
	}

	return G_ActionCheck(player, ent);
}

/**
 * @brief Checks whether the requested action is possible
 * @param[in] player Which player (human player) is trying to do the action
 * @param[in] ent Which of his units is trying to do the action.
 * @param[in] TU The time units to check against the ones ent has.
 * the action with
 * @sa G_ActionCheck
 */
qboolean G_ActionCheckForReaction (const player_t *player, edict_t *ent, int TU)
{
	if (TU > ent->TU) {
		return qfalse;
	}

	return G_ActionCheck(player, ent);
}

/**
 * @brief Sends the actual actor turn event over the netchannel
 */
static void G_ClientTurn (player_t * player, edict_t* ent, dvec_t dvec)
{
	const int dir = getDVdir(dvec);

	/* check if action is possible */
	if (!G_ActionCheckForCurrentTeam(player, ent, TU_TURN))
		return;

	/* check if we're already facing that direction */
	if (ent->dir == dir)
		return;

	/* do the turn */
	G_ActorDoTurn(ent, dir);
	G_ActorUseTU(ent, TU_TURN);

	/* send the turn */
	G_EventActorTurn(ent);

	/* send the new TUs */
	G_SendStats(ent);

	/* end the event */
	gi.EndEvents();
}

/**
 * @brief After an actor changed his state, he might get visible for other
 * players. Check the vis here and send the state change to the clients that
 * are seeing him already.
 * @param ent The actor edict
 */
static void G_ClientStateChangeUpdate (edict_t *ent)
{
	/* Send the state change. */
	G_EventSendState(G_VisToPM(ent->visflags), ent);

	/* Check if the player appears/perishes, seen from other teams. */
	G_CheckVis(ent, qtrue);

	/* Calc new vis for this player. */
	G_CheckVisTeamAll(ent->team, qfalse, ent);

	/* Send the new TUs. */
	G_SendStats(ent);

	/* End the event. */
	gi.EndEvents();
}

/**
 * @brief Changes the state of a player/soldier.
 * @param[in,out] player The player who controlls the actor
 * @param[in] ent the edict to perform the state change for
 * @param[in] reqState The bit-map of the requested state change
 * @param[in] checkaction only activate the events - network stuff is handled in the calling function
 * don't even use the G_ActionCheckForCurrentTeam function
 * @note Use checkaction true only for e.g. spawning values
 */
void G_ClientStateChange (const player_t* player, edict_t* ent, int reqState, qboolean checkaction)
{
	/* Check if any action is possible. */
	if (checkaction && !G_ActionCheckForCurrentTeam(player, ent, 0))
		return;

	if (!reqState)
		return;

	switch (reqState) {
	case STATE_CROUCHED: /* Toggle between crouch/stand. */
		/* Check if player has enough TUs (TU_CROUCH TUs for crouch/uncrouch). */
		if (!checkaction || G_ActionCheckForCurrentTeam(player, ent, TU_CROUCH)) {
			G_ToggleCrouched(ent);
			G_ActorUseTU(ent, TU_CROUCH);
			G_ActorSetMaxs(ent);
		}
		break;
	case ~STATE_REACTION: /* Request to turn off reaction fire. */
		if (G_IsReaction(ent)) {
			if (G_IsShaken(ent)) {
				G_ClientPrintf(player, PRINT_HUD, _("Currently shaken, won't let their guard down.\n"));
			} else {
				/* Turn off reaction fire. */
				G_RemoveReaction(ent);
				G_ActorReserveTUs(ent, 0, ent->chr.reservedTus.shot, ent->chr.reservedTus.crouch);
			}
		}
		break;
	/* Request to turn on multi- or single-reaction fire mode. */
	case STATE_REACTION:
		/* Disable reaction fire. */
		G_RemoveReaction(ent);

		if (G_ReactionFireSettingsReserveTUs(ent)) {
			/* Enable requested reaction fire. */
			G_SetState(ent, reqState);
		}
		break;
	default:
		gi.DPrintf("G_ClientStateChange: unknown request %i, ignoring\n", reqState);
		return;
	}

	/* Only activate the events - network stuff is handled in the calling function */
	if (!checkaction)
		return;

	G_ClientStateChangeUpdate(ent);
}

/**
 * @brief Returns true if actor can reload weapon
 * @sa AI_ActorThink
 */
qboolean G_ClientCanReload (edict_t *ent, containerIndex_t containerID)
{
	invList_t *ic;
	containerIndex_t container;
	const objDef_t *weapon;

	if (CONTAINER(ent, containerID)) {
		weapon = CONTAINER(ent, containerID)->item.t;
	} else if (containerID == gi.csi->idLeft && RIGHT(ent)->item.t->holdTwoHanded) {
		/* Check for two-handed weapon */
		containerID = gi.csi->idRight;
		weapon = CONTAINER(ent, containerID)->item.t;
	} else
		return qfalse;

	assert(weapon);

	/* also try the temp containers */
	for (container = 0; container < gi.csi->numIDs; container++)
		for (ic = CONTAINER(ent, container); ic; ic = ic->next)
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
void G_ClientGetWeaponFromInventory (edict_t *ent)
{
	invList_t *ic;
	invList_t *icFinal;
	const invDef_t *invDef;
	int tu;
	containerIndex_t container;
	const invDef_t *bestContainer;

	/* e.g. bloodspiders are not allowed to carry or collect weapons */
	if (!ent->chr.teamDef->weapons)
		return;

	/* search for weapons and select the one that is available easily */
	tu = 100;
	invDef = INVDEF(gi.csi->idRight);
	bestContainer = NULL;
	icFinal = NULL;

	/* also try the temp containers */
	for (container = 0; container < gi.csi->numIDs; container++) {
		if (INVDEF(container)->out < tu) {
			/* Once we've found at least one clip, there's no point
			 * searching other containers if it would take longer
			 * to retrieve the ammo from them than the one
			 * we've already found. */
			for (ic = CONTAINER(ent, container); ic; ic = ic->next) {
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
		G_ActorInvMove(ent, bestContainer, icFinal, invDef, 0, 0, qtrue);
}

/**
 * @brief This function 'uses' the edict. E.g. it opens the door when the player wants it to open
 * @sa PA_USE_DOOR
 * @param[in] player The player is trying to activate the door
 * @param[in,out] actor The actor the player is using to activate the entity
 * @param[in,out] edict The entity that is to be used
 * @todo Do we have to change the trigger position here, too? I don't think this is really needed.
 * @sa CL_ActorUse
 * @sa G_UseEdict
 */
qboolean G_ClientUseEdict (const player_t *player, edict_t *actor, edict_t *edict)
{
	/* check whether the actor has sufficient TUs to 'use' this edicts */
	if (!G_ActionCheckForCurrentTeam(player, actor, edict->TU))
		return qfalse;

	if (!G_UseEdict(edict, actor))
		return qfalse;

	/* using a group of edicts only costs TUs once (for the master) */
	G_ActorUseTU(actor, edict->TU);
	/* send the new TUs */
	G_SendStats(actor);

	gi.EndEvents();

	return qtrue;
}

/**
 * @brief The client sent us a message that he did something. We now execute the related function(s) and notify him if necessary.
 * @param[in] player The player to execute the action for (the actor belongs to this player)
 * @note a client action will also send the server side edict number to determine the actor
 */
int G_ClientAction (player_t * player)
{
	player_action_t action;
	int num;
	pos3_t pos;
	int i;
	fireDefIndex_t firemode;
	int from, fx, fy, to, tx, ty;
	actorHands_t hand;
	int fmIdx, objIdx;
	int resCrouch, resShot;
	edict_t *ent;
	const char *format;

	/* read the header */
	action = gi.ReadByte();
	num = gi.ReadShort();

	ent = G_EdictsGetByNum(num);
	if (ent == NULL)
		return action;

	format = pa_format[action];

	switch (action) {
	case PA_NULL:
		/* do nothing on a null action */
		break;

	case PA_TURN:
		gi.ReadFormat(format, &i);
		G_ClientTurn(player, ent, (dvec_t) i);
		break;

	case PA_MOVE:
		gi.ReadFormat(format, &pos);
		G_ClientMove(player, player->pers.team, ent, pos);
		break;

	case PA_STATE:
		gi.ReadFormat(format, &i);
		G_ClientStateChange(player, ent, i, qtrue);
		break;

	case PA_SHOOT:
		gi.ReadFormat(format, &pos, &i, &firemode, &from);
		G_ClientShoot(player, ent, pos, i, firemode, NULL, qtrue, from);
		break;

	case PA_INVMOVE:
		gi.ReadFormat(format, &from, &fx, &fy, &to, &tx, &ty);

		if (from < 0 || from >= gi.csi->numIDs || to < 0 || to >= gi.csi->numIDs) {
			gi.DPrintf("G_ClientAction: PA_INVMOVE Container index out of range. (from: %i, to: %i)\n", from, to);
		} else {
			const invDef_t *fromPtr = INVDEF(from);
			const invDef_t *toPtr = INVDEF(to);
			invList_t *fromItem = INVSH_SearchInInventory(&ent->chr.i, fromPtr, fx, fy);
			if (fromItem)
				G_ActorInvMove(ent, fromPtr, fromItem, toPtr, tx, ty, qtrue);
		}
		break;

	case PA_USE:
		if (ent->clientAction) {
			edict_t *actionEnt;

			/* read the door the client wants to open */
			gi.ReadFormat(format, &i);

			/* get the door edict */
			actionEnt = G_EdictsGetByNum(i);

			/* maybe the door is no longer 'alive' because it was destroyed */
			if (actionEnt && ent->clientAction == actionEnt) {
				if (G_IsDoor(actionEnt)) {
					/* check whether it's part of an edict group but not the master */
					if (actionEnt->flags & FL_GROUPSLAVE)
						actionEnt = actionEnt->groupMaster;

					G_ActorUseDoor(ent, actionEnt);
				}
			}
		}
		break;

	case PA_REACT_SELECT:
		gi.ReadFormat(format, &hand, &fmIdx, &objIdx);
		G_ReactionFireUpdate(ent, fmIdx, hand, INVSH_GetItemByIDX(objIdx));
		break;

	case PA_RESERVE_STATE:
		gi.ReadFormat(format, &resShot, &resCrouch);

		G_ActorReserveTUs(ent, ent->chr.reservedTus.reaction, resShot, resCrouch);
		break;

	default:
		gi.Error("G_ClientAction: Unknown action!\n");
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
	int playersInGame = 0;

	/* player has already a team */
	if (player->pers.team > 0) {
		Com_DPrintf(DEBUG_GAME, "Player %s is already on team %i\n", player->pers.netname, player->pers.team);
		return;
	}

	/* number of currently connected players (no ai players) */
	p = NULL;
	while ((p = G_PlayerGetNextActiveHuman(p)))
		playersInGame++;

	/* randomly assign a teamnumber in deathmatch games */
	if (playersInGame <= 1 && sv_maxclients->integer > 1 && !sv_teamplay->integer) {
		int spawnCheck[MAX_TEAMS];
		int spawnSpots = 0;
		int randomSpot;
		int i;
		/* skip civilian teams */
		for (i = TEAM_PHALANX; i < MAX_TEAMS; i++) {
			spawnCheck[i] = 0;
			/* check whether there are spawnpoints for this team */
			if (level.num_spawnpoints[i])
				spawnCheck[spawnSpots++] = i;
		}
		/* we need at least 2 different team spawnpoints for multiplayer in a death match game */
		if (spawnSpots < 2)
			gi.Error("G_GetTeam: Not enough spawn spots in map!");

		/* assign random valid team number */
		i = spawnSpots;
		randomSpot = rand() % spawnSpots;
		for (;;) {
			const int team = spawnCheck[randomSpot];
			if (i == 0)
				gi.Error("G_GetTeam: Could not assign a team!");
			if (G_SetTeamForPlayer(player, team)) {
				gi.DPrintf("%s has been randomly assigned to team %i\n",
						player->pers.netname, G_ClientGetTeamNum(player));
				break;
			}
			i--;
			randomSpot = (randomSpot + 1) % spawnSpots;
		}
		return;
	}

	/* find a team */
	if (sv_maxclients->integer == 1)
		G_SetTeamForPlayer(player, TEAM_PHALANX);
	else if (sv_teamplay->integer) {
		/* set the team specified in the userinfo */
		const int i = G_ClientGetTeamNumPref(player);
		gi.DPrintf("Get a team for teamplay for %s\n", player->pers.netname);
		/* civilians are at team zero */
		if (i > TEAM_CIVILIAN && sv_maxteams->integer >= i) {
			G_SetTeamForPlayer(player, i);
			gi.BroadcastPrintf(PRINT_CONSOLE, "serverconsole: %s has chosen team %i\n", player->pers.netname, i);
		} else {
			gi.DPrintf("Team %i is not valid - choose a team between 1 and %i\n", i, sv_maxteams->integer);
			G_SetTeamForPlayer(player, TEAM_DEFAULT);
		}
	} else {
		int i;
		/* search team */
		gi.DPrintf("Getting a multiplayer team for %s\n", player->pers.netname);
		for (i = TEAM_CIVILIAN + 1; i < MAX_TEAMS; i++) {
			if (level.num_spawnpoints[i]) {
				qboolean teamAvailable = qtrue;

				p = NULL;
				/* check if team is in use (only human controlled players) */
				while ((p = G_PlayerGetNextActiveAI(p))) {
					if (p->pers.team == i) {
						Com_DPrintf(DEBUG_GAME, "Team %i is already in use\n", i);
						/* team already in use */
						teamAvailable = qfalse;
						break;
					}
				}
				if (teamAvailable)
					break;
			}
		}

		/* set the team */
		if (i < MAX_TEAMS) {
			/* remove ai player */
			p = NULL;
			while ((p = G_PlayerGetNextActiveHuman(p))) {
				if (p->pers.team == i) {
					gi.BroadcastPrintf(PRINT_CONSOLE, "Removing ai player...");
					p->inuse = qfalse;
					break;
				}
			}
			Com_DPrintf(DEBUG_GAME, "Assigning %s to team %i\n", player->pers.netname, i);
			G_SetTeamForPlayer(player, i);
		} else {
			gi.DPrintf("No free team - disconnecting '%s'\n", player->pers.netname);
			G_ClientDisconnect(player);
		}
	}
}

/**
 * @brief Set the used team for the given player
 * @param[out] player The player the team should be set for
 * @param[in] team The team to set for the given player
 * @return <code>true</code> if the team was set successfully, <code>false</code> otherwise.
 */
qboolean G_SetTeamForPlayer (player_t* player, const int team)
{
	assert(player);
	assert(team >= TEAM_NO_ACTIVE && team < MAX_TEAMS);

	if (G_IsAIPlayer(player)) {
		if (team != TEAM_ALIEN && team != TEAM_CIVILIAN)
			return qfalse;
	} else {
		if (!sv_teamplay->integer) {
			player_t *p = NULL;
			while ((p = G_PlayerGetNextHuman(p)) != NULL) {
				if (p->pers.team == team)
					return qfalse;
			}
		}
	}

	player->pers.team = team;

	/* if we started in dev mode, we maybe don't have a
	 * starting position in this map */
	if (!g_nospawn->integer) {
		if (team >= 0 && team < MAX_TEAMS) {
			if (!level.num_spawnpoints[team])
				gi.Error("No spawnpoints for team %i", team);
		}
	}

	if (!G_IsAIPlayer(player))
		Info_SetValueForKeyAsInteger(player->pers.userinfo, sizeof(player->pers.userinfo), "cl_team", team);

	return qtrue;
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
	return Info_IntegerForKey(player->pers.userinfo, "cl_teamnum");
}

/**
 * @return @c true if the player is for starting the multiplayer match
 */
qboolean G_ClientIsReady (const player_t * player)
{
	assert(player);
	return player->isReady;
}

/**
 * @brief Chose a team that should start the match
 * @param[in] player In singleplayer mode the team of this player will get the first turn
 * @sa SVCmd_StartGame_f
 */
static void G_GetStartingTeam (const player_t* player)
{
	int teamCount;
	int playerCount;
	int knownTeams[MAX_TEAMS];
	player_t *p;

	/* return with no action if activeTeam already assigned or if are in single-player mode */
	if (G_MatchIsRunning())
		return;

	if (sv_maxclients->integer == 1) {
		level.activeTeam = player->pers.team;
		return;
	}

	/* count number of currently connected unique teams and players (human controlled players only) */
	p = NULL;
	teamCount = 0;
	playerCount = 0;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		int j;
		playerCount++;
		for (j = 0; j < teamCount; j++) {
			if (p->pers.team == knownTeams[j])
				break;
		}
		if (j == teamCount)
			knownTeams[teamCount++] = p->pers.team;
	}

	if (teamCount) {
		const int teamIndex = (int) (frand() * (teamCount - 1) + 0.5);
		G_PrintStats(va("Starting new game: %s with %i teams", level.mapname, teamCount));
		level.activeTeam = knownTeams[teamIndex];
		p = NULL;
		while ((p = G_PlayerGetNextActiveHuman(p)))
			if (p->pers.team != level.activeTeam)
				p->roundDone = qtrue;
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
	edict_t *ent = NULL;

	/* Abort for non-spawnpoints */
	assert(spawnType == ET_ACTORSPAWN || spawnType == ET_ACTOR2x2SPAWN);

	if (level.randomSpawn) {
		edict_t *list[MAX_EDICTS];
		int count = 0;
		while ((ent = G_EdictsGetNext(ent)))
			if (ent->type == spawnType && player->pers.team == ent->team) {
				if (G_GetLivingActorFromPos(ent->pos))
					continue;
				list[count++] = ent;
			}

		if (count)
			return list[rand() % count];
	} else {
		while ((ent = G_EdictsGetNext(ent)))
			if (ent->type == spawnType && player->pers.team == ent->team) {
				if (G_GetLivingActorFromPos(ent->pos))
					continue;
				return ent;
			}
	}

	return NULL;
}

/**
 * @brief Checks whether the spawn of an actor is allowed for the current running match.
 * @note Don't allow spawning of soldiers for multiplayer if:
 * + the sv_maxsoldiersperplayer limit is hit (e.g. the assembled team is bigger than the allowed number of soldiers)
 * + the team already hit the max allowed amount of soldiers
 * @param num The nth actor the player want to spawn in the game.
 * @param team The team the player is part of.
 * @return @c true if spawn is allowed, @c false otherwise.
 */
static inline qboolean G_ActorSpawnIsAllowed (const int num, const int team)
{
	if (sv_maxclients->integer == 1)
		return qtrue;

	return (num < sv_maxsoldiersperplayer->integer && level.num_spawned[team] < sv_maxsoldiersperteam->integer);
}

/**
 * @brief Think function for actors that spawn dead
 * @param ent The actor
 */
static void G_ThinkActorDieAfterSpawn (edict_t *ent)
{
	G_ActorDieOrStun(ent, NULL);
	ent->think = NULL;
}

/**
 * @brief Think function for actors that spawn crouched
 * @param ent The actor
 */
static void G_ThinkActorGoCrouch (edict_t *ent)
{
	G_ClientStateChange(G_PLAYER_FROM_ENT(ent), ent, STATE_CROUCHED, qtrue);
	ent->think = NULL;
}

/**
 * @brief Searches a free spawning point for a given actor size
 * @param[in] player The player to get the free spawn points for
 * @param[in] actorSize The actor size to get a spawning point for
 * @return An actor edict or @c NULL if no free spawning point was found
 */
edict_t* G_ClientGetFreeSpawnPointForActorSize (const player_t *player, const actorSizeEnum_t actorSize)
{
	edict_t *ent;

	if (actorSize == ACTOR_SIZE_NORMAL) {
		/* Find valid actor spawn fields for this player. */
		ent = G_ClientGetFreeSpawnPoint(player, ET_ACTORSPAWN);
		if (ent) {
			edict_t *copy = G_EdictDuplicate(ent);
			if (copy != NULL)
				copy->type = ET_ACTOR;
			ent = copy;
		}
	} else if (actorSize == ACTOR_SIZE_2x2) {
		/* Find valid actor spawn fields for this player. */
		ent = G_ClientGetFreeSpawnPoint(player, ET_ACTOR2x2SPAWN);
		if (ent) {
			edict_t *copy = G_EdictDuplicate(ent);
			if (copy != NULL) {
				copy->type = ET_ACTOR2x2;
				copy->morale = 100;
			}
			ent = copy;
		}
	} else {
		gi.Error("G_ClientGetFreeSpawnPointForActorSize: unknown fieldSize for actor edict (actorSize: %i)\n",
				actorSize);
	}

	if (!ent)
		return NULL;

	level.num_spawned[ent->team]++;
	ent->pnum = player->num;
	ent->chr.fieldSize = actorSize;
	ent->fieldSize = ent->chr.fieldSize;
	ent->flags |= FL_DESTROYABLE;

	gi.LinkEdict(ent);

	if (ent->spawnflags & STATE_CROUCHED) {
		ent->think = G_ThinkActorGoCrouch;
		ent->nextthink = 1;
	}

	if (ent->spawnflags & STATE_STUN) {
		if (ent->spawnflags & STATE_DEAD)
			ent->HP = 0;
		ent->think = G_ThinkActorDieAfterSpawn;
		ent->nextthink = 1;
	}

	G_ActorModifyCounters(NULL, ent, 1, 0, 0);

	G_ReactionFireTargetsCreate(ent);

	return ent;
}

/**
 * @brief Read the inventory from the clients team data
 * @param ent The actor's entity that should receive the items.
 */
static void G_ClientReadInventory (edict_t *ent)
{
	/* inventory */
	int nr = gi.ReadShort();

	for (; nr-- > 0;) {
		const invDef_t *container;
		item_t item;
		int x, y;
		G_ReadItem(&item, &container, &x, &y);
		if (container->temp)
			gi.Error("G_ClientReadInventory failed, tried to add '%s' to a temp container %i", item.t->id, container->id);
		if (!level.noEquipment && game.i.AddToInventory(&game.i, &ent->chr.i, &item, container, x, y, 1) == NULL)
			gi.Error("G_ClientReadInventory failed, could not add item '%s' to container %i (x:%i,y:%i)",
					item.t->id, container->id, x, y);
	}
}

/**
 * @brief Reads the character data from the netchannel that is needed to spawn an actor.
 * @param ent The actor entity to save the read data in.
 */
static void G_ClientReadCharacter (edict_t *ent)
{
	int k;
	int teamDefIdx;

	/* model */
	ent->chr.ucn = gi.ReadShort();
	gi.ReadString(ent->chr.name, sizeof(ent->chr.name));
	gi.ReadString(ent->chr.path, sizeof(ent->chr.path));
	gi.ReadString(ent->chr.body, sizeof(ent->chr.body));
	gi.ReadString(ent->chr.head, sizeof(ent->chr.head));
	ent->chr.skin = gi.ReadByte();

	ent->chr.HP = gi.ReadShort();
	ent->chr.minHP = ent->chr.HP;
	ent->chr.maxHP = gi.ReadShort();
	teamDefIdx = gi.ReadByte();
	if (teamDefIdx < 0 || teamDefIdx >= MAX_TEAMDEFS)
		gi.Error("Invalid team definition index given: %i", teamDefIdx);
	ent->chr.teamDef = &gi.csi->teamDef[teamDefIdx];

	ent->chr.gender = gi.ReadByte();
	ent->chr.STUN = gi.ReadByte();
	ent->chr.morale = gi.ReadByte();

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
}

/**
 * @brief Call this if you want to skip some actor netchannel data
 * @note The fieldsize is not skipped
 * @sa G_ClientTeamInfo
 */
static void G_ClientSkipActorInfo (void)
{
	int i, n;
	edict_t ent;
	invDef_t container;
	item_t item;
	int x, y;
	const invDef_t *c = &container;

	G_ClientReadCharacter(&ent);

	/* skip inventory */
	n = gi.ReadShort();
	for (i = 0; i < n; i++) {
		G_ReadItem(&item, &c, &x, &y);
	}
}

/**
 * @brief Used after spawning an actor to set some default values that are not read from the
 * network event.
 * @param ent The actor edict to set the values for.
 */
static void G_ClientAssignDefaultActorValues (edict_t *ent)
{
	/* Mission Scores */
	OBJZERO(scoreMission[scoreMissionNum]);
	ent->chr.scoreMission = &scoreMission[scoreMissionNum];
	scoreMissionNum++;

	/* set initial vital statistics */
	ent->HP = ent->chr.HP;
	ent->morale = ent->chr.morale;

	/** @todo for now, heal fully upon entering mission */
	ent->morale = GET_MORALE(ent->chr.score.skills[ABILITY_MIND]);

	/* set models */
	ent->body = gi.ModelIndex(CHRSH_CharGetBody(&ent->chr));
	ent->head = gi.ModelIndex(CHRSH_CharGetHead(&ent->chr));
}

/**
 * @brief This is called after the actors are spawned and will set actor states without consuming TUs
 * @param player The player to perform the action for
 */
void G_ClientInitActorStates (const player_t * player)
{
	const int length = gi.ReadByte(); /* Get the actor amount that the client sent. */
	int i;

	for (i = 0; i < length; i++) {
		const int ucn = gi.ReadShort();
		int saveTU;
		int hand, fmIdx, objIdx;
		edict_t *ent = G_ActorGetByUCN(ucn, player->pers.team);
		if (!ent)
			gi.Error("Could not find character on team %i with unique character number %i", player->pers.team, ucn);

		/* these state changes are not consuming any TUs */
		saveTU = ent->TU;
		G_ClientStateChange(player, ent, gi.ReadShort(), qfalse);
		hand = gi.ReadShort();
		fmIdx = gi.ReadShort();
		objIdx = gi.ReadShort();
		G_ActorSetTU(ent, saveTU);
		if (objIdx != NONE) {
			G_ReactionFireUpdate(ent, fmIdx, hand, INVSH_GetItemByIDX(objIdx));
		}
		G_ClientStateChangeUpdate(ent);
	}
}

/**
 * @brief The client lets the server spawn the actors for a given player by sending their information (models, inventory, etc..) over the network.
 * @param[in] player The player to spawn the actors for.
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa clc_teaminfo
 */
void G_ClientTeamInfo (const player_t * player)
{
	const int length = gi.ReadByte(); /* Get the actor amount that the client sent. */
	int i;

	for (i = 0; i < length; i++) {
		const actorSizeEnum_t actorFieldSize = gi.ReadByte();
		/* Search for a spawn point for each entry the client sent */
		if (player->pers.team == TEAM_NO_ACTIVE || !G_ActorSpawnIsAllowed(i, player->pers.team))
			G_ClientSkipActorInfo();
		else {
			edict_t *ent = G_ClientGetFreeSpawnPointForActorSize(player, actorFieldSize);
			if (ent) {
				Com_DPrintf(DEBUG_GAME, "Player: %i - team %i - size: %i\n", player->num, ent->team, ent->fieldSize);

				G_ClientReadCharacter(ent);

				G_ClientReadInventory(ent);

				G_ClientAssignDefaultActorValues(ent);

				G_ActorGiveTimeUnits(ent);

				G_TouchTriggers(ent);
			} else {
				gi.DPrintf("Not enough spawn points for team %i (actorsize: %i)\n", player->pers.team, actorFieldSize);

				G_ClientSkipActorInfo();
			}
		}
	}

	Com_Printf("Used inventory slots client %s spawn: %i\n", player->pers.netname, game.i.GetUsedSlots(&game.i));
}

/**
 * @brief Send brush models for entities like func_breakable and func_door and triggers
 * with their mins and maxs bounding boxes to the client and let him know about them.
 * There are also entities that are announced here, but fully handled clientside - like
 * func_rotating.
 * @sa CL_AddBrushModel
 * @sa EV_ADD_BRUSH_MODEL
 * @param[in] player The player the edicts are send to
 */
static void G_ClientSendEdictsAndBrushModels (const player_t *player)
{
	const int mask = G_PlayerToPM(player);
	/* skip the world */
	edict_t *ent = G_EdictsGetFirst();

	/* make SOLID_BSP edicts visible to the client */
	while ((ent = G_EdictsGetNextInUse(ent))) {
		/* brush models that have a type - not the world - keep in
		 * mind that there are several world edicts in the list in case of
		 * a map assembly */
		if (ent->solid != SOLID_BSP)
			continue;

		/* skip the world(s) in case of map assembly */
		if (ent->type > ET_NULL) {
			G_EventAddBrushModel(mask, ent);
			G_VisFlagsAdd(ent, ~ent->visflags);
		}
	}
}

/**
 * @brief This functions starts the client
 * @sa G_ClientStartMatch
 * @sa CL_StartGame
 */
qboolean G_ClientBegin (player_t* player)
{
	player->began = qtrue;
	level.numplayers++;

	/* find a team */
	G_GetTeam(player);
	if (!player->began)
		return qfalse;

	gi.ConfigString(CS_PLAYERCOUNT, "%i", level.numplayers);

	/* spawn camera (starts client rendering) */
	G_EventStart(player, sv_teamplay->integer);

	/* send things like doors and breakables */
	G_ClientSendEdictsAndBrushModels(player);

	/* ensure that the start event is send */
	gi.EndEvents();

	/* set the net name */
	gi.ConfigString(CS_PLAYERNAMES + player->num, "%s", player->pers.netname);

	/* inform all clients */
	gi.BroadcastPrintf(PRINT_CONSOLE, "%s has joined team %i\n", player->pers.netname, player->pers.team);

	return qtrue;
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
void G_ClientStartMatch (player_t * player)
{
	G_GetStartingTeam(player);

	/* do all the init events here... */
	/* reset the data */
	G_EventReset(player, level.activeTeam);

	/* show visible actors and add invisible actor */
	G_VisFlagsClear(player->pers.team);
	G_CheckVisPlayer(player, qfalse);
	G_SendInvisible(player);

	/* submit stats */
	G_SendPlayerStats(player);

	/* ensure that the last event is send, too */
	gi.EndEvents();

	if (sv_maxclients->integer > 1) {
		/* ensure that we restart the round time limit */
		sv_roundtimelimit->modified = qtrue;
	}

	/* inform all clients */
	gi.BroadcastPrintf(PRINT_CONSOLE, "%s has taken control over team %i.\n", player->pers.netname, player->pers.team);
}

/**
 * @brief called whenever the player updates a userinfo variable.
 * @note The game can override any of the settings in place (forcing skins or names, etc) before copying it off.
 */
void G_ClientUserinfoChanged (player_t * player, const char *userinfo)
{
	const qboolean alreadyReady = player->isReady;
	const int oldTeamnum = Info_IntegerForKey(player->pers.userinfo, "cl_teamnum");

	/* check for malformed or illegal info strings */
	if (!Info_Validate(userinfo))
		userinfo = "\\cl_name\\badinfo";

	/* set name */
	Q_strncpyz(player->pers.netname, Info_ValueForKey(userinfo, "cl_name"), sizeof(player->pers.netname));
	Q_strncpyz(player->pers.userinfo, userinfo, sizeof(player->pers.userinfo));
	player->autostand = Info_IntegerForKey(userinfo, "cl_autostand");
	player->reactionLeftover = Info_IntegerForKey(userinfo, "cl_reactionleftover");
	player->isReady = Info_IntegerForKey(userinfo, "cl_ready");

	/* send the updated config string */
	gi.ConfigString(CS_PLAYERNAMES + player->num, "%s", player->pers.netname);

	/* try to update to the preferred team */
	if (!G_MatchIsRunning() && oldTeamnum != Info_IntegerForKey(userinfo, "cl_teamnum")) {
		/* if the player is marked as ready he can't change his team */
		if (!alreadyReady || !player->isReady) {
			player->pers.team = TEAM_NO_ACTIVE;
			G_GetTeam(player);
		} else {
			Com_DPrintf(DEBUG_GAME, "G_ClientUserinfoChanged: player %s is already marked as being ready\n",
					player->pers.netname);
		}
	}
}

/**
 * @brief Checks whether the connection is valid or invalid and set some
 * user info keys.
 * @param[in,out] player The player that is trying to connect to the game
 * @param[in,out] userinfo The userinfo data that is checked and changed
 * @param[in] userinfoSize The size of the userinfo buffer
 * @sa G_ClientDisconnect
 * @sa CL_ConnectionlessPacket
 * @todo Check if the teamnum preference has already reached maxsoldiers
 * and reject connection if so
 * @return @c false if the connection is refused, @c true otherwise
 */
qboolean G_ClientConnect (player_t * player, char *userinfo, size_t userinfoSize)
{
	const char *value;

	value = Info_ValueForKey(userinfo, "ip");

	Com_Printf("connection attempt from %s\n", value);

	/* check to see if they are on the banned IP list */
	if (SV_FilterPacket(value)) {
		Info_SetValueForKey(userinfo, userinfoSize, "rejmsg", REJ_BANNED);
		return qfalse;
	}

	if (!G_PlayerToPM(player)) {
		Info_SetValueForKey(userinfo, userinfoSize, "rejmsg", "Server is full");
		return qfalse;
	}

	value = Info_ValueForKey(userinfo, "password");
	if (password->string[0] != '\0' && !Q_streq(password->string, "none") && !Q_streq(password->string, value)) {
		Info_SetValueForKey(userinfo, userinfoSize, "rejmsg", REJ_PASSWORD_REQUIRED_OR_INCORRECT);
		return qfalse;
	}

	/* fix for fast reconnects after a disconnect */
	if (player->inuse) {
		gi.BroadcastPrintf(PRINT_CONSOLE, "%s already in use.\n", player->pers.netname);
		G_ClientDisconnect(player);
	}

	/* reset persistent data */
	OBJZERO(player->pers);
	G_ClientUserinfoChanged(player, userinfo);

	gi.BroadcastPrintf(PRINT_CONSOLE, "%s is connecting...\n", player->pers.netname);
	return qtrue;
}

/**
 * @sa G_ClientConnect
 */
void G_ClientDisconnect (player_t * player)
{
#if 0
	edict_t *ent = NULL;
#endif

	/* only if the player already sent his began */
	if (player->began) {
		level.numplayers--;
		gi.ConfigString(CS_PLAYERCOUNT, "%i", level.numplayers);

		if (level.activeTeam == player->pers.team)
			G_ClientEndRound(player);

		/* if no more players are connected - stop the server */
		G_MatchEndCheck();
	}

#if 0
	/* now let's remove all the edicts that belongs to this player */
	while ((ent = G_EdictsGetNextLivingActor(ent))) {
		if (ent->pnum == player->num)
			G_ActorDie(ent, STATE_DEAD, NULL);
	}
	G_MatchEndCheck();
#endif

	player->began = qfalse;
	player->roundDone = qfalse;
	player->isReady = qfalse;

	gi.BroadcastPrintf(PRINT_CONSOLE, "%s disconnected.\n", player->pers.netname);
}

/**
 * @brief Called after every player has joined
 */
void G_ResetClientData (void)
{
	scoreMissionNum = 0;
	OBJZERO(scoreMission);
}
