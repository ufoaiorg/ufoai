/**
 * @file
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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

#include "g_health.h"
#include "g_actor.h"
#include "g_client.h"
#include "g_combat.h"
#include "g_edicts.h"
#include "g_match.h"

static byte G_GetImpactDirection(const edict_t *const target, const vec3_t impact)
{
	vec3_t vec1, vec2;

	VectorSubtract(impact, target->origin, vec1);
	vec1[2] = 0;
	VectorNormalize(vec1);
	VectorCopy(dvecs[target->dir], vec2);
	VectorNormalize(vec2);

	return AngleToDir(VectorAngleBetween(vec1, vec2) / torad);
}

/**
 * @brief Deals damage and causes wounds.
 * @param[in,out] target Pointer to the actor we want to damage.
 * @param[in] damage The value of the damage.
 * @param[in] impact Impact location @c NULL for splash damage.
 */
void G_DamageActor (edict_t *target, const int damage, const vec3_t impact)
{
	assert(target->chr.teamDef);

	G_TakeDamage(target, damage);
	if (damage > 0 && target->HP > 0) {
		short bodyPart;
		const teamDef_t *const teamDef = target->chr.teamDef;
		if (impact) {
			/* Direct hit */
			const byte impactDirection = G_GetImpactDirection(target, impact);
			const float impactHeight = impact[2] / (target->absmin[2] + target->absmax[2]);
			bodyPart = teamDef->bodyTemplate->getHitBodyPart(impactDirection, impactHeight);
			target->chr.wounds.woundLevel[bodyPart] += damage;
			if (target->chr.wounds.woundLevel[bodyPart] > target->chr.maxHP * teamDef->bodyTemplate->woundThreshold(bodyPart))
				G_ClientPrintf(G_PLAYER_FROM_ENT(target), PRINT_HUD, _("%s has been wounded!"), target->chr.name);
		} else {
			/* No direct hit (splash damage) */
			int bodyPart;
			for (bodyPart = 0; bodyPart < teamDef->bodyTemplate->numBodyParts(); ++bodyPart) {
				target->chr.wounds.woundLevel[bodyPart] += teamDef->bodyTemplate->getArea(bodyPart) * damage;
				if (target->chr.wounds.woundLevel[bodyPart] > target->chr.maxHP * teamDef->bodyTemplate->woundThreshold(bodyPart))
					G_ClientPrintf(G_PLAYER_FROM_ENT(target), PRINT_HUD, _("%s has been wounded!"), target->chr.name);
			}
		}
#if 0
		if (!CHRSH_IsTeamDefRobot(target->chr.teamDef))
			/* Knockback -- currently disabled, not planned in the specs, also there's no way to tell stunned and dead actors apart */
			target->STUN = std::min(255.0f, target->STUN + std::max(0.0f, damage * crand() * 0.25f));
#endif
	}
}

/**
 * @brief Heals a target and treats wounds.
 * @param[in,out] target Pointer to the actor who we want to treat.
 * @param[in] fd Pointer to the firedef used to heal the target.
 * @param[in] heal The value of the damage to heal.
 * @param[in] healerTeam The index of the team of the healer.
 */
void G_TreatActor (edict_t *target, const fireDef_t *const fd, const int heal, const int healerTeam)
{
	assert(target->chr.teamDef);

	/* Treat wounds */
	if (fd->dmgweight == gi.csi->damNormal) {
		int bodyPart, mostWounded = 0;
		woundInfo_t *wounds = &target->chr.wounds;

		/* Find the worst not treated wound */
		for (bodyPart = 0; bodyPart < target->chr.teamDef->bodyTemplate->numBodyParts(); ++bodyPart)
			if (wounds->woundLevel[bodyPart] > wounds->woundLevel[mostWounded])
				mostWounded = bodyPart;

		if (wounds->woundLevel[mostWounded] > 0) {
			const int woundsHealed = std::min(static_cast<int>(abs(heal) / target->chr.teamDef->bodyTemplate->bleedingFactor(mostWounded)),
					wounds->woundLevel[mostWounded]);
			G_TakeDamage(target, heal);
			wounds->woundLevel[mostWounded] -= woundsHealed;
			wounds->treatmentLevel[mostWounded] += woundsHealed;

			/* Update stats here to get info on how many HP the target received. */
			if (target->chr.scoreMission)
				target->chr.scoreMission->heal += abs(heal);
		}
	}

	/* Treat stunned actors */
	if (fd->dmgweight == gi.csi->damStunElectro && G_IsStunned(target)) {
		if (CHRSH_IsTeamDefAlien(target->chr.teamDef) && target->team != healerTeam)
			/** @todo According to specs it should only be possible to use the medikit to keep an alien sedated when
			 * 'live alien' is researched, is it possible to find if a tech is researched here? */
			target->STUN = std::min(255, target->STUN - heal);
		else
			target->STUN = std::max(0, target->STUN + heal);
		G_ActorCheckRevitalise(target);
	}

	/* Increase morale */
	if (fd->dmgweight == gi.csi->damShock)
		target->morale = std::min(GET_MORALE(target->chr.score.skills[ABILITY_MIND]), target->morale - heal);
}

/**
 * @brief Deal damage to each wounded team member.
 * @param[in] team The index of the team to deal damage to.
 */
void G_BleedWounds (const int team)
{
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, team))) {
		int bodyPart, damage = 0;
		if (CHRSH_IsTeamDefRobot(ent->chr.teamDef))
			continue;
		const teamDef_t *const teamDef = ent->chr.teamDef;
		woundInfo_t &wounds = ent->chr.wounds;
		for (bodyPart = 0; bodyPart < teamDef->bodyTemplate->numBodyParts(); ++bodyPart)
			if (wounds.woundLevel[bodyPart] > ent->chr.maxHP * teamDef->bodyTemplate->woundThreshold(bodyPart))
				damage += wounds.woundLevel[bodyPart] * teamDef->bodyTemplate->bleedingFactor(bodyPart);
		if (damage > 0) {
			G_PrintStats("%s is bleeding (damage: %i)", ent->chr.name, damage);
			G_TakeDamage(ent, damage);
			G_CheckDeathOrKnockout(ent, NULL, NULL, damage);
		}
	}
	/* Maybe the last team member bled to death */
	G_MatchEndCheck();
}

/**
 * @brief Send wound stats to network buffer
 * @sa G_SendStats
 */
void G_SendWoundStats (edict_t *const ent)
{
	int i;
	for (i = 0; i < ent->chr.teamDef->bodyTemplate->numBodyParts(); ++i) {
		/* Sanity checks */
		woundInfo_t &wounds = ent->chr.wounds;
		wounds.woundLevel[i] = std::max(0, wounds.woundLevel[i]);
		wounds.treatmentLevel[i] = std::max(0, wounds.treatmentLevel[i]);
		wounds.woundLevel[i] = std::min(255, wounds.woundLevel[i]);
		wounds.treatmentLevel[i] = std::min(255, wounds.treatmentLevel[i]);
		if (wounds.woundLevel[i] + wounds.treatmentLevel[i] > 0)
			G_EventActorWound(ent, i);
	}
}

/**
 * @brief Returns the penalty to the given stat caused by the actor wounds.
 * @param[in] ent Pointer to the actor we want to calculate the penalty for.
 * @param[in] type The stat we want to calculate the penalty for.
 * @return The given penalty for this actor.
 */
float G_ActorGetInjuryPenalty (const edict_t *const ent, const modifier_types_t type)
{
	int bodyPart;
	float penalty = 0;

	const teamDef_t *const teamDef = ent->chr.teamDef;
	for (bodyPart = 0; bodyPart < teamDef->bodyTemplate->numBodyParts(); ++bodyPart) {
		const int threshold = ent->chr.maxHP * teamDef->bodyTemplate->woundThreshold(bodyPart);
		const int injury = (ent->chr.wounds.woundLevel[bodyPart] + ent->chr.wounds.treatmentLevel[bodyPart] * 0.5);
		if (injury > threshold)
			penalty += 2 * teamDef->bodyTemplate->penalty(bodyPart, type) * injury / ent->chr.maxHP;
	}

	switch (type) {
	case MODIFIER_REACTION:
		penalty += G_ActorGetInjuryPenalty(ent, MODIFIER_SHOOTING);
		break;
	case MODIFIER_SHOOTING:
	case MODIFIER_ACCURACY:
		++penalty;
		break;
	case MODIFIER_TU:
	case MODIFIER_SIGHT:
		penalty = 1 - penalty;
		break;
	case MODIFIER_MOVEMENT:
		penalty = ceil(penalty);
		break;
	default:
		gi.DPrintf("G_ActorGetInjuryPenalty: Unknown modifier type %i\n", type);
		penalty = 0;
		break;
	}

	return penalty;
}
