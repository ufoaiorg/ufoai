/**
 * @file
 * @brief Artificial Intelligence for opponents or human controlled actors that are panicked.
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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

#define SCORE_HIDE			60
#define SCORE_CLOSE_IN		20
#define SCORE_KILL			30
#define SCORE_RANDOM		10
#define SCORE_REACTION_ERADICATION 30
#define SCORE_REACTION_FEAR_FACTOR 20
#define SCORE_NONHIDING_PLACE_PENALTY 25
#define SCORE_RAGE			40
#define SCORE_CIV_FACTOR	0.25
#define SCORE_DISABLED_FACTOR 0.25

#define SCORE_CIV_RANDOM	10
#define SCORE_RUN_AWAY		50
#define SCORE_CIV_LAZINESS	5
#define RUN_AWAY_DIST		160
#define WAYPOINT_CIV_DIST	768
#define HERD_THRESHOLD 128
#define SCORE_HERDING_PENALTY 100
#define SCORE_NOSAFE_POSITION_PENALTY 500

#define SCORE_MISSION_OPPONENT_TARGET	50
#define SCORE_MISSION_TARGET	60
#define SCORE_MISSION_HOLD		25
#define MISSION_HOLD_DIST		96

#define SCORE_PANIC_RUN_TO_FRIENDS 300.0
#define SCORE_PANIC_FLEE_FROM_STRANGERS 500.0
#define SCORE_PANIC_RANDOM 25.0

#define AI_ACTION_NOTHING_FOUND -10000.0

#define CLOSE_IN_DIST		1200.0
#define SPREAD_FACTOR		8.0
#define	SPREAD_NORM(x)		((x) > 0 ? SPREAD_FACTOR/((x)*torad) : 0)
/** @brief distance for (ai) hiding in grid tiles */
#define HIDE_DIST			7
#define HERD_DIST			7
#define HOLD_DIST			3

#define INVDEF_FOR_SHOOTTYPE(st) (IS_SHOT_RIGHT(st)?INVDEF(CID_RIGHT):IS_SHOT_LEFT(st)?INVDEF(CID_LEFT):IS_SHOT_HEADGEAR(st)?INVDEF(CID_HEADGEAR):nullptr)

static pathing_t* hidePathingTable;
static pathing_t* herdPathingTable;

void AI_Init (void)
{
	hidePathingTable = nullptr;
	herdPathingTable = nullptr;
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
	if (spread < 1.0)
		spread = 1.0;
	spread *= torad;

	float cosSpread = cos(spread);
	vec3_t dtarget;
	VectorSubtract(target, ent->origin, dtarget);
	VectorNormalizeFast(dtarget);
	vec3_t back;
	VectorScale(dtarget, PLAYER_WIDTH / spread, back);

	Actor* check = nullptr;
	while ((check = G_EdictsGetNextLivingActorOfTeam(check, ent->team))) {
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
static bool AI_FighterCheckShoot (const Edict* ent, const Edict* check, const fireDef_t* fd, float* dist)
{
	/* check range */
	*dist = VectorDist(ent->origin, check->origin);
	if (*dist > fd->range)
		return false;

	/* if insane, we don't check more */
	if (G_IsInsane(ent))
		return true;

	/* don't shoot - we are to close */
	if (*dist < fd->splrad)
		return false;

	/* check FF */
	vec2_t effSpread;
	G_CalcEffectiveSpread(ent, fd, effSpread);
	if (AI_CheckFF(ent, check->origin, effSpread[0], fd->splrad))
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
	switch (ent->team) {
	case TEAM_ALIEN: {
		/* only use the door when there is no civilian or phalanx to kill */
		Actor* check = nullptr;

		/* see if there are enemies */
		while ((check = G_EdictsGetNextLivingActor2(check))) {
			/* don't check for aliens */
			if (check->team == ent->team)
				continue;
			/* check whether the origin of the enemy is inside the
			 * AI actors view frustum */
			if (!G_FrustumVis(check, ent->origin))
				continue;
			/* check whether the enemy is close enough to change the state */
			if (VectorDist(check->origin, ent->origin) > G_VisCheckDist(ent))
				continue;
			const float actorVis = G_ActorVis(check->origin, check, ent, true);
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
		gi.DPrintf("Invalid team in AI_CheckUsingDoor: %i for ent type: %i\n",
			ent->team, ent->type);
		break;
	}
	return true;
}

/**
 * @brief Checks whether it would be smart to change the state to STATE_CROUCHED
 * @param[in] ent The AI controlled actor to chech the state change for
 * @returns true if the actor should go into STATE_CROUCHED, false otherwise
 */
static bool AI_CheckCrouch (const Edict* ent)
{
	Actor* check = nullptr;

	/* see if we are very well visible by an enemy */
	while ((check = G_EdictsGetNextLivingActor2(check))) {
		/* don't check for civilians or aliens */
		if (check->team == ent->team || G_IsCivilian(check))
			continue;
		/* check whether the origin of the enemy is inside the
		 * AI actors view frustum */
		if (!G_FrustumVis(check, ent->origin))
			continue;
		/* check whether the enemy is close enough to change the state */
		if (VectorDist(check->origin, ent->origin) > G_VisCheckDist(ent))
			continue;
		const float actorVis = G_ActorVis(check->origin, check, ent, true);
		if (actorVis >= ACTOR_VIS_50)
			return true;
	}
	return false;
}

/**
 * @brief Checks whether the given alien should try to hide because there are enemies close
 * enough to shoot the alien.
 * @param[in] ent The alien edict that should (maybe) hide
 * @return @c true if hide is needed or @c false if the alien thinks that it is not needed
 */
static bool AI_HideNeeded (const Edict* ent)
{
	/* aliens will consider hiding if they are not brave, or there is a dangerous enemy in sight */
	if (ent->morale <= mor_brave->integer)
		return true;

	Actor* from = nullptr;
	/* test if ent is visible */
	while ((from = G_EdictsGetNextLivingActor2(from))) {
		if (from->team == ent->team)
			continue;

		if (G_IsCivilian(from))
			continue;

		const Item* item = from->getRightHandItem();
		if (!item)
			item = from->getLeftHandItem();
		if (!item)
			continue;

		const fireDef_t* fd = item->getFiredefs();
		/* search the (visible) inventory (by just checking the weapon in the hands of the enemy) */
		if (fd != nullptr && fd->range * fd->range >= VectorDistSqr(ent->origin, from->origin)) {
			const int damageRand = fd->damage[0] + fd->spldmg[0] + ((fd->damage[1] + fd->spldmg[1]) * crand());
			const int damage = std::max(0, damageRand);
			if (damage >= ent->HP / 3) {
				const int hidingTeam = AI_GetHidingTeam(ent);
				/* now check whether this enemy is visible for this alien */
				if (G_Vis(hidingTeam, ent, from, VT_NOFRUSTUM))
					return true;
			}
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
	return -ent->team;
}

/**
 * @brief Checks if the  actor's position is safe to stand on.
 * @return @c true if the actor's position is deemed safe.
 */
static bool AI_CheckPosition (const Edict* const ent)
{
	if (G_IsInsane(ent))
		return true;

	/* Don't stand on hurt triggers or fire/stun gas */
	if (G_GetEdictFromPos(ent->pos, ET_TRIGGER_HURT) || G_GetEdictFromPos(ent->pos, ET_SMOKESTUN) ||
			G_GetEdictFromPos(ent->pos, ET_FIRE))
		return false;

	return true;
}

/**
 * @brief Tries to search a hiding spot
 * @param[out] ent The actor edict. The position of the actor is updated here to perform visibility checks
 * @param[in] from The grid position the actor is (theoretically) standing at and searching a hiding location from
 * @param[in,out] tuLeft The amount of left TUs to find a hiding spot. The TUs needed to walk to the grid position
 * is subtracted. May not be @c nullptr.
 * @param[in] team The team from which actor tries to hide
 * @return @c true if hiding is possible, @c false otherwise
 */
bool AI_FindHidingLocation (int team, Edict* ent, const pos3_t from, int tuLeft)
{
	/* We need a local table to calculate the hiding steps */
	if (!hidePathingTable)
		hidePathingTable = (pathing_t*) G_TagMalloc(sizeof(*hidePathingTable), TAG_LEVEL);

	/* search hiding spot */
	const int distance = std::min(tuLeft, HIDE_DIST * 2);
	G_MoveCalcLocal(hidePathingTable, 0, ent, from, distance);
	ent->pos[2] = from[2];
	const byte minX = std::max(from[0] - HIDE_DIST, 0);
	const byte minY = std::max(from[1] - HIDE_DIST, 0);
	const byte maxX = std::min(from[0] + HIDE_DIST, PATHFINDING_WIDTH - 1);
	const byte maxY = std::min(from[1] + HIDE_DIST, PATHFINDING_WIDTH - 1);

	int bestScore = AI_ACTION_NOTHING_FOUND;
	pos3_t bestPos = {from[0], from[1], from[2]};
	for (ent->pos[1] = minY; ent->pos[1] <= maxY; ent->pos[1]++) {
		for (ent->pos[0] = minX; ent->pos[0] <= maxX; ent->pos[0]++) {
			/* Don't have TUs  to walk there */
			const pos_t delta = G_ActorMoveLength(ent, hidePathingTable, ent->pos, false);
			if (delta > tuLeft || delta == ROUTING_NOT_REACHABLE)
				continue;

			/* If enemies see this position, it doesn't qualify as hiding spot */
			ent->calcOrigin();
			if (G_TestVis(team, ent, VT_PERISHCHK | VT_NOFRUSTUM) & VS_YES)
				continue;

			/* Don't stand on dangerous terrain! */
			if (!AI_CheckPosition(ent))
				continue;
			const int score = tuLeft - delta;
			if (score > bestScore) {
				bestScore = score;
				VectorCopy(ent->pos, bestPos);
			}
		}
	}

	if (!VectorCompare(from, bestPos))
		VectorCopy(bestPos, ent->pos);
	return bestScore != AI_ACTION_NOTHING_FOUND;
}

/**
 * @brief Tries to search a spot where actor will be more closer to the target and
 * behind the target from enemy
 * @param[in] ent The actor edict.
 * @param[in] from The grid position the actor is (theoretically) standing at and
 * searching the nearest location from
 * @param[in] target Tries to find the nearest position to this location
 * @param[in] tu The available TUs of the actor
 */
bool AI_FindHerdLocation (Edict* ent, const pos3_t from, const vec3_t target, int tu)
{
	if (!herdPathingTable)
		herdPathingTable = (pathing_t*) G_TagMalloc(sizeof(*herdPathingTable), TAG_LEVEL);

	/* find the nearest enemy actor to the target*/
	vec_t bestlength = 0.0f;
	Actor* next = nullptr;
	Actor* enemy = nullptr;
	while ((next = G_EdictsGetNextLivingActorOfTeam(next, AI_GetHidingTeam(ent)))) {
		const vec_t length = VectorDistSqr(target, next->origin);
		if (!bestlength || length < bestlength) {
			enemy = next;
			bestlength = length;
		}
	}
	assert(enemy);

	/* calculate move table */
	const int distance = std::min(tu, HERD_DIST * 2);
	G_MoveCalcLocal(herdPathingTable, 0, ent, from, distance);
	ent->pos[2] = from[2];
	const byte minX = std::max(from[0] - HERD_DIST, 0);
	const byte minY = std::max(from[1] - HERD_DIST, 0);
	const byte maxX = std::min(from[0] + HERD_DIST, PATHFINDING_WIDTH - 1);
	const byte maxY = std::min(from[1] + HERD_DIST, PATHFINDING_WIDTH - 1);

	/* search the location */
	pos3_t bestpos = {from[0], from[1], from[2]};
	bestlength = VectorDistSqr(target, ent->origin);
	for (ent->pos[1] = minY; ent->pos[1] <= maxY; ent->pos[1]++) {
		for (ent->pos[0] = minX; ent->pos[0] <= maxX; ent->pos[0]++) {
			/* time */
			const pos_t delta = G_ActorMoveLength(ent, herdPathingTable, ent->pos, false);
			if (delta > tu || delta == ROUTING_NOT_REACHABLE)
				continue;

			/* Don't stand on dangerous terrain! */
			if (!AI_CheckPosition(ent))
				continue;

			ent->calcOrigin();
			const vec_t length = VectorDistSqr(ent->origin, target);
			if (length < bestlength) {
				vec3_t vfriend, venemy;
				/* check this position to locate behind target from enemy */
				VectorSubtract(target, ent->origin, vfriend);
				VectorNormalizeFast(vfriend);
				VectorSubtract(enemy->origin, ent->origin, venemy);
				VectorNormalizeFast(venemy);
				if (DotProduct(vfriend, venemy) > 0.5) {
					bestlength = length;
					VectorCopy(ent->pos, bestpos);
				}
			}
		}
	}

	if (!VectorCompare(from, bestpos)) {
		VectorCopy(bestpos, ent->pos);
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
static Edict* AI_SearchDestroyableObject (const Edict* ent, const fireDef_t* fd)
{
#if 0
	/* search best none human target */
	Edict* check = nullptr;
	float dist;

	while ((check = G_EdictsGetNextInUse(check))) {
		if (G_IsBreakable(check)) {
			if (!AI_FighterCheckShoot(ent, check, fd, &dist))
				continue;

			/* check whether target is visible enough */
			const float vis = G_ActorVis(ent->origin, check, true);
			if (vis < ACTOR_VIS_0)
				continue;

			/* take the first best breakable or door and try to shoot it */
			return check;
		}
	}
#endif
	return nullptr;
}

#define GRENADE_CHECK_PARTITIONS	4
static bool AI_CheckLineOfFire (const Edict* shooter, const Edict* target, const fireDef_t* fd, int shots) {
	vec3_t dir, origin;
	VectorSubtract(target->origin, shooter->origin, dir);
	G_GetShotOrigin(shooter, fd, dir, origin);
	if (!fd->gravity) {
		/* gun-to-target line free? */
		const trace_t trace = G_Trace(Line(origin, target->origin), shooter, MASK_SHOT);
		const Edict* trEnt = G_EdictsGetByNum(trace.entNum);
		const bool hitBreakable = trEnt && G_IsBrushModel(trEnt) && G_IsBreakable(trEnt);
		const bool shotBreakable = hitBreakable && (fd->shots > 1 || shots > 1) && trEnt->HP < fd->damage[0] + fd->spldmg[0];
		if (trace.fraction < 1.0 && (!trEnt || (!VectorCompare(trEnt->pos, target->pos) && !shotBreakable)))
			return false;
	} else {
		/* gun-to-target *parabola* free? */
		vec3_t at, v;
		VectorCopy(target->origin, at);
		/* Grenades are targeted at the ground in G_ShootGrenade */
		at[2] -= GROUND_DELTA;
		const float dt = gi.GrenadeTarget(origin, at, fd->range, fd->launched, fd->rolled, v) / GRENADE_CHECK_PARTITIONS;
		if (!dt)
			return false;
		VectorSubtract(at, origin, dir);
		VectorScale(dir, 1.0 / GRENADE_CHECK_PARTITIONS, dir);
		dir[2] = 0;
		float vz = v[2];
		int i;
		for (i = 0; i < GRENADE_CHECK_PARTITIONS; i++) {
			VectorAdd(origin, dir, at);
			at[2] += dt * (vz - 0.5 * GRAVITY * dt);
			vz -= GRAVITY * dt;
			const trace_t trace = G_Trace(Line(origin, at), shooter, MASK_SHOT);
			const Edict* trEnt = G_EdictsGetByNum(trace.entNum);
			if (trace.fraction < 1.0 && (!trEnt || !VectorCompare(trEnt->pos, target->pos))) {
				break;
			}
			VectorCopy(at, origin);
		}
		if (i < GRENADE_CHECK_PARTITIONS)
			return false;
	}
	return true;
}

/**
 * @todo timed firedefs that bounce around should not be thrown/shoot about the whole distance
 */
static void AI_SearchBestTarget (AiAction* aia, const Edict* ent, Edict* check, const Item* item, shoot_types_t shootType, int tu, float* maxDmg, int* bestTime, const fireDef_t* fdArray)
{
	float vis = ACTOR_VIS_0;
	bool visChecked = false;	/* only check visibility once for an actor */
	bool hasLineOfFire = false;
	int shotChecked = NONE;

	for (fireDefIndex_t fdIdx = 0; fdIdx < item->ammoDef()->numFiredefs[fdArray->weapFdsIdx]; fdIdx++) {
		const fireDef_t* fd = &fdArray[fdIdx];
		const float acc = GET_ACC(ent->chr.score.skills[ABILITY_ACCURACY], fd->weaponSkill) *
				G_ActorGetInjuryPenalty(ent, MODIFIER_ACCURACY);
		const float nspread = SPREAD_NORM((fd->spread[0] + fd->spread[1]) * 0.5 + acc);
		const int time = G_ActorGetModifiedTimeForFiredef(ent, fd, false);
		/* how many shoots can this actor do */
		const int shots = tu / time;
		if (shots) {
			float dist;
			const bool stunWeapon = (item->def()->dmgtype == gi.csi->damStunElectro || item->def()->dmgtype == gi.csi->damStunGas);
			if (stunWeapon && !G_IsInsane(ent) && (G_IsStunned(check) || CHRSH_IsTeamDefRobot(check->chr.teamDef)))
				return;
			if (!AI_FighterCheckShoot(ent, check, fd, &dist))
				continue;

			/* check how good the target is visible */
			if (!visChecked) {	/* only do this once per actor ! */
				vis = G_ActorVis(ent->origin, ent, check, true);
				visChecked = true;
			}

			if (vis == ACTOR_VIS_0)
				return;

			/*
			 * check weapon can hit, we only want to do this once unless the LoF actually changes
			 * between shots, only hand grenades seem to do this (rolled vs thrown)
			 */
			const int shotFlags = fd->gravity | (fd->launched << 1) | (fd->rolled << 2);
			if (shotChecked != shotFlags) {
				shotChecked = shotFlags;
				hasLineOfFire = AI_CheckLineOfFire(ent, check, fd, shots);
			}
			if (!hasLineOfFire)
				continue;

			/* calculate expected damage */
			float dmg = vis * (fd->damage[0] + fd->spldmg[0]) * fd->shots * shots;
			if (nspread && dist > nspread)
				dmg *= nspread / dist;

			/* take into account armour */
			if (check->getArmour()) {
				const objDef_t* ad = check->getArmour()->def();
				dmg *= 1.0 - ad->protection[fd->dmgweight] * 0.01;
			}

			if (dmg > check->HP && G_IsReaction(check))
				/* reaction shooters eradication bonus */
				dmg = check->HP + SCORE_KILL + SCORE_REACTION_ERADICATION;
			else if (dmg > check->HP)
				/* standard kill bonus */
				dmg = check->HP + SCORE_KILL;

			/* ammo is limited and shooting gives away your position */
			if ((dmg < 25.0 && vis < 0.2) /* too hard to hit */
				|| (dmg < 10.0 && vis < 0.6) /* uber-armour */
				|| dmg < 0.1) /* at point blank hit even with a stick */
				continue;

			/* civilian malus */
			if (G_IsCivilian(check) && !G_IsInsane(ent))
				dmg *= SCORE_CIV_FACTOR;

			/* Stunned malus */
			if (G_IsStunned(check) && !G_IsRaged(ent))
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
				if (!fd->gravity && (fd->splrad > 0.0 || G_IsStunned(check)))
					aia->z_align = GROUND_DELTA;
				else
					aia->z_align = 0;
			}

			if (!aia->target) {
				aia->target = AI_SearchDestroyableObject(ent, fd);
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
 * @param[in] ent The actor that makes the check.
 * @param[in] check The actor which is a possible opponent.
 * @returns @c true if enemies. @c false otherwise
 * @todo Should we really know if the other actor is controlled by the other team (STATE_XVI)?
 * aliens would of course know if an actor is infected (becomes part of the hive mind), but humans?
 */
static inline bool AI_IsOpponent (const Edict* ent, const Edict* check)
{
	const bool entControlled = G_IsState(ent, STATE_XVI);
	const bool checkControlled = G_IsState(check, STATE_XVI);
	if (check->team == ent->team)
		return entControlled ? !checkControlled : checkControlled;

	bool opponent = true;
	switch (ent->team) {
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
 * @brief Check if @c ent perceives @c target as hostile.
 * @note Takes lose of sanity in consideration.
 * @param[in] ent The actor that checks for hostiles.
 * @param[in] target The possible hostile actor.
 * @returns @c true if ent perceives target as hostile
 */
static inline bool AI_IsHostile (const Edict* ent, const Edict* target)
{
	if (ent == target)
		return false;

	if (G_IsInsane(ent))
		return true;

	if (!AI_IsOpponent(ent, target))
		return false;

	/* don't shoot civs in multiplayer */
	if (G_IsMultiPlayer())
		return !G_IsCivilian(target);

	return true;
}

/**
 * @brief Search the edict's inventory for a grenade or other one-use weapon.
 */
static const invDef_t* AI_SearchGrenade (const Actor* actor, Item** ip)
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
	if (IS_SHOT_RIGHT(shootType)) {
		const Item* item = actor->getRightHandItem();
		return item == nullptr;
	}
	if (IS_SHOT_LEFT(shootType)) {
		const Item* left = actor->getLeftHandItem();
		const Item* right = actor->getRightHandItem();
		return left == nullptr && (right == nullptr || !right->isHeldTwoHanded());
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
	int tu = G_ActorUsableTUs(actor) - move;

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
	float maxDmg = 0.0;
	float bestActionScore = 0.0;
	int bestTime = -1;
	Actor* check = nullptr;

	while ((check = G_EdictsGetNextLivingActor2(check))) {
		if (check->isSamePosAs(to) || !AI_IsHostile(actor, check))
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
			AI_SearchBestTarget(aia, actor, check, item, shootType, tu - invMoveCost, &maxDmg, &bestTime, fdArray);
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
	if (!AI_CheckPosition(actor))
		bestActionScore -= SCORE_NOSAFE_POSITION_PENALTY;

	if (!G_IsRaged(actor)) {
		const int hidingTeam = AI_GetHidingTeam(actor);
		/* hide */
		if (!AI_HideNeeded(actor) || !(G_TestVis(hidingTeam, actor, VT_PERISHCHK | VT_NOFRUSTUM) & VS_YES)) {
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
	} else if (G_IsRaged(actor)) {
		/* reward closing in */
		float minDist = CLOSE_IN_DIST;
		check = nullptr;
		while ((check = G_EdictsGetNextLivingActor2(check))) {
			if (check->team != actor->team) {
				const float dist = VectorDist(actor->origin, check->origin);
				minDist = std::min(dist, minDist);
			}
		}
		bestActionScore += SCORE_CLOSE_IN * (1.0 - minDist / CLOSE_IN_DIST);
	} else {
		/* if no target available let them wander around until they find one */
		bestActionScore += SCORE_RANDOM * frand();
	}

	/* penalize herding */
	check = nullptr;
	while ((check = G_EdictsGetNextLivingActorOfTeam(check, actor->team))) {
		const float dist = VectorDist(actor->origin, check->origin);
		if (dist < HERD_THRESHOLD)
			bestActionScore -= SCORE_HERDING_PENALTY;
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
	const int tu = G_ActorUsableTUs(actor) - move;

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
		if (!G_IsPanicked(actor) && teamDef->weapons)
			return AI_FighterCalcActionScore(actor, to, aia);
	} else
		gi.DPrintf("AI_CivilianCalcActionScore: Error - civilian team with no teamdef\n");

	/* run away */
	float minDist, minDistCivilian, minDistFighter;
	minDist = minDistCivilian = minDistFighter = RUN_AWAY_DIST * UNIT_SIZE;

	Actor* check = nullptr;
	while ((check = G_EdictsGetNextLivingActor2(check))) {
		float dist;
		if (actor == check)
			continue;
		dist = VectorDist(actor->origin, check->origin);
		/* if we are trying to walk to a position that is occupied by another actor already we just return */
		if (!dist)
			return AI_ACTION_NOTHING_FOUND;
		switch (check->team) {
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
	check = nullptr;
	while ((check = G_EdictsGetNextLivingActor2(check))) {
		if (actor == check)
			continue;
		if (!(G_IsAlien(check) || G_IsInsane(actor)))
			continue;

		if (G_ActorVis(check->origin, check, actor, true) > 0.25)
			reactionTrap += SCORE_NONHIDING_PLACE_PENALTY;
	}
	delta -= reactionTrap;
	float bestActionScore = delta;

	/* Try not to stand in dangerous terrain */
	if (!AI_CheckPosition(actor))
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
	const int tu = G_ActorUsableTUs(actor) - move;

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
	while ((check = G_EdictsGetNextLivingActor2(check))) {
		float dist;
		if (actor == check)
			continue;
		dist = VectorDist(actor->origin, check->origin);
		/* if we are trying to walk to a position that is occupied by another actor already we just return */
		if (!dist)
			return AI_ACTION_NOTHING_FOUND;
		if (check->team == actor->team) {
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
	while ((check = G_EdictsGetNextLivingActor2(check))) {
		if (actor == check)
			continue;
		if (G_IsInsane(actor))
			continue;

		if (G_ActorVis(check->origin, check, actor, true) > 0.25)
			bestActionScore -= SCORE_NONHIDING_PLACE_PENALTY;
	}

	/* Try not to stand in dangerous terrain */
	if (!AI_CheckPosition(actor))
		bestActionScore -= SCORE_NOSAFE_POSITION_PENALTY;

	/* add random effects */
	bestActionScore += SCORE_PANIC_RANDOM * frand();

	return bestActionScore;
}

/**
 * @brief Try to go close to a mission edict
 * @param[in,out] ent The actor edict.
 * @param[in] to The target position.
 * @return @c true if hiding is possible, @c false otherwise
 */
static bool AI_FindMissionLocation (Edict* ent, const pos3_t to)
{
	const byte minX = std::max(to[0] - HOLD_DIST, 0);
	const byte minY = std::max(to[1] - HOLD_DIST, 0);
	const byte maxX = std::min(to[0] + HOLD_DIST, PATHFINDING_WIDTH - 1);
	const byte maxY = std::min(to[1] + HOLD_DIST, PATHFINDING_WIDTH - 1);
	int bestDist = ROUTING_NOT_REACHABLE;
	pos3_t bestPos = {to[0], to[1], to[2]};

	ent->pos[2] = to[2];
	for (ent->pos[1] = minY; ent->pos[1] <= maxY; ++ent->pos[1]) {
		for (ent->pos[0] = minX; ent->pos[0] <= maxX; ++ent->pos[0]) {
			/* Can't walk there */
			if (G_ActorMoveLength(ent, level.pathingMap, ent->pos, true) == ROUTING_NOT_REACHABLE)
				continue;
			/* Don't stand on dangerous terrain! */
			if (!AI_CheckPosition(ent))
				continue;

			const int distX = std::abs(ent->pos[0] - to[0]);
			const int distY = std::abs(ent->pos[1] - to[1]);
			const int dist = distX + distY + std::max(distX, distY);
			if (dist < bestDist) {
				bestDist = dist;
				VectorCopy(ent->pos, bestPos);
			}
		}
	}
	if (!VectorCompare(to, bestPos))
		VectorCopy(bestPos, ent->pos);

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

	if (actor->team == TEAM_CIVILIAN) {
		Edict* checkPoint = nullptr;
		int i = 0;
		/* find waypoints in a closer distance - if civilians are not close enough, let them walk
		 * around until they came close */
		for (checkPoint = level.ai_waypointList; checkPoint != nullptr; checkPoint = checkPoint->groupChain) {
			if (checkPoint->inuse)
				continue;

			/* the lower the count value - the nearer the final target */
			if (checkPoint->count < actor->count) {
				if (VectorDist(actor->origin, checkPoint->origin) <= WAYPOINT_CIV_DIST) {
					if (!AI_FindMissionLocation(actor, checkPoint->pos))
						continue;

					const int length = G_ActorUsableTUs(actor) - G_ActorMoveLength(actor, level.pathingMap, actor->pos, true);
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
	} else if (actor->team == TEAM_ALIEN) {
		/* search for a mission edict */
		Edict* mission = nullptr;
		while ((mission = G_EdictsGetNextInUse(mission))) {
			if (mission->type == ET_MISSION) {
				if (!AI_FindMissionLocation(actor, mission->pos))
					continue;
				if (player.getTeam() == mission->team) {
					actionScore = SCORE_MISSION_TARGET;
				} else {
					/* try to prevent the phalanx from reaching their mission target */
					actionScore = SCORE_MISSION_OPPONENT_TARGET;
				}

				actor->calcOrigin();
				/* Don't cluster everybody in the same place */
				Actor* check = nullptr;
				while ((check = G_EdictsGetNextLivingActor2(check))) {
					const float dist = VectorDist(actor->origin, check->origin);
					if (dist < MISSION_HOLD_DIST) {
						if (check->team == actor->team)
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
	/* check if the actor is in crouched state and try to stand up before doing the move */
	if (G_IsCrouched(actor))
		G_ClientStateChange(player, actor, STATE_CROUCHED, true);

	/* calculate move table */
	G_MoveCalc(0, actor, actor->pos, G_ActorUsableTUs(actor));
	Com_DPrintf(DEBUG_ENGINE, "AI_PrepBestAction: Called MoveMark.\n");
	gi.MoveStore(level.pathingMap);

	/* set borders */
	const int dist = (G_ActorUsableTUs(actor) + 1) / 2;
	const int xl = std::max((int) actor->pos[0] - dist, 0);
	const int yl = std::max((int) actor->pos[1] - dist, 0);
	const int xh = std::min((int) actor->pos[0] + dist, PATHFINDING_WIDTH);
	const int yh = std::min((int) actor->pos[1] + dist, PATHFINDING_WIDTH);

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
	for (to[2] = 0; to[2] < PATHFINDING_HEIGHT; ++to[2]) {
		for (to[1] = yl; to[1] < yh; ++to[1]) {
			for (to[0] = xl; to[0] < xh; ++to[0]) {
				const pos_t move = G_ActorMoveLength(actor, level.pathingMap, to, true);
				if (move >= ROUTING_NOT_REACHABLE)
					continue;
				if (move > G_ActorUsableTUs(actor))
					continue;

				if (G_IsCivilian(actor))
					bestActionScore = AI_CivilianCalcActionScore(actor, to, &aia);
				else if (G_IsPanicked(actor))
					bestActionScore = AI_PanicCalcActionScore(actor, to, &aia);
				else
					bestActionScore = AI_FighterCalcActionScore(actor, to, &aia);

				if (bestActionScore > best) {
					bestAia = aia;
					best = bestActionScore;
				}
			}
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
		if (G_IsDead(actor))
			break;
		G_ClientMove(player, 0, actor, bestAia.to);
		if (actor->isSamePosAs(bestAia.to))
			break;
		const pos_t length = G_ActorMoveLength(actor, level.pathingMap, bestAia.to, false);
		if (length > G_ActorUsableTUs(actor) || length >= ROUTING_NOT_REACHABLE)
			break;
	}
	/* test for possible death during move. reset bestAia due to dead status */
	if (G_IsDead(actor))
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
	const byte crouchingState = G_IsCrouched(actor) ? 1 : 0;
	G_MoveCalc(actor->team, actor, pos, G_ActorUsableTUs(actor));

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
static bool AI_TryToReloadWeapon (Actor* actor, containerIndex_t containerID)
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
void AI_ActorThink (Player& player, Actor* actor)
{
	/* if a weapon can be reloaded we attempt to do so if TUs permit, otherwise drop it */
	Item* rightH = actor->getRightHandItem();
	Item* leftH = actor->getLeftHandItem();
	if (!G_IsPanicked(actor)) {
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
			if (G_IsDead(actor))
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
		G_ClientMove(player, actor->team, actor, bestAia.stop);
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

	/** Duke's playground for a completely new AI. While in developement, it is only available to Phalanx */
	if (player.getTeam() == TEAM_PHALANX && g_aihumans->integer == 2) {
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
				if (g_ailua->integer)
					AIL_ActorThink(player, actor);
				else
					AI_ActorThink(player, actor);
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
 * @param ent Actor to set the stats for.
 * @param team Phalanx, civilian or alien ?
 */
static void AI_SetStats (Edict* ent, int team)
{
	const char* templateId = "";
	if (team != TEAM_CIVILIAN && gi.csi->numAlienTeams) {
		for (int i = 0; i < gi.csi->numAlienTeams; ++i) {
			if (gi.csi->alienTeams[i] == ent->chr.teamDef && gi.csi->alienChrTemplates[i]) {
				templateId = gi.csi->alienChrTemplates[i]->id;
				break;
			}
		}
	}

	CHRSH_CharGenAbilitySkills(&ent->chr, G_IsMultiPlayer(), templateId);

	ent->HP = ent->chr.HP;
	ent->setMorale(ent->chr.morale);
	ent->STUN = 0;

	/* hurt aliens in ufo crash missions (5%: almost dead, 10%: wounded, 15%: stunned)  */
	if (level.hurtAliens && CHRSH_IsTeamDefAlien(ent->chr.teamDef)) {
		const float random = frand();
		int damage = 0, stun = 0;
		if (random <= 0.05f) {
			damage = ent->HP * 0.95f;
		} else if (random <= 0.15f) {
			stun = ent->HP * 0.3f;
			damage = ent->HP * 0.5f;
		} else if (random <= 0.3f) {
			stun = ent->HP * 0.75f;
		}
		ent->HP -= damage;
		if (!CHRSH_IsTeamDefRobot(ent->chr.teamDef))
			ent->STUN = stun;

		for (int i = 0; i < ent->chr.teamDef->bodyTemplate->numBodyParts(); ++i)
			ent->chr.wounds.treatmentLevel[ent->chr.teamDef->bodyTemplate->getRandomBodyPart()] += damage / BODYPART_MAXTYPE;
	}

	G_ActorGiveTimeUnits(ent);
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
	if (ent->chr.teamDef->robot && ent->chr.teamDef->onlyWeapon) {
		const objDef_t* weapon = ent->chr.teamDef->onlyWeapon;
		if (weapon->numAmmos > 0)
			game.invi.EquipActorRobot(&ent->chr.inv, weapon);
		else if (weapon->fireTwoHanded)
			game.invi.EquipActorMelee(&ent->chr.inv, ent->chr.teamDef);
		else
			gi.DPrintf("AI_InitPlayer: weapon %s has no ammo assigned and must not be fired two handed\n", weapon->id);
	} else if (ent->chr.teamDef->weapons) {
		game.invi.EquipActor(&ent->chr, ed, ent->chr.score.skills[ABILITY_POWER]);
	} else {
		gi.DPrintf("AI_InitPlayer: actor with no equipment\n");
	}
}

/**
 * @brief Initializes the actor.
 * @param[in] player Player to which this actor belongs.
 * @param[in,out] ent Pointer to Edict representing actor.
 * @param[in] ed Equipment definition for the new actor. Might be @c nullptr.
 */
static void AI_InitPlayer (const Player& player, Edict* ent, const equipDef_t* ed)
{
	const int team = player.getTeam();

	/* Set the model and chose alien race. */
	AI_SetCharacterValues(ent, team);

	/* Calculate stats. */
	AI_SetStats(ent, team);

	/* Give equipment. */
	if (ed != nullptr)
		AI_SetEquipment(ent, ed);

	/* after equipping the actor we can also get the model indices */
	ent->body = gi.ModelIndex(CHRSH_CharGetBody(&ent->chr));
	ent->head = gi.ModelIndex(CHRSH_CharGetHead(&ent->chr));

	/* no need to call G_SendStats for the AI - reaction fire is serverside only for the AI */
	if (frand() < 0.75f) {
		G_ClientStateChange(player, ent, STATE_REACTION, false);
	}

	/* initialize the LUA AI now */
	if (team == TEAM_CIVILIAN)
		AIL_InitActor(ent, "civilian", "default");
	else if (team == TEAM_ALIEN)
		AIL_InitActor(ent, "alien", "default");
	else
		gi.DPrintf("AI_InitPlayer: unknown team AI\n");
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

static Edict* G_SpawnAIPlayer (const Player& player, const equipDef_t* ed)
{
	Edict* ent = G_ClientGetFreeSpawnPointForActorSize(player, ACTOR_SIZE_NORMAL);
	if (!ent) {
		gi.DPrintf("Not enough spawn points for team %i\n", player.getTeam());
		return nullptr;
	}

	/* initialize the new actor */
	AI_InitPlayer(player, ent, ed);

	G_TouchTriggers(ent);

	gi.DPrintf("Spawned ai player for team %i with entnum %i (%s)\n", ent->team, ent->getIdNum(), ent->chr.name);
	G_CheckVis(ent, VT_PERISHCHK | VT_NEW);
	G_CheckVisTeamAll(ent->team, 0, ent);

	return ent;
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
		Edict* ent = G_SpawnAIPlayer(*player, ed);
		if (ent == nullptr)
			break;

		const playermask_t playerMask = G_VisToPM(ent->visflags);
		G_AppearPerishEvent(playerMask, true, *ent, nullptr);
		G_EventActorAdd(~playerMask, *ent);

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
