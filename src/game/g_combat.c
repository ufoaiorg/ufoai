/**
 * @file g_client.c
 * @brief Main part of the game logic.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

/*
 * 0: Stores the used TUs for Reaction fire for each edict.
 * 1: Stores if the edict has fired in rection.
*/
int TU_REACTIONS[MAX_EDICTS][2];

typedef enum {
	ML_WOUND,
	ML_DEATH
} morale_modifiers;

/**
 * @brief test if point is "visible" from team
 */
qboolean G_TeamPointVis(int team, vec3_t point)
{
	edict_t *from;
	vec3_t eye;
	int i;

	/* test if point is visible from team */
	for (i = 0, from = g_edicts; i < globals.num_edicts; i++, from++)
		if (from->inuse && (from->type == ET_ACTOR || from->type == ET_UGV) && !(from->state & STATE_DEAD) && from->team == team && G_FrustomVis(from, point)) {
			/* get viewers eye height */
			VectorCopy(from->origin, eye);
			if (from->state & (STATE_CROUCHED | STATE_PANIC))
				eye[2] += EYE_CROUCH;
			else
				eye[2] += EYE_STAND;

			/* line of sight */
			if (!gi.TestLine(eye, point))
				return qtrue;
		}

	/* not visible */
	return qfalse;
}

/**
 * @brief Applies morale changes to actors around a wounded or killed actor
 * @note only called when mor_panic is not zero
 */
static void G_Morale(int type, edict_t * victim, edict_t * attacker, int param)
{
	edict_t *ent;
	int i, newMorale;
	float mod;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		/* this only applies to ET_ACTOR but not ET_UGV */
		if (ent->inuse && ent->type == ET_ACTOR && !(ent->state & STATE_DEAD) && ent->team != TEAM_CIVILIAN) {
			switch (type) {
			case ML_WOUND:
			case ML_DEATH:
				/* morale damage is damage dependant */
				mod = mob_wound->value * param;
				/* death hurts morale even more than just damage */
				if (type == ML_DEATH)
					mod += mob_death->value;
				/* seeing how someone gets shot increases the morale change */
				if (ent == victim || (G_ActorVis(ent->origin, victim, qfalse) && G_FrustomVis(ent, victim->origin)))
					mod *= mof_watching->value;
				if (ent->team == attacker->team) {
					/* teamkills are considered to be bad form, but won't cause an increased morale boost for the enemy */
					/* morale boost isn't equal to morale loss (it's lower, but morale gets regenerated) */
					if (victim->team == attacker->team)
						mod *= mof_teamkill->value;
					else
						mod *= mof_enemy->value;
				}
				/* seeing a civi die is more "acceptable" */
				if (victim->team == TEAM_CIVILIAN)
					mod *= mof_civilian->value;
				/* if an ally (or in singleplayermode, as human, a civilian) got shot, lower the morale, don't heighten it. */
				if (victim->team == ent->team || (victim->team == TEAM_CIVILIAN && ent->team != TEAM_ALIEN && (int) sv_maxclients->value == 1))
					mod *= -1;
				/* if you stand near to the attacker or the victim, the morale change is higher. */
				mod *= mor_default->value + pow(0.5, VectorDist(ent->origin, victim->origin) / mor_distance->value)
					* mor_victim->value + pow(0.5, VectorDist(ent->origin, attacker->origin) / mor_distance->value)
					* mor_attacker->value;
				/* morale damage is dependant on the number of living allies */
				mod *= (1 - mon_teamfactor->value)
					+ mon_teamfactor->value * (level.num_spawned[victim->team] + 1)
					/ (level.num_alive[victim->team] + 1);
				/* being hit isn't fun */
				if (ent == victim)
					mod *= mor_pain->value;
				break;
			default:
				Com_Printf("Undefined morale modifier type %i\n", type);
				mod = 0;
			}
			/* clamp new morale */
			/*+0.9 to allow weapons like flamethrowers to inflict panic (typecast rounding) */
			newMorale = ent->morale + (int) (MORALE_RANDOM(mod) + 0.9);
			if (newMorale > GET_MORALE(ent->chr.skills[ABILITY_MIND]))
				ent->morale = GET_MORALE(ent->chr.skills[ABILITY_MIND]);
			else if (newMorale < 0)
				ent->morale = 0;
			else
				ent->morale = newMorale;

			/* send phys data */
			G_SendStats(ent);
		}
}

/**
 * @brief Stores the TUs for reaction fire that are used (if any).
 * @note Normally called on end of turn.
 * @todo Comment on the AddEvent code.
 * @sa G_ClientStateChange
 * @param[in] team Index of team to loop through.
 */
void G_ResetReactionFire(int team)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && !(ent->state & STATE_DEAD) && (ent->type == ET_ACTOR || ent->type == ET_UGV) && ent->team == team) {
			TU_REACTIONS[ent->number][1]  = 0;	/* reset 'RF used' flag */
			if ( ent->state & STATE_REACTION ) {
				if (ent->TU >= TU_REACTION ) {
					/* Enough TUs for reaction fire available. */
					ent->TU -= TU_REACTION;
					TU_REACTIONS[ent->number][0] = TU_REACTION;	/* Save the used TUs for possible later re-adding. */
				} else if (ent->TU > 0) {
					/* Not enough TUs for reaction fire available. */
					TU_REACTIONS[ent->number][0] = ent->TU;	/* Save the used TUs for possible later re-adding. */
					ent->TU = 0;
				} else {
					/* No TUs at all available. */
					TU_REACTIONS[ent->number][0] = -1;
				}
			} else {
				TU_REACTIONS[ent->number][0] = 0;	/* Reset saved TUs. */
			}
			ent->state &= ~STATE_SHAKEN;
			gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
			gi.WriteShort(ent->number);
			gi.WriteShort(ent->state);
		}
}

/**
 * @brief
 * @param[in] mock
 * @param[in] shooter
 * @param[in] struck
 * @param[in] damage
 * @sa G_Damage
 */
static void G_UpdateShotMock(shot_mock_t *mock, edict_t *shooter, edict_t *struck, int damage)
{
	assert(struck->number != shooter->number || mock->allow_self);

	if (damage > 0) {
		if (!struck || !struck->inuse || struck->state & STATE_DEAD)
			return;
		else if (!(struck->visflags & (1 << shooter->team)))
			return;
		else if (struck->team == TEAM_CIVILIAN)
			mock->civilian += 1;
		else if (struck->team == shooter->team)
			mock->friend += 1;
		else if (struck->type == ET_ACTOR || struck->type == ET_UGV)
			mock->enemy += 1;
		else
			return;

		mock->damage += damage;
	}
}

/**
 * @brief Deals damage of a give type and amount to a target.
 */
static void G_Damage(edict_t * ent, int dmgtype, int damage, edict_t * attacker, shot_mock_t *mock)
{
	qboolean stun = (dmgtype == gi.csi->damStun);
	qboolean shock = (dmgtype == gi.csi->damShock);

	assert (ent);
	assert (ent->type == ET_ACTOR
			|| ent->type == ET_UGV
			|| ent->type == ET_BREAKABLE);

	/* Breakables are immune to stun & shock damage. */
	if ((stun || shock) && (ent->type == ET_BREAKABLE))
 		return;

	/* Breakables */
	if (ent->type == ET_BREAKABLE) {
		if (!mock) {
			if (damage >= ent->HP) {
				gi.AddEvent(PM_ALL, EV_MODEL_EXPLODE);
				gi.WriteShort(ent->mapNum);
				if (ent->particle && Q_strcmp(ent->particle, "null")) {
					/* TODO: Spawn the breaking particle here */
					gi.AddEvent(PM_ALL, EV_SPAWN_PARTICLE);
					gi.WriteShort(ent->spawnflags);
					gi.WriteGPos(ent->pos);
					gi.WriteShort((int)strlen(ent->particle));
					gi.WriteString(ent->particle);
				}
				gi.unlinkentity(ent);
				ent->inuse = qfalse;
				G_RecalcRouting(ent);
				G_FreeEdict(ent);
			} else
				ent->HP -= damage;
		}
		return;
	}

	/* Actors don't die again. */
	if (ent->state & STATE_DEAD)
		return;

	/* Apply difficulty settings. */
	if (sv_maxclients->value == 1) {
		if (attacker->team == TEAM_ALIEN && ent->team < TEAM_ALIEN)
			damage *= pow(1.3, difficulty->value);
		else if (attacker->team < TEAM_ALIEN && ent->team == TEAM_ALIEN)
			damage *= pow(1.3, -difficulty->value);
	}

	/* Apply armor effects. */
	if (damage > 0 && ent->i.c[gi.csi->idArmor]) {
		objDef_t *ad;
		int totalDamage;

		ad = &gi.csi->ods[ent->i.c[gi.csi->idArmor]->item.t];

		totalDamage = damage;

		if (ad->protection[dmgtype] > 0)
			damage *= 1.0 - ad->protection[dmgtype] * ent->AP * 0.0001;
		else
			damage *= 1.0 - ad->protection[dmgtype] * 0.01;

		if (!mock) {
			if (ad->hardness[dmgtype]) {
				int armorDamage;

				armorDamage = (totalDamage - damage) / ad->hardness[dmgtype];
				ent->AP = armorDamage < ent->AP ? ent->AP - armorDamage : 0;
			}
		}
	}

	assert((attacker->team >= 0) && (attacker->team < MAX_TEAMS));
	assert((ent->team >= 0) && (ent->team < MAX_TEAMS));
#ifdef DEBUG
	if ((attacker->team < 0) || (attacker->team >= MAX_TEAMS))
		return;	/* never reached. need for code analyst. */
	if ((ent->team < 0) || (ent->team >= MAX_TEAMS))
		return;	/* never reached. need for code analyst. */
#endif

	if (g_nodamage != NULL && !g_nodamage->value)
	{
		/* hit */
		if (mock) {
			G_UpdateShotMock(mock, attacker, ent, damage);
		} else if (stun) {
			ent->STUN += damage;
		} else if (shock) {
			/* Only do this if it's not one from our own team ... they should known that there is a flashbang coming. */
			if (ent->team != attacker->team) {
				ent->AP = 0;
				return;
			}
		} else {
			ent->HP -= damage;
		}
	}

	if (mock)
		return;

	/* check death/knockouth */
	if (ent->HP <= 0 || ent->HP <= ent->STUN) {
		G_SendStats(ent);
		G_ActorDie(ent, ent->HP <= 0 ? STATE_DEAD : STATE_STUN);

		/* apply morale changes */
		if (mor_panic->value)
			G_Morale(ML_DEATH, ent, attacker, damage);

		/* count kills */
		if (ent->HP<=0)
			level.num_kills[attacker->team][ent->team]++;
		/*count stuns*/
		else
			level.num_stuns[attacker->team][ent->team]++;

		/* count score */
		if (ent->team == TEAM_CIVILIAN)
			attacker->chr.kills[KILLED_CIVILIANS]++;
		else if (attacker->team == ent->team)
			attacker->chr.kills[KILLED_TEAM]++;
		else
			attacker->chr.kills[KILLED_ALIENS]++;

		/* check for win conditions */
		G_CheckEndGame();
	} else {
		if (damage > 0 && mor_panic->value) {
			G_Morale(ML_WOUND, ent, attacker, damage);
		}
		else { /* medikit, etc. */
			if (ent->HP > GET_HP(ent->chr.skills[ABILITY_POWER]))
				ent->HP = GET_HP(ent->chr.skills[ABILITY_POWER]);
		}
		G_SendStats(ent);
	}
}

/**
 * @brief
 */
void G_StunTeam(void)
{
	/* default is to kill all teams */
	int teamToKill = -1, i;
	edict_t *ent;

	/* with a parameter we will be able to kill a specific team */
	if (gi.argc() == 2)
		teamToKill = atoi(gi.argv(1));

	gi.dprintf("G_StunTeam: stun team %i\n", teamToKill);

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD)) {
			if (teamToKill >= 0 && ent->team != teamToKill)
				continue;

			/* die */
			G_ActorDie(ent, STATE_STUN);

			if (teamToKill == TEAM_ALIEN)
				level.num_stuns[1][TEAM_ALIEN]++;
			else
				level.num_stuns[7][teamToKill]++;
		}

	/* check for win conditions */
	G_CheckEndGame();
}

/**
 * @brief
 */
void G_SplashDamage(edict_t * ent, fireDef_t * fd, vec3_t impact, shot_mock_t *mock)
{
	edict_t *check;
	vec3_t center;
	float dist;
	int damage;
	int i;

	qboolean shock = (fd->dmgtype==gi.csi->damShock);

	assert (fd->splrad);

	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++) {
		/* check basic info */
		if (!check->inuse)
			continue;

		/* If we use a blinding weapon we skip the target if it's looking away from the impact location. */
		if (shock && !G_FrustomVis(ent, impact))
			continue;

		if (check->type == ET_ACTOR || check->type == ET_UGV)
			VectorCopy(check->origin, center);
		else if (check->type == ET_BREAKABLE) {
			VectorAdd(check->absmin, check->absmax, center);
			VectorScale(center, 0.5, center);
		} else
			continue;

		/* check for distance */
		dist = VectorDist(impact, center);
		dist = dist > UNIT_SIZE / 2 ? dist - UNIT_SIZE / 2 : 0;
		if (dist > fd->splrad)
			continue;

		/* FIXME: don't make aliens in back visible */
		if (fd->irgoggles && (check->type == ET_ACTOR || check->type == ET_UGV)) {
			if (G_FrustomVis(ent, check->origin)) {
				if (!mock) {
					G_AppearPerishEvent(~G_VisToPM(check->visflags), 1, check);
					check->visflags |= ~check->visflags;
				}
				continue;
			}
		}

		/* check for walls */
		if ((check->type == ET_ACTOR || check->type == ET_UGV) && !G_ActorVis(impact, check, qfalse))
			continue;

		/* do damage */
		if (shock)
			damage = 0;
		else
			damage = (fd->spldmg[0] + fd->spldmg[1] * crand()) * (1.0 - dist / fd->splrad);

		if (mock)
			mock->allow_self = qtrue;
		G_Damage(check, fd->dmgtype, damage, ent, mock);
		if (mock)
			mock->allow_self = qfalse;
	}
}

#define GRENADE_DT			0.1
#define GRENADE_STOPSPEED	60.0
/**
 * @brief
 */
void G_ShootGrenade(player_t * player, edict_t * ent, fireDef_t * fd, int type, vec3_t from, pos3_t at, int mask, item_t * weapon, shot_mock_t *mock)
{
	vec3_t last, target, temp;
	vec3_t startV, curV, oldPos, newPos;
	vec3_t angles;
	float dt, time, speed;
	float acc;
	trace_t tr;
	int bounce;
/*	int i; */
	byte flags;

	/* get positional data */
	VectorCopy(from, last);
	gi.GridPosToVec(gi.map, at, target);

	/* prefer to aim grenades at the ground */
	target[2] -= 28;

	/* calculate parabola */
	dt = gi.GrenadeTarget(last, target, fd->range, fd->launched, fd->rolled, startV);
	if (!dt) {
		if (!mock)
			gi.cprintf(player, PRINT_HIGH, _("Can't perform action - impossible throw!\n"));
		return;
	}

	/* cap start speed */
	speed = VectorLength(startV);
	if (speed > fd->range)
		speed = fd->range;

	/* add random effects and get new dir */
	acc = GET_ACC(ent->chr.skills[ABILITY_ACCURACY], fd->weaponSkill ? ent->chr.skills[fd->weaponSkill] : 0);

	VecToAngles(startV, angles);
	angles[PITCH] += crand() * fd->spread[0] * acc;
	angles[YAW] += crand() * fd->spread[1] * acc;
	AngleVectors(angles, startV, NULL, NULL);
	VectorScale(startV, speed, startV);

	/* move */
	VectorCopy(last, oldPos);
	VectorCopy(startV, curV);
	time = 0;
	dt = 0;
	bounce = 0;
	flags = SF_BOUNCING;
	while (qtrue) {
		/* kinematics */
		VectorMA(oldPos, GRENADE_DT, curV, newPos);
		newPos[2] -= 0.5 * GRAVITY * GRENADE_DT * GRENADE_DT;
		curV[2] -= GRAVITY * GRENADE_DT;

		/* trace */
		tr = gi.trace(oldPos, NULL, NULL, newPos, ent, MASK_SHOT);
		if (tr.fraction < 1.0 || time + dt > 4.0) {
			/* advance time */
			dt += tr.fraction * GRENADE_DT;
			time += dt;
			bounce++;

			if (tr.fraction < 1.0)
				VectorCopy(tr.endpos, newPos);

#if 0
/* please debug, currently it causes double sounds */
			/* calculate additional visibility */
			for (i = 0; i < MAX_TEAMS; i++)
				if (G_TeamPointVis(i, newPos))
					mask |= 1 << i;
#endif

			if
				/* enough bouncing around */
				(VectorLength(curV) < GRENADE_STOPSPEED || time > 4.0 || bounce > fd->bounce
				 /* or we have sensors that tell us enemy is near */
				 || (!fd->delay && tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV))) {

				if (!mock) {
					/* explode */
					gi.AddEvent(G_VisToPM(mask), EV_ACTOR_THROW);
					gi.WriteShort(dt * 1000);
					gi.WriteByte(type);
					if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV))
						gi.WriteByte(flags | SF_BODY);
					else
						gi.WriteByte(flags | SF_IMPACT);
					gi.WritePos(last);
					gi.WritePos(startV);
				}

				tr.endpos[2] += 10;

				/* check if this is a stone, ammor clip or grenade */
				if (fd->splrad) {
					G_SplashDamage(ent, fd, tr.endpos, mock);
				} else if (!mock) {
					/* spawn the stone on the floor */
					if (fd->ammo && !fd->splrad && gi.csi->ods[weapon->t].thrown) {
						pos3_t drop;
						edict_t *floor, *actor;

						VecToPos(tr.endpos, drop);

						for (floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++) {
							if (floor->inuse
								&& floor->type == ET_ITEM
								&& VectorCompare(drop, floor->pos))
								break;
						}

						if (floor == &g_edicts[globals.num_edicts]) {
							floor = G_SpawnFloor(drop);

							for (actor = g_edicts; actor < &g_edicts[globals.num_edicts]; actor++)
								if ( actor->inuse
									 && (actor->type == ET_ACTOR || actor->type == ET_UGV)
									 && VectorCompare(drop, actor->pos) )
									FLOOR(actor) = FLOOR(floor);
						} else {
							gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
							gi.WriteShort(floor->number);
							floor->visflags = 0;
						}
						Com_TryAddToInventory(&floor->i, *weapon, gi.csi->idFloor);

						/* send item info to the clients */
						G_CheckVis(floor, qtrue);
					}
				}
				return;
			}

			if (!mock) {
				/* send */
				gi.AddEvent(G_VisToPM(mask), EV_ACTOR_THROW);
				gi.WriteShort(dt * 1000);
				gi.WriteByte(type);
				gi.WriteByte(flags);
				gi.WritePos(last);
				gi.WritePos(startV);
			}
			flags |= SF_BOUNCED;

			/* bounce */
			VectorScale(curV, fd->bounceFac, curV);
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
		}
	}
}


/**
 * @brief Fires straight shots.
 * @param[in] ent The attacker.
 * @param[in] fd ?? TODO
 * @param[in] wi ?? TODO
 * @param[in] from Location of the gun muzzle.
 * @param[in] at Grid coordinate of the target.
 * @param[in] mask ?? TODO Visibility bit-mask of the others?
 */
void G_ShootSingle(edict_t * ent, fireDef_t * fd, int wi, vec3_t from, pos3_t at, int mask, item_t * weapon, shot_mock_t *mock)
{
	vec3_t dir;	/* Direction from the location of the gun muzzle ("from") to the target ("at") */
	vec3_t angles;	/* ?? TODO The random dir-modifier ?? */
	vec3_t cur_loc;	/* The current location of the projectile. */
	vec3_t impact;	/* The location of the target (-center?) */
	vec3_t temp;
	trace_t tr;	/* ?? TODO */
	float acc;	/* Accuracy modifier for the angle of the shot. */
	float range;	/* ?? TODO */
	float gauss1;
	float gauss2;   /* For storing 2 gaussian distributed random values. */
	int bounce;	/* ?? TODO */
	int damage;	/* The damage to be dealt to the target. */
	byte flags;	/* ?? TODO */
/*	int i; */

	/* Calc direction of the shot. */
	gi.GridPosToVec(gi.map, at, impact);	/* Get the position of the targetted grid-cell. ('impact' is used only temporary here)*/
	VectorCopy(from, cur_loc);		/* Set current location of the projectile to the starting (muzzle) location. */
	VectorSubtract(impact, cur_loc, dir);	/* Calculate the vector from current location to the target. */
	VectorNormalize(dir);			/* Normalize the vector i.e. make length 1.0 */
	VectorMA(cur_loc, sv_shot_origin->value, dir, cur_loc);	/* ?? TODO: Probably places the starting-location a bit away (cur_loc+8*dir) from the attacker-model/grid. Might need some change to reflect 2x2 units. Also might need a check if the distace is bigger than the one to the impact location.*/
	VecToAngles(dir, angles);		/* Get the angles of the direction vector. */

	/* Get accuracy value for this attacker. */
	acc = GET_ACC(ent->chr.skills[ABILITY_ACCURACY], fd->weaponSkill ? ent->chr.skills[fd->weaponSkill] : 0);

	/* Get 2 gaussian distributed random values */
	gaussrand(&gauss1, &gauss2);

	/* Make most of the shots be within the spread cone */
	gauss1 *= 0.5;
	gauss2 *= 0.5;

	/* Modify the angles with the accuracy modifier as a randomizer-range. If the attacker is crouched this modifier is included as well.  */
	if ((ent->state & STATE_CROUCHED) && fd->crouch) {
		angles[PITCH] += gauss1 * fd->spread[0] * fd->crouch * acc;
		angles[YAW] += gauss2 * fd->spread[1] * fd->crouch * acc;
	} else {
		angles[PITCH] += gauss1 * fd->spread[0] * acc;
		angles[YAW] += gauss2 * fd->spread[1] * acc;
	}
	/* Convert changed angles into new direction. */
	AngleVectors(angles, dir, NULL, NULL);

	/* shoot and bounce */
	range = fd->range;
	bounce = 0;
	flags = 0;
	do {
		/* Calc 'impact' vector that is located at the end of the range
		   defined by the fireDef_t. This is not really the impact location,
		   but rather the 'endofrange' location, see below for another use.*/
		VectorMA(cur_loc, range, dir, impact);

		/* Do the trace from current position of the projectile
		   to the end_of_range location.*/
		tr = gi.trace(cur_loc, NULL, NULL, impact, ent, MASK_SHOT);
		/* _Now_ we copy the correct impact location. */
		VectorCopy(tr.endpos, impact);

		/* set flags */
		if (tr.fraction < 1.0) {
			if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV)
				/* check if we differenciate between body and wall */
				&& !fd->delay)
				flags |= SF_BODY;
			else if (bounce < fd->bounce)
				flags |= SF_BOUNCING;
			else
				flags |= SF_IMPACT;
		}

#if 0
/* please debug, currently it causes double sounds */
		/* calculate additional visibility */
		for (i = 0; i < MAX_TEAMS; i++)
			if (G_TeamPointVis(i, impact))
				mask |= 1 << i;

		/* victims see shots */
		if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV))
			mask |= 1 << tr.ent->team;
#endif

		if (!mock) {
			/* send shot */
			gi.AddEvent(G_VisToPM(mask), EV_ACTOR_SHOOT);
			gi.WriteShort(ent->number);
			gi.WriteByte(wi);
			gi.WriteByte(flags);
			gi.WritePos(cur_loc);
			gi.WritePos(impact);
			gi.WriteDir(tr.plane.normal);

			/* send shot sound to the others */
			gi.AddEvent(~G_VisToPM(mask), EV_ACTOR_SHOOT_HIDDEN);
			gi.WriteByte(qfalse);
			gi.WriteByte(wi);
		}

		/* do splash damage */
		if (tr.fraction < 1.0 && fd->splrad && !fd-> bounce) {
			VectorMA(impact, sv_shot_origin->value, tr.plane.normal, impact);
			G_SplashDamage(ent, fd, impact, mock);
		}

		/* do damage */
		if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV || tr.ent->type == ET_BREAKABLE)) {
			damage = fd->damage[0] + fd->damage[1] * crand();
			G_Damage(tr.ent, fd->dmgtype, damage, ent, mock);
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
	while (1);

	if (!mock) {
		/* spawn the knife on the floor */
		if (fd->ammo && !fd->splrad && gi.csi->ods[weapon->t].thrown) {
			pos3_t drop;
			edict_t *floor, *actor;

			if (VectorCompare(ent->pos, at)) { /* throw under his own feet */
				VectorCopy(at, drop);
			} else {
				impact[2] -= 20; /* a hack: no-gravity items are flying high */
				VecToPos(impact, drop);
			}

			for (floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++) {
				if (floor->inuse
					&& floor->type == ET_ITEM
					&& VectorCompare(drop, floor->pos))
					break;
			}

			if (floor == &g_edicts[globals.num_edicts]) {
				floor = G_SpawnFloor(drop);

				for (actor = g_edicts; actor < &g_edicts[globals.num_edicts]; actor++)
					if ( actor->inuse
						 && (actor->type == ET_ACTOR || actor->type == ET_UGV)
						 && VectorCompare(drop, actor->pos) )
						FLOOR(actor) = FLOOR(floor);
			} else {
				gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
				gi.WriteShort(floor->number);
				floor->visflags = 0;
			}
			Com_TryAddToInventory(&floor->i, *weapon, gi.csi->idFloor);

			/* send item info to the clients */
			G_CheckVis(floor, qtrue);
		}
	}
}

void G_GetShotOrigin(edict_t *shooter, fireDef_t *fd, vec3_t dir, vec3_t shotOrigin)
{
	/* get weapon position */
	gi.GridPosToVec(gi.map, shooter->pos, shotOrigin);
	/* adjust height: */
	shotOrigin[2] += fd->shotOrg[1];
	/* adjust horizontal: */
	if (fd->shotOrg[0] != 0) {
		float x, y, length;

		/* get "right" and "left" of a unit(rotate dir 90 on the x-y plane): */
		x = dir[1];
		y = -dir[0];
		length = sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
		/* assign adjustments: */
		shotOrigin[0] += x * fd->shotOrg[0] / length;
		shotOrigin[1] += y * fd->shotOrg[0] / length;
	}
}

qboolean G_GetShotFromType(edict_t *ent, int type, item_t **weapon, int *container, fireDef_t **fd)
{
	if (type >= ST_NUM_SHOOT_TYPES)
		gi.error("G_GetShotFromType: unknown shoot type %i.\n", type);

	if (IS_SHOT_RIGHT(type)) {
		if (!RIGHT(ent))
			return qfalse;
		*weapon = &RIGHT(ent)->item;
		*container = gi.csi->idRight;
	} else {
		if (!LEFT(ent))
			return qfalse;
		*weapon = &LEFT(ent)->item;
		*container = gi.csi->idLeft;
	}

	if ((*weapon)->m == NONE) {
		*weapon = NULL;
		return qfalse;
	}

	*fd = &gi.csi->ods[(*weapon)->m].fd[SHOT_FD_PRIO(type)];

	return qtrue;
}

/**
 * @brief
 */
qboolean G_ClientShoot(player_t * player, int num, pos3_t at, int type, shot_mock_t *mock)
{
	fireDef_t *fd;
	edict_t *ent;
	item_t *weapon;
	vec3_t dir, center, target, shotOrigin;
	int i, ammo, wi, prev_dir = 0, reaction_leftover, shots;
	int container, mask;
	qboolean quiet;

	ent = g_edicts + num;
	quiet = mock != NULL;

	if (!G_GetShotFromType(ent, type, &weapon, &container, &fd)) {
		if (!weapon && !quiet)
			gi.cprintf(player, PRINT_HIGH, _("Can't perform action - object not activable!\n"));
		return qfalse;
	}

	wi = weapon->m | (SHOT_FD_PRIO(type) << 7/*move to byte end*/);
	ammo = weapon->a;
	reaction_leftover = IS_SHOT_REACTION(type) ? sv_reaction_leftover->value : 0;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, fd->time + reaction_leftover, quiet))
		return qfalse;

	/* check that we're not firing a twohanded weapon with one hand! */
	if (gi.csi->ods[weapon->t].firetwohanded &&	LEFT(ent)) {
		if (!quiet)
			gi.cprintf(player, PRINT_HIGH, _("Can't perform action - weapon cannot be fired one handed!\n"));
		return qfalse;
	}

	/* check we're not out of ammo */
	if (!ammo && fd->ammo && !gi.csi->ods[weapon->t].thrown) {
		if (!quiet)
			gi.cprintf(player, PRINT_HIGH, _("Can't perform action - no ammo!\n"));
		return qfalse;
	}

	/* fire shots */
	shots = fd->shots;
	if (fd->ammo && !gi.csi->ods[weapon->t].thrown) {
		if (ammo < fd->ammo) {
			shots = fd->shots * ammo / fd->ammo;
			ammo = 0;
		} else {
			ammo -= fd->ammo;
		}
		if (shots < 1) {
			if (!quiet)
				gi.cprintf(player, PRINT_HIGH, _("Can't perform action - not enough ammo!\n"));
			return qfalse;
		}
	}

	/* rotate the player */
	if (mock)
		prev_dir = ent->dir;
	VectorSubtract(at, ent->pos, dir);
	ent->dir = AngleToDV((int) (atan2(dir[1], dir[0]) * 180 / M_PI));

	if (!mock) {
		G_CheckVisTeam(ent->team, NULL, qfalse);

		gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_TURN);
		gi.WriteShort(num);
		gi.WriteByte(ent->dir);
	}

	/* calculate visibility */
	gi.GridPosToVec(gi.map, at, target);
	VectorSubtract(target, ent->origin, dir);
	VectorMA(ent->origin, 0.5, dir, center);
	mask = 0;
	for (i = 0; i < MAX_TEAMS; i++)
		if (ent->visflags & (1 << i) || G_TeamPointVis(i, target) || G_TeamPointVis(i, center))
			mask |= 1 << i;

	if (!mock) {
		/* start shoot */
		gi.AddEvent(G_VisToPM(mask), EV_ACTOR_START_SHOOT);
		gi.WriteShort(ent->number);
		gi.WriteByte(wi);
		gi.WriteGPos(ent->pos);
		gi.WriteGPos(at);

		/* send shot sound to the others */
		gi.AddEvent(~G_VisToPM(mask), EV_ACTOR_SHOOT_HIDDEN);
		gi.WriteByte(qtrue);
		gi.WriteByte(wi);

		/* ammo... */
		if (fd->ammo) {
			if ( ammo > 0
				 || !gi.csi->ods[weapon->t].thrown ) {
				gi.AddEvent(G_VisToPM(ent->visflags), EV_INV_AMMO);
				gi.WriteShort(num);
				gi.WriteByte(ammo);
				gi.WriteByte(weapon->m);
				weapon->a = ammo;
				if (IS_SHOT_RIGHT(type))
					gi.WriteByte(gi.csi->idRight);
				else
					gi.WriteByte(gi.csi->idLeft);
			} else { /* delete the knife or the rifle without ammo */
				gi.AddEvent(G_VisToPM(ent->visflags), EV_INV_DEL);
				gi.WriteShort(num);
				gi.WriteByte(container);
				assert (gi.csi->ids[container].single);
				Com_EmptyContainer(&ent->i, container);
			}
			/* x and y value */
			gi.WriteByte(0);
			gi.WriteByte(0);
		}
	}

	G_GetShotOrigin(ent, fd, dir, shotOrigin);

	/* fire all shots */
	for (i = 0; i < shots; i++)
		if (fd->gravity)
			G_ShootGrenade(player, ent, fd, wi, shotOrigin, at, mask, weapon, mock);
		else
			G_ShootSingle(ent, fd, wi, shotOrigin, at, mask, weapon, mock);

	if (!mock) {
		/* send TUs */
		if (ent->inuse) {
			ent->TU -= fd->time;
			G_SendStats(ent);
		}

		/* end events */
		gi.EndEvents();

		/* check for Reaction fire against the shooter */
		G_ReactionFire(ent, mock ? qfalse : qtrue);
	} else {
		ent->dir = prev_dir;
	}


	return qtrue;
}

/**
 * @brief
 * @param[in] player
 * @param[in] num
 * @param[in] at
 * @param[in] type
 * @sa G_ReactionFire
 * @sa G_ClientShoot
 */
static qboolean G_FireWithJudgementCall(player_t * player, int num, pos3_t at, int type)
{
	shot_mock_t mock;
	edict_t *shooter;
	int ff, i, maxff, minhit;

	shooter = g_edicts + num;

	minhit = shooter->reaction_minhit;
	if (shooter->state & STATE_INSANE)
		maxff = 100;
	else if (shooter->state & STATE_RAGE)
		maxff = 60;
	else if (shooter->state & STATE_PANIC)
		maxff = 30;
	else if (shooter->state & STATE_SHAKEN)
		maxff = 15;
	else
		maxff = 5;

	memset(&mock, 0, sizeof(mock));
	for (i = 0; i < 100; i++)
		G_ClientShoot(player, num, at, type, &mock);

	Com_DPrintf("G_FireWithJudgementCall: Hit: %d/%d FF+Civ: %d+%d=%d/%d Self: %d.\n",
		mock.enemy, minhit, mock.friend, mock.civilian, mock.friend + mock.civilian, maxff, mock.self);

	ff = mock.friend + (shooter->team == TEAM_ALIEN ? 0 : mock.civilian);
	if (ff <= maxff && mock.enemy >= minhit)
		return G_ClientShoot(player, num, at, type, NULL);
	else
		return qfalse;
}

/**
 * @brief	Checks if an edict has reaction fire enabled and sees the target. Then fire at him.
 * @param[in,out] target The edict that will be fired on.
 */
qboolean G_ReactionFire(edict_t * target, qboolean doShoot)
{
	qboolean fired, frustom;
	player_t *player;
	edict_t *ent;
	int i, team;
	float actorVis;

	fired = qfalse;
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if ( ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD) &&
				(ent->state & STATE_SHAKEN || ent->state & STATE_REACTION_MANY || (ent->state & STATE_REACTION_ONCE && !TU_REACTIONS[ent->number][1]))) {
			if (VectorDistSqr(ent->origin, target->origin) > MAX_SPOT_DIST * MAX_SPOT_DIST)
				continue;
			actorVis = G_ActorVis(ent->origin, target, qtrue);
			frustom = G_FrustomVis(ent, target->origin);
			if (actorVis > 0.2 && frustom) {
				/* If reaction fire is triggered by a friendly unit
				   and the shooter is still sane, don't shoot;
				   well, if the shooter isn't sane anymore... */
				if (target->team == TEAM_CIVILIAN || target->team == ent->team)
					if (!(ent->state & STATE_SHAKEN) || (float) ent->morale / mor_shaken->value > frand())
						continue;

				/* Don't react in your own turn, trust your commander. Can't use
				   level.activeTeam, because this function touches it recursively. */
				if (ent->team == turnTeam)
					continue;

				/* Get player. */
				player = game.players + ent->pnum;
				if (!player)
					continue;

				/* Change active team for this shot only. */
				team = level.activeTeam;
				level.activeTeam = ent->team;

				/* Fire the first weapon in hands if everything is ok. */
				if ( RIGHT(ent)
					 && (RIGHT(ent)->item.m != NONE)
					 && gi.csi->ods[RIGHT(ent)->item.t].weapon
					 && (!gi.csi->ods[RIGHT(ent)->item.t].reload
						 || RIGHT(ent)->item.a > 0)
					 && gi.csi->ods[RIGHT(ent)->item.m].fd[FD_PRIMARY].time + sv_reaction_leftover->value <= ent->TU
					 && gi.csi->ods[RIGHT(ent)->item.m].fd[FD_PRIMARY].range > VectorDist(ent->origin, target->origin) ) {
					fired = !doShoot ? qtrue : G_FireWithJudgementCall(player, ent->number, target->pos, ST_RIGHT_PRIMARY_REACTION);
				}
				if (!fired
					&& LEFT(ent)
					&& (LEFT(ent)->item.m != NONE)
					&& gi.csi->ods[LEFT(ent)->item.t].weapon
					&& (!gi.csi->ods[LEFT(ent)->item.t].reload
						|| LEFT(ent)->item.a > 0)
					&& gi.csi->ods[LEFT(ent)->item.m].fd[FD_PRIMARY].time + sv_reaction_leftover->value <= ent->TU
					&& gi.csi->ods[LEFT(ent)->item.m].fd[FD_PRIMARY].range > VectorDist(ent->origin, target->origin) ) {
					fired = !doShoot ? qtrue : G_FireWithJudgementCall(player, ent->number, target->pos, ST_LEFT_PRIMARY_REACTION);
				}

				/* Revert active team. */
				level.activeTeam = team;

				if (fired && doShoot) {
					ent->state &= ~STATE_SHAKEN;
					TU_REACTIONS[ent->number][1] += 1; /* Save the fact that the ent has fired. */
				}

				/* Check for death of the target. */
				if (target->state & STATE_DEAD)
					return fired;
			}
		}

	return fired;
}

