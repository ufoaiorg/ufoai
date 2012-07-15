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

#include "g_local.h"

/**
 * @brief Deals damage and causes wounds.
 * @param[in,out] target Pointer to the actor we want to damage.
 * @param[in] damage The value of the damage.
 */
void G_DamageActor (edict_t *target, const int damage)
{
	assert(target->chr.teamDef);
	int bodyPart = target->chr.teamDef->bodyTemplate->getRandomBodyPart();

	G_TakeDamage(target, damage);
	if (damage > 0 && target->HP > 0) {

		target->chr.wounds.woundLevel[bodyPart] += damage;
#if 0
		if (!CHRSH_IsTeamDefRobot(target->chr.teamDef))
			/* Knockback -- currently disabled, not planned in the specs, also there's no way to tell stunned and dead actors apart */
			target->STUN = std::min(255.0f, target->STUN + std::max(0.0f, damage * crand() * 0.25f));
#endif
		if (target->chr.wounds.woundLevel[bodyPart] > target->chr.maxHP * target->chr.teamDef->bodyTemplate->woundThreshold(bodyPart))
			G_ClientPrintf(G_PLAYER_FROM_ENT(target), PRINT_HUD, _("%s has been wounded!\n"), target->chr.name);
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
		for (bodyPart = 0; bodyPart < ent->chr.teamDef->bodyTemplate->numBodyParts(); ++bodyPart)
			if (ent->chr.wounds.woundLevel[bodyPart] > ent->chr.maxHP * ent->chr.teamDef->bodyTemplate->woundThreshold(bodyPart))
				damage += ent->chr.wounds.woundLevel[bodyPart] * ent->chr.teamDef->bodyTemplate->bleedingFactor(bodyPart);
		if (damage > 0) {
			G_PrintStats("%s is bleeding (damage: %i)\n", ent->chr.name, damage);
			G_TakeDamage(ent, damage);
			G_CheckDeathOrKnockout(ent, NULL, NULL, damage);
		}
	}
	/* Maybe the last team member bled to death */
	G_MatchEndCheck();
}

void G_SendWoundStats (edict_t *const ent)
{
	int i;
	for (i = 0; i < ent->chr.teamDef->bodyTemplate->numBodyParts(); ++i) {
		/* Sanity checks */
		ent->chr.wounds.woundLevel[i] = std::max(0, ent->chr.wounds.woundLevel[i]);
		ent->chr.wounds.treatmentLevel[i] = std::max(0, ent->chr.wounds.treatmentLevel[i]);
		ent->chr.wounds.woundLevel[i] = std::min(255, ent->chr.wounds.woundLevel[i]);
		ent->chr.wounds.treatmentLevel[i] = std::min(255, ent->chr.wounds.treatmentLevel[i]);
		if (ent->chr.wounds.woundLevel[i] + ent->chr.wounds.treatmentLevel[i] > 0)
			G_EventActorWound(ent, i);
	}
}

/**
 * @brief Returns the penalty to the given stat caused by the actor wounds.
 * @param[in] ent Pointer to the actor we want to calculate the penalty for.
 * @param[in] type The stat we want to calculate the penalty for.
 * @return The penalty as a percent.
 */
float G_ActorGetInjuryPenalty (const edict_t *const ent, const modifier_types_t type)
{
	int bodyPart;
	float penalty = 0;

	for (bodyPart = 0; bodyPart < ent->chr.teamDef->bodyTemplate->numBodyParts(); ++bodyPart) {
		const int threshold = ent->chr.maxHP * ent->chr.teamDef->bodyTemplate->woundThreshold(bodyPart);
		const int injury = (ent->chr.wounds.woundLevel[bodyPart] + ent->chr.wounds.treatmentLevel[bodyPart] * 0.5);
		if (injury > threshold)
			penalty += 2 * ent->chr.teamDef->bodyTemplate->penalty(bodyPart, type) * injury / ent->chr.maxHP;
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
