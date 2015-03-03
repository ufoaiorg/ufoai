/**
 * @file
 * @brief All parts of the main game logic that are combat related
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

#include "g_combat.h"
#include "g_actor.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_health.h"
#include "g_inventory.h"
#include "g_match.h"
#include "g_reaction.h"
#include "g_spawn.h"
#include "g_utils.h"
#include "g_vis.h"

#define MAX_WALL_THICKNESS_FOR_SHOOTING_THROUGH 8

typedef enum {
	ML_SHOOT,
	ML_WOUND,
	ML_DEATH
} morale_modifiers;

/**
 * @brief Test if point is "visible" from team.
 * @param[in] team A team to test.
 * @param[in] point A point to check.
 * @return true if point is "visible"
 */
static bool G_TeamPointVis (int team, const vec3_t point)
{
	/* test if point is visible from team */
	Actor* from = nullptr;
	while ((from = G_EdictsGetNextLivingActorOfTeam(from, team))) {
		if (!G_FrustumVis(from, point))
			continue;
		/* get viewers eye height */
		vec3_t eye;
		G_ActorGetEyeVector(from, eye);

		/* line of sight */
		if (G_TestLine(eye, point))
			continue;
		const float distance = VectorDist(from->origin, point);
		bool blocked = false;
		/* check visibility in the smoke */
		if (distance >= UNIT_SIZE) {
			Edict* e = nullptr;
			while ((e = G_EdictsGetNextInUse(e))) {
				if (!G_IsSmoke(e))
					continue;
				if (!RayIntersectAABB(eye, point, e->absBox))
					continue;

				blocked = true;
				break;
			}
		}
		if (!blocked)
			return true;
	}

	/* not visible */
	return false;
}

/**
 * @brief Applies morale changes to actors around a wounded or killed actor.
 * @note only called when mor_panic is not zero
 * @param[in] type Type of morale modifier (@sa morale_modifiers)
 * @param[in] victim An actor being a victim of the attack.
 * @param[in] attacker An actor being attacker in this attack.
 * @param[in] param Used to modify morale changes, for G_Damage() it is value of damage.
 * @sa G_Damage
 */
static void G_Morale (morale_modifiers type, const Edict* victim, const Edict* attacker, int param)
{
	Actor* actor = nullptr;
	while ((actor = G_EdictsGetNextLivingActor(actor))) {
		/* this only applies to ET_ACTOR but not ET_ACTOR2x2 */
		if (actor->type != ET_ACTOR)
			continue;
		if (G_IsCivilian(actor))
			continue;

		/* morale damage depends on the damage */
		float mod = mob_wound->value * param;
		if (type == ML_SHOOT)
			mod *= mob_shoot->value;
		/* death hurts morale even more than just damage */
		if (type == ML_DEATH)
			mod += mob_death->value;
		/* seeing how someone gets shot increases the morale change */
		if (actor == victim || (G_FrustumVis(actor, victim->origin) && G_ActorVis(actor, victim, false)))
			mod *= mof_watching->value;
		if (attacker != nullptr && actor->isSameTeamAs(attacker)) {
			/* teamkills are considered to be bad form, but won't cause an increased morale boost for the enemy */
			/* morale boost isn't equal to morale loss (it's lower, but morale gets regenerated) */
			if (victim->isSameTeamAs(attacker))
				mod *= mof_teamkill->value;
			else
				mod *= mof_enemy->value;
		}
		/* seeing a civilian die is more "acceptable" */
		if (G_IsCivilian(victim))
			mod *= mof_civilian->value;
		/* if an ally (or in singleplayermode, as human, a civilian) got shot, lower the morale, don't heighten it. */
		if (victim->isSameTeamAs(actor) || (G_IsCivilian(victim) && !G_IsAlien(actor) && G_IsSinglePlayer()))
			mod *= -1;
		if (attacker != nullptr) {
			/* if you stand near to the attacker or the victim, the morale change is higher. */
			mod *= mor_default->value + pow(0.5, VectorDist(actor->origin, victim->origin) / mor_distance->value)
				* mor_victim->value + pow(0.5, VectorDist(actor->origin, attacker->origin) / mor_distance->value)
				* mor_attacker->value;
		} else {
			mod *= mor_default->value + pow(0.5, VectorDist(actor->origin, victim->origin) / mor_distance->value)
				* mor_victim->value;
		}
		/* morale damage depends on the number of living allies */
		mod *= (1 - mon_teamfactor->value)
			+ mon_teamfactor->value * (level.num_spawned[victim->getTeam()] + 1)
			/ (level.num_alive[victim->getTeam()] + 1);
		/* being hit isn't fun */
		if (actor == victim)
			mod *= mor_pain->value;
		/* clamp new morale */
		/*+0.9 to allow weapons like flamethrowers to inflict panic (typecast rounding) */
		const int newMorale = actor->morale + (int) (MORALE_RANDOM(mod) + 0.9);
		if (newMorale > GET_MORALE(actor->chr.score.skills[ABILITY_MIND]))
			actor->setMorale(GET_MORALE(actor->chr.score.skills[ABILITY_MIND]));
		else if (newMorale < 0)
			actor->setMorale(0);
		else
			actor->setMorale(newMorale);

		/* send phys data */
		G_SendStats(*actor);
	}
}

/**
 * @brief Applies morale changes to actors who find themselves in the general direction of a shot.
 * @param shooter The shooting actor.
 * @param fd The firedef used to shoot.
 * @param from The weapon's muzzle location.
 * @param weapon The weapon used to shoot.
 * @param impact The shoot's impact location.
 */
static void G_ShotMorale (const Actor* shooter, const fireDef_t* fd, const vec3_t from, const Item* weapon, const vec3_t impact)
{
#if 0
	/* Skip not detectable shoots */
	if (weapon->def()->dmgtype == gi.csi->damLaser || fd->irgoggles)
		return;

	Actor* check = nullptr;
	const float minDist = UNIT_SIZE * 1.5f;
	while ((check = G_EdictsGetNextLivingActor(check))) {
		/* Skip yourself */
		if (check == shooter)
			continue;
		pos3_t target;
		VecToPos(impact, target);
		/* Skip the hit actor -- morale was already handled */
		if (check->isSamePosAs(target))
			continue;
		vec3_t dir1, dir2;
		VectorSubtract(check->origin, from, dir1);
		VectorSubtract(impact, from, dir2);
		const float len1 = VectorLength(dir1);
		const float len2 = VectorLength(dir2);
		const float dot = DotProduct(dir1, dir2);
		if (dot / (len1 * len2) < 0.7f)
			continue;
		/* Skip if shooting next or over an ally */
		if (check->isSameTeamAs(shooter) && VectorDistSqr(check->origin, shooter->origin)
				<= minDist * minDist)
			continue;
		vec3_t vec1;
		if (len1 > len2) {
			VectorSubtract(dir2, dir1, vec1);
		} else {
			VectorScale(dir2, dot / (len2 * len2), vec1);
			VectorSubtract(dir1, vec1, vec1);
		}
		const float morDist = (check->isSamePosAs(target) ? UNIT_SIZE * 0.5f : minDist);
		if (VectorLengthSqr(vec1) <= morDist * morDist) {
			/* @todo Add a visibility check here? */
			G_Morale(ML_SHOOT, check, shooter, fd->damage[0]);
			gi.DPrintf("Suppressing: %s\n", check->chr.name);
		}
	}
#endif
}

/**
 * @brief Function to calculate possible damages for mock pseudoaction.
 * @param[in,out] mock Pseudo action - only for calculating mock values - nullptr for real action.
 * @param[in] shooter Pointer to attacker for this mock pseudoaction.
 * @param[in] struck Pointer to victim of this mock pseudoaction.
 * @param[in] damage Updates mock value of damage.
 * @note Called only from G_Damage().
 * @sa G_Damage
 */
static void G_UpdateShotMock (shot_mock_t* mock, const Edict* shooter, const Edict* struck, int damage)
{
	assert(!struck->isSameAs(shooter) || mock->allow_self);

	if (damage <= 0)
		return;

	if (!struck->inuse || G_IsDead(struck))
		return;

	if (!G_IsAI(shooter) && !G_IsVisibleForTeam(struck, shooter->getTeam()))
		return;

	if (G_IsCivilian(struck))
		mock->civilian += 1;
	else if (struck->isSameTeamAs(shooter))
		mock->friendCount += 1;
	else if (G_IsActor(struck))
		mock->enemyCount += 1;
	else
		return;

	mock->damage += damage;
}

/**
 * @brief Update character stats for this mission after successful shoot.
 * @note Mind you that this code is always from the view of PHALANX soldiers right now, not anybody else!
 * @param[in,out] attacker Pointer to attacker.
 * @param[in] fd Pointer to fireDef_t used in shoot.
 * @param[in] target Pointer to target.
 * @sa G_UpdateCharacterSkills
 */
static void G_UpdateCharacterBodycount (Edict* attacker, const fireDef_t* fd, const Actor* target)
{
	if (!attacker || !target)
		return;

	chrScoreGlobal_t* scoreGlobal = &attacker->chr.score;
	chrScoreMission_t* scoreMission = attacker->chr.scoreMission;
	/* only phalanx soldiers have this */
	if (!scoreMission)
		return;

	killtypes_t type;
	switch (target->getTeam()) {
	case TEAM_ALIEN:
		type = KILLED_ENEMIES;
		if (fd) {
			assert(fd->weaponSkill >= 0);
			assert(fd->weaponSkill < lengthof(scoreMission->skillKills));
			scoreMission->skillKills[fd->weaponSkill]++;
		}
		break;
	case TEAM_CIVILIAN:
		type = KILLED_CIVILIANS;
		break;
	case TEAM_PHALANX:
		type = KILLED_TEAM;
		break;
	default:
		return;
	}

	if (target->isStunned()) {
		scoreMission->stuns[type]++;
		scoreGlobal->stuns[type]++;
	} else if (target->isDead()) {
		scoreMission->kills[type]++;
		scoreGlobal->kills[type]++;
	}
}

/**
 * @brief Increases the 'hit' score by one for all affected teams/skills by one (except splash damage, read below).
 * @param[in,out] attacker The soldier to update (he/she dealt the damage)
 * @param[in] target The hit target.
 * @param[in] fd the used fire definition.
 * @param[in] splashDamage Do we count it as splashdamage? If this value is not zero the stats well be counted as splashdamage and the value will be added to the overall splash-damage count.
 */
static void G_UpdateHitScore (Edict* attacker, const Edict* target, const fireDef_t* fd, const int splashDamage)
{
	if (!attacker || !target || !fd)
		return;

	chrScoreMission_t* score = attacker->chr.scoreMission;
	/* Abort if no player team. */
	if (!score)
		return;

	killtypes_t type;
	switch (target->getTeam()) {
		case TEAM_CIVILIAN:
			type = KILLED_CIVILIANS;
			break;
		case TEAM_ALIEN:
			type = KILLED_ENEMIES;
			break;
		default:
			return;
	}

	if (splashDamage) {
		if (attacker->isSameTeamAs(target)) {
			/* Increase friendly fire counter. */
			score->hitsSplashDamage[fd->weaponSkill][KILLED_TEAM] += splashDamage;
			if (!score->firedSplashHit[KILLED_TEAM]) {
				score->hitsSplash[fd->weaponSkill][KILLED_TEAM]++;
				score->firedSplashHit[KILLED_TEAM] = true;
			}
		}

		score->hitsSplashDamage[fd->weaponSkill][type] += splashDamage;
		if (!score->firedSplashHit[type]) {
			score->hitsSplash[fd->weaponSkill][type]++;
			score->firedSplashHit[type] = true;
		}
	} else {
		if (attacker->isSameTeamAs(target) && !score->firedHit[KILLED_TEAM]) {
			/* Increase friendly fire counter. */
			score->hits[fd->weaponSkill][KILLED_TEAM]++;
			score->firedHit[KILLED_TEAM] = true;
		}

		if (!score->firedHit[type]) {
			score->hits[fd->weaponSkill][type]++;
			score->firedHit[type] = true;
		}
	}
}

/**
 * @brief Reduces damage by armour and natural protection.
 * @param[in] target What we want to damage.
 * @param[in] dmgWeight The type of damage is dealt.
 * @param[in] damage The value of the damage.
 * @return The new amount of damage after applying armour and resistance.
 */
int G_ApplyProtection (const Edict* target, const byte dmgWeight, int damage)
{
	const int naturalProtection = target->chr.teamDef->resistance[dmgWeight];
	if (target->getArmour()) {
		const objDef_t* armourDef = target->getArmour()->def();
		const short armourProtection = armourDef->protection[dmgWeight];
		const short totalProtection = armourProtection + naturalProtection;
		damage = std::min(std::max(0, damage - armourProtection), std::max(1, damage - totalProtection));
	} else {
		damage = std::max(1, damage - naturalProtection);
	}
	return damage;
}

/**
 * @brief Deals damage of a give type and amount to a target.
 * @param[in,out] target What we want to damage.
 * @param[in] fd The fire definition that defines what type of damage is dealt.
 * @param[in] damage The value of the damage.
 * @param[in] attacker The attacker.
 * @param[in] mock pseudo shooting - only for calculating mock values - nullptr for real shots
 * @param[in] impact impact location - @c nullptr for splash damage
 * @return @c true if damage could be dealt (even if it was 0) @c false otherwise
 * @sa G_SplashDamage
 * @sa G_TakeDamage
 * @sa G_PrintActorStats
 */
static bool G_Damage (Edict* target, const fireDef_t* fd, int damage, Actor* attacker, shot_mock_t* mock, const vec3_t impact)
{
	assert(target);

	const bool stunEl = (fd->obj->dmgtype == gi.csi->damStunElectro);
	const bool stunGas = (fd->obj->dmgtype == gi.csi->damStunGas);
	const bool shock = (fd->obj->dmgtype == gi.csi->damShock);
	const bool smoke = (fd->obj->dmgtype == gi.csi->damSmoke);

	/* Breakables */
	if (G_IsBrushModel(target) && G_IsBreakable(target)) {
		/* Breakables are immune to stun & shock damage. */
		if (stunEl || stunGas || shock || mock || smoke)
			return false;

		if (damage >= target->HP) {
			/* don't reset the HP value here, this value is used to distinguish
			 * between triggered destroy and a shoot */
			assert(target->destroy);
			target->destroy(target);

			/* maybe the attacker is seeing something new? */
			G_CheckVisTeamAll(attacker->getTeam(), 0, attacker);

			/* check if attacker appears/perishes for any other team */
			G_CheckVis(attacker);
		} else {
			G_TakeDamage(target, damage);
		}
		return true;
	}

	/* Actors don't die again. */
	if (!G_IsLivingActor(target))
		return false;
	/* Now we know that the target is an actor */
	Actor* victim = makeActor(target);

	/* only actors after this point - and they must have a teamdef */
	assert(victim->chr.teamDef);
	const bool isRobot = CHRSH_IsTeamDefRobot(victim->chr.teamDef);

	/* Apply armour effects. */
	if (damage > 0) {
		damage = G_ApplyProtection(victim, fd->dmgweight, damage);
	} else if (damage < 0) {
		/* Robots can't be healed. */
		if (isRobot)
			return false;
	}
	Com_DPrintf(DEBUG_GAME, " Total damage: %d\n", damage);

	/* Apply difficulty settings. */
	if (G_IsSinglePlayer()) {
		if (G_IsAlien(attacker) && !G_IsAlien(victim))
			damage *= pow(1.18, g_difficulty->value);
		else if (!G_IsAlien(attacker) && G_IsAlien(victim))
			damage *= pow(1.18, -g_difficulty->value);
	}

	assert(attacker->getTeam() >= 0 && attacker->getTeam() < MAX_TEAMS);
	assert(victim->getTeam() >= 0 && victim->getTeam() < MAX_TEAMS);

	if ((g_nodamage != nullptr && !g_nodamage->integer) || mock) {
		/* hit */
		if (mock) {
			G_UpdateShotMock(mock, attacker, victim, damage);
		} else if (stunEl) {
			victim->addStun(damage);
		} else if (stunGas) {
			if (!isRobot) /* Can't stun robots with gas */
				victim->addStun(damage);
		} else if (shock) {
			/* Only do this if it's not one from our own team ... they should have known that there was a flashbang coming. */
			if (!isRobot && !victim->isSameTeamAs(attacker)) {
				/** @todo there should be a possible protection, too */
				/* dazed entity wont reaction fire */
				victim->removeReaction();
				G_ActorReserveTUs(victim, 0, victim->chr.reservedTus.shot, victim->chr.reservedTus.crouch);
				/* flashbangs kill TUs */
				G_ActorSetTU(victim, 0);
				G_SendStats(*victim);
				/* entity is dazed */
				victim->setDazed();
				G_ClientPrintf(victim->getPlayer(), PRINT_HUD, _("Soldier is dazed!\nEnemy used flashbang!"));
				return !mock;
			} else {
				return false;
			}
		} else {
			if (damage < 0) {
				/* The 'attacker' is healing the victim. */
				G_TreatActor(victim, fd, damage, attacker->getTeam());
			} else {
				/* Real damage was dealt. */
				G_DamageActor(victim, damage, impact);
				/* Update overall splash damage for stats/score. */
				if (!mock && damage > 0 && fd->splrad) /**< Check for >0 and splrad to not count this as direct hit. */
					G_UpdateHitScore(attacker, victim, fd, damage);
			}
		}
	}

	if (mock)
		return false;

	G_CheckDeathOrKnockout(victim, attacker, fd, damage);
	return true;
}

void G_CheckDeathOrKnockout (Actor* target, Actor* attacker, const fireDef_t* fd, int damage)
{
	/* Sanity check */
	target->HP = std::min(std::max(target->HP, 0), target->chr.maxHP);
	/* Check death/knockout. */
	if (target->HP == 0 || target->HP <= target->getStun()) {
		G_SendStats(*target);

		if (G_ActorDieOrStun(target, attacker)) {
			G_PrintActorStats(target, attacker, fd);

			/* apply morale changes */
			if (mor_panic->integer)
				G_Morale(ML_DEATH, target, attacker, damage);

			/* Update number of killed/stunned actors for this attacker. */
			G_UpdateCharacterBodycount(attacker, fd, target);
		}
	} else {
		target->chr.minHP = std::min(target->chr.minHP, target->HP);
		if (damage > 0) {
			if (mor_panic->integer)
				G_Morale(ML_WOUND, target, attacker, damage);
		}
		G_SendStats(*target);
	}
}

/**
 * @brief Checks surface vulnerability for firedefinition damagetype.
 * @return True if the surface has the fireaffected flag set and the firedef
 * might produce fire (e.g. flamer)
 * @param[in] surface The collision surface to check the surface flag for
 * @param[in] fd The firedef to check the @c dmgtype for
 * @todo Such function should check notonly fire - it should be generic function to check
 * surface vulnerability for given damagetype.
 */
static inline bool G_FireAffectedSurface (const cBspSurface_t* surface, const fireDef_t* fd)
{
	if (!surface)
		return false;

	if (!(surface->surfaceFlags & SURF_BURN))
		return false;

	if (fd->obj->dmgtype == gi.csi->damIncendiary || fd->obj->dmgtype == gi.csi->damFire || fd->obj->dmgtype == gi.csi->damBlast)
		return true;

	return false;
}

/**
 * @brief Deals splash damage to a target and its surroundings.
 * @param[in] ent The shooting actor
 * @param[in] fd The fire definition that defines what type of damage is dealt and how big the splash radius is.
 * @param[in] impact The impact vector where the grenade is exploding
 * @param[in,out] mock pseudo shooting - only for calculating mock values - nullptr for real shots
 * @param[in] tr The trace where the grenade hits something (or not)
 */
static void G_SplashDamage (Actor* ent, const fireDef_t* fd, vec3_t impact, shot_mock_t* mock, const trace_t* tr)
{
	assert(fd->splrad > 0.0f);

	const bool shock = (fd->obj->dmgtype == gi.csi->damShock);

	Edict* check = nullptr;
	while ((check = G_EdictsGetNextInUse(check))) {
		/* If we use a blinding weapon we skip the target if it's looking
		 * away from the impact location. */
		if (shock && !G_FrustumVis(check, impact))
			continue;

		const bool isActor = G_IsLivingActor(check);
		vec3_t center;
		if (G_IsBrushModel(check) && G_IsBreakable(check))
			check->absBox.getCenter(center);
		else if (isActor || G_IsBreakable(check))
			VectorCopy(check->origin, center);
		else
			continue;

		/* check for distance */
		float dist = VectorDist(impact, center);
		dist = dist > UNIT_SIZE / 2 ? dist - UNIT_SIZE / 2 : 0.0f;
		if (dist > fd->splrad)
			continue;

		if (fd->irgoggles) {
			if (isActor) {
				/* check whether this actor (check) is in the field of view of the 'shooter' (ent) */
				if (G_FrustumVis(ent, check->origin)) {
					if (!mock) {
						const unsigned int playerMask = G_TeamToPM(ent->getTeam()) ^ G_VisToPM(check->visflags);
						G_AppearPerishEvent(playerMask, true, *check, ent);
						G_VisFlagsAdd(*check, G_PMToVis(playerMask));
					}
				}
			}
			continue;
		}

		/* check for walls */
		if (isActor && G_TestLine(impact, check->origin))
			continue;

		/* do damage */
		const int damage = shock ? 0 : fd->spldmg[0] * (1.0f - dist / fd->splrad);

		if (mock)
			mock->allow_self = true;
		/* Send hurt sounds for actors, but only if they'll recieve damage from this attack */
		if (G_Damage(check, fd, damage, ent, mock, nullptr) && isActor
				&& (G_ApplyProtection(check, fd->dmgweight,  damage) > 0) && !shock) {
			const teamDef_t* teamDef = check->chr.teamDef;
			const int gender = check->chr.gender;
			const char* sound = teamDef->getActorSound(gender, SND_HURT);
			G_EventSpawnSound(G_VisToPM(check->visflags), *check, nullptr, sound);
		}
		if (mock)
			mock->allow_self = false;
	}

	/** @todo splash might also hit other surfaces and the trace doesn't handle that */
	if (tr && G_FireAffectedSurface(tr->surface, fd)) {
		/* move a little away from the impact vector */
		VectorMA(impact, 1, tr->plane.normal, impact);
		G_SpawnParticle(impact, tr->contentFlags >> 8, "burning");
	}
}

/**
 * @brief Spawn an item on the floor. A new ET_ITEM edict is created if needed.
 * @param[in] pos The grid position to spawn the item at
 * @param[in] item The item to spawn
 */
static void G_SpawnItemOnFloor (const pos3_t pos, const Item* item)
{
	Edict* floor = G_GetFloorItemFromPos(pos);
	if (floor == nullptr) {
		floor = G_SpawnFloor(pos);

		if (!game.invi.tryAddToInventory(&floor->chr.inv, item, INVDEF(CID_FLOOR))) {
			G_FreeEdict(floor);
		} else {
			Actor* actor = G_EdictsGetLivingActorFromPos(pos);

			/* send the inventory */
			G_CheckVis(floor);

			if (actor != nullptr)
				G_GetFloorItems(actor);
		}
	} else {
		if (game.invi.tryAddToInventory(&floor->chr.inv, item, INVDEF(CID_FLOOR))) {
			/* make it invisible to send the inventory in the below vis check */
			G_EventPerish(*floor);
			G_VisFlagsReset(*floor);
			G_CheckVis(floor, true);
		}
	}
}

/**
 * @brief Calculate the effective spread for the given actor and firemode.
 * @param[in] shooter The shooting actor
 * @param[in] fd The firedefinition the actor is shooting with
 * @param[out] effSpread The adjusted spread
 */
void G_CalcEffectiveSpread (const Actor* shooter, const fireDef_t* fd, vec2_t effSpread)
{
	/* Get accuracy value for this attacker. */
	const float acc = GET_ACC(shooter->chr.score.skills[ABILITY_ACCURACY], fd->weaponSkill ? shooter->chr.score.skills[fd->weaponSkill] : 0);

	/* Base spread multiplier comes from the firedef's spread values. Soldier skills further modify the spread.
	 * A good soldier will tighten the spread, a bad one will widen it, for skillBalanceMinimum values between 0 and 1.*/
	const float commonfactor = (WEAPON_BALANCE + SKILL_BALANCE * acc) * G_ActorGetInjuryPenalty(shooter, MODIFIER_ACCURACY);
	effSpread[PITCH] = fd->spread[0] * commonfactor;
	effSpread[YAW] = fd->spread[1] * commonfactor;

	/* If the attacker is crouched this modifier is included as well. */
	if (shooter->isCrouched() && fd->crouch > 0.0f) {
		effSpread[PITCH] *= fd->crouch;
		effSpread[YAW] *= fd->crouch;
	}
}

#define GRENADE_DT			0.1f
#define GRENADE_STOPSPEED	60.0f
/**
 * @brief A parabola-type shoot (grenade, throw).
 * @sa G_ShootSingle
 * @sa Com_GrenadeTarget
 * @param[in] player The shooting player
 * @param[in] shooter The shooting actor
 * @param[in] fd The firedefinition the actor is shooting with
 * @param[in] from The position the actor is shooting from
 * @param[in] at The grid position the actor is shooting to (or trying to shoot to)
 * @param[in] mask The team visibility mask (who's seeing the event)
 * @param[in] weapon The weapon to shoot with
 * @param[in] mock pseudo shooting - only for calculating mock values - nullptr for real shots
 * @param[in] z_align This value may change the target z height
 * @param[out] impact The location of the target (-center?)
 */
static void G_ShootGrenade (const Player& player, Actor* shooter, const fireDef_t* fd,
	const vec3_t from, const pos3_t at, int mask, const Item* weapon, shot_mock_t* mock, int z_align, vec3_t impact)
{
	/* Check if the shooter is still alive (me may fire with area-damage ammo and have just hit the near ground). */
	if (shooter->isDead())
		return;

	/* get positional data */
	vec3_t last;
	VectorCopy(from, last);
	vec3_t target;
	gi.GridPosToVec(shooter->fieldSize, at, target);
	/* first apply z_align value */
	target[2] -= z_align;

	/* prefer to aim grenades at the ground */
	target[2] -= GROUND_DELTA;

	/* calculate parabola */
	vec3_t startV;
	float dt = gi.GrenadeTarget(last, target, fd->range, fd->launched, fd->rolled, startV);
	if (!dt) {
		if (!mock)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - impossible throw!"));
		return;
	}

	/* cap start speed */
	const float speed = std::min(VectorLength(startV), fd->range);

	/* add random effects and get new dir */
	vec3_t angles;
	VecToAngles(startV, angles);

	/* Get 2 gaussian distributed random values */
	float gauss1;
	float gauss2;
	gaussrand(&gauss1, &gauss2);

	vec2_t effSpread;
	G_CalcEffectiveSpread(shooter, fd, effSpread);

	angles[PITCH] += gauss1 * effSpread[0];
	angles[YAW] += gauss2 * effSpread[1];

	AngleVectors(angles, startV, nullptr, nullptr);
	VectorScale(startV, speed, startV);

	/* move */
	vec3_t oldPos;
	VectorCopy(last, oldPos);
	vec3_t curV;
	VectorCopy(startV, curV);
	float time = 0.0f;
	dt = 0.0f;
	int bounce = 0;
	byte flags = SF_BOUNCING;
	vec3_t newPos;

	VectorMA(oldPos, GRENADE_DT, curV, newPos);
	newPos[2] -= 0.5f * GRAVITY * GRENADE_DT * GRENADE_DT;
	trace_t tr = G_Trace(Line(oldPos, newPos), shooter, MASK_SHOT);
	if (tr.fraction < 1.0f) {
		const Edict* trEnt = G_EdictsGetByNum(tr.entNum);
		if (trEnt && (trEnt->isSameTeamAs(shooter) || G_IsCivilian(trEnt)) && G_IsCrouched(trEnt)) {
			dt += GRENADE_DT;
			VectorCopy(newPos, oldPos);
			VectorCopy(newPos, impact);
		}
	}

	for (;;) {
		/* kinematics */
		VectorMA(oldPos, GRENADE_DT, curV, newPos);
		newPos[2] -= 0.5f * GRAVITY * GRENADE_DT * GRENADE_DT;
		curV[2] -= GRAVITY * GRENADE_DT;

		/* trace */
		tr = G_Trace(Line(oldPos, newPos), shooter, MASK_SHOT);
		if (tr.fraction < 1.0f || time + dt > 4.0f) {
			/* the ent possibly hit by the trace */
			const Edict* trEnt = G_EdictsGetByNum(tr.entNum);
			const float bounceFraction = tr.surface ? gi.GetBounceFraction(tr.surface->name) : 1.0f;

			/* advance time */
			dt += tr.fraction * GRENADE_DT;
			time += dt;
			bounce++;

			if (tr.fraction < 1.0f)
				VectorCopy(tr.endpos, newPos);

			/* calculate additional visibility */
			if (!mock) {
				for (int i = 0; i < MAX_TEAMS; i++)
					if (player.getTeam() != level.activeTeam && G_TeamPointVis(i, newPos))
						mask |= 1 << i;
			}

			/* enough bouncing around or we have hit an actor */
			if (VectorLength(curV) < GRENADE_STOPSPEED || time > 4.0f || bounce > fd->bounce
			 || (!fd->delay && trEnt && G_IsActor(trEnt))) {
				if (!mock) {
					/* explode */
					byte impactFlags = flags;
					if (trEnt && G_IsActor(trEnt))
						impactFlags |= SF_BODY;
					else
						impactFlags |= SF_IMPACT;
					G_EventThrow(mask, fd, dt, impactFlags, last, startV);
				}

				/* Grenade flew out of map! */
				if (tr.fraction > 1.0f)
					return;

				tr.endpos[2] += 10.0f;
				VectorCopy(tr.endpos, impact);

				/* check if this is a stone, ammo clip or grenade */
				if (fd->splrad > 0.0f) {
					G_SplashDamage(shooter, fd, impact, mock, &tr);
				} else if (!mock) {
					/* spawn the stone on the floor */
					if (fd->ammo && !fd->splrad && weapon->def()->thrown) {
						pos3_t drop;
						VecToPos(impact, drop);
						G_SpawnItemOnFloor(drop, weapon);
					}
				}
				return;
			}

			/* send */
			if (!mock)
				G_EventThrow(mask, fd, dt, flags, last, startV);

			flags |= SF_BOUNCED;

			/* bounce */
			VectorScale(curV, fd->bounceFac * bounceFraction, curV);
			vec3_t temp;
			VectorScale(tr.plane.normal, -DotProduct(tr.plane.normal, curV), temp);
			VectorAdd(temp, curV, startV);
			VectorAdd(temp, startV, curV);

			/* prepare next move */
			VectorCopy(tr.endpos, last);
			VectorCopy(tr.endpos, oldPos);
			VectorCopy(curV, startV);
			dt = 0.0f;
		} else {
			dt += GRENADE_DT;
			VectorCopy(newPos, oldPos);
			VectorCopy(newPos, impact);
		}
	}
}

#ifdef DEBUG
/**
 * @brief Displays the results of a trace. Used to see if a bullet hit something.
 * @param[in] start The starting location of the trace.
 * @param[in] tr The trace data.
 */
static void DumpTrace (vec3_t start, const trace_t& tr)
{
	Com_DPrintf(DEBUG_GAME, "start (%i, %i, %i) end (%i, %i, %i)\n",
		(int)start[0], (int)start[1], (int)start[2],
		(int)tr.endpos[0], (int)tr.endpos[1], (int)tr.endpos[2]);
	Com_DPrintf(DEBUG_GAME, "allsolid:%s startsolid:%s fraction:%f contentFlags:%X\n",
		tr.allsolid ? "true" : "false",
		tr.startsolid ? "true" : "false",
		tr.fraction, tr.contentFlags);
	Edict* trEnt = G_EdictsGetByNum(tr.entNum);	/* the ent possibly hit by the trace */
	Com_DPrintf(DEBUG_GAME, "is entity:%s %s %i\n",
		trEnt ? "yes" : "no",
		trEnt ? trEnt->classname : "",
		trEnt ? trEnt->HP : 0);
}

/**
 * @brief Displays data about all server entities.
 */
static void DumpAllEntities (void)
{
	int i = 0;
	Edict* check = nullptr;

	while ((check = G_EdictsGetNext(check))) {
		char absBoxStr[AABB_STRING];
		char entBoxStr[AABB_STRING];
		check->absBox.asIntString(absBoxStr, sizeof(absBoxStr));
		check->absBox.asIntString(entBoxStr, sizeof(entBoxStr));
		Com_DPrintf(DEBUG_GAME, "%i %s %s %s %s %s\n", i,
			check->inuse ? "in use" : "unused",
			check->classname,
			check->model,
			absBoxStr,
			entBoxStr);
		i++;
	}
}
#endif

/**
 * @brief Fires straight shots.
 * @param[in] ent The attacker.
 * @param[in] fd The fire definition that is used for the shot.
 * @param[in] from Location of the gun muzzle.
 * @param[in] at Grid coordinate of the target.
 * @param[in] mask Visibility bit-mask (who's seeing the event)
 * @param[in] weapon The weapon the actor is shooting with
 * @param[in] mock pseudo shooting - only for calculating mock values - nullptr for real shots
 * @param[in] z_align This value may change the target z height
 * @param[in] i The ith shot
 * @param[in] shootType The firemode (ST_NUM_SHOOT_TYPES)
 * @param[out] impact The location of the target (-center?)
 * @sa CL_TargetingStraight
 */
static void G_ShootSingle (Actor* ent, const fireDef_t* fd, const vec3_t from, const pos3_t at,
	int mask, const Item* weapon, shot_mock_t* mock, int z_align, int i, shoot_types_t shootType, vec3_t impact)
{
	/* Check if the shooter is still alive (me may fire with area-damage ammo and have just hit the near ground). */
	if (ent->isDead()) {
		Com_DPrintf(DEBUG_GAME, "G_ShootSingle: Shooter is dead, shot not possible.\n");
		return;
	}
	/* Calc direction of the shot. */
	gi.GridPosToVec(ent->fieldSize, at, impact);	/* Get the position of the targeted grid-cell. ('impact' is used only temporary here)*/
	const bool pointTrace = VectorCompare(impact, from);
	if (!pointTrace)
		impact[2] -= z_align;
	vec3_t cur_loc;
	VectorCopy(from, cur_loc);		/* Set current location of the projectile to the starting (muzzle) location. */
	vec3_t dir;
	VectorSubtract(impact, cur_loc, dir);	/* Calculate the vector from current location to the target. */

	if (!pointTrace) {
		VectorNormalizeFast(dir);			/* Normalize the vector i.e. make length 1.0 */

		vec3_t angles;
		VecToAngles(dir, angles);		/* Get the angles of the direction vector. */

		/* Get 2 gaussian distributed random values */
		float gauss1;
		float gauss2;
		gaussrand(&gauss1, &gauss2);

		vec2_t effSpread;
		G_CalcEffectiveSpread(ent, fd, effSpread);

		/* Modify the angles with the accuracy modifier as a randomizer-range. */
		angles[PITCH] += gauss1 * effSpread[0];
		angles[YAW] += gauss2 * effSpread[1];

		/* Convert changed angles into new direction. */
		AngleVectors(angles, dir, nullptr, nullptr);
	}

	/* shoot and bounce */
	int throughWall = fd->throughWall;
	float range = fd->range;
	int bounce = 0;
	byte flags = 0;

	int damage;
	/* Are we healing? */
	if (FIRESH_IsMedikit(fd))
		damage = fd->damage[0] + (fd->damage[1] * crand());
	else
		damage = std::max(0.0f, fd->damage[0] + (fd->damage[1] * crand()));

	/* Check if we are shooting over an adjacent crouching friendly unit */
	VectorMA(cur_loc, UNIT_SIZE * 1.4f, dir, impact);
	const Edict* passEnt = ent;
	trace_t tr = G_Trace(Line(cur_loc, impact), passEnt, MASK_SHOT);
	Edict* trEnt = G_EdictsGetByNum(tr.entNum);	/* the ent possibly hit by the trace */
	if (trEnt && (trEnt->isSameTeamAs(ent) || G_IsCivilian(trEnt)) && G_IsCrouched(trEnt) && !FIRESH_IsMedikit(fd)) {
		VectorMA(cur_loc, UNIT_SIZE * 1.4f, dir, cur_loc);
		passEnt = trEnt;
	}

	vec3_t tracefrom;	/* sum */
	VectorCopy(cur_loc, tracefrom);

	vec3_t temp;
	for (;;) {
		/* Calc 'impact' vector that is located at the end of the range
		 * defined by the fireDef_t. This is not really the impact location,
		 * but rather the 'endofrange' location, see below for another use.*/
		VectorMA(cur_loc, range, dir, impact);

		/* Do the trace from current position of the projectile
		 * to the end_of_range location.*/
		tr = G_Trace(Line(tracefrom, impact), passEnt, MASK_SHOT);
		trEnt = G_EdictsGetByNum(tr.entNum);	/* the ent possibly hit by the trace */

#ifdef DEBUG
		DumpAllEntities();
		DumpTrace(tracefrom, tr);
#endif

		/* maybe we start the trace from within a brush (e.g. in case of throughWall) */
		if (tr.startsolid)
			break;

		/* _Now_ we copy the correct impact location. */
		VectorCopy(tr.endpos, impact);

		/* set flags when trace hit something */
		if (tr.fraction < 1.0f) {
			if (trEnt && G_IsActor(trEnt)
				/* check if we differentiate between body and wall */
				&& !fd->delay)
				flags |= SF_BODY;
			else if (bounce < fd->bounce)
				flags |= SF_BOUNCING;
			else
				flags |= SF_IMPACT;
		}

		/* victims see shots */
		if (trEnt && G_IsActor(trEnt))
			mask |= G_TeamToVisMask(trEnt->getTeam());

		if (!mock) {
			/* send shot */
			const bool firstShot = (i == 0);
			G_EventShoot(*ent, mask, fd, firstShot, shootType, flags, &tr, tracefrom, impact);

			/* send shot sound to the others */
			G_EventShootHidden(mask, fd, false);

			if (firstShot && G_FireAffectedSurface(tr.surface, fd)) {
				vec3_t origin;
				/* sent particle to all players */
				VectorMA(impact, 1, tr.plane.normal, origin);
				G_SpawnParticle(origin, tr.contentFlags >> 8, "fire");
			}
		}

		if (tr.fraction < 1.0f && !fd->bounce) {
			/* check for shooting through wall */
			if (throughWall && (tr.contentFlags & CONTENTS_SOLID)) {
				throughWall--;
				Com_DPrintf(DEBUG_GAME, "Shot through wall, %i walls left.\n", throughWall);
				/* reduce damage */
				/** @todo reduce even more if the wall was hit far away and
				 * not close by the shooting actor */
				damage /= sqrt(fd->throughWall - throughWall + 1);
				VectorMA(tr.endpos, MAX_WALL_THICKNESS_FOR_SHOOTING_THROUGH, dir, tracefrom);
				continue;
			}

			/* do splash damage */
			if (fd->splrad > 0.0f) {
				VectorMA(impact, sv_shot_origin->value, tr.plane.normal, impact);
				G_SplashDamage(ent, fd, impact, mock, &tr);
			}
		}

		const bool hitEnt = trEnt && (G_IsActor(trEnt) || (G_IsBreakable(trEnt) && damage > 0));
		/* bounce is checked here to see if the rubber rocket hit walls enough times to wear out*/
		bounce++;
		/* stop, we hit something or have bounced enough */
		if (hitEnt || bounce > fd->bounce || tr.fraction >= 1.0f) {
			if (!mock) {
				/* spawn the throwable item on the floor but only if it is not depletable */
				if (fd->ammo && !fd->splrad && weapon->def()->thrown && !weapon->def()->deplete) {
					pos3_t drop;

					if (ent->isSamePosAs(at)) { /* throw under his own feet */
						VectorCopy(at, drop);
					} else {
						impact[2] -= 20.0f; /* a hack: no-gravity items are flying high */
						VecToPos(impact, drop);
					}

					G_SpawnItemOnFloor(drop, weapon);
				}
			}

			/* do damage if the trace hit an entity */
			if (hitEnt) {
				VectorCopy(tr.endpos, impact);
				G_Damage(trEnt, fd, damage, ent, mock, impact);

				if (!mock) { /* check for firedHit is done in G_UpdateHitScore */
					/* Count this as a hit of this firemode. */
					G_UpdateHitScore(ent, trEnt, fd, 0);
				}
			}

			/* do splash damage if we haven't yet */
			if (tr.fraction >= 1.0f && fd->splrad > 0.0f) {
				VectorMA(impact, sv_shot_origin->value, tr.plane.normal, impact);
				G_SplashDamage(ent, fd, impact, mock, &tr);
			}
			break;
		}

		range -= tr.fraction * range;
		VectorCopy(impact, cur_loc);
		VectorScale(tr.plane.normal, -DotProduct(tr.plane.normal, dir), temp);
		VectorAdd(temp, dir, dir);
		VectorAdd(temp, dir, dir);
		flags |= SF_BOUNCED;
	}
}

/**
 * @brief Prepares weapon, firemode and container used for shoot.
 * @param[in] ent Pointer to attacker.
 * @param[in] shootType Type of shot.
 * @param[in] firemode An index of used firemode.
 * @param[in,out] weapon Weapon being used. It is nullptr when calling this function.
 * @param[in,out] container Container with weapon being used. It is 0 when calling this function.
 * @param[in,out] fd Firemode being used. It is nullptr when calling this function.
 * @return true if function is able to check and set everything correctly.
 * @sa G_ClientShoot
 */
static bool G_PrepareShot (Edict* ent, shoot_types_t shootType, fireDefIndex_t firemode, Item** weapon, containerIndex_t* container, const fireDef_t** fd)
{
	if (shootType >= ST_NUM_SHOOT_TYPES)
		gi.Error("G_GetShotFromType: unknown shoot type %i.\n", shootType);

	Item* item;
	if (IS_SHOT_HEADGEAR(shootType)) {
		item = ent->chr.inv.getHeadgear();
		if (!item)
			return false;
		*container = CID_HEADGEAR;
	} else if (IS_SHOT_RIGHT(shootType)) {
		item = ent->getRightHandItem();
		if (!item)
			return false;
		*container = CID_RIGHT;
	} else {
		item = ent->getLeftHandItem();
		if (!item)
			return false;
		*container = CID_LEFT;
	}

	/* Get firedef from the weapon entry instead */
	const fireDef_t* fdArray = item->getFiredefs();
	if (fdArray == nullptr)
		return false;

	*weapon = item;

	assert(firemode >= 0);
	*fd = &fdArray[firemode];

	return true;
}

/**
 * @brief Setup for shooting, either real or mock
 * @param[in] player The player this action belongs to (i.e. either the ai or the player)
 * @param[in] actor the edict that is doing the shot
 * @param[in] at Position to fire on.
 * @param[in] shootType What type of shot this is (left, right reaction-left etc...).
 * @param[in] firemode The firemode index of the ammo for the used weapon.
 * @param[in] mock pseudo shooting - only for calculating mock values - nullptr for real shots
 * @param[in] allowReaction Set to true to check whether this has forced any reaction fire, otherwise false.
 * @return true if everything went ok (i.e. the shot(s) where fired ok), otherwise false.
 * @param[in] z_align This value may change the target z height - often @c GROUND_DELTA is used to calculate
 * the alignment. This can be used for splash damage weapons. It's often useful to target the ground close to the
 * victim. That way you don't need a 100 percent chance to hit your target. Even if you don't hit it, the splash
 * damage might reduce the health of your target.
 */
bool G_ClientShoot (const Player& player, Actor* actor, const pos3_t at, shoot_types_t shootType,
		fireDefIndex_t firemode, shot_mock_t* mock, bool allowReaction, int z_align)
{
	/* just in 'test-whether-it's-possible'-mode or the player is an
	 * ai - no readable feedback needed */
	const bool quiet = (mock != nullptr) || G_IsAIPlayer(&player);

	Item* weapon = nullptr;
	const fireDef_t* fd = nullptr;
	containerIndex_t container = 0;
	if (!G_PrepareShot(actor, shootType, firemode, &weapon, &container, &fd)) {
		if (!weapon && !quiet)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - object not activatable!"));
		return false;
	}


	/* check if action is possible
	 * if allowReaction is false, it is a shot from reaction fire, so don't check the active team */
	const int tusNeeded = G_ActorGetModifiedTimeForFiredef(actor, fd, false);
	if (allowReaction) {
		if (!G_ActionCheckForCurrentTeam(player, actor, tusNeeded))
			return false;
	} else {
		if (!G_ActionCheckForReaction(player, actor, tusNeeded))
			return false;
	}

	/* Don't allow to shoot yourself */
	if (!fd->irgoggles && actor->isSamePosAs(at))
		return false;
	const Actor* targetEnt = nullptr;
	if (FIRESH_IsMedikit(fd)) {
		targetEnt = G_EdictsGetLivingActorFromPos(at);
		if (!targetEnt)
			return false;
		else if (fd->dmgweight == gi.csi->damNormal && !G_IsActorWounded(targetEnt)) {
			if (!quiet)
				G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - target is not wounded!"));
			return false;
		}
	}

	/* check that we're not firing a twohanded weapon with one hand! */
	if (weapon->def()->fireTwoHanded && actor->getLeftHandItem()) {
		if (!quiet)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - weapon cannot be fired one handed!"));
		return false;
	}

	/* check we're not out of ammo */
	int ammo = weapon->getAmmoLeft();
	if (!ammo && fd->ammo && !weapon->def()->thrown) {
		if (!quiet)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - no ammo!"));
		return false;
	}

	/* check target is not out of range */
	vec3_t target;
	gi.GridPosToVec(actor->fieldSize, at, target);
	if (fd->range < VectorDist(actor->origin, target)) {
		if (!quiet)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - target out of range!"));
		return false;
	}

	/* Count for stats if it's no mock-shot and it's a Phalanx soldier (aliens do not have this info yet). */
	if (!mock && actor->chr.scoreMission) {
		/* Count this start of the shooting for stats/score. */
		/** @todo check for direct shot / splash damage shot? */
		if (fd->splrad > 0.0) {
			/* Splash damage */
			actor->chr.scoreMission->firedSplashTUs[fd->weaponSkill] += tusNeeded;
			actor->chr.scoreMission->firedSplash[fd->weaponSkill]++;
			for (int i = 0; i < KILLED_NUM_TYPES; i++) {
				/** Reset status. @see G_UpdateHitScore for the check. */
				actor->chr.scoreMission->firedSplashHit[i] = false;
			}
		} else {
			/* Direct hits */
			actor->chr.scoreMission->firedTUs[fd->weaponSkill] += tusNeeded;
			actor->chr.scoreMission->fired[fd->weaponSkill]++;
			for (int i = 0; i < KILLED_NUM_TYPES; i++) {
				/** Reset status. @see G_UpdateHitScore for the check. */
				actor->chr.scoreMission->firedHit[i] = false;
			}
		}
	}

	/* fire shots */
	int shots = fd->shots;
	if (fd->ammo && !weapon->def()->thrown) {
		/**
		 * If loaded ammo is less than needed ammo from firedef
		 * then reduce shot-number relative to the difference.
		 * @todo This really needs an overhaul.And it might get dumped completely when
		 * Feature Request "[1814158] Extended shot-definitions in firedef"
		 * https://sourceforge.net/tracker/?func=detail&atid=805245&aid=1814158&group_id=157793
		 * gets implemented.
		 */
		if (ammo < fd->ammo) {
			shots = fd->shots * ammo / fd->ammo;
			ammo = 0;
		} else {
			ammo -= fd->ammo;
		}
		if (shots < 1) {
			if (!quiet)
				G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - not enough ammo!"));
			return false;
		}
	}

	/* rotate the player */
	const int prevDir = mock ? actor->dir : 0;
	vec3_t dir;

	if (!actor->isSamePosAs(at)) {
		VectorSubtract(at, actor->pos, dir);
		actor->dir = AngleToDir((int) (atan2(dir[1], dir[0]) * todeg));
		assert(actor->dir < CORE_DIRECTIONS);

		if (!mock) {
			G_CheckVisTeamAll(actor->getTeam(), 0, actor);
			G_EventActorTurn(*actor);
		}
	}

	/* calculate visibility */
	target[2] -= z_align;
	VectorSubtract(target, actor->origin, dir);
	vec3_t center;
	VectorMA(actor->origin, 0.5, dir, center);
	int mask = 0;
	for (int i = 0; i < MAX_TEAMS; i++)
		if (G_IsVisibleForTeam(actor, i) || G_TeamPointVis(i, target) || G_TeamPointVis(i, center))
			mask |= G_TeamToVisMask(i);

	if (!mock) {
		/* check whether this has forced any reaction fire */
		if (allowReaction) {
			G_ReactionFirePreShot(actor, tusNeeded);  /* if commented out,  this disables the 'draw' situation */
			if (actor->isDead())
				/* dead men can't shoot */
				return false;
		}
		/* Check we aren't trying to heal a dead actor */
		if (targetEnt != nullptr && (targetEnt->isDead() && !targetEnt->isStunned()))
			return false;

		/* start shoot */
		G_EventStartShoot(*actor, mask, shootType, at);

		/* send shot sound to the others */
		G_EventShootHidden(mask, fd, true);

		/* State info so we can check if an item was already removed. */
		bool itemAlreadyRemoved = false;
		/* ammo... */
		if (fd->ammo) {
			if (ammo > 0 || !weapon->def()->thrown) {
				G_EventInventoryAmmo(*actor, weapon->ammoDef(), ammo, shootType);
				weapon->setAmmoLeft(ammo);
			} else { /* delete the knife or the rifle without ammo */
				const invDef_t* invDef = INVDEF(container);
				assert(invDef->single);
				itemAlreadyRemoved = true;	/* for assert only */
				game.invi.emptyContainer(&actor->chr.inv, invDef->id);
				G_ReactionFireSettingsUpdate(actor, actor->chr.RFmode.getFmIdx(), actor->chr.RFmode.getHand(),
						actor->chr.RFmode.getWeapon());
				G_EventInventoryDelete(*actor, G_VisToPM(actor->visflags), invDef->id, 0, 0);
			}
		}

		/* remove throwable oneshot && deplete weapon from inventory */
		if (weapon->def()->thrown && weapon->def()->oneshot && weapon->def()->deplete) {
			if (itemAlreadyRemoved)
				gi.Error("Item %s is already removed", weapon->def()->id);
			const invDef_t* invDef = INVDEF(container);
			assert(invDef->single);
			game.invi.emptyContainer(&actor->chr.inv, invDef->id);
			G_ReactionFireSettingsUpdate(actor, actor->chr.RFmode.getFmIdx(), actor->chr.RFmode.getHand(),
					actor->chr.RFmode.getWeapon());
			G_EventInventoryDelete(*actor, G_VisToPM(actor->visflags), invDef->id, 0, 0);
		}
	}

	vec3_t shotOrigin;
	fd->getShotOrigin(actor->origin, dir, actor->isCrouched(), shotOrigin);

	/* Fire all shots. */
	vec3_t impact;
	for (int i = 0; i < shots; i++)
		if (fd->gravity) {
			G_ShootGrenade(player, actor, fd, shotOrigin, at, mask, weapon, mock, z_align, impact);
		} else {
			G_ShootSingle(actor, fd, shotOrigin, at, mask, weapon, mock, z_align, i, shootType, impact);
			if (mor_panic->integer)
				G_ShotMorale(actor, fd, shotOrigin, weapon, impact);
		}

	if (!mock) {
		const bool smoke = fd->obj->dmgtype == gi.csi->damSmoke;
		const bool incendiary = fd->obj->dmgtype == gi.csi->damIncendiary;
		const bool stunGas = fd->obj->dmgtype == gi.csi->damStunGas;
		/* Ignore off-map impacts when spawning fire, smoke, etc fields */
		if (gi.isOnMap(impact)) {
			if (smoke) {
				const int damage = std::max(0.0f, fd->damage[0] + fd->damage[1] * crand());
				const int rounds = std::max(2, fd->rounds);
				G_SpawnSmokeField(impact, "smokefield", rounds, damage, fd->splrad);
			} else if (incendiary) {
				const int damage = std::max(0.0f, fd->damage[0] + fd->damage[1] * crand());
				const int rounds = std::max(2, fd->rounds);
				G_SpawnFireField(impact, "firefield", rounds, damage, fd->splrad);
			} else if (stunGas) {
				const int damage = std::max(0.0f, fd->damage[0] + fd->damage[1] * crand());
				const int rounds = std::max(2, fd->rounds);
				G_SpawnStunSmokeField(impact, "green_smoke", rounds, damage, fd->splrad);
			}
		}

		/* check whether this caused a touch event for close actors */
		if (smoke || incendiary || stunGas) {
			const entity_type_t type = smoke ? ET_SMOKE : incendiary ? ET_FIRE : ET_SMOKESTUN;
			Edict* closeActor = nullptr;
			while ((closeActor = G_FindRadius(closeActor, impact, fd->splrad, ET_ACTOR))) {
				G_TouchTriggers(closeActor, type);
			}
		}

		/* send TUs if actor still alive */
		if (actor->inuse && !actor->isDead()) {
			G_ActorSetTU(actor, actor->getTus() - tusNeeded);
			G_SendStats(*actor);
		}

		G_EventEndShoot(*actor, mask);

		/* end events */
		G_EventEnd();

		/* check for win/draw conditions */
		G_MatchEndCheck();

		/* check for Reaction fire against the shooter */
		if (allowReaction)
			G_ReactionFirePostShot(actor);
	} else {
		actor->dir = prevDir;
		assert(actor->dir < CORE_DIRECTIONS);
	}
	return true;
}
