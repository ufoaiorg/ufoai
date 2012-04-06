/**
 * @file g_combat.c
 * @brief All parts of the main game logic that are combat related
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

#define MAX_WALL_THICKNESS_FOR_SHOOTING_THROUGH 8

typedef enum {
	ML_WOUND,
	ML_DEATH
} morale_modifiers;

/**
 * @brief Test if point is "visible" from team.
 * @param[in] team A team to test.
 * @param[in] point A point to check.
 * @return qtrue if point is "visible"
 */
static qboolean G_TeamPointVis (int team, const vec3_t point)
{
	edict_t *from = NULL;
	vec3_t eye;

	/* test if point is visible from team */
	while ((from = G_EdictsGetNextLivingActorOfTeam(from, team))) {
		if (G_FrustumVis(from, point)) {
			/* get viewers eye height */
			VectorCopy(from->origin, eye);
			if (G_IsCrouched(from))
				eye[2] += EYE_CROUCH;
			else
				eye[2] += EYE_STAND;

			/* line of sight */
			if (!G_TestLine(eye, point))
				return qtrue;
		}
	}

	/* not visible */
	return qfalse;
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
static void G_Morale (int type, const edict_t * victim, const edict_t * attacker, int param)
{
	edict_t *ent = NULL;
	int newMorale;
	float mod;

	while ((ent = G_EdictsGetNextInUse(ent))) {
		/* this only applies to ET_ACTOR but not ET_ACTOR2x2 */
		if (ent->type == ET_ACTOR && !G_IsDead(ent) && ent->team != TEAM_CIVILIAN) {
			switch (type) {
			case ML_WOUND:
			case ML_DEATH:
				/* morale damage depends on the damage */
				mod = mob_wound->value * param;
				/* death hurts morale even more than just damage */
				if (type == ML_DEATH)
					mod += mob_death->value;
				/* seeing how someone gets shot increases the morale change */
				if (ent == victim || (G_FrustumVis(ent, victim->origin) && G_ActorVis(ent->origin, ent, victim, qfalse)))
					mod *= mof_watching->value;
				if (attacker != NULL && ent->team == attacker->team) {
					/* teamkills are considered to be bad form, but won't cause an increased morale boost for the enemy */
					/* morale boost isn't equal to morale loss (it's lower, but morale gets regenerated) */
					if (victim->team == attacker->team)
						mod *= mof_teamkill->value;
					else
						mod *= mof_enemy->value;
				}
				/* seeing a civilian die is more "acceptable" */
				if (G_IsCivilian(victim))
					mod *= mof_civilian->value;
				/* if an ally (or in singleplayermode, as human, a civilian) got shot, lower the morale, don't heighten it. */
				if (victim->team == ent->team || (G_IsCivilian(victim) && ent->team != TEAM_ALIEN && sv_maxclients->integer == 1))
					mod *= -1;
				if (attacker != NULL) {
					/* if you stand near to the attacker or the victim, the morale change is higher. */
					mod *= mor_default->value + pow(0.5, VectorDist(ent->origin, victim->origin) / mor_distance->value)
						* mor_victim->value + pow(0.5, VectorDist(ent->origin, attacker->origin) / mor_distance->value)
						* mor_attacker->value;
				} else {
					mod *= mor_default->value + pow(0.5, VectorDist(ent->origin, victim->origin) / mor_distance->value)
						* mor_victim->value;
				}
				/* morale damage depends on the number of living allies */
				mod *= (1 - mon_teamfactor->value)
					+ mon_teamfactor->value * (level.num_spawned[victim->team] + 1)
					/ (level.num_alive[victim->team] + 1);
				/* being hit isn't fun */
				if (ent == victim)
					mod *= mor_pain->value;
				break;
			default:
				gi.DPrintf("Undefined morale modifier type %i\n", type);
				mod = 0;
				break;
			}
			/* clamp new morale */
			/*+0.9 to allow weapons like flamethrowers to inflict panic (typecast rounding) */
			newMorale = ent->morale + (int) (MORALE_RANDOM(mod) + 0.9);
			if (newMorale > GET_MORALE(ent->chr.score.skills[ABILITY_MIND]))
				ent->morale = GET_MORALE(ent->chr.score.skills[ABILITY_MIND]);
			else if (newMorale < 0)
				ent->morale = 0;
			else
				ent->morale = newMorale;

			/* send phys data */
			G_SendStats(ent);
		}
	}
}

/**
 * @brief Function to calculate possible damages for mock pseudoaction.
 * @param[in,out] mock Pseudo action - only for calculating mock values - NULL for real action.
 * @param[in] shooter Pointer to attacker for this mock pseudoaction.
 * @param[in] struck Pointer to victim of this mock pseudoaction.
 * @param[in] damage Updates mock value of damage.
 * @note Called only from G_Damage().
 * @sa G_Damage
 */
static void G_UpdateShotMock (shot_mock_t *mock, const edict_t *shooter, const edict_t *struck, int damage)
{
	assert(struck->number != shooter->number || mock->allow_self);

	if (damage > 0) {
		if (!struck->inuse || G_IsDead(struck))
			return;
		else if (!G_IsVisibleForTeam(struck, shooter->team))
			return;
		else if (G_IsCivilian(struck))
			mock->civilian += 1;
		else if (struck->team == shooter->team)
			mock->friendCount += 1;
		else if (G_IsActor(struck))
			mock->enemyCount += 1;
		else
			return;

		mock->damage += damage;
	}
}

/**
 * @brief Update character stats for this mission after successful shoot.
 * @note Mind you that this code is always from the view of PHALANX soldiers right now, not anybody else!
 * @param[in,out] attacker Pointer to attacker.
 * @param[in] fd Pointer to fireDef_t used in shoot.
 * @param[in] target Pointer to target.
 * @sa G_UpdateCharacterSkills
 */
static void G_UpdateCharacterBodycount (edict_t *attacker, const fireDef_t *fd, const edict_t *target)
{
	chrScoreMission_t *scoreMission;
	chrScoreGlobal_t *scoreGlobal;
	killtypes_t type;

	if (!attacker || !target)
		return;

	scoreGlobal = &attacker->chr.score;
	scoreMission = attacker->chr.scoreMission;
	/* only phalanx soldiers have this */
	if (!scoreMission)
		return;

	switch (target->team) {
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

	if (G_IsStunned(target)) {
		scoreMission->stuns[type]++;
		scoreGlobal->stuns[type]++;
	} else if (G_IsDead(target)) {
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
static void G_UpdateHitScore (edict_t * attacker, const edict_t * target, const fireDef_t * fd, const int splashDamage)
{
	chrScoreMission_t *score;
	killtypes_t type;

	if (!attacker || !target || !fd)
		return;

	score = attacker->chr.scoreMission;
	/* Abort if no player team. */
	if (!score)
		return;

	switch (target->team) {
		case TEAM_CIVILIAN:
			type = KILLED_CIVILIANS;
			break;
		case TEAM_ALIEN:
			type = KILLED_ENEMIES;
			break;
		default:
			return;
	}

	if (!splashDamage) {
		if (attacker->team == target->team && !score->firedHit[KILLED_TEAM]) {
			/* Increase friendly fire counter. */
			score->hits[fd->weaponSkill][KILLED_TEAM]++;
			score->firedHit[KILLED_TEAM] = qtrue;
		}

		if (!score->firedHit[type]) {
			score->hits[fd->weaponSkill][type]++;
			score->firedHit[type] = qtrue;
		}
	} else {
		if (attacker->team == target->team) {
			/* Increase friendly fire counter. */
			score->hitsSplashDamage[fd->weaponSkill][KILLED_TEAM] += splashDamage;
			if (!score->firedSplashHit[KILLED_TEAM]) {
				score->hitsSplash[fd->weaponSkill][KILLED_TEAM]++;
				score->firedSplashHit[KILLED_TEAM] = qtrue;
			}
		}

		score->hitsSplashDamage[fd->weaponSkill][type] += splashDamage;
		if (!score->firedSplashHit[type]) {
			score->hitsSplash[fd->weaponSkill][type]++;
			score->firedSplashHit[type] = qtrue;
		}
	}
}

/**
 * @brief Deals damage of a give type and amount to a target.
 * @param[in,out] target What we want to damage.
 * @param[in] fd The fire definition that defines what type of damage is dealt.
 * @param[in] damage The value of the damage.
 * @param[in] attacker The attacker.
 * @param[in] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @sa G_SplashDamage
 * @sa G_TakeDamage
 * @sa G_PrintActorStats
 */
static void G_Damage (edict_t *target, const fireDef_t *fd, int damage, edict_t *attacker, shot_mock_t *mock)
{
	const qboolean stunEl = (fd->obj->dmgtype == gi.csi->damStunElectro);
	const qboolean stunGas = (fd->obj->dmgtype == gi.csi->damStunGas);
	const qboolean shock = (fd->obj->dmgtype == gi.csi->damShock);
	const qboolean smoke = (fd->obj->dmgtype == gi.csi->damSmoke);
	qboolean isRobot;

	assert(target);

	/* Breakables */
	if (G_IsBrushModel(target) && G_IsBreakable(target)) {
		/* Breakables are immune to stun & shock damage. */
		if (stunEl || stunGas || shock || mock || smoke)
			return;

		if (damage >= target->HP) {
			/* don't reset the HP value here, this value is used to distinguish
			 * between triggered destroy and a shoot */
			assert(target->destroy);
			target->destroy(target);

			/* maybe the attacker is seeing something new? */
			G_CheckVisTeamAll(attacker->team, qfalse, attacker);

			/* check if attacker appears/perishes for any other team */
			G_CheckVis(attacker, qtrue);
		} else {
			G_TakeDamage(target, damage);
		}
		return;
	}

	/* Actors don't die again. */
	if (!G_IsLivingActor(target))
		return;

	/* only actors after this point - and they must have a teamdef */
	assert(target->chr.teamDef);
	isRobot = CHRSH_IsTeamDefRobot(target->chr.teamDef);

	/* Apply armour effects. */
	if (damage > 0) {
		const int nd = target->chr.teamDef->resistance[fd->dmgweight];
		if (CONTAINER(target, gi.csi->idArmour)) {
			const objDef_t *ad = CONTAINER(target, gi.csi->idArmour)->item.t;
			damage = max(1, damage - ad->protection[fd->dmgweight] - nd);
		} else {
			damage = max(1, damage - nd);
		}
	} else if (damage < 0) {
		/* Robots can't be healed. */
		if (isRobot)
			return;
	}
	Com_DPrintf(DEBUG_GAME, " Total damage: %d\n", damage);

	/* Apply difficulty settings. */
	if (sv_maxclients->integer == 1) {
		if (G_IsAlien(attacker) && !G_IsAlien(target))
			damage *= pow(1.18, g_difficulty->value);
		else if (!G_IsAlien(attacker) && G_IsAlien(target))
			damage *= pow(1.18, -g_difficulty->value);
	}

	assert(attacker->team >= 0 && attacker->team < MAX_TEAMS);
	assert(target->team >= 0 && target->team < MAX_TEAMS);

	if (g_nodamage != NULL && !g_nodamage->integer) {
		/* hit */
		if (mock) {
			G_UpdateShotMock(mock, attacker, target, damage);
		} else if (stunEl) {
			target->STUN += damage;
		} else if (stunGas) {
			if (!isRobot) /* Can't stun robots with gas */
				target->STUN += damage;
		} else if (shock) {
			/* Only do this if it's not one from our own team ... they should have known that there was a flashbang coming. */
			if (!isRobot && target->team != attacker->team) {
				/** @todo there should be a possible protection, too */
				/* dazed entity wont reaction fire */
				G_RemoveReaction(target);
				G_ActorReserveTUs(target, 0, target->chr.reservedTus.shot, target->chr.reservedTus.crouch);
				/* flashbangs kill TUs */
				G_ActorSetTU(target, 0);
				G_SendStats(target);
				/* entity is dazed */
				G_SetDazed(target);
				G_ClientPrintf(G_PLAYER_FROM_ENT(target), PRINT_HUD, _("Soldier is dazed!\nEnemy used flashbang!\n"));
				return;
			}
		} else {
			G_TakeDamage(target, damage);
			if (damage < 0) {
				/* The 'attacker' is healing the target. */
				/* Update stats here to get info on how many TUs the target received. */
				if (target->chr.scoreMission)
					target->chr.scoreMission->heal += abs(damage);

				/** @todo Also increase the morale a little bit when
				 * soldier gets healing and morale is lower than max possible? */
				if (G_IsStunned(target)) {
					/* reduce STUN */
					target->STUN += damage;
					G_ActorCheckRevitalise(target);
				}
			} else {
				/* Real damage was dealt. */

				/* Update overall splash damage for stats/score. */
				if (!mock && damage > 0 && fd->splrad) /**< Check for >0 and splrad to not count this as direct hit. */
					G_UpdateHitScore(attacker, target, fd, damage);
			}
		}
	}

	if (mock)
		return;

	G_CheckDeathOrKnockout(target, attacker, fd, damage);
}

void G_CheckDeathOrKnockout (edict_t *target, edict_t *attacker, const fireDef_t *fd, int damage)
{
	/* Check death/knockout. */
	if (target->HP == 0 || target->HP <= target->STUN) {
		G_SendStats(target);

		if (G_ActorDieOrStun(target, attacker)) {
			G_PrintActorStats(target, attacker, fd);

			/* apply morale changes */
			if (mor_panic->integer)
				G_Morale(ML_DEATH, target, attacker, damage);

			/* Update number of killed/stunned actors for this attacker. */
			G_UpdateCharacterBodycount(attacker, fd, target);
		}
	} else {
		target->chr.minHP = min(target->chr.minHP, target->HP);
		if (damage > 0) {
			if (mor_panic->integer)
				G_Morale(ML_WOUND, target, attacker, damage);
		} else { /* medikit, etc. */
			const int hp = GET_HP(target->chr.score.skills[ABILITY_POWER]);
			if (target->HP > hp) {
				target->HP = min(max(hp, 0), target->chr.maxHP);
			}
		}
		G_SendStats(target);
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
static inline qboolean G_FireAffectedSurface (const cBspSurface_t *surface, const fireDef_t *fd)
{
	if (!surface)
		return qfalse;

	if (!(surface->surfaceFlags & SURF_BURN))
		return qfalse;

	if (fd->obj->dmgtype == gi.csi->damIncendiary || fd->obj->dmgtype == gi.csi->damFire || fd->obj->dmgtype == gi.csi->damBlast)
		return qtrue;

	return qfalse;
}

/**
 * @brief Deals splash damage to a target and its surroundings.
 * @param[in] ent The shooting actor
 * @param[in] fd The fire definition that defines what type of damage is dealt and how big the splash radius is.
 * @param[in] impact The impact vector where the grenade is exploding
 * @param[in,out] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @param[in] tr The trace where the grenade hits something (or not)
 */
static void G_SplashDamage (edict_t *ent, const fireDef_t *fd, vec3_t impact, shot_mock_t *mock, const trace_t* tr)
{
	edict_t *check = NULL;
	vec3_t center;
	float dist;
	int damage;

	const qboolean shock = (fd->obj->dmgtype == gi.csi->damShock);

	assert(fd->splrad > 0.0);

	while ((check = G_EdictsGetNextInUse(check))) {
		/* If we use a blinding weapon we skip the target if it's looking
		 * away from the impact location. */
		if (shock && !G_FrustumVis(check, impact))
			continue;

		if (G_IsBrushModel(check) && G_IsBreakable(check))
			VectorCenterFromMinsMaxs(check->absmin, check->absmax, center);
		else if (G_IsLivingActor(check) || G_IsBreakable(check))
			VectorCopy(check->origin, center);
		else
			continue;

		/* check for distance */
		dist = VectorDist(impact, center);
		dist = dist > UNIT_SIZE / 2 ? dist - UNIT_SIZE / 2 : 0;
		if (dist > fd->splrad)
			continue;

		if (fd->irgoggles && G_IsActor(check)) {
			/* check whether this actor (check) is in the field of view of the 'shooter' (ent) */
			if (G_FrustumVis(ent, check->origin)) {
				if (!mock) {
					const unsigned int playerMask = G_TeamToPM(ent->team) ^ G_VisToPM(check->visflags);
					G_AppearPerishEvent(playerMask, qtrue, check, ent);
					G_VisFlagsAdd(check, G_PMToVis(playerMask));
				}
				continue;
			}
		}

		/* check for walls */
		if (G_IsLivingActor(check) && !G_ActorVis(impact, ent, check, qfalse))
			continue;

		/* do damage */
		if (shock)
			damage = 0;
		else
			damage = fd->spldmg[0] * (1.0 - dist / fd->splrad);

		if (mock)
			mock->allow_self = qtrue;
		G_Damage(check, fd, damage, ent, mock);
		if (mock)
			mock->allow_self = qfalse;
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
static void G_SpawnItemOnFloor (const pos3_t pos, const item_t *item)
{
	edict_t *floor;

	floor = G_GetFloorItemsFromPos(pos);
	if (floor == NULL) {
		floor = G_SpawnFloor(pos);

		if (!game.i.TryAddToInventory(&game.i, &floor->chr.i, item, INVDEF(gi.csi->idFloor))) {
			G_FreeEdict(floor);
		} else {
			edict_t *actor = G_GetLivingActorFromPos(pos);

			/* send the inventory */
			G_CheckVis(floor, qtrue);

			if (actor != NULL)
				G_GetFloorItems(actor);
		}
	} else {
		if (game.i.TryAddToInventory(&game.i, &floor->chr.i, item, INVDEF(gi.csi->idFloor))) {
			/* make it invisible to send the inventory in the below vis check */
			G_EventPerish(floor);
			G_VisFlagsReset(floor);
			G_CheckVis(floor, qtrue);
		}
	}
}

#define GRENADE_DT			0.1
#define GRENADE_STOPSPEED	60.0
/**
 * @brief A parabola-type shoot (grenade, throw).
 * @sa G_ShootSingle
 * @sa Com_GrenadeTarget
 * @param[in] player The shooting player
 * @param[in] ent The shooting actor
 * @param[in] fd The firedefinition the actor is shooting with
 * @param[in] from The position the actor is shooting from
 * @param[in] at The grid position the actor is shooting to (or trying to shoot to)
 * @param[in] mask The team visibility mask (who's seeing the event)
 * @param[in] weapon The weapon to shoot with
 * @param[in] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @param[in] z_align This value may change the target z height
 * @param[out] impact The location of the target (-center?)
 */
static void G_ShootGrenade (const player_t *player, edict_t *ent, const fireDef_t *fd,
	const vec3_t from, const pos3_t at, int mask, const item_t *weapon, shot_mock_t *mock, int z_align, vec3_t impact)
{
	vec3_t last, target, temp;
	vec3_t startV, curV, oldPos, newPos;
	vec3_t angles;
	float dt, time, speed;
	float acc;
	trace_t tr;
	int bounce;
	byte flags;

	/* Check if the shooter is still alive (me may fire with area-damage ammo and have just hit the near ground). */
	if (G_IsDead(ent))
		return;

	/* get positional data */
	VectorCopy(from, last);
	gi.GridPosToVec(gi.routingMap, ent->fieldSize, at, target);
	/* first apply z_align value */
	target[2] -= z_align;

	/* prefer to aim grenades at the ground */
	target[2] -= GROUND_DELTA;

	/* calculate parabola */
	dt = gi.GrenadeTarget(last, target, fd->range, fd->launched, fd->rolled, startV);
	if (!dt) {
		if (!mock)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - impossible throw!\n"));
		return;
	}

	/* cap start speed */
	speed = VectorLength(startV);
	if (speed > fd->range)
		speed = fd->range;

	/* add random effects and get new dir */
	acc = GET_ACC(ent->chr.score.skills[ABILITY_ACCURACY], fd->weaponSkill ? ent->chr.score.skills[fd->weaponSkill] : 0);

	VecToAngles(startV, angles);
	/** @todo Remove the 2.0f and use gaussian random number instead of crand() */
	angles[PITCH] += crand() * 2.0f * (fd->spread[0] * (WEAPON_BALANCE + SKILL_BALANCE * acc));
	angles[YAW] += crand() * 2.0f * (fd->spread[1] * (WEAPON_BALANCE + SKILL_BALANCE * acc));
	AngleVectors(angles, startV, NULL, NULL);
	VectorScale(startV, speed, startV);

	/* move */
	VectorCopy(last, oldPos);
	VectorCopy(startV, curV);
	time = 0;
	dt = 0;
	bounce = 0;
	flags = SF_BOUNCING;
	for (;;) {
		/* kinematics */
		VectorMA(oldPos, GRENADE_DT, curV, newPos);
		newPos[2] -= 0.5 * GRAVITY * GRENADE_DT * GRENADE_DT;
		curV[2] -= GRAVITY * GRENADE_DT;

		/* trace */
		tr = G_Trace(oldPos, newPos, ent, MASK_SHOT);
		if (tr.fraction < 1.0 || time + dt > 4.0) {
			const float bounceFraction = tr.surface ? gi.GetBounceFraction(tr.surface->name) : 1.0f;
			int i;

			/* advance time */
			dt += tr.fraction * GRENADE_DT;
			time += dt;
			bounce++;

			if (tr.fraction < 1.0)
				VectorCopy(tr.endpos, newPos);

			/* calculate additional visibility */
			if (!mock) {
				for (i = 0; i < MAX_TEAMS; i++)
					if (player->pers.team != level.activeTeam && G_TeamPointVis(i, newPos))
						mask |= 1 << i;
			}

			/* enough bouncing around or we have hit an actor */
			if (VectorLength(curV) < GRENADE_STOPSPEED || time > 4.0 || bounce > fd->bounce
			 || (!fd->delay && tr.ent && G_IsActor(tr.ent))) {
				if (!mock) {
					/* explode */
					byte impactFlags = flags;
					if (tr.ent && G_IsActor(tr.ent))
						impactFlags |= SF_BODY;
					else
						impactFlags |= SF_IMPACT;
					G_EventThrow(mask, fd, dt, impactFlags, last, startV);
				}

				tr.endpos[2] += 10;
				VectorCopy(tr.endpos, impact);

				/* check if this is a stone, ammo clip or grenade */
				if (fd->splrad > 0.0) {
					G_SplashDamage(ent, fd, impact, mock, &tr);
				} else if (!mock) {
					/* spawn the stone on the floor */
					if (fd->ammo && !fd->splrad && weapon->t->thrown) {
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
			VectorScale(tr.plane.normal, -DotProduct(tr.plane.normal, curV), temp);
			VectorAdd(temp, curV, startV);
			VectorAdd(temp, startV, curV);

			/* prepare next move */
			VectorCopy(tr.endpos, last);
			VectorCopy(tr.endpos, oldPos);
			VectorCopy(curV, startV);
			dt = 0;
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
 * @param[in] start The starting loaction of the trace.
 * @param[in] tr The trace data.
 */
static void DumpTrace (vec3_t start, trace_t tr)
{
	Com_DPrintf(DEBUG_GAME, "start (%i, %i, %i) end (%i, %i, %i)\n",
		(int)start[0], (int)start[1], (int)start[2],
		(int)tr.endpos[0], (int)tr.endpos[1], (int)tr.endpos[2]);
	Com_DPrintf(DEBUG_GAME, "allsolid:%s startsolid:%s fraction:%f contentFlags:%X\n",
		tr.allsolid ? "true" : "false",
		tr.startsolid ? "true" : "false",
		tr.fraction, tr.contentFlags);
	Com_DPrintf(DEBUG_GAME, "is entity:%s %s %i\n",
		tr.ent ? "yes" : "no",
		tr.ent ? tr.ent->classname : "",
		tr.ent ? tr.ent->HP : 0);
}

/**
 * @brief Displays data about all server entities.
 */
static void DumpAllEntities (void)
{
	int i = 0;
	edict_t *check = NULL;

	while ((check = G_EdictsGetNext(check))) {
		Com_DPrintf(DEBUG_GAME, "%i %s %s %s (%i, %i, %i) (%i, %i, %i) [%i, %i, %i] [%i, %i, %i]\n", i,
			check->inuse ? "in use" : "unused",
			check->classname,
			check->model,
			(int) check->absmin[0], (int) check->absmin[1], (int) check->absmin[2],
			(int) check->absmax[0], (int) check->absmax[1], (int) check->absmax[2],
			(int) check->mins[0], (int) check->mins[1], (int) check->mins[2],
			(int) check->maxs[0], (int) check->maxs[1], (int) check->maxs[2]);
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
 * @param[in] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @param[in] z_align This value may change the target z height
 * @param[in] i The ith shot
 * @param[in] shootType The firemode (ST_NUM_SHOOT_TYPES)
 * @param[out] impact The location of the target (-center?)
 * @sa CL_TargetingStraight
 */
static void G_ShootSingle (edict_t *ent, const fireDef_t *fd, const vec3_t from, const pos3_t at,
	int mask, const item_t *weapon, shot_mock_t *mock, int z_align, int i, shoot_types_t shootType, vec3_t impact)
{
	vec3_t dir;			/* Direction from the location of the gun muzzle ("from") to the target ("at") */
	vec3_t angles;		/** @todo The random dir-modifier? */
	vec3_t cur_loc;		/* The current location of the projectile. */
	vec3_t temp;
	vec3_t tracefrom;	/* sum */
	trace_t tr;			/* the traceing */
	float acc;			/* Accuracy modifier for the angle of the shot. */
	float range;
	float gauss1;
	float gauss2;		/* For storing 2 gaussian distributed random values. */
	float commonfactor; /* common to pitch and yaw spread, avoid extra multiplications */
	float injurymultiplier;
	int bounce;			/* count the bouncing */
	int damage;			/* The damage to be dealt to the target. */
	byte flags;
	int throughWall;	/* shoot through x walls */

	/* Check if the shooter is still alive (me may fire with area-damage ammo and have just hit the near ground). */
	if (G_IsDead(ent)) {
		Com_DPrintf(DEBUG_GAME, "G_ShootSingle: Shooter is dead, shot not possible.\n");
		return;
	}

	/* Calc direction of the shot. */
	gi.GridPosToVec(gi.routingMap, ent->fieldSize, at, impact);	/* Get the position of the targeted grid-cell. ('impact' is used only temporary here)*/
	impact[2] -= z_align;
	VectorCopy(from, cur_loc);		/* Set current location of the projectile to the starting (muzzle) location. */
	VectorSubtract(impact, cur_loc, dir);	/* Calculate the vector from current location to the target. */
	VectorNormalizeFast(dir);			/* Normalize the vector i.e. make length 1.0 */

	/* places the starting-location a bit away from the attacker-model/grid. */
	/** @todo need some change to reflect 2x2 units.
	 * Also might need a check if the distance is bigger than the one to the impact location. */
	/** @todo can't we use the fd->shotOrg here and get rid of the sv_shot_origin cvar? */
	VectorMA(cur_loc, sv_shot_origin->value, dir, cur_loc);
	VecToAngles(dir, angles);		/* Get the angles of the direction vector. */

	/* Get accuracy value for this attacker. */
	acc = GET_ACC(ent->chr.score.skills[ABILITY_ACCURACY], fd->weaponSkill ? ent->chr.score.skills[fd->weaponSkill] : 0);

	/* Get 2 gaussian distributed random values */
	gaussrand(&gauss1, &gauss2);

	/* Calculate spread multiplier to give worse precision when HPs are not at max */
	injurymultiplier = GET_INJURY_MULT(ent->chr.score.skills[ABILITY_MIND], ent->HP, ent->chr.maxHP == 0 ? 100 : ent->chr.maxHP);
	Com_DPrintf(DEBUG_GAME, "G_ShootSingle: injury spread multiplier = %5.3f (mind %d, HP %d, maxHP %d)\n", injurymultiplier,
		ent->chr.score.skills[ABILITY_MIND], ent->HP, ent->chr.maxHP == 0 ? 100 : ent->chr.maxHP);

	/* Modify the angles with the accuracy modifier as a randomizer-range. If the attacker is crouched this modifier is included as well.  */
	/* Base spread multiplier comes from the firedef's spread values. Soldier skills further modify the spread.
	 * A good soldier will tighten the spread, a bad one will widen it, for skillBalanceMinimum values between 0 and 1.*/
	commonfactor = (WEAPON_BALANCE + SKILL_BALANCE * acc) * injurymultiplier;
	if (G_IsCrouched(ent) && fd->crouch > 0.0) {
		angles[PITCH] += gauss1 * (fd->spread[0] * commonfactor) * fd->crouch;
		angles[YAW] += gauss2 * (fd->spread[1] * commonfactor) * fd->crouch;
	} else {
		angles[PITCH] += gauss1 * (fd->spread[0] * commonfactor);
		angles[YAW] += gauss2 * (fd->spread[1] * commonfactor);
	}
	/* Convert changed angles into new direction. */
	AngleVectors(angles, dir, NULL, NULL);

	/* shoot and bounce */
	throughWall = fd->throughWall;
	range = fd->range;
	bounce = 0;
	flags = 0;

	/* Are we healing? */
	if (FIRESH_IsMedikit(fd))
		damage = fd->damage[0] + (fd->damage[1] * crand());
	else
		damage = max(0, fd->damage[0] + (fd->damage[1] * crand()));

	VectorMA(cur_loc, UNIT_SIZE, dir, impact);
	tr = G_Trace(cur_loc, impact, ent, MASK_SHOT);
	if (tr.ent && (tr.ent->team == ent->team || G_IsCivilian(tr.ent)) && G_IsCrouched(tr.ent) && !FIRESH_IsMedikit(fd))
		VectorMA(cur_loc, UNIT_SIZE * 1.4, dir, cur_loc);

	VectorCopy(cur_loc, tracefrom);

	for (;;) {
		/* Calc 'impact' vector that is located at the end of the range
		 * defined by the fireDef_t. This is not really the impact location,
		 * but rather the 'endofrange' location, see below for another use.*/
		VectorMA(cur_loc, range, dir, impact);

		/* Do the trace from current position of the projectile
		 * to the end_of_range location.*/
		tr = G_Trace(tracefrom, impact, ent, MASK_SHOT);

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
		if (tr.fraction < 1.0) {
			if (tr.ent && G_IsActor(tr.ent)
				/* check if we differentiate between body and wall */
				&& !fd->delay)
				flags |= SF_BODY;
			else if (bounce < fd->bounce)
				flags |= SF_BOUNCING;
			else
				flags |= SF_IMPACT;
		}

		/* victims see shots */
		if (tr.ent && G_IsActor(tr.ent))
			mask |= G_TeamToVisMask(tr.ent->team);

		if (!mock) {
			/* send shot */
			const qboolean firstShot = (i == 0);
			G_EventShoot(ent, mask, fd, firstShot, shootType, flags, &tr, tracefrom, impact);

			/* send shot sound to the others */
			G_EventShootHidden(mask, fd, qfalse);

			if (i == 0 && G_FireAffectedSurface(tr.surface, fd)) {
				vec3_t origin;
				/* sent particle to all players */
				VectorMA(impact, 1, tr.plane.normal, origin);
				G_SpawnParticle(origin, tr.contentFlags >> 8, "fire");
			}
		}

		if (tr.fraction < 1.0 && !fd->bounce) {
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
			if (fd->splrad > 0.0) {
				VectorMA(impact, sv_shot_origin->value, tr.plane.normal, impact);
				G_SplashDamage(ent, fd, impact, mock, &tr);
			}
		}

		/* do damage if the trace hit an entity */
		if (tr.ent && (G_IsActor(tr.ent) || (G_IsBreakable(tr.ent) && damage > 0))) {
			G_Damage(tr.ent, fd, damage, ent, mock);

			if (!mock) { /* check for firedHit is done in G_UpdateHitScore */
				/* Count this as a hit of this firemode. */
				G_UpdateHitScore(ent, tr.ent, fd, 0);
			}
			break;
		}

		/* bounce is checked here to see if the rubber rocket hit walls enough times to wear out*/
		bounce++;
		if (bounce > fd->bounce || tr.fraction >= 1.0)
			break;

		range -= tr.fraction * range;
		VectorCopy(impact, cur_loc);
		VectorScale(tr.plane.normal, -DotProduct(tr.plane.normal, dir), temp);
		VectorAdd(temp, dir, dir);
		VectorAdd(temp, dir, dir);
		flags |= SF_BOUNCED;
	}

	if (!mock) {
		/* spawn the throwable item on the floor but only if it is not depletable */
		if (fd->ammo && !fd->splrad && weapon->t->thrown && !weapon->t->deplete) {
			pos3_t drop;

			if (G_EdictPosIsSameAs(ent, at)) { /* throw under his own feet */
				VectorCopy(at, drop);
			} else {
				impact[2] -= 20; /* a hack: no-gravity items are flying high */
				VecToPos(impact, drop);
			}

			G_SpawnItemOnFloor(drop, weapon);
		}
	}
}

void G_GetShotOrigin (const edict_t *shooter, const fireDef_t *fd, const vec3_t dir, vec3_t shotOrigin)
{
	/* get weapon position */
	gi.GridPosToVec(gi.routingMap, shooter->fieldSize, shooter->pos, shotOrigin);
	/* adjust height: */
	shotOrigin[2] += fd->shotOrg[1];
	/* adjust horizontal: */
	if (fd->shotOrg[0] != 0) {
		/* get "right" and "left" of a unit(rotate dir 90 on the x-y plane): */
		const float x = dir[1];
		const float y = -dir[0];
		const float length = sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
		/* assign adjustments: */
		shotOrigin[0] += x * fd->shotOrg[0] / length;
		shotOrigin[1] += y * fd->shotOrg[0] / length;
	}
}

/**
 * @brief Prepares weapon, firemode and container used for shoot.
 * @param[in] ent Pointer to attacker.
 * @param[in] shootType Type of shot.
 * @param[in] firemode An index of used firemode.
 * @param[in,out] weapon Weapon being used. It is NULL when calling this function.
 * @param[in,out] container Container with weapon being used. It is 0 when calling this function.
 * @param[in,out] fd Firemode being used. It is NULL when calling this function.
 * @return qtrue if function is able to check and set everything correctly.
 * @sa G_ClientShoot
 */
static qboolean G_PrepareShot (edict_t *ent, shoot_types_t shootType, fireDefIndex_t firemode, item_t **weapon, containerIndex_t *container, const fireDef_t **fd)
{
	const fireDef_t *fdArray;
	item_t *item;

	if (shootType >= ST_NUM_SHOOT_TYPES)
		gi.Error("G_GetShotFromType: unknown shoot type %i.\n", shootType);

	if (IS_SHOT_HEADGEAR(shootType)) {
		if (!HEADGEAR(ent))
			return qfalse;
		item = &HEADGEAR(ent)->item;
		*container = gi.csi->idHeadgear;
	} else if (IS_SHOT_RIGHT(shootType)) {
		if (!RIGHT(ent))
			return qfalse;
		item = &RIGHT(ent)->item;
		*container = gi.csi->idRight;
	} else {
		if (!LEFT(ent))
			return qfalse;
		item = &LEFT(ent)->item;
		*container = gi.csi->idLeft;
	}

	/* Get firedef from the weapon entry instead */
	fdArray = FIRESH_FiredefForWeapon(item);
	if (fdArray == NULL)
		return qfalse;

	*weapon = item;

	assert(firemode >= 0);
	*fd = &fdArray[firemode];

	return qtrue;
}

/**
 * @brief Setup for shooting, either real or mock
 * @param[in] player The player this action belongs to (i.e. either the ai or the player)
 * @param[in] ent the edict that is doing the shot
 * @param[in] at Position to fire on.
 * @param[in] shootType What type of shot this is (left, right reaction-left etc...).
 * @param[in] firemode The firemode index of the ammo for the used weapon.
 * @param[in] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @param[in] allowReaction Set to qtrue to check whether this has forced any reaction fire, otherwise qfalse.
 * @return qtrue if everything went ok (i.e. the shot(s) where fired ok), otherwise qfalse.
 * @param[in] z_align This value may change the target z height - often @c GROUND_DELTA is used to calculate
 * the alignment. This can be used for splash damage weapons. It's often useful to target the ground close to the
 * victim. That way you don't need a 100 percent chance to hit your target. Even if you don't hit it, the splash
 * damage might reduce the health of your target.
 */
qboolean G_ClientShoot (const player_t * player, edict_t* ent, const pos3_t at, shoot_types_t shootType,
		fireDefIndex_t firemode, shot_mock_t *mock, qboolean allowReaction, int z_align)
{
	const fireDef_t *fd;
	item_t *weapon;
	vec3_t dir, center, target, shotOrigin;
	int i, ammo, prevDir, reactionLeftover, shots;
	containerIndex_t container;
	int mask;
	qboolean quiet;
	vec3_t impact;

	/* just in 'test-whether-it's-possible'-mode or the player is an
	 * ai - no readable feedback needed */
	quiet = (mock != NULL) || G_IsAIPlayer(player);

	weapon = NULL;
	fd = NULL;
	container = 0;
	if (!G_PrepareShot(ent, shootType, firemode, &weapon, &container, &fd)) {
		if (!weapon && !quiet)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - object not activatable!\n"));
		return qfalse;
	}

	ammo = weapon->a;
	/* if this is reaction fire, don't keep trying to reserve TUs for reaction fire */
	reactionLeftover = IS_SHOT_REACTION(shootType) ? max(0, player->reactionLeftover - ent->chr.reservedTus.reaction) : 0;

	/* check if action is possible
	 * if allowReaction is false, it is a shot from reaction fire, so don't check the active team */
	if (allowReaction) {
		if (!G_ActionCheckForCurrentTeam(player, ent, fd->time + reactionLeftover))
			return qfalse;
	} else {
		if (!G_ActionCheckForReaction(player, ent, fd->time + reactionLeftover))
			return qfalse;
	}

	/* Don't allow to shoot yourself */
	if (!fd->irgoggles && G_EdictPosIsSameAs(ent, at))
		return qfalse;

	/* check that we're not firing a twohanded weapon with one hand! */
	if (weapon->t->fireTwoHanded &&	LEFT(ent)) {
		if (!quiet)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - weapon cannot be fired one handed!\n"));
		return qfalse;
	}

	/* check we're not out of ammo */
	if (!ammo && fd->ammo && !weapon->t->thrown) {
		if (!quiet)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - no ammo!\n"));
		return qfalse;
	}

	/* check target is not out of range */
	gi.GridPosToVec(gi.routingMap, ent->fieldSize, at, target);
	if (fd->range < VectorDist(ent->origin, target)) {
		if (!quiet)
			G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - target out of range!\n"));
		return qfalse;
	}

	/* Count for stats if it's no mock-shot and it's a Phalanx soldier (aliens do not have this info yet). */
	if (!mock && ent->chr.scoreMission) {
		/* Count this start of the shooting for stats/score. */
		/** @todo check for direct shot / splash damage shot? */
		if (fd->splrad > 0.0) {
			/* Splash damage */
			ent->chr.scoreMission->firedSplashTUs[fd->weaponSkill] += fd->time;
			ent->chr.scoreMission->firedSplash[fd->weaponSkill]++;
			for (i = 0; i < KILLED_NUM_TYPES; i++) {
				/** Reset status. @see G_UpdateHitScore for the check. */
				ent->chr.scoreMission->firedSplashHit[i] = qfalse;
			}
		} else {
			/* Direct hits */
			ent->chr.scoreMission->firedTUs[fd->weaponSkill] += fd->time;
			ent->chr.scoreMission->fired[fd->weaponSkill]++;
			for (i = 0; i < KILLED_NUM_TYPES; i++) {
				/** Reset status. @see G_UpdateHitScore for the check. */
				ent->chr.scoreMission->firedHit[i] = qfalse;
			}
		}
	}

	/* fire shots */
	shots = fd->shots;
	if (fd->ammo && !weapon->t->thrown) {
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
				G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - not enough ammo!\n"));
			return qfalse;
		}
	}

	/* rotate the player */
	if (mock)
		prevDir = ent->dir;
	else
		prevDir = 0;

	if (!G_EdictPosIsSameAs(ent, at)) {
		VectorSubtract(at, ent->pos, dir);
		ent->dir = AngleToDir((int) (atan2(dir[1], dir[0]) * todeg));
		assert(ent->dir < CORE_DIRECTIONS);

		if (!mock) {
			G_CheckVisTeamAll(ent->team, qfalse, ent);
			G_EventActorTurn(ent);
		}
	}

	/* calculate visibility */
	target[2] -= z_align;
	VectorSubtract(target, ent->origin, dir);
	VectorMA(ent->origin, 0.5, dir, center);
	mask = 0;
	for (i = 0; i < MAX_TEAMS; i++)
		if (G_IsVisibleForTeam(ent, i) || G_TeamPointVis(i, target) || G_TeamPointVis(i, center))
			mask |= G_TeamToVisMask(i);

	if (!mock) {
		/* State info so we can check if an item was already removed. */
		qboolean itemAlreadyRemoved = qfalse;

		/* check whether this has forced any reaction fire */
		if (allowReaction) {
			G_ReactionFirePreShot(ent, fd->time);
			if (G_IsDead(ent))
				/* dead men can't shoot */
				return qfalse;
		}

		/* start shoot */
		G_EventStartShoot(ent, mask, shootType, at);

		/* send shot sound to the others */
		G_EventShootHidden(mask, fd, qtrue);

		/* ammo... */
		if (fd->ammo) {
			if (ammo > 0 || !weapon->t->thrown) {
				G_EventInventoryAmmo(ent, weapon->m, ammo, shootType);
				weapon->a = ammo;
			} else { /* delete the knife or the rifle without ammo */
				const invDef_t *invDef = INVDEF(container);
				assert(invDef->single);
				itemAlreadyRemoved = qtrue;	/* for assert only */
				game.i.EmptyContainer(&game.i, &ent->chr.i, invDef);
				G_EventInventoryDelete(ent, G_VisToPM(ent->visflags), invDef, 0, 0);
			}
		}

		/* remove throwable oneshot && deplete weapon from inventory */
		if (weapon->t->thrown && weapon->t->oneshot && weapon->t->deplete) {
			const invDef_t *invDef = INVDEF(container);
			if (itemAlreadyRemoved)
				gi.Error("Item %s is already removed", weapon->t->id);
			assert(invDef->single);
			game.i.EmptyContainer(&game.i, &ent->chr.i, invDef);
			G_EventInventoryDelete(ent, G_VisToPM(ent->visflags), invDef, 0, 0);
		}
	}

	G_GetShotOrigin(ent, fd, dir, shotOrigin);

	/* Fire all shots. */
	for (i = 0; i < shots; i++)
		if (fd->gravity)
			G_ShootGrenade(player, ent, fd, shotOrigin, at, mask, weapon, mock, z_align, impact);
		else
			G_ShootSingle(ent, fd, shotOrigin, at, mask, weapon, mock, z_align, i, shootType, impact);

	if (!mock) {
		if (fd->obj->dmgtype == gi.csi->damSmoke) {
			G_SpawnSmokeField(impact, "smokefield", 2);
		} else if (fd->obj->dmgtype == gi.csi->damIncendiary) {
			const int damage = max(0, fd->damage[0] + (fd->damage[1] * crand()));
			G_SpawnFireField(impact, "firefield", 2, damage);
		}

		/* send TUs if ent still alive */
		if (ent->inuse && !G_IsDead(ent)) {
			G_ActorSetTU(ent, ent->TU - fd->time);
			G_SendStats(ent);
		}

		/* end events */
		gi.EndEvents();

		/* check for win/draw conditions */
		G_MatchEndCheck();

		/* check for Reaction fire against the shooter */
		if (allowReaction)
			G_ReactionFirePostShot(ent);
	} else {
		ent->dir = prevDir;
		assert(ent->dir < CORE_DIRECTIONS);
	}
	return qtrue;
}
