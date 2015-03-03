/**
 * @file
 * @brief Artificial Intelligence for opponents or human controlled actors that are panicked.
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

#include "g_ai.h"
#include "g_actor.h"
#include "g_client.h"
#include "g_combat.h"
#include "g_edicts.h"
#include "g_health.h"
#include "g_inventory.h"
#include "g_move.h"
#include "g_utils.h"
#include "g_vis.h"
#include "g_reaction.h"

class AiAction {
public:
	pos3_t to;			/**< grid pos to walk to for performing the action */
	pos3_t stop;		/**< grid pos to end turn at (e.g. hiding spots) */
	shoot_types_t shootType;	/**< the shoot type */
	byte shots;			/**< how many shots can this actor do - only set this if the target is an actor */
	Edict* target;		/**< the target edict */
	const fireDef_t* fd;/**< the firemode to use for shooting */
	int z_align;		/**< the z-align for every shot */

	inline void reset() {
		OBJZERO(*this);
	}
};

/**
 * @brief Initializes an AiAreaSearch object to default values.
 */
AiAreaSearch::AiAreaSearch(void) {
	plotArea(pos3_origin, 0);
}

/**
 * @brief Initializes an AiAreaSearch object with specific starting point and search radius.
 */
AiAreaSearch::AiAreaSearch(const pos3_t origin, int radius, bool flat) {
	plotArea(origin, radius, flat);
}

/**
 * @brief Clear AiAreaSearch internal data.
 */
AiAreaSearch::~AiAreaSearch(void) {
	_area.clear();
}

/**
 * @brief Get next position in the search area.
 * @param[out] pos The next position in the search area.
 * return @c true if a new position was found @c false otherwise.
 */
bool AiAreaSearch::getNext(pos3_t pos) {
	return _area.dequeue(pos);
}

/** @brief Clear LQueue internal data. */
AiAreaSearch::LQueue::~LQueue(void) {
	clear();
}

/**
 * @brief Add an entry to the queue.
 * @param[in] data Data to add.
 */
void AiAreaSearch::LQueue::enqueue(const pos3_t data) {
	qnode_s* node = static_cast<qnode_s*>(G_TagMalloc(sizeof(qnode_s), TAG_LEVEL));
	VectorCopy(data, node->data);
	node->next = nullptr;

	if (isEmpty())
		_head = _tail = node;
	else {
		_tail->next = node;
		_tail = node;
	}
	_count++;
}

/**
 * @brief Retrieve an entry form the queue.
 * @param[out] The data retrieved.
 * @return @c true if the data was retrieved @c false otherwise.
 */
bool AiAreaSearch::LQueue::dequeue(pos3_t data) {
	if (isEmpty())
		return false;

	VectorCopy(_head->data, data);
	qnode_s* node = _head;
	_head = _head->next;
	_count--;
	G_MemFree(node);

	return true;
}

/**
 * @brief Remove all data from the queue.
 */
void AiAreaSearch::LQueue::clear(void) {
	qnode_s* next = nullptr;
	for (qnode_s* node = _head; node; node = next) {
		next = node->next;
		G_MemFree(node);
	}
	_head = _tail = nullptr;
	_count = 0;
}

/**
 * @brief Calculate the search area.
 * @param[in] origin The starting position for the search.
 * @param[in] radius Radius to search around the starting position.
 * @param[in] flat If true only the level with the starting position is searched.
 */
void AiAreaSearch::plotArea(const pos3_t origin, int radius, bool flat) {
		/* Starting with a sphere with radius 0 (just the center pos) out to the given radius */
		for (int i = 0; i <= radius; ++i) {
			/* Battlescape has max PATHFINDING_HEIGHT levels, so cap the z offset to the max levels
			 * the unit can currently go up or down */
			const int zOfsMax = std::min(std::max(static_cast<int>(origin[2]), PATHFINDING_HEIGHT - origin[2] - 1), i);
			/* Starting a the center z level up to zOfsMax levels */
			if (flat)
				plotCircle(origin, i);
			else
				for (int zOfs = 0; zOfs <= zOfsMax; ++zOfs) {
					pos3_t center = {origin[0], origin[1], static_cast<pos_t>(origin[2] + zOfs)};
					plotCircle(center, i - zOfs);
					/* We need to cover both up and down directions, so we Flip the sign (if there's actually a z offset) */
					if (zOfs != 0) {
						center[2] = origin[2] - zOfs;
						plotCircle(center, i - zOfs);
					}
				}
		}
}

void AiAreaSearch::plotCircle(const pos3_t origin, int radius) {
	/* There are always more levels in one direction than the other (since the number is even)
	 * so check if out of bounds */
	if (origin[2] >= PATHFINDING_HEIGHT)
		return;
	/* This is the main loop of the midpoint circle algorithm this specific variation
	 * circles of consecutive integer radius won't overlap in any point or leave gaps in between,
	 * Coincidentally (at time of writing) a circle of radius n will be exactly the farthest a normal actor can walk
	 * with n * 2 TU, in ideal conditions (standing, no obstacles, no penalties) */
	for (int dy = 1, xOfs = 0, yOfs = radius; xOfs <= yOfs; ++xOfs, dy += 1, yOfs = radius - (dy >> 1)) {
		/* Have the offsets, now to fill the octants */
		plotPos(origin, xOfs, yOfs);
		/* Need to Flip the signs, for each one */
		if (xOfs != 0) {
			plotPos(origin, -xOfs, yOfs);
		}
		if (yOfs != 0) {
			plotPos(origin, xOfs, -yOfs);
		}
		if (xOfs != 0 && yOfs != 0) {
			plotPos(origin, -xOfs, -yOfs);
		}
		/* And need to Flip the x and y offsets too */
		if (xOfs != yOfs) {
			plotPos(origin, yOfs, xOfs);
			if (yOfs != 0) {
				plotPos(origin, -yOfs, xOfs);
			}
			if (xOfs != 0) {
				plotPos(origin, yOfs, -xOfs);
			}
			if (yOfs != 0 && xOfs != 0) {
				plotPos(origin, -yOfs, -xOfs);
			}
		}
	}
}

void AiAreaSearch::plotPos(const pos3_t origin, int xOfs, int dy) {
	/* There's more distance to one (possible) edge of the map than the other
	 *  check if out of bounds to be safe */
	if (origin[0] + xOfs < 0 || origin[0] + xOfs >= PATHFINDING_WIDTH || origin[1] + dy < 0
			|| origin[1] + dy >= PATHFINDING_WIDTH)
		return;
	const pos3_t pos = {static_cast<pos_t>(origin[0] + xOfs), static_cast<pos_t>(origin[1] + dy), origin[2]};
	/* Most maps won't use the full space available so check against the actual map area */
	vec3_t vec;
	PosToVec(pos, vec);
	if (!gi.isOnMap(vec))
		return;
	_area.enqueue(pos);
}

#define SCORE_HIDE			60
#define SCORE_CLOSE_IN		20
#define SCORE_KILL			30
#define SCORE_RANDOM		10
#define SCORE_REACTION_ERADICATION 30
#define SCORE_REACTION_FEAR_FACTOR 20
#define SCORE_NONHIDING_PLACE_PENALTY 25
#define SCORE_RAGE			40
#define SCORE_DAMAGE		100.0f
#define SCORE_DAMAGE_FACTOR	1.25f
#define SCORE_CIV_FACTOR	0.25f
#define SCORE_DISABLED_FACTOR 0.25f
#define SCORE_DAMAGE_WORHT_FACTOR	0.1f

#define SCORE_CIV_RANDOM	10
#define SCORE_RUN_AWAY		50
#define SCORE_CIV_LAZINESS	5
#define RUN_AWAY_DIST		160
#define WAYPOINT_CIV_DIST	768
#define HERD_THRESHOLD		128.0f
#define SCORE_HERDING_PENALTY 100
#define SCORE_NOSAFE_POSITION_PENALTY 500

#define SCORE_MISSION_OPPONENT_TARGET	50
#define SCORE_MISSION_TARGET	60
#define SCORE_MISSION_HOLD		25
#define MISSION_HOLD_DIST		96

#define SCORE_PANIC_RUN_TO_FRIENDS 300.0f
#define SCORE_PANIC_FLEE_FROM_STRANGERS 500.0f
#define SCORE_PANIC_RANDOM 25.0f

#define AI_ACTION_NOTHING_FOUND -10000.0f

#define CLOSE_IN_DIST		1200.0f
#define SPREAD_FACTOR		8.0f
#define	SPREAD_NORM(x)		((x) > 0 ? SPREAD_FACTOR/((x)*torad) : 0)
/** @brief distance for (ai) hiding in grid tiles */
#define HIDE_DIST			25
#define HERD_DIST			25
#define HOLD_DIST			3

#define CALC_DAMAGE_SAMPLES	10.0
#define INVDEF_FOR_SHOOTTYPE(st) (IS_SHOT_RIGHT(st)?INVDEF(CID_RIGHT):IS_SHOT_LEFT(st)?INVDEF(CID_LEFT):IS_SHOT_HEADGEAR(st)?INVDEF(CID_HEADGEAR):nullptr)

static pathing_t* hidePathingTable;
static pathing_t* herdPathingTable;

void AI_Init (void)
{
	hidePathingTable = nullptr;
	herdPathingTable = nullptr;
}

/**
 * @brief Check if @c actor has a line of fire to the @c target given.
 */
bool AI_HasLineOfFire (const Actor* actor, const Edict* target)
{
	for (shoot_types_t shootType = ST_RIGHT; shootType < ST_NUM_SHOOT_TYPES; shootType++) {
		const Item* item = AI_GetItemForShootType(shootType, actor);
		if (item == nullptr)
			continue;

		const fireDef_t* fdArray = item->getFiredefs();
		if (fdArray == nullptr)
			continue;

		for (fireDefIndex_t fdIdx = 0; fdIdx < item->ammoDef()->numFiredefs[fdArray->weapFdsIdx]; fdIdx++) {
			const fireDef_t* fd = &fdArray[fdIdx];
			if (AI_CheckLineOfFire(actor, target, fd, 1))
				return true;
		}
	}
	return false;
}

/**
 * @brief Test if @c check is exposed to the enemy @c team.
 */
static bool AI_IsExposed (int team, Actor* check)
{
	if (G_TestVis(team, check, VT_PERISHCHK | VT_NOFRUSTUM) & VS_YES)
		return true;

	Actor* from = nullptr;
	while ((from = G_EdictsGetNextLivingActor(from))) {
		const int fromTeam = from->getTeam();
		if ((team >= 0 && fromTeam != team) || (team < 0 && fromTeam == -team))
			continue;

		if (AI_HasLineOfFire(from, check))
			return true;
	}

	return false;
}
/**
 * @brief Check whether friendly units are in the line of fire when shooting
 * @param[in] ent AI that is trying to shoot
 * @param[in] target Shoot to this location
 * @param[in] spread The spread of the weapon used (from fireDef)
 * @param[in] radius The splash radius of the weapon
 */
static bool AI_CheckFF (const Edict* ent, const vec3_t target, float spread, float radius)
{
	/* spread data */
	spread *= 2;
	if (spread < 1.0)
		spread = 1.0;
	spread *= torad;

	const float cosSpread = cos(spread);
	vec3_t dtarget;
	VectorSubtract(target, ent->origin, dtarget);
	VectorNormalizeFast(dtarget);
	vec3_t back;
	VectorScale(dtarget, PLAYER_WIDTH / spread, back);

	Actor* check = nullptr;
	while ((check = G_EdictsGetNextLivingActorOfTeam(check, ent->getTeam()))) {
		if (!ent->isSameAs(check)) {
			vec3_t dcheck;
			/* found ally */
			VectorSubtract(check->origin, ent->origin, dcheck);
			if (DotProduct(dtarget, dcheck) > 0.0) {
				/* ally in front of player */
				VectorAdd(dcheck, back, dcheck);
				VectorNormalizeFast(dcheck);
				if (DotProduct(dtarget, dcheck) > cosSpread)
					return true;
			}
		}
		if (VectorDist(target, check->origin) < radius + UNIT_SIZE)
			return true;
	}

	/* no ally in danger */
	return false;
}

/**
 * @brief Check whether the fighter should perform the shoot
 * @todo Check whether radius and power of fd are to to big for dist
 * @todo Check whether the alien will die when shooting
 */
bool AI_FighterCheckShoot (const Actor* actor, const Edict* check, const fireDef_t* fd, float* dist)
{
	/* check range */
	*dist = VectorDist(actor->origin, check->origin);
	if (*dist > fd->range)
		return false;

	/* if insane, we don't check more */
	if (actor->isInsane())
		return true;

	/* don't shoot - we are too close */
	if (*dist < fd->splrad)
		return false;

	/* check FF */
	vec2_t effSpread;
	G_CalcEffectiveSpread(actor, fd, effSpread);
	if (AI_CheckFF(actor, check->origin, effSpread[0], fd->splrad))
		return false;

	return true;
}

/**
 * @brief Checks whether the AI controlled actor wants to use a door
 * @param[in] ent The AI controlled actor
 * @param[in] door The door edict
 * @returns true if the AI wants to use (open/close) that door, false otherwise
 * @note Don't start any new events in here, don't change the actor state
 * @sa Touch_DoorTrigger
 * @todo Finish implementation
 */
bool AI_CheckUsingDoor (const Edict* ent, const Edict* door)
{
	/* don't try to use the door in every case */
	if (frand() < 0.3)
		return false;

	/* not in the view frustum - don't use the door while not seeing it */
	if (!G_FrustumVis(door, ent->origin))
		return false;

	/* if the alien is trying to hide and the door is
	* still opened, close it */
	if (ent->hiding && door->doorState == STATE_OPENED)
		return true;

	/* aliens and civilians need different handling */
	switch (ent->getTeam()) {
	case TEAM_ALIEN: {
		/* only use the door when there is no civilian or phalanx to kill */
		Actor* check = nullptr;

		/* see if there are enemies */
		while ((check = G_EdictsGetNextLivingActor(check))) {
			/* don't check for aliens */
			if (check->isSameTeamAs(ent))
				continue;
			/* check whether the origin of the enemy is inside the
			 * AI actors view frustum */
			if (!G_FrustumVis(check, ent->origin))
				continue;
			/* check whether the enemy is close enough to change the state */
			if (VectorDist(check->origin, ent->origin) > G_VisCheckDist(ent))
				continue;
			const float actorVis = G_ActorVis(check, ent, true);
			/* there is a visible enemy, don't use that door */
			if (actorVis > ACTOR_VIS_0)
				return false;
		}
	}
		break;
	case TEAM_CIVILIAN:
		/* don't use any door if no alien is inside the viewing angle  - but
		 * try to hide behind the door when there is an alien */
		break;
	default:
		gi.DPrintf("Invalid team in AI_CheckUsingDoor: %i for ent type: %i\n", ent->getTeam(), ent->type);
		break;
	}
	return true;
}

/**
 * @brief Checks whether it would be smart to change the state to STATE_CROUCHED
 * @param[in] actor The AI controlled actor to chech the state change for
 * @returns true if the actor should go into STATE_CROUCHED, false otherwise
 */
static bool AI_CheckCrouch (const Actor* actor)
{
	if (G_IsCrouched(actor))
		return false;

	Actor* check = nullptr;

	/* see if we are very well visible by an enemy */
	while ((check = G_EdictsGetNextLivingActor(check))) {
		/* don't check for civilians or aliens */
		if (check->isSameTeamAs(actor) || G_IsCivilian(check))
			continue;
		/* check whether the origin of the enemy is inside the
		 * AI actors view frustum */
		if (!G_FrustumVis(check, actor->origin))
			continue;
		/* check whether the enemy is close enough to change the state */
		if (VectorDist(check->origin, actor->origin) > G_VisCheckDist(actor))
			continue;
		const float actorVis = G_ActorVis(check, actor, true);
		if (actorVis >= ACTOR_VIS_50)
			return true;
	}
	return false;
}

/**
 * @brief Checks whether the given alien should try to hide because there are enemies close
 * enough to shoot the alien.
 * @param[in] actor The alien edict that should (maybe) hide
 * @return @c true if hide is needed or @c false if the alien thinks that it is not needed
 */
bool AI_HideNeeded (const Actor* actor)
{
	/* aliens will consider hiding if they are not brave, or there is a dangerous enemy in sight */
	const bool brave = actor->morale > mor_brave->integer;

	Actor* from = nullptr;
	/* test if actor is visible */
	while ((from = G_EdictsGetNextLivingActor(from))) {
		if (!AI_IsHostile(actor, from))
			continue;

		const Item* item = from->getRightHandItem();
		if (!item)
			item = from->getLeftHandItem();
		if (!item)
			continue;

		const fireDef_t* fd = item->getFiredefs();
		/* search the (visible) inventory (by just checking the weapon in the hands of the enemy) */
		const bool inRange = fd != nullptr && fd->range * fd->range >= VectorDistSqr(actor->origin, from->origin);
		const int damageRand = !inRange ? 0 : fd->damage[0] + fd->spldmg[0] + ((fd->damage[1] + fd->spldmg[1]) * crand());
		const int damage = std::max(0, damageRand);
		if (!brave || damage >= actor->HP / 3) {
			const int hidingTeam = AI_GetHidingTeam(actor);
			/* now check whether this enemy is visible for this alien */
			if (G_Vis(hidingTeam, actor, from, VT_NOFRUSTUM) || AI_HasLineOfFire(from, actor))
				return true;
		}
	}
	return false;
}

/**
 * @brief Returns useable item from the given inventory list. That means that
 * the 'weapon' has ammunition left or must not be reloaded.
 * @param ic The inventory to search a useable weapon in.
 * @return Ready to fire weapon.
 */
static inline const Item* AI_GetItemFromInventory (const Item* ic)
{
	if (ic == nullptr)
		return nullptr;

	const Item* item = ic;
	if (item->ammoDef() && item->isWeapon() && !item->mustReload())
		return item;

	return nullptr;
}

/**
 * Returns the item of the currently chosen shoot type of the ai actor.
 * @param shootType The current selected shoot type
 * @param ent The ai actor
 * @return The item that was selected for the given shoot type. This might be @c nullptr if
 * no item was found.
 */
const Item* AI_GetItemForShootType (shoot_types_t shootType, const Edict* ent)
{
	/* optimization: reaction fire is automatic */
	if (IS_SHOT_REACTION(shootType))
		return nullptr;

	/* check that the current selected shoot type also has a valid item in its
	 * corresponding hand slot of the inventory. */
	if (IS_SHOT_RIGHT(shootType)) {
		const Item* item = ent->getRightHandItem();
		return AI_GetItemFromInventory(item);
	} else if (IS_SHOT_LEFT(shootType)) {
		const Item* item = ent->getLeftHandItem();
		return AI_GetItemFromInventory(item);
	} else if (IS_SHOT_HEADGEAR(shootType)) {
		return nullptr;
	}

	return nullptr;
}

/**
 * @brief Returns the value for the vis check whenever an ai actor tries to hide. For aliens this
 * is the inverse team - see the vis check code for the inverse team rules to see how this works.
 * For civilians we have to specify the alien team and can't use the inverse team rules. This is
 * needed because the inverse team rules aren't working for the civilian team - see @c TEAM_CIVILIAN
 * @return A negative team number means "every other team" as the one from the given ent. See the vis
 * check functions for the inverse team rules for more information.
 */
int AI_GetHidingTeam (const Edict* ent)
{
	if (G_IsCivilian(ent))
		return TEAM_ALIEN;
	return -ent->getTeam();
}

/**
 * @brief Checks if the given position is safe to stand on.
 * @return @c true if the actor's position is deemed safe.
 */
bool AI_CheckPosition (const Actor* const actor, const pos3_t pos)
{
	if (actor->isInsane())
		return true;

	/* Don't stand on hurt triggers or fire/stun gas */
	Edict* check = nullptr;
	while ((check = G_EdictsGetNextInUse(check))) {
		if (!check->isSamePosAs(pos) || check->dmg <= 0)
			continue;
		if (G_ApplyProtection(actor, check->dmgtype, check->dmg) > 0)
			return false;
	}
	return true;
}

/**
 * @brief Tries to search a hiding spot
 * @param[out] actor The actor edict. The position of the actor is updated here to perform visibility checks
 * @param[in] from The grid position the actor is (theoretically) standing at and searching a hiding location from
 * @param[in,out] tuLeft The amount of left TUs to find a hiding spot. The TUs needed to walk to the grid position
 * is subtracted. May not be @c nullptr.
 * @param[in] team The team from which actor tries to hide
 * @return @c true if hiding is possible, @c false otherwise
 */
bool AI_FindHidingLocation (int team, Actor* actor, const pos3_t from, int tuLeft)
{
	/* We need a local table to calculate the hiding steps */
	if (!hidePathingTable)
		hidePathingTable = (pathing_t*) G_TagMalloc(sizeof(*hidePathingTable), TAG_LEVEL);

	/* search hiding spot */
	const int maxTUs = std::min(tuLeft, HIDE_DIST * 2);
	const int distance = (maxTUs + 1) / TU_MOVE_STRAIGHT;
	G_MoveCalcLocal(hidePathingTable, 0, actor, from, maxTUs);

	int bestScore = AI_ACTION_NOTHING_FOUND;
	pos3_t bestPos = {0, 0, PATHFINDING_HEIGHT};
	AiAreaSearch searchArea(from, distance, true);
	while (searchArea.getNext(actor->pos)) {
		/* Don't have TUs  to walk there */
		const pos_t delta = G_ActorMoveLength(actor, hidePathingTable, actor->pos, false);
		if (delta > tuLeft || delta == ROUTING_NOT_REACHABLE)
			continue;

		/* Don't stand on dangerous terrain! */
		if (!AI_CheckPosition(actor, actor->pos))
			continue;

		/* If enemies see this position, it doesn't qualify as hiding spot */
		actor->calcOrigin();
		if (AI_IsExposed(team, actor))
			continue;

		const int score = tuLeft - delta;
		if (score > bestScore) {
			bestScore = score;
			VectorCopy(actor->pos, bestPos);
		}
	}

	if (bestPos[2] != PATHFINDING_HEIGHT) {
		VectorCopy(bestPos, actor->pos);
		return true;
	}
	return false;
}

/**
 * @brief Tries to search a spot where actor will be more closer to the target and
 * behind the target from enemy (ie hide behind target)
 * @param[in] actor The actor edict.
 * @param[in] from The grid position the actor is (theoretically) standing at and
 * searching the nearest location from
 * @param[in] target Tries to find the nearest position to this location
 * @param[in] tu The available TUs of the actor
 * @param[in] inverse Try to shield the target instead of using target as shield
 */
bool AI_FindHerdLocation (Actor* actor, const pos3_t from, const vec3_t target, int tu, bool inverse)
{
	if (!herdPathingTable)
		herdPathingTable = (pathing_t*) G_TagMalloc(sizeof(*herdPathingTable), TAG_LEVEL);

	/* find the nearest enemy actor to the target*/
	vec_t bestLength = -1.0f;
	Actor* next = nullptr;
	Actor* enemy = nullptr;
	int team = AI_GetHidingTeam(actor);
	const bool invTeam = team < 0;
	team = std::abs(team);
	while ((next = G_EdictsGetNextLivingActor(next))) {
		if (next->getTeam() == team ? invTeam : !invTeam)
			continue;
		const vec_t length = VectorDistSqr(target, next->origin);
		if (length < bestLength || bestLength < 0.0f) {
			enemy = next;
			bestLength = length;
		}
	}
	if (!enemy)
		return false;

	/* calculate move table */
	const int maxTUs = std::min(tu, HERD_DIST * 2);
	const int distance = (maxTUs + 1) / TU_MOVE_STRAIGHT;
	G_MoveCalcLocal(herdPathingTable, 0, actor, from, maxTUs);

	/* search the location */
	pos3_t bestPos = {0, 0, PATHFINDING_HEIGHT};
	bestLength = VectorDistSqr(target, actor->origin);
	AiAreaSearch searchArea(from, distance, true);
	while (searchArea.getNext(actor->pos)) {
		/* time */
		const pos_t delta = G_ActorMoveLength(actor, herdPathingTable, actor->pos, false);
		if (delta > tu || delta == ROUTING_NOT_REACHABLE)
			continue;

		/* Don't stand on dangerous terrain! */
		if (!AI_CheckPosition(actor, actor->pos))
			continue;

		actor->calcOrigin();
		const vec_t length = VectorDistSqr(actor->origin, target);
		/* Don't pack them too close */
		if (length < HERD_THRESHOLD * HERD_THRESHOLD)
			continue;

		if (length < bestLength || bestPos[2] == PATHFINDING_HEIGHT) {
			vec3_t vfriend, venemy;
			/* check this position to locate behind target from enemy */
			VectorSubtract(target, actor->origin, vfriend);
			VectorNormalizeFast(vfriend);
			VectorSubtract(enemy->origin, actor->origin, venemy);
			VectorNormalizeFast(venemy);
			const float dotProd = DotProduct(vfriend, venemy);
			if ((inverse ? -dotProd : dotProd) > 0.5f) {
				bestLength = length;
				VectorCopy(actor->pos, bestPos);
			}
		}
	}

	if (bestPos[2] != PATHFINDING_HEIGHT) {
		VectorCopy(bestPos, actor->pos);
		return true;
	}

	return false;
}

/**
 * @todo This feature causes the 'aliens shoot at walls'-bug.
 * I considered adding a visibility check, but that wouldn't prevent aliens
 * from shooting at the breakable parts of their own ship.
 * So I disabled it for now. Duke, 23.10.09
 */
static Edict* AI_SearchDestroyableObject (const Actor* actor, const fireDef_t* fd)
{
#if 0
	/* search best none human target */
	Edict* check = nullptr;
	float dist;

	while ((check = G_EdictsGetNextInUse(check))) {
		if (G_IsBreakable(check)) {
			if (!AI_FighterCheckShoot(actor, check, fd, &dist))
				continue;

			/* check whether target is visible enough */
			const float vis = G_ActorVis(actor->origin, check, true);
			if (vis < ACTOR_VIS_0)
				continue;

			/* take the first best breakable or door and try to shoot it */
			return check;
		}
	}
#endif
	return nullptr;
}

#define LOF_CHECK_PARTITIONS	4
bool AI_CheckLineOfFire (const Actor* shooter, const Edict* target, const fireDef_t* fd, int shots)
{
	vec3_t dir, origin;
	VectorSubtract(target->origin, shooter->origin, dir);
	fd->getShotOrigin(shooter->origin, dir, shooter->isCrouched(), origin);
	if (!fd->gravity) {
		/* gun-to-target line free? */
		const trace_t trace = G_Trace(Line(origin, target->origin), shooter, MASK_SHOT);
		const Edict* trEnt = G_EdictsGetByNum(trace.entNum);
		const bool hitBreakable = trEnt && G_IsBrushModel(trEnt) && G_IsBreakable(trEnt);
		const bool shotBreakable = hitBreakable && (fd->shots > 1 || shots > 1)
				&& trEnt->HP < fd->damage[0] && !(fd->splrad > 0.0f) && !fd->bounce;
		if (trace.fraction < 1.0f && (!trEnt || (!VectorCompare(trEnt->pos, target->pos) && !shotBreakable)))
			return false;
	} else {
		/* gun-to-target *parabola* free? */
		vec3_t at, v;
		VectorCopy(target->origin, at);
		/* Grenades are targeted at the ground in G_ShootGrenade */
		at[2] -= GROUND_DELTA;
		const float dt = gi.GrenadeTarget(origin, at, fd->range, fd->launched, fd->rolled, v) / LOF_CHECK_PARTITIONS;
		if (!dt)
			return false;
		VectorSubtract(at, origin, dir);
		VectorScale(dir, 1.0f / LOF_CHECK_PARTITIONS, dir);
		dir[2] = 0;
		float vz = v[2];
		int i;
		for (i = 0; i < LOF_CHECK_PARTITIONS; i++) {
			VectorAdd(origin, dir, at);
			at[2] += dt * (vz - 0.5f * GRAVITY * dt);
			vz -= GRAVITY * dt;
			const trace_t trace = G_Trace(Line(origin, at), shooter, MASK_SHOT);
			const Edict* trEnt = G_EdictsGetByNum(trace.entNum);
			if (trace.fraction < 1.0f && (!trEnt || !VectorCompare(trEnt->pos, target->pos))) {
				break;
			}
			VectorCopy(at, origin);
		}
		if (i < LOF_CHECK_PARTITIONS)
			return false;
	}
	return true;
}

/**
 * @brief Calculate estimated damage per single shoot
 */
float AI_CalcShotDamage (Actor* actor, Actor* target, const fireDef_t* fd, shoot_types_t shotType)
{
	const int shots = ceil(CALC_DAMAGE_SAMPLES / fd->shots);
	const int zAlign = !fd->gravity && (fd->splrad > 0.0f || target->isStunned()) ? GROUND_DELTA : 0;
	shot_mock_t mock;
	for (int i = 0; i < shots; ++i)
		G_ClientShoot(actor->getPlayer(), actor, target->pos, shotType, fd->fdIdx, &mock, false, zAlign);

	if (mock.damage == 0)
		return 0.0f;

	const int totalCount = mock.enemyCount + mock.friendCount + mock.civilian;
	const int eCount = totalCount - (mock.friendCount + (G_IsAlien(actor) ? 0 : mock.civilian));
	return mock.damage * (static_cast<float>(eCount) / totalCount) / shots;
}

/**
 * @todo timed firedefs that bounce around should not be thrown/shoot about the whole distance
 */
static void AI_FindBestFiredef (AiAction* aia, Actor* actor, Actor* check, const Item* item, shoot_types_t shootType, int tu, float* maxDmg, int* bestTime, const fireDef_t* fdArray)
{
	bool hasLineOfFire = false;
	int shotChecked = NONE;

	for (fireDefIndex_t fdIdx = 0; fdIdx < item->ammoDef()->numFiredefs[fdArray->weapFdsIdx]; fdIdx++) {
		const fireDef_t* fd = &fdArray[fdIdx];
		const int time = G_ActorGetModifiedTimeForFiredef(actor, fd, false);
		/* how many shoots can this actor do */
		const int shots = tu / time;
		if (shots) {
			const bool stunWeapon = (item->def()->dmgtype == gi.csi->damStunElectro || item->def()->dmgtype == gi.csi->damStunGas);
			if (stunWeapon && !actor->isInsane() && (check->isStunned() || CHRSH_IsTeamDefRobot(check->chr.teamDef)))
				return;

			float dist;
			if (!AI_FighterCheckShoot(actor, check, fd, &dist))
				continue;

			/*
			 * check weapon can hit, we only want to do this once unless the LoF actually changes
			 * between shots, only hand grenades seem to do this (rolled vs thrown)
			 */
			const int shotFlags = fd->gravity | (fd->launched << 1) | (fd->rolled << 2);
			if (shotChecked != shotFlags) {
				shotChecked = shotFlags;
				hasLineOfFire = AI_CheckLineOfFire(actor, check, fd, shots);
			}
			if (!hasLineOfFire)
				continue;

			/* calculate expected damage */
			float dmg = AI_CalcShotDamage(actor, check, fd, shootType) * shots;

			/* It is said that there is no kill like overkill but... */
			dmg = std::min(dmg, check->HP * SCORE_DAMAGE_FACTOR) * SCORE_DAMAGE / check->HP;

			if (dmg > check->HP && check->isReaction())
				/* reaction shooters eradication bonus */
				dmg = check->HP + SCORE_KILL + SCORE_REACTION_ERADICATION;
			else if (dmg > check->HP)
				/* standard kill bonus */
				dmg = check->HP + SCORE_KILL;

			/* ammo is limited and shooting gives away your position */
			if (dmg < check->HP * SCORE_DAMAGE_WORHT_FACTOR)
				continue;

			/* civilian malus */
			if (G_IsCivilian(check) && !actor->isRaged())
				dmg *= SCORE_CIV_FACTOR;

			/* Stunned malus */
			if (check->isStunned() && !actor->isRaged())
				dmg *= SCORE_DISABLED_FACTOR;

			/* add random effects */
			if (dmg > 0)
				dmg += SCORE_RANDOM * frand();

			/* check if most damage can be done here */
			if (dmg > *maxDmg) {
				*maxDmg = dmg;
				*bestTime = time * shots;
				aia->shootType = shootType;
				aia->shots = shots;
				aia->target = check;
				aia->fd = fd;
				if (!fd->gravity && (fd->splrad > 0.0f || check->isStunned()))
					aia->z_align = GROUND_DELTA;
				else
					aia->z_align = 0;
			}

			if (!aia->target) {
				aia->target = AI_SearchDestroyableObject(actor, fd);
				if (aia->target) {
					/* don't take vis into account, don't multiply with amount of shots
					 * others (human victims) should be preferred, that's why we don't
					 * want a too high value here */
					*maxDmg = (fd->damage[0] + fd->spldmg[0]);
					*bestTime = time * shots;
					aia->shootType = shootType;
					aia->shots = shots;
					aia->fd = fd;
				}
			}
		}
	}
}

/**
 * @brief Check if given actors are enemies.
 * @param[in] actor The actor that makes the check.
 * @param[in] check The actor which is a possible opponent.
 * @returns @c true if enemies. @c false otherwise
 * @todo Should we really know if the other actor is controlled by the other team (STATE_XVI)?
 * aliens would of course know if an actor is infected (becomes part of the hive mind), but humans?
 */
static inline bool AI_IsOpponent (const Actor* actor, const Edict* check)
{
	const bool entControlled = G_IsState(actor, STATE_XVI);
	const bool checkControlled = G_IsState(check, STATE_XVI);
	if (check->isSameTeamAs(actor))
		return entControlled ? !checkControlled : checkControlled;

	bool opponent = true;
	switch (actor->getTeam()) {
	/* Aliens: hostile to everyone */
	case TEAM_ALIEN:
		opponent = !checkControlled;
		break;
	/* Civilians: Only hostile to aliens */
	case TEAM_CIVILIAN:
		opponent = G_IsAlien(check) || checkControlled;
		break;
	/* PHALANX and MP teams: Hostile to aliens and rival teams
	 * (under AI control while under panic/rage or when g_aihumans is non-zero) */
	default:
		opponent = !G_IsCivilian(check) || checkControlled;
		break;
	}

	return entControlled ? !opponent : opponent;
}

/**
 * @brief Check if @c actor perceives @c target as hostile.
 * @note Takes lose of sanity in consideration.
 * @param[in] actor The actor that checks for hostiles.
 * @param[in] target The possible hostile actor.
 * @returns @c true if actor perceives target as hostile
 */
bool AI_IsHostile (const Actor* actor, const Edict* target)
{
	if (actor == target)
		return false;

	if (actor->isInsane())
		return true;

	if (!AI_IsOpponent(actor, target))
		return false;

	/* don't shoot civs in multiplayer */
	if (G_IsMultiPlayer())
		return !G_IsCivilian(target);

	return true;
}

/**
 * @brief Search the edict's inventory for a grenade or other one-use weapon.
 */
const invDef_t* AI_SearchGrenade (const Actor* actor, Item** ip)
{
	/* search for grenades and select the one that is available easily */
	const Container* cont = nullptr;
	const invDef_t* bestContainer = nullptr;
	Item* weapon = nullptr;
	int cost = 100;
	while ((cont = actor->chr.inv.getNextCont(cont, true))) {
		if (cont->def()->out >= cost)
			continue;
		Item* item = nullptr;
		while ((item = cont->getNextItem(item))) {
			assert(item->def());
			const objDef_t* obj = item->def();
			if (item->isWeapon() && !item->mustReload() && ((obj->thrown && obj->oneshot && obj->deplete)
				|| Q_streq(obj->type, "grenade"))) {
				weapon = item;
				bestContainer = cont->def();
				cost = bestContainer->out;
				break;
			}
		}
	}

	*ip = weapon;
	return bestContainer;
}

/**
 * @brief Check if the hand for the given shoot type is free.
 * @returns @c true if shoot type uses a hand and it is free, @c false if hand isn't free
 * or shoot type doesn't use a hand.
 */
static bool AI_IsHandForForShootTypeFree (shoot_types_t shootType, Actor* actor)
{
	if (!IS_SHOT_REACTION(shootType)) {
		if (IS_SHOT_RIGHT(shootType)) {
			const Item* item = actor->getRightHandItem();
			return item == nullptr;
		}
		if (IS_SHOT_LEFT(shootType)) {
			const Item* left = actor->getLeftHandItem();
			const Item* right = actor->getRightHandItem();
			return left == nullptr && (right == nullptr || !right->isHeldTwoHanded());
		}
	}

	return false;
}

/**
 * @sa AI_ActorThink
 * @todo fill z_align values
 * @todo optimize this
 */
static float AI_FighterCalcActionScore (Actor* actor, const pos3_t to, AiAction* aia)
{
	const pos_t move = G_ActorMoveLength(actor, level.pathingMap, to, true);
	int tu = actor->getUsableTUs() - move;

	/* test for time */
	if (tu < 0 || move == ROUTING_NOT_REACHABLE)
		return AI_ACTION_NOTHING_FOUND;

	/* set basic parameters */
	aia->reset();
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	actor->setOrigin(to);

	/* pre-find a grenade */
	Item* grenade = nullptr;
	const invDef_t* fromCont = AI_SearchGrenade(actor, &grenade);

	/* search best target */
	float maxDmg = 0.0f;
	float bestActionScore = 0.0f;
	int bestTime = -1;
	Actor* check = nullptr;

	while ((check = G_EdictsGetNextLivingActor(check))) {
		if (check->isSamePosAs(to) || !AI_IsHostile(actor, check))
			continue;

		if (!G_IsVisibleForTeam(check, actor->getTeam()) && G_ActorVis(actor, check, true) < ACTOR_VIS_10)
			continue;

		/* shooting */
		for (shoot_types_t shootType = ST_RIGHT; shootType < ST_NUM_SHOOT_TYPES; shootType++) {
			const bool freeHand = AI_IsHandForForShootTypeFree(shootType, actor);
			const Item* item = freeHand ? grenade : AI_GetItemForShootType(shootType, actor);
			if (!item)
				continue;

			const fireDef_t* fdArray = item->getFiredefs();
			if (fdArray == nullptr)
				continue;

			const invDef_t* toCont = INVDEF_FOR_SHOOTTYPE(shootType);
			const int invMoveCost = freeHand && grenade ? fromCont->out + toCont->in : 0;
			AI_FindBestFiredef(aia, actor, check, item, shootType, tu - invMoveCost, &maxDmg, &bestTime, fdArray);
			if (aia->shootType == shootType)
				bestTime += invMoveCost;
		}
	}
	/* add damage to bestActionScore */
	if (aia->target) {
		bestActionScore += maxDmg;
		assert(bestTime > 0);
		tu -= bestTime;
	}

	/* Try not to stand in dangerous terrain (eg. fireField) */
	if (!AI_CheckPosition(actor, actor->pos))
		bestActionScore -= SCORE_NOSAFE_POSITION_PENALTY;

	if (!actor->isRaged()) {
		const int hidingTeam = AI_GetHidingTeam(actor);
		/* hide */
		if (!AI_HideNeeded(actor)) {
			/* is a hiding spot */
			bestActionScore += SCORE_HIDE + (aia->target ? SCORE_CLOSE_IN + SCORE_REACTION_FEAR_FACTOR : 0);
		} else if (aia->target && tu >= TU_MOVE_STRAIGHT) {
			/* reward short walking to shooting spot, when seen by enemies; */
			/** @todo do this decently, only penalizing the visible part of walk
			 * and penalizing much more for reaction shooters around;
			 * now it may remove some tactical options from aliens,
			 * e.g. they may now choose only the closer doors;
			 * however it's still better than going three times around soldier
			 * and only then firing at him */
			bestActionScore += std::max(SCORE_CLOSE_IN - move, 0);

			if (!AI_FindHidingLocation(hidingTeam, actor, to, tu)) {
				/* nothing found */
				actor->setOrigin(to);
			} else {
				/* found a hiding spot */
				VectorCopy(actor->pos, aia->stop);
				actor->calcOrigin();
				bestActionScore += SCORE_HIDE;
				/** @todo also add bonus for fleeing from reaction fire
				 * and a huge malus if more than 1 move under reaction */
			}
		}
	} else {
		if (aia->target)
			bestActionScore += aia->shots * SCORE_RAGE - move;
		else
			bestActionScore += move;
	}

	if (aia->target) {
		const float dist = VectorDist(actor->origin, aia->target->origin);
		bestActionScore += SCORE_CLOSE_IN * (1.0 - dist / CLOSE_IN_DIST);
	} else if (actor->isRaged()) {
		/* reward closing in */
		float minDist = CLOSE_IN_DIST;
		check = nullptr;
		while ((check = G_EdictsGetNextLivingActor(check))) {
			if (!check->isSameTeamAs(actor)) {
				const float dist = VectorDist(actor->origin, check->origin);
				minDist = std::min(dist, minDist);
			}
		}
		bestActionScore += SCORE_CLOSE_IN * (1.0f - minDist / CLOSE_IN_DIST);
	} else {
		/* if no target available let them wander around until they find one */
		bestActionScore += SCORE_RANDOM * frand();
	}

	/* penalize herding */
	if (!actor->isRaged()) {
		check = nullptr;
		while ((check = G_EdictsGetNextLivingActorOfTeam(check, actor->getTeam()))) {
			const float dist = VectorDist(actor->origin, check->origin);
			if (dist < HERD_THRESHOLD)
				bestActionScore -= SCORE_HERDING_PENALTY;
		}
	}

	return bestActionScore;
}

/**
 * @brief Calculates possible actions for a civilian.
 * @param[in] actor Pointer to an edict being civilian.
 * @param[in] to The grid position to walk to.
 * @param[in] aia Pointer to aiAction containing information about possible action.
 * @sa AI_ActorThink
 * @note Even civilians can use weapons if the teamdef allows this
 */
static float AI_CivilianCalcActionScore (Actor* actor, const pos3_t to, AiAction* aia)
{
	const pos_t move = G_ActorMoveLength(actor, level.pathingMap, to, true);
	const int tu = actor->getUsableTUs() - move;

	/* test for time */
	if (tu < 0 || move == ROUTING_NOT_REACHABLE)
		return AI_ACTION_NOTHING_FOUND;

	/* set basic parameters */
	aia->reset();
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	actor->setOrigin(to);

	/* check whether this civilian can use weapons */
	if (actor->chr.teamDef) {
		const teamDef_t* teamDef = actor->chr.teamDef;
		if (!actor->isPanicked() && teamDef->weapons)
			return AI_FighterCalcActionScore(actor, to, aia);
	} else
		gi.DPrintf("AI_CivilianCalcActionScore: Error - civilian team with no teamdef\n");

	/* run away */
	float minDist, minDistCivilian, minDistFighter;
	minDist = minDistCivilian = minDistFighter = RUN_AWAY_DIST * UNIT_SIZE;

	Actor* check = nullptr;
	while ((check = G_EdictsGetNextLivingActor(check))) {
		float dist;
		if (actor == check)
			continue;
		dist = VectorDist(actor->origin, check->origin);
		/* if we are trying to walk to a position that is occupied by another actor already we just return */
		if (!dist)
			return AI_ACTION_NOTHING_FOUND;
		switch (check->getTeam()) {
		case TEAM_ALIEN:
			if (dist < minDist)
				minDist = dist;
			break;
		case TEAM_CIVILIAN:
			if (dist < minDistCivilian)
				minDistCivilian = dist;
			break;
		case TEAM_PHALANX:
			if (dist < minDistFighter)
				minDistFighter = dist;
			break;
		}
	}

	minDist /= UNIT_SIZE;
	minDistCivilian /= UNIT_SIZE;
	minDistFighter /= UNIT_SIZE;

	float delta;
	if (minDist < 8.0) {
		/* very near an alien: run away fast */
		delta = 4.0 * minDist;
	} else if (minDist < 16.0) {
		/* near an alien: run away */
		delta = 24.0 + minDist;
	} else if (minDist < 24.0) {
		/* near an alien: run away slower */
		delta = 40.0 + (minDist - 16) / 4;
	} else {
		delta = 42.0;
	}
	/* near a civilian: join him (1/3) */
	if (minDistCivilian < 10.0)
		delta += (10.0 - minDistCivilian) / 3.0;
	/* near a fighter: join him (1/5) */
	if (minDistFighter < 15.0)
		delta += (15.0 - minDistFighter) / 5.0;
	/* don't go close to a fighter to let him move */
	if (minDistFighter < 2.0)
		delta /= 10.0;

	/* try to hide */
	float reactionTrap = 0.0;
	if (!actor->isInsane()) {
		check = nullptr;
		while ((check = G_EdictsGetNextLivingActor(check))) {
			if (actor == check)
				continue;
			if (!(G_IsAlien(check)))
				continue;

			if (G_ActorVis(check, actor, true) > ACTOR_VIS_10)
				reactionTrap += SCORE_NONHIDING_PLACE_PENALTY;
		}
	}
	delta -= reactionTrap;
	float bestActionScore = delta;

	/* Try not to stand in dangerous terrain */
	if (!AI_CheckPosition(actor, actor->pos))
		bestActionScore -= SCORE_NOSAFE_POSITION_PENALTY;

	/* add laziness */
	if (actor->getTus())
		bestActionScore += SCORE_CIV_LAZINESS * tu / actor->getTus();
	/* add random effects */
	bestActionScore += SCORE_CIV_RANDOM * frand();

	return bestActionScore;
}

/**
 * @brief Calculates possible actions for a panicking unit.
 * @param[in] actor Pointer to an edict which is panicking.
 * @param[in] to The grid position to walk to.
 * @param[in] aia Pointer to aiAction containing information about possible action.
 * @sa AI_ActorThink
 * @note Panicking units will run away from everyone other than their own team (e.g. aliens will run away even from civilians)
 */
static float AI_PanicCalcActionScore (Actor* actor, const pos3_t to, AiAction* aia)
{
	const pos_t move = G_ActorMoveLength(actor, level.pathingMap, to, true);
	const int tu = actor->getUsableTUs() - move;

	/* test for time */
	if (tu < 0 || move == ROUTING_NOT_REACHABLE)
		return AI_ACTION_NOTHING_FOUND;

	/* set basic parameters */
	aia->reset();
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	actor->setOrigin(to);

	/* run away */
	float minDistFriendly, minDistOthers;
	minDistFriendly = minDistOthers = RUN_AWAY_DIST * UNIT_SIZE;

	Actor* check = nullptr;
	while ((check = G_EdictsGetNextLivingActor(check))) {
		float dist;
		if (actor == check)
			continue;
		dist = VectorDist(actor->origin, check->origin);
		/* if we are trying to walk to a position that is occupied by another actor already we just return */
		if (!dist)
			return AI_ACTION_NOTHING_FOUND;
		if (check->isSameTeamAs(actor)) {
			if (dist < minDistFriendly)
				minDistFriendly = dist;
		} else {
			if (dist < minDistOthers)
				minDistOthers = dist;
		}
	}

	minDistFriendly /= UNIT_SIZE;
	minDistOthers /= UNIT_SIZE;

	float bestActionScore = SCORE_PANIC_RUN_TO_FRIENDS / minDistFriendly;
	bestActionScore -= SCORE_PANIC_FLEE_FROM_STRANGERS / minDistOthers;

	/* try to hide */
	check = nullptr;
	if (!actor->isInsane())
		while ((check = G_EdictsGetNextLivingActor(check))) {
			if (actor == check)
				continue;

			if (G_ActorVis(check, actor, true) > ACTOR_VIS_10)
				bestActionScore -= SCORE_NONHIDING_PLACE_PENALTY;
		}

	/* Try not to stand in dangerous terrain */
	if (!AI_CheckPosition(actor, actor->pos))
		bestActionScore -= SCORE_NOSAFE_POSITION_PENALTY;

	/* add random effects */
	bestActionScore += SCORE_PANIC_RANDOM * frand();

	return bestActionScore;
}

/**
 * @brief Try to go close to a mission edict
 * @param[in,out] actor The actor edict.
 * @param[in] to The target position.
 * @return @c true if found a suitable position, @c false otherwise
 */
bool AI_FindMissionLocation (Actor* actor, const pos3_t to, int tus)
{
	int bestDist = ROUTING_NOT_REACHABLE;
	pos3_t bestPos = {to[0], to[1], to[2]};

	AiAreaSearch searchArea(to, HOLD_DIST, true);
	while (searchArea.getNext(actor->pos)) {
		const pos_t length = G_ActorMoveLength(actor, level.pathingMap, actor->pos, true);
		/* Can't walk there */
		if (length == ROUTING_NOT_REACHABLE || length > tus)
			continue;
		/* Don't stand on dangerous terrain! */
		if (!AI_CheckPosition(actor, actor->pos))
			continue;

		const int distX = std::abs(actor->pos[0] - to[0]);
		const int distY = std::abs(actor->pos[1] - to[1]);
		const int dist = distX + distY + std::max(distX, distY);
		if (dist < bestDist) {
			bestDist = dist;
			VectorCopy(actor->pos, bestPos);
		}
	}
	if (!VectorCompare(to, bestPos))
		VectorCopy(bestPos, actor->pos);

	return bestDist < ROUTING_NOT_REACHABLE;
}

/**
 * @brief Searches the map for mission edicts and try to get there
 * @sa AI_PrepBestAction
 * @note The routing table is still valid, so we can still use
 * gi.MoveLength for the given edict here
 */
static int AI_CheckForMissionTargets (const Player& player, Actor* actor, AiAction* aia)
{
	int bestActionScore = AI_ACTION_NOTHING_FOUND;
	int actionScore;

	/* reset any previous given action set */
	aia->reset();

	if (G_IsCivilian(actor)) {
		Edict* checkPoint = nullptr;
		int i = 0;
		/* find waypoints in a closer distance - if civilians are not close enough, let them walk
		 * around until they came close */
		for (checkPoint = level.ai_waypointList; checkPoint != nullptr; checkPoint = checkPoint->groupChain) {
			if (!checkPoint->inuse)
				continue;

			/* the lower the count value - the nearer the final target */
			if (checkPoint->count < actor->count) {
				if (VectorDist(actor->origin, checkPoint->origin) <= WAYPOINT_CIV_DIST) {
					const int actorTUs = actor->getUsableTUs();
					if (!AI_FindMissionLocation(actor, checkPoint->pos, actorTUs))
						continue;

					const int length = actorTUs - G_ActorMoveLength(actor, level.pathingMap, actor->pos, true);
					i++;

					/* test for time and distance */
					actionScore = SCORE_MISSION_TARGET + length;

					actor->calcOrigin();
					/* Don't walk to enemy ambush */
					Actor* check = nullptr;
					while ((check = G_EdictsGetNextLivingActorOfTeam(check, TEAM_ALIEN))) {
						const float dist = VectorDist(actor->origin, check->origin);
						/* @todo add visibility check here? */
						if (dist < RUN_AWAY_DIST)
							actionScore -= SCORE_RUN_AWAY;
					}
					if (actionScore > bestActionScore) {
						bestActionScore = actionScore;
						actor->count = checkPoint->count;
						VectorCopy(actor->pos, aia->to);
						VectorCopy(actor->pos, aia->stop);
					}
				}
			}
		}
		/* reset the count value for this civilian to restart the search */
		if (!i)
			actor->count = 100;
	} else if (G_IsAlien(actor)) {
		/* search for a mission edict */
		Edict* mission = nullptr;
		while ((mission = G_EdictsGetNextInUse(mission))) {
			if (mission->type == ET_MISSION) {
				if (!AI_FindMissionLocation(actor, mission->pos, actor->getUsableTUs()))
					continue;
				if (player.getTeam() == mission->getTeam()) {
					actionScore = SCORE_MISSION_TARGET;
				} else {
					/* try to prevent the phalanx from reaching their mission target */
					actionScore = SCORE_MISSION_OPPONENT_TARGET;
				}

				actor->calcOrigin();
				/* Don't cluster everybody in the same place */
				Actor* check = nullptr;
				while ((check = G_EdictsGetNextLivingActor(check))) {
					const float dist = VectorDist(actor->origin, check->origin);
					if (dist < MISSION_HOLD_DIST) {
						if (check->isSameTeamAs(actor))
							actionScore -= SCORE_MISSION_HOLD;
						else
							/* @todo add a visibility check here? */
							actionScore += SCORE_MISSION_HOLD;
					}
				}

				if (actionScore > bestActionScore) {
					bestActionScore = actionScore;
					VectorCopy(actor->pos, aia->to);
					VectorCopy(actor->pos, aia->stop);
					aia->target = mission;
				}
			}
		}
	}

	return bestActionScore;
}

/**
 * @brief Attempts to find the best action for an alien. Moves the alien
 * into the starting position for that action and returns the action.
 * @param[in] player The AI player
 * @param[in] actor The AI actor
 */
static AiAction AI_PrepBestAction (const Player& player, Actor* actor)
{
	/* calculate move table */
	G_MoveCalc(0, actor, actor->pos, actor->getUsableTUs());
	Com_DPrintf(DEBUG_ENGINE, "AI_PrepBestAction: Called MoveMark.\n");
	gi.MoveStore(level.pathingMap);

	/* set borders */
	const int dist = (actor->getUsableTUs() + 1) / 2;

	/* search best action */
	pos3_t oldPos;
	vec3_t oldOrigin;
	VectorCopy(actor->pos, oldPos);
	VectorCopy(actor->origin, oldOrigin);

	/* evaluate moving to every possible location in the search area,
	 * including combat considerations */
	float bestActionScore, best = AI_ACTION_NOTHING_FOUND;
	AiAction aia, bestAia;
	pos3_t to;
	AiAreaSearch searchArea(oldPos, dist);
	while (searchArea.getNext(to)) {
		const pos_t move = G_ActorMoveLength(actor, level.pathingMap, to, true);
		if (move >= ROUTING_NOT_REACHABLE)
			continue;
		if (move > actor->getUsableTUs())
			continue;

		if (G_IsCivilian(actor))
			bestActionScore = AI_CivilianCalcActionScore(actor, to, &aia);
		else if (actor->isPanicked())
			bestActionScore = AI_PanicCalcActionScore(actor, to, &aia);
		else
			bestActionScore = AI_FighterCalcActionScore(actor, to, &aia);

		if (bestActionScore > best) {
			bestAia = aia;
			best = bestActionScore;
		}
	}

	bestActionScore = AI_CheckForMissionTargets(player, actor, &aia);
	if (bestActionScore > best) {
		bestAia = aia;
		best = bestActionScore;
	}

	VectorCopy(oldPos, actor->pos);
	VectorCopy(oldOrigin, actor->origin);

	/* nothing found to do */
	if (best == AI_ACTION_NOTHING_FOUND) {
		bestAia.target = nullptr;
		return bestAia;
	}

	/* do the move */
	for (;;) {
		if (actor->isDead())
			break;
		G_ClientMove(player, 0, actor, bestAia.to);
		if (actor->isSamePosAs(bestAia.to))
			break;
		const pos_t length = G_ActorMoveLength(actor, level.pathingMap, bestAia.to, false);
		if (length > actor->getUsableTUs() || length >= ROUTING_NOT_REACHABLE)
			break;
	}
	/* test for possible death during move. reset bestAia due to dead status */
	if (actor->isDead())
		bestAia.reset();

	/* if we are throwing a grenade from the inventory grab it now */
	if (bestAia.target && AI_IsHandForForShootTypeFree(bestAia.shootType, actor)) {
		Item* grenade = nullptr;
		const invDef_t* fromCont = AI_SearchGrenade(actor, &grenade);
		const invDef_t* toCont = INVDEF_FOR_SHOOTTYPE(bestAia.shootType);
		if (!grenade || !fromCont || !toCont || !G_ActorInvMove(actor, fromCont, grenade, toCont, NONE, NONE, true))
			bestAia.target = nullptr;
	}

	return bestAia;
}

void G_AddToWayPointList (Edict* ent)
{
	if (!level.ai_waypointList) {
		level.ai_waypointList = ent;
		return;
	}

	Edict* e = level.ai_waypointList;
	while (e->groupChain) {
		e = e->groupChain;
	}
	e->groupChain = ent;
}

/**
 * @brief This function will turn the AI actor into the direction that is needed to walk
 * to the given location
 * @param[in] actor The actor to turn
 * @param[in] pos The position to set the direction for
 */
void AI_TurnIntoDirection (Actor* actor, const pos3_t pos)
{
	const byte crouchingState = actor->isCrouched() ? 1 : 0;
	G_MoveCalc(actor->getTeam(), actor, pos, actor->getUsableTUs());

	const int dvec = gi.MoveNext(level.pathingMap, pos, crouchingState);
	if (dvec != ROUTING_UNREACHABLE) {
		const byte dir = getDVdir(dvec);
		/* Only attempt to turn if the direction is not a vertical only action */
		if (dir < CORE_DIRECTIONS || dir >= FLYING_DIRECTIONS)
			G_ActorDoTurn(actor, dir & (CORE_DIRECTIONS - 1));
	}
}

/**
 * @brief if a weapon can be reloaded we attempt to do so if TUs permit, otherwise drop it
 */
bool AI_TryToReloadWeapon (Actor* actor, containerIndex_t containerID)
{
	if (G_ClientCanReload(actor, containerID)) {
		return G_ActorReload(actor, INVDEF(containerID));
	} else {
		G_ActorInvMove(actor, INVDEF(containerID), actor->getContainer(containerID), INVDEF(CID_FLOOR), NONE, NONE, true);
		G_ReactionFireSettingsUpdate(actor, actor->chr.RFmode.getFmIdx(), actor->chr.RFmode.getHand(), actor->chr.RFmode.getWeapon());
	}
	return false;
}

/**
 * @brief The think function for the ai controlled aliens or panicked humans
 * @param[in] player The AI or human player object
 * @param[in] actor The ai controlled edict or the human actor edict
 * @sa AI_FighterCalcActionScore
 * @sa AI_CivilianCalcActionScore
 */
static void AI_ActorThink (Player& player, Actor* actor)
{
	/* if a weapon can be reloaded we attempt to do so if TUs permit, otherwise drop it */
	Item* rightH = actor->getRightHandItem();
	Item* leftH = actor->getLeftHandItem();
	if (!actor->isPanicked()) {
		if (rightH && rightH->mustReload())
			AI_TryToReloadWeapon(actor, CID_RIGHT);
		if (leftH && leftH->mustReload())
			AI_TryToReloadWeapon(actor, CID_LEFT);
	}

	/* if both hands are empty, attempt to get a weapon out of backpack or the
	 * floor (if TUs permit) */
	/** @note we need to re-check the items here because they may have been dropped in the previous step */
	if (!actor->getLeftHandItem() && !actor->getRightHandItem())
		G_ClientGetWeaponFromInventory(actor);

	AiAction bestAia = AI_PrepBestAction(player, actor);

	/* shoot and hide */
	if (bestAia.target) {
		const fireDefIndex_t fdIdx = bestAia.fd ? bestAia.fd->fdIdx : 0;
		/* shoot until no shots are left or target died */
		while (bestAia.shots) {
			G_ClientShoot(player, actor, bestAia.target->pos, bestAia.shootType, fdIdx, nullptr, true, bestAia.z_align);
			bestAia.shots--;
			/* died by our own shot? */
			if (actor->isDead())
				return;
			/* check for target's death */
			if (G_IsDead(bestAia.target)) {
				/* search another target now */
				bestAia = AI_PrepBestAction(player, actor);
				/* no other target found - so no need to hide */
				if (!bestAia.target)
					return;
			}
		}
		actor->hiding = true;

		/* now hide - for this we use the team of the alien actor because a phalanx soldier
		 * might become visible during the hide movement */
		G_ClientMove(player, actor->getTeam(), actor, bestAia.stop);
		/* no shots left, but possible targets left - maybe they shoot back
		 * or maybe they are still close after hiding */

		/* decide whether the actor wants to crouch */
		if (AI_CheckCrouch(actor))
			G_ClientStateChange(player, actor, STATE_CROUCHED, true);

		/* actor is still alive - try to turn into the appropriate direction to see the target
		 * actor once he sees the ai, too */
		AI_TurnIntoDirection(actor, bestAia.target->pos);

		/** @todo If possible targets that can shoot back (check their inventory for weapons, not for ammo)
		 * are close, go into reaction fire mode, too */
		/* G_ClientStateChange(player, actor->number, STATE_REACTION_ONCE, true); */

		actor->hiding = false;
	}
}

void AI_ActorRun (Player& player, Actor* actor)
{
	if (g_ailua->integer)
		AIL_ActorThink(player, actor);
	else
		AI_ActorThink(player, actor);
}

#if 0
#include "g_ai2.cpp"
#else
static bool AI_TeamThink (Player& player)
{
	return false;
}
#endif

static void AI_PlayerRun (Player& player)
{
	if (level.activeTeam != player.getTeam() || player.roundDone)
		return;

	if (g_ailua->integer > 1) {
		if (AIL_TeamThink(player))
			/* did some thinking, come back next time */
			return;
		/* finished thinking, end round */
	}
	/** Duke's playground for a completely new AI. While in developement, it is only available to Phalanx */
	 else if (player.getTeam() == TEAM_PHALANX && g_aihumans->integer == 2) {
		if (AI_TeamThink(player))
			return;		/* did some thinking, come back next frame */
		/* finished thinking, end round */
	}
	else {
		/* find next actor to handle */
		Actor* actor = player.pers.getLastActor();
		while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, player.getTeam()))) {
			const int beforeTUs = actor->getTus();
			if (beforeTUs > 0) {
				AI_ActorRun(player, actor);
				player.pers.setLastActor(actor);

				if (beforeTUs > actor->getTus())
					return;
			}
		}
	}

	/* nothing left to do, request endround */
	G_ClientEndRound(player);
	player.pers.setLastActor(nullptr);
}

/**
 * @brief Every server frame one single actor is handled - always in the same order
 * @sa G_RunFrame
 */
void AI_Run (void)
{
	/* don't run this too often to prevent overflows */
	if (level.framenum % 10)
		return;

	/* set players to ai players and cycle over all of them */
	Player* player = nullptr;
	while ((player = G_PlayerGetNextActiveAI(player))) {
		AI_PlayerRun(*player);
	}

	if (g_aihumans->integer) {
		player = nullptr;
		while ((player = G_PlayerGetNextActiveHuman(player))) {
			AI_PlayerRun(*player);
		}
	}
}

/**
 * @brief Initializes the actor's stats like morals, strength and so on.
 * @param actor Actor to set the stats for.
 * @param team Phalanx, civilian or alien ?
 */
static void AI_SetStats (Actor* actor, int team)
{
	const char* templateId = "";
	if (team != TEAM_CIVILIAN && gi.csi->numAlienTeams) {
		for (int i = 0; i < gi.csi->numAlienTeams; ++i) {
			if (gi.csi->alienTeams[i] == actor->chr.teamDef && gi.csi->alienChrTemplates[i]) {
				templateId = gi.csi->alienChrTemplates[i]->id;
				break;
			}
		}
	}

	CHRSH_CharGenAbilitySkills(&actor->chr, G_IsMultiPlayer(), templateId);

	actor->HP = actor->chr.HP;
	actor->setMorale(actor->chr.morale);
	actor->setStun(0);

	/* hurt aliens in ufo crash missions (5%: almost dead, 10%: wounded, 15%: stunned)  */
	if (level.hurtAliens && CHRSH_IsTeamDefAlien(actor->chr.teamDef)) {
		const float random = frand();
		int damage = 0, stun = 0;
		if (random <= 0.05f) {
			damage = actor->HP * 0.95f;
		} else if (random <= 0.15f) {
			stun = actor->HP * 0.3f;
			damage = actor->HP * 0.5f;
		} else if (random <= 0.3f) {
			stun = actor->HP * 0.75f;
		}
		actor->HP -= damage;
		if (!CHRSH_IsTeamDefRobot(actor->chr.teamDef))
			actor->setStun(stun);

		for (int i = 0; i < actor->chr.teamDef->bodyTemplate->numBodyParts(); ++i)
			actor->chr.wounds.treatmentLevel[actor->chr.teamDef->bodyTemplate->getRandomBodyPart()] += damage / BODYPART_MAXTYPE;
	}

	G_ActorGiveTimeUnits(actor);
}

/**
 * @brief Sets an actor's character values.
 * @param ent Actor to set the model for.
 * @param[in] team Team to which actor belongs.
 */
static void AI_SetCharacterValues (Edict* ent, int team)
{
	/* Set model. */
	const char* teamDefinition;
	if (team != TEAM_CIVILIAN) {
		if (gi.csi->numAlienTeams) {
			const int alienTeam = rand() % gi.csi->numAlienTeams;
			const teamDef_t* td = gi.csi->alienTeams[alienTeam];
			assert(td);
			teamDefinition = td->id;
		} else {
			teamDefinition = gi.Cvar_String("ai_alienteam");
		}
	} else {
		teamDefinition = gi.Cvar_String("ai_civilianteam");
	}
	gi.GetCharacterValues(teamDefinition, &ent->chr);
	if (!ent->chr.teamDef)
		gi.Error("Could not set teamDef for character: '%s'", teamDefinition);
}


/**
 * @brief Sets the actor's equipment.
 * @param ent Actor to give equipment to.
 * @param[in] ed Equipment definition for the new actor.
 */
static void AI_SetEquipment (Edict* ent, const equipDef_t* ed)
{
	/* Pack equipment. */
	game.invi.EquipActor(&ent->chr, ed, ent->chr.teamDef->onlyWeapon, ent->chr.score.skills[ABILITY_POWER]);
}

/**
 * @brief Initializes the actor.
 * @param[in] player Player to which this actor belongs.
 * @param[in,out] actor Pointer to Edict representing actor.
 * @param[in] ed Equipment definition for the new actor. Might be @c nullptr.
 */
static void AI_InitPlayer (const Player& player, Actor* actor, const equipDef_t* ed)
{
	const int team = player.getTeam();

	/* Set the model and chose alien race. */
	AI_SetCharacterValues(actor, team);

	/* Calculate stats. */
	AI_SetStats(actor, team);

	/* Give equipment. */
	if (ed != nullptr)
		AI_SetEquipment(actor, ed);

	/* after equipping the actor we can also get the model indices */
	actor->setBody(gi.ModelIndex(CHRSH_CharGetBody(&actor->chr)));
	actor->setHead(gi.ModelIndex(CHRSH_CharGetHead(&actor->chr)));

	/* no need to call G_SendStats for the AI - reaction fire is serverside only for the AI */
	if (frand() < 0.75f) {
		G_ClientStateChange(player, actor, STATE_REACTION, false);
	}

	/* initialize the LUA AI now */
	AIL_InitActor(actor);
}

static const equipDef_t* G_GetEquipmentForAISpawn (int team)
{
	/* prepare equipment */
	if (team == TEAM_CIVILIAN)
		return nullptr;

	const char* equipID = gi.Cvar_String("ai_equipment");
	const equipDef_t* ed = G_GetEquipDefByID(equipID);
	if (ed == nullptr)
		ed = &gi.csi->eds[0];
	return ed;
}

static Actor* G_SpawnAIPlayer (const Player& player, const equipDef_t* ed)
{
	Actor* actor = G_ClientGetFreeSpawnPointForActorSize(player, ACTOR_SIZE_NORMAL);
	if (!actor) {
		gi.DPrintf("Not enough spawn points for team %i\n", player.getTeam());
		return nullptr;
	}

	/* initialize the new actor */
	AI_InitPlayer(player, actor, ed);

	G_TouchTriggers(actor);

	gi.DPrintf("Spawned ai player for team %i with entnum %i (%s)\n", actor->getTeam(), actor->getIdNum(), actor->chr.name);
	G_CheckVis(actor, VT_PERISHCHK | VT_NEW);
	G_CheckVisTeamAll(actor->getTeam(), 0, actor);

	return actor;
}

/**
 * @brief Spawn civilians and aliens
 * @param[in] player
 * @param[in] numSpawn
 * @sa AI_CreatePlayer
 */
static void G_SpawnAIPlayers (const Player& player, int numSpawn)
{
	const equipDef_t* ed = G_GetEquipmentForAISpawn(player.getTeam());

	for (int i = 0; i < numSpawn; i++) {
		if (G_SpawnAIPlayer(player, ed) == nullptr)
			break;
	}

	/* show visible actors */
	G_VisFlagsClear(player.getTeam());
	G_CheckVis(nullptr, 0);
}

/**
 * @brief If the cvar g_endlessaliens is set we will endlessly respawn aliens
 * @note This can be used for rescue or collect missions where it is enough to do something,
 * and then leave the map (rescue zone)
 */
void AI_CheckRespawn (int team)
{
	if (!g_endlessaliens->integer)
		return;

	if (team != TEAM_ALIEN)
		return;

	const int spawned = level.initialAlienActorsSpawned;
	const int alive = level.num_alive[team];
	int diff = spawned - alive;
	const equipDef_t* ed = G_GetEquipmentForAISpawn(team);

	while (diff > 0) {
		const Player* player = G_GetPlayerForTeam(team);
		Actor* actor = G_SpawnAIPlayer(*player, ed);
		if (actor == nullptr)
			break;

		const playermask_t playerMask = G_VisToPM(actor->visflags);
		G_AppearPerishEvent(playerMask, true, *actor, nullptr);
		G_EventActorAdd(~playerMask, *actor);

		diff--;
	}
}

/**
 * @brief Spawn civilians and aliens
 * @param[in] team
 * @sa G_SpawnAIPlayer
 * @return Player pointer
 * @note see cvars ai_singleplayeraliens, ai_numcivilians, ai_multiplayeraliens
 */
Player* AI_CreatePlayer (int team)
{
	if (!sv_ai->integer) {
		gi.DPrintf("AI deactivated - set sv_ai cvar to 1 to activate it\n");
		return nullptr;
	}

	/* set players to ai players and cycle over all of them */
	Player* p = nullptr;
	while ((p = G_PlayerGetNextAI(p))) {
		if (p->isInUse())
			continue;
		p->reset();
		p->setInUse(true);
		p->setNum(p - game.players);
		p->pers.ai = true;
		G_SetTeamForPlayer(*p, team);
		if (p->getTeam() == TEAM_CIVILIAN) {
			G_SpawnAIPlayers(*p, ai_numcivilians->integer);
		} else {
			if (G_IsSinglePlayer())
				G_SpawnAIPlayers(*p, ai_singleplayeraliens->integer);
			else
				G_SpawnAIPlayers(*p, ai_multiplayeraliens->integer);

			level.initialAlienActorsSpawned = level.num_spawned[p->getTeam()];
		}

		gi.DPrintf("Created AI player (team %i)\n", p->getTeam());
		return p;
	}

	/* nothing free */
	return nullptr;
}
