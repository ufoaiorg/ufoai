/**
 * @file
 * @brief Main part of the game logic.
 *
 * @section Connection
 *
 * In case the connection is established (@c G_ClientConnect), the client
 * state is @c cs_connected. The client will send SV_STATE_NEW, @c SV_New_f sets the
 * state to @c cs_spawning, and asks the client to precache the data after
 * sending the configstrings. After the client is done with it, it will send
 * "begin" to the server. @c SV_Begin_f will now set the client state to @c cs_began
 * and calls @c G_ClientBegin. The server will ask the client to execute
 * "spawnsoldiers" now. The client is answering with @c clc_teaminfo and the server
 * will call @c G_ClientTeamInfo. The client state is now @c cs_spawned. Last but not
 * least the server informs the client that the match can now get started by
 * asking to execute "startmatch". The client answers with NET_STATE_STARTMATCH and
 * @c G_ClientStartMatch is executed.
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

#include "g_client.h"
#include "g_actor.h"
#include "g_ai.h"
#include "g_combat.h"
#include "g_edicts.h"
#include "g_inventory.h"
#include "g_match.h"
#include "g_move.h"
#include "g_reaction.h"
#include "g_utils.h"
#include "g_vis.h"

static chrScoreMission_t scoreMission[MAX_EDICTS];
static int scoreMissionNum = 0;

/**
 * @brief Iterate through the list of players
 * @param lastPlayer The player found in the previous iteration; if nullptr, we start at the beginning
 */
Player* G_PlayerGetNextHuman (Player* lastPlayer)
{
	if (!game.sv_maxplayersperteam)
		return nullptr;

	if (!lastPlayer)
		return game.players;

	const Player* endOfPlayers = &game.players[game.sv_maxplayersperteam];

	assert(lastPlayer >= game.players);
	assert(lastPlayer < endOfPlayers);

	Player* player = lastPlayer;

	player++;
	if (player >= endOfPlayers)
		return nullptr;
	else
		return player;
}

/**
 * @brief Iterate through the list of players
 * @param lastPlayer The player found in the previous iteration; if nullptr, we start at the beginning
 */
Player* G_PlayerGetNextAI (Player* lastPlayer)
{
	if (!game.sv_maxplayersperteam)
		return nullptr;

	if (!lastPlayer)
		return &game.players[game.sv_maxplayersperteam];

	const Player* endOfPlayers = &game.players[game.sv_maxplayersperteam * 2];	/* two teams */

	assert(lastPlayer >= &game.players[game.sv_maxplayersperteam]);	/* AI players are in the upper half of the players array */
	assert(lastPlayer < endOfPlayers);

	Player* player = lastPlayer;

	player++;
	if (player >= endOfPlayers)
		return nullptr;
	else
		return player;
}

/**
 * @brief Iterate through the list of players
 * @param lastPlayer The player found in the previous iteration; if nullptr, we start at the beginning
 */
Player* G_PlayerGetNextActiveHuman (Player* lastPlayer)
{
	Player* player = lastPlayer;

	while ((player = G_PlayerGetNextHuman(player))) {
		if (player->isInUse())
			return player;
	}

	return nullptr;
}

/**
 * @brief Iterate through the list of players
 * @param lastPlayer The player found in the previous iteration; if nullptr, we start at the beginning
 */
Player* G_PlayerGetNextActiveAI (Player* lastPlayer)
{
	Player* player = lastPlayer;

	while ((player = G_PlayerGetNextAI(player))) {
		if (player->isInUse())
			return player;
	}

	return nullptr;
}

/**
 * @brief Generates the player bit mask for a given team
 * @param[in] team The team to create the player bit mask for
 * @note E.g. multiplayer team play can have more than one human player on the
 * same team.
 */
playermask_t G_TeamToPM (int team)
{
	playermask_t playerMask = 0;
	Player* p = nullptr;

	/* don't handle the ai players, here */
	while ((p = G_PlayerGetNextHuman(p))) {
		if (p->isInUse() && team == p->getTeam())
			playerMask |= G_PlayerToPM(*p);
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
teammask_t G_PMToVis (playermask_t playerMask)
{
	teammask_t teamMask = 0;
	Player* p = nullptr;

	/* don't handle the ai players, here */
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		if (playerMask & G_PlayerToPM(*p))
			teamMask |= G_TeamToVisMask(p->getTeam());
	}

	return teamMask;
}

/**
 * @brief Converts vis mask to player mask
 * @param[in] teamMask The visibility bit mask (contains the team numbers) that
 * is converted to a player mask
 * @return Returns a playermask for all the teams of the connected players that
 * are marked in the given @c vis_mask.
 */
playermask_t G_VisToPM (teammask_t teamMask)
{
	playermask_t playerMask = 0;
	Player* p = nullptr;

	/* don't handle the ai players, here */
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		if (teamMask & G_TeamToVisMask(p->getTeam()))
			playerMask |= G_PlayerToPM(*p);
	}

	return playerMask;
}

/**
 * Send messages to human players
 * @param player A player (AI players are ignored here)
 * @param printLevel A numeric value to restrict and channel the printing (CONSOLE, HUD, CHAT...)
 * @param fmt A format string as in printf
 */
void G_ClientPrintf (const Player& player, int printLevel, const char* fmt, ...)
{
	/* there is no client for an AI controlled player on the server where we
	 * could send the message to */
	if (G_IsAIPlayer(&player))
		return;

	va_list ap;
	va_start(ap, fmt);
	gi.PlayerPrintf(&player, printLevel, fmt, ap);
	va_end(ap);
}

/**
 * @brief Network function to update the time units (TUs) for each team-member.
 * @param[in] team The index of the team to update the values for.
 * @sa G_SendStats
 */
void G_GiveTimeUnits (int team)
{
	Actor* actor = nullptr;

	while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, team))) {
		G_ActorGiveTimeUnits(actor);
		G_SendStats(*actor);
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
 * or perish. Might be @c nullptr.
 * @sa CL_ActorAppear
 */
void G_AppearPerishEvent (playermask_t playerMask, bool appear, Edict& check, const Edict* ent)
{
	/* test for pointless player mask */
	if (!playerMask)
		return;

	const teammask_t teamMaskDiff = G_PMToVis(playerMask);
	G_VisFlagsSwap(check, teamMaskDiff);

	if (appear) {
		/* appear */
		switch (check.type) {
		case ET_ACTOR:
		case ET_ACTOR2x2: {
			Actor* actor = makeActor(&check);
			G_EventActorAppear(playerMask, *actor, ent);
			break;
		}

		case ET_CAMERA:
			G_EventCameraAppear(playerMask, check);
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
			if (G_IsVisibleOnBattlefield(&check))
				gi.Error("Missing edict type %i in G_AppearPerishEvent", check.type);
			break;
		}
	} else if (G_IsVisibleOnBattlefield(&check)) {
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
void G_SendInvisible (const Player& player)
{
	const int team = player.getTeam();

	assert(team != TEAM_NO_ACTIVE);
	if (!level.num_alive[team])
		return;

	Actor* actor = nullptr;
	/* check visibility */
	while ((actor = G_EdictsGetNextActor(actor))) {
		if (actor->getTeam() == team)
			continue;
		/* not visible for this team - so add the le only */
		if (!G_IsVisibleForTeam(actor, team)) {
			G_EventActorAdd(G_PlayerToPM(player), *actor);
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
static bool G_ActionCheck (const Player& player, Edict* ent)
{
	if (!ent || !ent->inuse) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - object not present!"));
		return false;
	}

	if (!G_IsActor(ent)) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - not an actor!"));
		return false;
	}

	if (G_IsStunned(ent)) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - actor is stunned!"));
		return false;
	}

	if (G_IsDead(ent)) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - actor is dead!"));
		return false;
	}

	if (ent->getTeam() != player.getTeam()) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - not on same team!"));
		return false;
	}

	if (ent->getPlayerNum() != player.getNum()) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - no control over allied actors!"));
		return false;
	}

	/* could be possible */
	return true;
}

/**
 * @brief Checks whether the requested action is possible for the current active team
 * @param[in] player Which player (human player) is trying to do the action
 * @param[in] ent Which of his units is trying to do the action.
 * @param[in] TU The time units to check against the ones ent has.
 * the action with
 */
bool G_ActionCheckForCurrentTeam (const Player& player, Actor* ent, int TU)
{
	/* a generic tester if an action could be possible */
	if (level.activeTeam != player.getTeam()) {
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - it is not your turn!"));
		return false;
	}

	if (TU > G_ActorUsableTUs(ent)) {
		return false;
	}

	return G_ActionCheck(player, ent);
}

/**
 * @brief Checks whether the requested action is possible
 * @param[in] player Which player (human player) is trying to do the action
 * @param[in] actor Which of his units is trying to do the action.
 * @param[in] TU The time units to check against the ones actor has.
 * the action with
 * @sa G_ActionCheck
 */
bool G_ActionCheckForReaction (const Player& player, Actor* actor, int TU)
{
	if (TU > actor->getTus()) {
		return false;
	}

	return G_ActionCheck(player, actor);
}

/**
 * @brief Sends the actual actor turn event over the netchannel
 */
static void G_ClientTurn (Player& player, Actor* actor, dvec_t dvec)
{
	/* check if action is possible */
	if (!G_ActionCheckForCurrentTeam(player, actor, TU_TURN))
		return;

	/* check if we're already facing that direction */
	const int dir = getDVdir(dvec);
	if (actor->dir == dir)
		return;

	/* do the turn */
	G_ActorDoTurn(actor, dir);
	G_ActorUseTU(actor, TU_TURN);

	/* send the turn */
	G_EventActorTurn(*actor);

	/* send the new TUs */
	G_SendStats(*actor);

	/* end the event */
	G_EventEnd();
}

/**
 * @brief After an actor changed his state, he might get visible for other
 * players. Check the vis here and send the state change to the clients that
 * are seeing him already.
 * @param ent The actor edict
 */
static void G_ClientStateChangeUpdate (Edict& ent)
{
	/* Send the state change. */
	G_EventSendState(G_VisToPM(ent.visflags), ent);

	/* Check if the player appears/perishes, seen from other teams. */
	G_CheckVis(&ent);

	/* Calc new vis for this player. */
	G_CheckVisTeamAll(ent.getTeam(), 0, &ent);

	/* Send the new TUs. */
	G_SendStats(ent);

	/* End the event. */
	G_EventEnd();
}

/**
 * @brief Changes the state of a player/soldier.
 * @param[in,out] player The player who controls the actor
 * @param[in] actor the edict to perform the state change for
 * @param[in] reqState The bit-map of the requested state change
 * @param[in] checkaction if false only activate the events - network stuff is handled in the calling function
 * don't even use the G_ActionCheckForCurrentTeam function
 * @note Use checkaction false only for e.g. spawning values
 */
void G_ClientStateChange (const Player& player, Actor* actor, int reqState, bool checkaction)
{
	/* Check if any action is possible. */
	if (checkaction && !G_ActionCheckForCurrentTeam(player, actor, 0))
		return;

	if (!reqState)
		return;

	switch (reqState) {
	case STATE_CROUCHED: /* Toggle between crouch/stand. */
		/* Check if player has enough TUs (TU_CROUCH TUs for crouch/uncrouch). */
		if (!checkaction || G_ActionCheckForCurrentTeam(player, actor, TU_CROUCH)) {
			if (actor->isCrouched()) {
				if (!gi.CanActorStandHere(actor->fieldSize, actor->pos))
					break;
			}
			G_ToggleCrouched(actor);
			G_ActorUseTU(actor, TU_CROUCH);
			G_ActorSetMaxs(actor);
		}
		break;
	case ~STATE_REACTION: /* Request to turn off reaction fire. */
		if (actor->isReaction()) {
			if (actor->isShaken() && G_ReactionFireSettingsReserveTUs(actor)) {
				G_ClientPrintf(player, PRINT_HUD, _("Currently shaken, won't let their guard down."));
			} else {
				/* Turn off reaction fire. */
				actor->removeReaction();
				G_ActorReserveTUs(actor, 0, actor->chr.reservedTus.shot, actor->chr.reservedTus.crouch);
				if (!G_IsAI(actor))
					G_EventReactionFireChange(*actor);
			}
		}
		break;
	/* Request to turn on multi- or single-reaction fire mode. */
	case STATE_REACTION:
		/* Disable reaction fire. */
		actor->removeReaction();

		if (G_ReactionFireSettingsReserveTUs(actor)) {
			/* Enable requested reaction fire. */
			G_SetState(actor, reqState);
		}
		if (!G_IsAI(actor))
			G_EventReactionFireChange(*actor);
		break;
	default:
		gi.DPrintf("G_ClientStateChange: unknown request %i, ignoring\n", reqState);
		return;
	}

	/* Only activate the events - network stuff is handled in the calling function */
	if (!checkaction)
		return;

	G_ClientStateChangeUpdate(*actor);
}

/**
 * @brief Returns true if actor can reload weapon
 * @sa AI_ActorThink
 */
bool G_ClientCanReload (Actor* actor, containerIndex_t containerID)
{
	const objDef_t* weapon;

	if (actor->getContainer(containerID)) {
		weapon = actor->getContainer(containerID)->def();
	} else if (containerID == CID_LEFT && actor->getRightHandItem()->isHeldTwoHanded()) {
		/* Check for two-handed weapon */
		weapon = actor->getRightHandItem()->def();
	} else
		return false;

	assert(weapon);

	/* also try the temp containers */
	const Container* cont = nullptr;
	while ((cont = actor->chr.inv.getNextCont(cont, true))) {
		Item* item = nullptr;
		while ((item = cont->getNextItem(item))) {
			if (item->def()->isLoadableInWeapon(weapon))
				return true;
		}
	}
	return false;
}

/**
 * @brief Retrieve or collect a loaded weapon from any linked container for the actor's right hand
 * @note This function will also collect items from floor containers when the actor
 * is standing on a given point.
 * @sa AI_ActorThink
 */
bool G_ClientGetWeaponFromInventory (Actor* actor)
{
	/* e.g. bloodspiders are not allowed to carry or collect weapons */
	if (!actor->chr.teamDef->weapons)
		return false;

	/* search for weapons and select the one that is available easily */
	const invDef_t* bestContainer = nullptr;
	Item* theWeapon = nullptr;
	int tu = 100;
	const Container* cont = nullptr;
	while ((cont = actor->chr.inv.getNextCont(cont, true))) {
		if (cont->def()->out >= tu)
			continue;
		Item* item = nullptr;
		while ((item = cont->getNextItem(item))) {
			/* We are looking for the *fastest* way to get a weapon,
			 * no matter what kind of weapon it is. */
			assert(item->def());
			if (item->isWeapon() && !item->mustReload()) {
				theWeapon = item;
				bestContainer = cont->def();
				tu = bestContainer->out;
				break;
			}
		}
	}

	/* send request */
	const invDef_t* invDef = INVDEF(CID_RIGHT);
	if (bestContainer) {
		return G_ActorInvMove(actor, bestContainer, theWeapon, invDef, 0, 0, true);
	}
	return false;	/* no weapon found */
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
bool G_ClientUseEdict (const Player& player, Actor* actor, Edict* edict)
{
	/* check whether the actor has sufficient TUs to 'use' this edicts */
	if (!G_ActionCheckForCurrentTeam(player, actor, edict->TU))
		return false;

	if (!G_UseEdict(edict, actor))
		return false;

	/* using a group of edicts only costs TUs once (for the master) */
	G_ActorUseTU(actor, edict->TU);
	/* send the new TUs */
	G_SendStats(*actor);

	G_EventEnd();

	return true;
}

/**
 * @brief The client sent us a message that he did something. We now execute the related function(s) and notify him if necessary.
 * @param[in] player The player to execute the action for (the actor belongs to this player)
 * @note a client action will also send the server side edict number to determine the actor
 */
int G_ClientAction (Player& player)
{
	pos3_t pos;
	int i;
	int from;

	/* read the header */
	player_action_t action = (player_action_t)gi.ReadByte();
	int num = gi.ReadShort();

	Edict* ent = G_EdictsGetByNum(num);
	if (ent == nullptr)
		return action;

	/* we expext this ent to be an Actor */
	Actor* actor = makeActor(ent);

	const char* format = pa_format[action];

	switch (action) {
	case PA_NULL:
		/* do nothing on a null action */
		break;

	case PA_TURN:
		gi.ReadFormat(format, &i);
		G_ClientTurn(player, actor, (dvec_t) i);
		break;

	case PA_MOVE:
		gi.ReadFormat(format, &pos);
		G_ClientMove(player, player.getTeam(), actor, pos);
		break;

	case PA_STATE:
		gi.ReadFormat(format, &i);
		G_ClientStateChange(player, actor, i, true);
		break;

	case PA_SHOOT:
		fireDefIndex_t firemode;
		gi.ReadFormat(format, &pos, &i, &firemode, &from);
		G_ClientShoot(player, actor, pos, i, firemode, nullptr, true, from);
		break;

	case PA_INVMOVE:
		int fx, fy, to, tx, ty;
		gi.ReadFormat(format, &from, &fx, &fy, &to, &tx, &ty);

		if (!isValidContId(from) || !isValidContId(to)) {
			gi.DPrintf("G_ClientAction: PA_INVMOVE Container index out of range. (from: %i, to: %i)\n", from, to);
		} else {
			const invDef_t* fromPtr = INVDEF(from);
			const invDef_t* toPtr = INVDEF(to);
			Item* fromItem = ent->chr.inv.getItemAtPos(fromPtr, fx, fy);
			if (fromItem)
				G_ActorInvMove(actor, fromPtr, fromItem, toPtr, tx, ty, true);
		}
		break;

	case PA_USE:
		if (actor->clientAction) {
			/* read the door the client wants to open */
			gi.ReadFormat(format, &i);

			/* get the door edict */
			Edict* actionEnt = G_EdictsGetByNum(i);

			/* maybe the door is no longer 'alive' because it was destroyed */
			if (actionEnt && actor->clientAction == actionEnt) {
				if (G_IsDoor(actionEnt)) {
					G_ActorUseDoor(actor, actionEnt);
				}
			}
		}
		break;

	case PA_REACT_SELECT:
		actorHands_t hand;
		int fmIdx, objIdx;
		gi.ReadFormat(format, &hand, &fmIdx, &objIdx);
		G_ReactionFireSettingsUpdate(actor, fmIdx, hand, INVSH_GetItemByIDX(objIdx));
		break;

	case PA_RESERVE_STATE:
		int resCrouch, resShot;
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
static void G_GetTeam (Player& player)
{
	/* player has already a team */
	if (player.getTeam() > 0) {
		Com_DPrintf(DEBUG_GAME, "Player %s is already on team %i\n", player.pers.netname, player.getTeam());
		return;
	}

	/* number of currently connected players (no ai players) */
	int playersInGame = 0;
	Player* p = nullptr;
	while ((p = G_PlayerGetNextActiveHuman(p)))
		playersInGame++;

	/* randomly assign a teamnumber in deathmatch games */
	if (playersInGame <= 1 && G_IsMultiPlayer() && !sv_teamplay->integer) {
		int spawnCheck[MAX_TEAMS];
		int spawnSpots = 0;
		/* skip civilian teams */
		for (int i = TEAM_PHALANX; i < MAX_TEAMS; i++) {
			spawnCheck[i] = 0;
			/* check whether there are spawnpoints for this team */
			if (level.num_spawnpoints[i])
				spawnCheck[spawnSpots++] = i;
		}
		/* we need at least 2 different team spawnpoints for multiplayer in a death match game */
		if (spawnSpots < 2)
			gi.Error("G_GetTeam: Not enough spawn spots in map!");

		/* assign random valid team number */
		int i = spawnSpots;
		int randomSpot = rand() % spawnSpots;
		for (;;) {
			const int team = spawnCheck[randomSpot];
			if (i == 0)
				gi.Error("G_GetTeam: Could not assign a team!");
			if (G_SetTeamForPlayer(player, team)) {
				gi.DPrintf("%s has been randomly assigned to team %i\n",
						player.pers.netname, G_ClientGetTeamNum(player));
				break;
			}
			i--;
			randomSpot = (randomSpot + 1) % spawnSpots;
		}
		return;
	}

	/* find a team */
	if (G_IsSinglePlayer()) {
		G_SetTeamForPlayer(player, TEAM_PHALANX);
	} else if (sv_teamplay->integer) {
		/* set the team specified in the userinfo */
		const int i = G_ClientGetTeamNumPref(player);
		gi.DPrintf("Get a team for teamplay for %s\n", player.pers.netname);
		/* civilians are at team zero */
		if (i > TEAM_CIVILIAN && sv_maxteams->integer >= i) {
			G_SetTeamForPlayer(player, i);
			gi.BroadcastPrintf(PRINT_CONSOLE, "serverconsole: %s has chosen team %i\n", player.pers.netname, i);
		} else {
			gi.DPrintf("Team %i is not valid - choose a team between 1 and %i\n", i, sv_maxteams->integer);
			G_SetTeamForPlayer(player, TEAM_DEFAULT);
		}
	} else {
		int i;
		/* search team */
		gi.DPrintf("Getting a multiplayer team for %s\n", player.pers.netname);
		for (i = TEAM_CIVILIAN + 1; i < MAX_TEAMS; i++) {
			if (level.num_spawnpoints[i]) {
				bool teamAvailable = true;

				p = nullptr;
				/* check if team is in use (only human controlled players) */
				while ((p = G_PlayerGetNextActiveAI(p))) {
					if (p->getTeam() == i) {
						Com_DPrintf(DEBUG_GAME, "Team %i is already in use\n", i);
						/* team already in use */
						teamAvailable = false;
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
			p = nullptr;
			while ((p = G_PlayerGetNextActiveHuman(p))) {
				if (p->getTeam() == i) {
					gi.BroadcastPrintf(PRINT_CONSOLE, "Removing ai player...");
					p->setInUse(false);
					break;
				}
			}
			Com_DPrintf(DEBUG_GAME, "Assigning %s to team %i\n", player.pers.netname, i);
			G_SetTeamForPlayer(player, i);
		} else {
			gi.DPrintf("No free team - disconnecting '%s'\n", player.pers.netname);
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
bool G_SetTeamForPlayer (Player& player, const int team)
{
	assert(team >= TEAM_NO_ACTIVE && team < MAX_TEAMS);

	if (G_IsAIPlayer(&player)) {
		if (team != TEAM_ALIEN && team != TEAM_CIVILIAN)
			return false;
	} else {
		if (!sv_teamplay->integer) {
			Player* p = nullptr;
			while ((p = G_PlayerGetNextHuman(p)) != nullptr) {
				if (p->getTeam() == team)
					return false;
			}
		}
	}

	player.setTeam(team);

	/* if we started in dev mode, we maybe don't have a
	 * starting position in this map */
	if (!g_nospawn->integer) {
		if (team >= 0 && team < MAX_TEAMS) {
			if (!level.num_spawnpoints[team])
				gi.Error("No spawnpoints for team %i", team);
		}
	}

	if (!G_IsAIPlayer(&player))
		Info_SetValueForKeyAsInteger(player.pers.userinfo, sizeof(player.pers.userinfo), "cl_team", team);

	return true;
}

/**
 * @brief Returns the assigned team number of the player
 */
int G_ClientGetTeamNum (const Player& player)
{
	return player.getTeam();
}

/**
 * @brief Returns the preferred team number for the player
 */
int G_ClientGetTeamNumPref (const Player& player)
{
	return Info_IntegerForKey(player.pers.userinfo, "cl_teamnum");
}

/**
 * @return @c true if the player is for starting the multiplayer match
 */
bool G_ClientIsReady (const Player* player)
{
	assert(player);
	return player->isReady();
}

/**
 * @brief Chose a team that should start the match
 * @param[in] player In singleplayer mode the team of this player will get the first turn
 * @sa SVCmd_StartGame_f
 */
static void G_GetStartingTeam (const Player& player)
{
	/* return with no action if activeTeam already assigned or if are in single-player mode */
	if (G_MatchIsRunning())
		return;

	if (G_IsSinglePlayer()) {
		level.activeTeam = player.getTeam();
		level.teamOfs = MAX_TEAMS - level.activeTeam;
		return;
	}

	/* count number of currently connected unique teams and players (human controlled players only) */
	int knownTeams[MAX_TEAMS];
	Player* p = nullptr;
	int teamCount = 0;
	int playerCount = 0;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		int j;
		playerCount++;
		for (j = 0; j < teamCount; j++) {
			if (p->getTeam() == knownTeams[j])
				break;
		}
		if (j == teamCount)
			knownTeams[teamCount++] = p->getTeam();
	}

	if (teamCount) {
		const int teamIndex = (int) (frand() * (teamCount - 1) + 0.5);
		G_PrintStats("Starting new game: %s with %i teams", level.mapname, teamCount);
		level.activeTeam = knownTeams[teamIndex];
		level.teamOfs = MAX_TEAMS - level.activeTeam;
		p = nullptr;
		while ((p = G_PlayerGetNextActiveHuman(p)))
			if (p->getTeam() != level.activeTeam)
				p->roundDone = true;
	}
}

/**
 * @brief Find valid actor spawn fields for this player.
 * @note Already used spawn-point are not found because ent->type is changed in G_ClientTeamInfo.
 * @param[in] player The player to spawn the actors for.
 * @param[in] spawnType The type of spawn-point so search for (ET_ACTORSPAWN or ET_ACTOR2x2SPAWN)
 * @return A pointer to a found spawn point or nullptr if nothing was found or on error.
 */
static Edict* G_ClientGetFreeSpawnPoint (const Player& player, int spawnType)
{
	/* Abort for non-spawnpoints */
	assert(spawnType == ET_ACTORSPAWN || spawnType == ET_ACTOR2x2SPAWN);

	Edict* ent = nullptr;
	if (level.noRandomSpawn) {
		while ((ent = G_EdictsGetNextInUse(ent)))
			if (ent->type == spawnType && player.getTeam() == ent->getTeam()) {
				if (G_EdictsGetLivingActorFromPos(ent->pos))
					continue;
				return ent;
			}
	} else {
		Edict* list[MAX_EDICTS];
		int count = 0;
		while ((ent = G_EdictsGetNextInUse(ent)))
			if (ent->type == spawnType && player.getTeam() == ent->getTeam()) {
				if (G_EdictsGetLivingActorFromPos(ent->pos))
					continue;
				list[count++] = ent;
			}

		if (count)
			return list[rand() % count];
	}

	return nullptr;
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
static inline bool G_ActorSpawnIsAllowed (const int num, const int team)
{
	if (G_IsSinglePlayer())
		return true;

	return num < sv_maxsoldiersperplayer->integer && level.num_spawned[team] < sv_maxsoldiersperteam->integer;
}

/**
 * @brief Think function for actors that spawn dead
 * @param ent The actor
 */
static void G_ThinkActorDieAfterSpawn (Edict* ent)
{
	/* We can safely assume that this is an Actor */
	Actor* actor = makeActor(ent);
	G_ActorDieOrStun(actor, nullptr);
	actor->think = nullptr;
}

/**
 * @brief Think function for actors that spawn crouched
 * @param ent The actor
 */
static void G_ThinkActorGoCrouch (Edict* ent)
{
	Actor* actor = makeActor(ent);
	G_ClientStateChange(actor->getPlayer(), actor, STATE_CROUCHED, true);
	actor->think = nullptr;
}

/**
 * @brief Searches a free spawning point for a given actor size and turns it into an actor
 * @param[in] player The player to get the free spawn points for
 * @param[in] actorSize The actor size to get a spawning point for
 * @return An actor edict or @c nullptr if no free spawning point was found
 */
Actor* G_ClientGetFreeSpawnPointForActorSize (const Player& player, const actorSizeEnum_t actorSize)
{
	Edict* ent;

	if (actorSize == ACTOR_SIZE_NORMAL) {
		/* Find valid actor spawn fields for this player. */
		ent = G_ClientGetFreeSpawnPoint(player, ET_ACTORSPAWN);
		if (ent) {
			/* preserve the spawpoint so we can later spawn more (alien rush) */
			Edict* copy = G_EdictDuplicate(ent);
			if (copy != nullptr)
				copy->type = ET_ACTOR;
			ent = copy;
		}
	} else if (actorSize == ACTOR_SIZE_2x2) {
		/* Find valid actor spawn fields for this player. */
		ent = G_ClientGetFreeSpawnPoint(player, ET_ACTOR2x2SPAWN);
		if (ent) {
			Edict* copy = G_EdictDuplicate(ent);
			if (copy != nullptr) {
				copy->type = ET_ACTOR2x2;
				copy->setMorale(100);
			}
			ent = copy;
		}
	} else {
		gi.Error("G_ClientGetFreeSpawnPointForActorSize: unknown fieldSize for actor edict (actorSize: %i)\n", actorSize);
	}

	if (!ent)
		return nullptr;

	level.num_spawned[ent->getTeam()]++;
	ent->setPlayerNum(player.getNum());
	ent->chr.fieldSize = actorSize;
	ent->fieldSize = ent->chr.fieldSize;
	ent->flags |= FL_DESTROYABLE;
	G_VisFlagsReset(*ent);
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

	G_ActorModifyCounters(nullptr, ent, 1, 0, 0);

	G_ReactionFireTargetsCreate(ent);

	return makeActor(ent);
}

/**
 * @brief Read the inventory from the clients team data
 * @param ent The actor's entity that should receive the items.
 */
static void G_ClientReadInventory (Edict* ent)
{
	/* inventory */
	for (int nr = gi.ReadShort(); nr > 0; nr--) {
		const invDef_t* container;
		Item item;
		int x, y;
		G_ReadItem(&item, &container, &x, &y);
		if (container->temp)
			gi.Error("G_ClientReadInventory failed, tried to add '%s' to a temp container %i", item.def()->id, container->id);
		/* ignore the overload for now */
		if (!ent->chr.inv.canHoldItemWeight(CID_EQUIP, container->id, item, ent->chr.score.skills[ABILITY_POWER]))
			Com_Printf("G_ClientReadInventory: Item %s exceeds ent %i weight capacity\n", item.def()->id, ent->getIdNum());
		if (!level.noEquipment && game.invi.addToInventory(&ent->chr.inv, &item, container, x, y, 1) == nullptr)
			gi.Error("G_ClientReadInventory failed, could not add item '%s' to container %i (x:%i,y:%i)",
					item.def()->id, container->id, x, y);
	}
}

/**
 * @brief Reads the character data from the netchannel that is needed to spawn an actor.
 * @param ent The actor entity to save the read data in.
 */
static void G_ClientReadCharacter (Edict* ent)
{
	ent->chr.init();
	/* model */
	ent->chr.ucn = gi.ReadShort();
	gi.ReadString(ent->chr.name, sizeof(ent->chr.name));
	gi.ReadString(ent->chr.path, sizeof(ent->chr.path));
	gi.ReadString(ent->chr.body, sizeof(ent->chr.body));
	gi.ReadString(ent->chr.head, sizeof(ent->chr.head));
	ent->chr.bodySkin = gi.ReadByte();
	ent->chr.headSkin = gi.ReadByte();

	ent->chr.HP = gi.ReadShort();
	ent->chr.minHP = ent->chr.HP;
	ent->chr.maxHP = gi.ReadShort();
	const int teamDefIdx = gi.ReadByte();
	if (teamDefIdx < 0 || teamDefIdx >= MAX_TEAMDEFS)
		gi.Error("Invalid team definition index given: %i", teamDefIdx);
	ent->chr.teamDef = &gi.csi->teamDef[teamDefIdx];

	ent->chr.gender = gi.ReadByte();
	ent->chr.STUN = gi.ReadByte();
	ent->chr.morale = gi.ReadByte();

	for (int k = 0; k < ent->chr.teamDef->bodyTemplate->numBodyParts(); ++k)
		ent->chr.wounds.treatmentLevel[k] = gi.ReadByte();

	for (int k = 0; k < SKILL_NUM_TYPES + 1; k++) /* new attributes */
		ent->chr.score.experience[k] = gi.ReadLong();
	for (int k = 0; k < SKILL_NUM_TYPES; k++) /* new attributes */
		ent->chr.score.skills[k] = gi.ReadByte();
	for (int k = 0; k < KILLED_NUM_TYPES; k++)
		ent->chr.score.kills[k] = gi.ReadShort();
	for (int k = 0; k < KILLED_NUM_TYPES; k++)
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
	Edict ent;
	invDef_t container;
	Item item;
	int x, y;
	const invDef_t* c = &container;

	G_ClientReadCharacter(&ent);

	/* skip inventory */
	const int n = gi.ReadShort();
	for (int i = 0; i < n; i++) {
		G_ReadItem(&item, &c, &x, &y);
	}
}

/**
 * @brief Used after spawning an actor to set some default values that are not read from the
 * network event.
 * @param actor The actor edict to set the values for.
 */
static void G_ClientAssignDefaultActorValues (Actor* actor)
{
	/* Mission Scores */
	scoreMission[scoreMissionNum].init();
	actor->chr.scoreMission = &scoreMission[scoreMissionNum];
	scoreMissionNum++;

	/* set initial vital statistics */
	actor->HP = actor->chr.HP;
	actor->setMorale(actor->chr.morale);

	/** @todo for now, heal fully upon entering mission */
	actor->setMorale(GET_MORALE(actor->chr.score.skills[ABILITY_MIND]));

	/* set models */
	actor->setBody(gi.ModelIndex(CHRSH_CharGetBody(&actor->chr)));
	actor->setHead(gi.ModelIndex(CHRSH_CharGetHead(&actor->chr)));

	actor->chr.scoreMission->carriedWeight = actor->chr.inv.getWeight();
}

/**
 * @brief This is called after the actors are spawned and will set actor states without consuming TUs
 * @param player The player to perform the action for
 */
void G_ClientInitActorStates (const Player& player)
{
	const int length = gi.ReadByte(); /* Get the actor amount that the client sent. */

	for (int i = 0; i < length; i++) {
		const int ucn = gi.ReadShort();
		Actor* actor = G_EdictsGetActorByUCN(ucn, player.getTeam());
		if (!actor) {
			gi.DPrintf("Could not find character on team %i with unique character number %i\n", player.getTeam(), ucn);
			/* Skip actor info */
			gi.ReadShort();
			gi.ReadShort();
			gi.ReadShort();
			gi.ReadShort();
			continue;
		}

		/* these state changes are not consuming any TUs */
		const int saveTU = actor->getTus();
		G_ClientStateChange(player, actor, gi.ReadShort(), false);
		const actorHands_t hand = (actorHands_t)gi.ReadShort();
		const fireDefIndex_t fmIdx = gi.ReadShort();
		const int objIdx = gi.ReadShort();
		G_ActorSetTU(actor, saveTU);
		if (objIdx != NONE) {
			if (fmIdx == NONE)
				G_ReactionFireSettingsReserveTUs(actor);
			else
				G_ReactionFireSettingsUpdate(actor, fmIdx, hand, INVSH_GetItemByIDX(objIdx));
		}
		G_ClientStateChangeUpdate(*actor);
	}
}

/**
 * @brief The client lets the server spawn the actors for a given player by sending their information (models, inventory, etc..) over the network.
 * @param[in] player The player to spawn the actors for.
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa clc_teaminfo
 */
void G_ClientTeamInfo (const Player& player)
{
	const int length = gi.ReadByte(); /* Get the actor amount that the client sent. */

	for (int i = 0; i < length; i++) {
		const actorSizeEnum_t actorFieldSize = gi.ReadByte();
		/* Search for a spawn point for each entry the client sent */
		if (player.getTeam() == TEAM_NO_ACTIVE || !G_ActorSpawnIsAllowed(i, player.getTeam()))
			G_ClientSkipActorInfo();
		else {
			Actor* actor = G_ClientGetFreeSpawnPointForActorSize(player, actorFieldSize);
			if (actor) {
				Com_DPrintf(DEBUG_GAME, "Player: %i - team %i - size: %i\n", player.getNum(), actor->getTeam(), actor->fieldSize);

				G_ClientReadCharacter(actor);
				G_ClientReadInventory(actor);
				G_ClientAssignDefaultActorValues(actor);
				G_ActorGiveTimeUnits(actor);
				G_TouchTriggers(actor);
				actor->contentFlags = G_ActorGetContentFlags(actor->origin);
				AIL_InitActor(actor);
			} else {
				gi.DPrintf("Not enough spawn points for team %i (actorsize: %i)\n", player.getTeam(), actorFieldSize);

				G_ClientSkipActorInfo();
			}
		}
	}

	Com_Printf("Used inventory slots client %s spawn: %i\n", player.pers.netname, game.invi.GetUsedSlots());
}

/**
 * @brief Send brush models for entities like func_breakable and func_door and triggers
 * with their bounding boxes to the client and let him know about them.
 * There are also entities that are announced here, but fully handled clientside - like
 * func_rotating.
 * @sa CL_AddBrushModel
 * @sa EV_ADD_BRUSH_MODEL
 * @param[in] player The player the edicts are send to
 */
static void G_ClientSendEdictsAndBrushModels (const Player& player)
{
	const int mask = G_PlayerToPM(player);
	/* skip the world */
	Edict* ent = G_EdictsGetFirst();

	/* make SOLID_BSP edicts visible to the client */
	while ((ent = G_EdictsGetNextInUse(ent))) {
		/* brush models that have a type - not the world - keep in
		 * mind that there are several world edicts in the list in case of
		 * a map assembly */
		if (ent->solid != SOLID_BSP)
			continue;

		/* skip the world(s) in case of map assembly */
		if (ent->type > ET_NULL) {
			G_EventAddBrushModel(mask, *ent);
			G_VisFlagsAdd(*ent, ~ent->visflags);
		}
	}
}

/**
 * @brief This functions starts the client
 * @sa G_ClientStartMatch
 * @sa CL_StartGame
 */
bool G_ClientBegin (Player& player)
{
	player.began = true;
	level.numplayers++;

	/* find a team */
	G_GetTeam(player);
	if (!player.began)
		return false;

	gi.ConfigString(CS_PLAYERCOUNT, "%i", level.numplayers);

	/* spawn camera (starts client rendering) */
	G_EventStart(player, sv_teamplay->integer);

	/* send things like doors and breakables */
	G_ClientSendEdictsAndBrushModels(player);

	/* ensure that the start event is send */
	G_EventEnd();

	/* set the net name */
	gi.ConfigString(CS_PLAYERNAMES + player.getNum(), "%s", player.pers.netname);

	/* inform all clients */
	gi.BroadcastPrintf(PRINT_CONSOLE, "%s has joined team %i\n", player.pers.netname, player.getTeam());

	return true;
}

/**
 * @brief Sets the team, init the TU and sends the player stats.
 * @sa G_SendPlayerStats
 * @sa G_GetTeam
 * @sa G_GiveTimeUnits
 * @sa G_ClientBegin
 * @sa CL_Reset
 */
void G_ClientStartMatch (Player& player)
{
	G_GetStartingTeam(player);

	/* do all the init events here... */
	/* reset the data */
	G_EventReset(player, level.activeTeam);

	/* show visible actors and add invisible actor */
	G_VisFlagsClear(player.getTeam());
	G_CheckVisPlayer(player, false);
	G_SendInvisible(player);

	/* submit stats */
	G_SendPlayerStats(player);

	/* ensure that the last event is send, too */
	G_EventEnd();

	if (G_IsMultiPlayer()) {
		/* ensure that we restart the round time limit */
		sv_roundtimelimit->modified = true;
	}

	/* inform all clients */
	gi.BroadcastPrintf(PRINT_CONSOLE, "%s has taken control over team %i.\n", player.pers.netname, player.getTeam());
}

/**
 * @brief called whenever the player updates a userinfo variable.
 * @note The game can override any of the settings in place (forcing skins or names, etc) before copying it off.
 */
void G_ClientUserinfoChanged (Player& player, const char* userinfo)
{
	const bool alreadyReady = player.isReady();
	const int oldTeamnum = Info_IntegerForKey(player.pers.userinfo, "cl_teamnum");

	/* check for malformed or illegal info strings */
	if (!Info_Validate(userinfo))
		userinfo = "\\cl_name\\badinfo";

	/* set name */
	Q_strncpyz(player.pers.netname, Info_ValueForKey(userinfo, "cl_name"), sizeof(player.pers.netname));
	Q_strncpyz(player.pers.userinfo, userinfo, sizeof(player.pers.userinfo));
	player.autostand = Info_IntegerForKey(userinfo, "cl_autostand");
	player.setReady(Info_IntegerForKey(userinfo, "cl_ready"));

	/* send the updated config string */
	gi.ConfigString(CS_PLAYERNAMES + player.getNum(), "%s", player.pers.netname);

	/* try to update to the preferred team */
	if (!G_MatchIsRunning() && oldTeamnum != Info_IntegerForKey(userinfo, "cl_teamnum")) {
		/* if the player is marked as ready he can't change his team */
		if (!alreadyReady || !player.isReady()) {
			player.setTeam(TEAM_NO_ACTIVE);
			G_GetTeam(player);
		} else {
			Com_DPrintf(DEBUG_GAME, "G_ClientUserinfoChanged: player %s is already marked as being ready\n",
					player.pers.netname);
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
bool G_ClientConnect (Player* player, char* userinfo, size_t userinfoSize)
{
	const char* value = Info_ValueForKey(userinfo, "ip");

	Com_Printf("connection attempt from %s\n", value);

	/* check to see if they are on the banned IP list */
	if (SV_FilterPacket(value)) {
		Info_SetValueForKey(userinfo, userinfoSize, "rejmsg", REJ_BANNED);
		return false;
	}

	if (!G_PlayerToPM(*player)) {
		Info_SetValueForKey(userinfo, userinfoSize, "rejmsg", REJ_SERVER_FULL);
		return false;
	}

	value = Info_ValueForKey(userinfo, "password");
	if (password->string[0] != '\0' && !Q_streq(password->string, "none") && !Q_streq(password->string, value)) {
		Info_SetValueForKey(userinfo, userinfoSize, "rejmsg", REJ_PASSWORD_REQUIRED_OR_INCORRECT);
		return false;
	}

	/* fix for fast reconnects after a disconnect */
	if (player->isInUse()) {
		gi.BroadcastPrintf(PRINT_CONSOLE, "%s already in use.\n", player->pers.netname);
		G_ClientDisconnect(*player);
	}

	/* reset persistent data */
	OBJZERO(player->pers);
	G_ClientUserinfoChanged(*player, userinfo);

	gi.BroadcastPrintf(PRINT_CONSOLE, "%s is connecting...\n", player->pers.netname);
	return true;
}

/**
 * @sa G_ClientConnect
 */
void G_ClientDisconnect (Player& player)
{
	/* only if the player already sent his began */
	if (player.began) {
		level.numplayers--;
		gi.ConfigString(CS_PLAYERCOUNT, "%i", level.numplayers);

		if (level.activeTeam == player.getTeam())
			G_ClientEndRound(player);

		/* if no more players are connected - stop the server */
		G_MatchEndCheck();
	}

#if 0
	/* now let's remove all the edicts that belongs to this player */
	Actor* actor = nullptr;
	while ((actor = G_EdictsGetNextLivingActor(actor))) {
		if (actor->pnum == player.num)
			G_ActorDie(actor, STATE_DEAD, nullptr);
	}
	G_MatchEndCheck();
#endif

	player.began = false;
	player.roundDone = false;
	player.setReady(false);

	gi.BroadcastPrintf(PRINT_CONSOLE, "%s disconnected.\n", player.pers.netname);
}

/**
 * @brief Called after every player has joined
 */
void G_ResetClientData (void)
{
	scoreMissionNum = 0;
	OBJZERO(scoreMission);
}
