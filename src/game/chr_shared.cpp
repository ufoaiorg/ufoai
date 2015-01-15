/**
 * @file
 * @note Shared character generating functions prefix: CHRSH_
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

#include "q_shared.h"
#include "chr_shared.h"

character_s::character_s ()
{
	init();
}

void character_s::init ()
{
	name[0] = path[0] = body[0] = head[0] = '\0';
	ucn = bodySkin = headSkin = HP = minHP = maxHP = STUN = morale = state = gender = 0;
	fieldSize = 0;
	scoreMission = nullptr;
	teamDef = nullptr;
	inv.init();
	memset(implants, 0, sizeof(implants));
}

/**
 * @brief Returns the actor sounds for a given category
 * @param[in] gender The gender of the actor
 * @param[in] soundType Which sound category (actorSound_t)
 */
const char* teamDef_s::getActorSound (int gender, actorSound_t soundType) const
{
	if (gender < 0 || gender >= NAME_LAST) {
		Com_DPrintf(DEBUG_SOUND|DEBUG_CLIENT, "getActorSound: invalid gender: %i\n", gender);
		return nullptr;
	}
	if (numSounds[soundType][gender] <= 0) {
		Com_DPrintf(DEBUG_SOUND|DEBUG_CLIENT, "getActorSound: no sound defined for sound type: %i, teamID: '%s', gender: %i\n", soundType, id, gender);
		return nullptr;
	}

	// Can't use LIST_GetRandom() or LIST_GetByIdx() in the game module
	const int random = rand() % numSounds[soundType][gender];
	const linkedList_t* list = sounds[soundType][gender];
	for (int j = 0; j < random; j++) {
		assert(list);
		list = list->next;
	}

	assert(list);
	assert(list->data);
	return (const char*)list->data;
}

/**
 * @brief Check if a team definition is alien.
 * @param[in] td Pointer to the team definition to check.
 */
bool CHRSH_IsTeamDefAlien (const teamDef_t* const td)
{
	return td->team == TEAM_ALIEN;
}

bool CHRSH_IsArmourUseableForTeam (const objDef_t* od, const teamDef_t* teamDef)
{
	assert(teamDef);
	assert(od->isArmour());

	if (!teamDef->armour)
		return false;

	return od->useable == teamDef->team;
}

/**
 * @brief Check if a team definition is a robot.
 * @param[in] td Pointer to the team definition to check.
 */
bool CHRSH_IsTeamDefRobot (const teamDef_t* const td)
{
	return td->robot;
}

const chrTemplate_t* CHRSH_GetTemplateByID (const teamDef_t* teamDef, const char* templateId)
{
	if (!Q_strnull(templateId))
		for (int i = 0; i < teamDef->numTemplates; i++)
			if (Q_streq(teamDef->characterTemplates[i]->id, templateId))
				return teamDef->characterTemplates[i];

	return nullptr;
}

/**
 * @brief Assign the effect values to the character
 */
static void CHRSH_UpdateCharacterWithEffect (character_t& chr, const itemEffect_t& e)
{
	chrScoreGlobal_t& s = chr.score;
	if (fabs(e.accuracy) > EQUAL_EPSILON)
		s.skills[ABILITY_ACCURACY] *= 1.0f + e.accuracy;
	if (fabs(e.mind) > EQUAL_EPSILON)
		s.skills[ABILITY_MIND] *= 1.0f + e.mind;
	if (fabs(e.power) > EQUAL_EPSILON)
		s.skills[ABILITY_POWER] *= 1.0f + e.power;
	if (fabs(e.TUs) > EQUAL_EPSILON)
		s.skills[ABILITY_SPEED] *= 1.0f + e.TUs;

	if (fabs(e.morale) > EQUAL_EPSILON)
		chr.morale *= 1.0f + e.morale;
}

/**
 * @brief Updates the characters permanent implants. Called every day.
 */
void CHRSH_UpdateImplants (character_t& chr)
{
	for (int i = 0; i < lengthof(chr.implants); i++) {
		implant_t& implant = chr.implants[i];
		const implantDef_t* def = implant.def;
		/* empty slot */
		if (def == nullptr || def->item == nullptr)
			continue;
		const objDef_t& od = *def->item;
		const itemEffect_t* e = od.strengthenEffect;

		if (implant.installedTime > 0) {
			implant.installedTime--;
			if (implant.installedTime == 0 && e != nullptr && e->isPermanent) {
				CHRSH_UpdateCharacterWithEffect(chr, *e);
			}
		}

		if (implant.removedTime > 0) {
			implant.removedTime--;
			if (implant.removedTime == 0) {
				implant.def = nullptr;
				continue;
			}
		}
		if (e == nullptr || e->period <= 0)
			continue;

		implant.trigger--;
		if (implant.trigger <= 0)
			continue;

		CHRSH_UpdateCharacterWithEffect(chr, *e);
		implant.trigger = e->period;
	}
}

/**
 * @brief Add a new implant to a character
 */
const implant_t* CHRSH_ApplyImplant (character_t& chr, const implantDef_t& def)
{
	const objDef_t& od = *def.item;

	if (!od.implant) {
		Com_Printf("object '%s' is no implant\n", od.id);
		return nullptr;
	}

	const itemEffect_t* e = od.strengthenEffect;
	if (e != nullptr && e->period <= 0 && !e->isPermanent) {
		Com_Printf("object '%s' is not permanent\n", od.id);
		return nullptr;
	}

	for (int i = 0; i < lengthof(chr.implants); i++) {
		implant_t& implant = chr.implants[i];
		/* already filled slot */
		if (implant.def != nullptr)
			continue;

		OBJZERO(implant);
		implant.def = &def;
		if (e != nullptr && !e->isPermanent)
			implant.trigger = e->period;
		implant.installedTime = def.installationTime;

		return &chr.implants[i];
	}
	Com_Printf("no free implant slot\n");
	return nullptr;
}

/**
 * @brief Generates a skill and ability set for any character.
 * @param[in] chr Pointer to the character, for which we generate stats.
 * @param[in] multiplayer If this is true we use the skill values from @c soldier_mp
 * @param[in] templateId Specifies the template to be used for non-MP
 * @note mulitplayer is a special case here
 * @todo Add modifiers for difficulty setting here!
 */
void CHRSH_CharGenAbilitySkills (character_t* chr, bool multiplayer, const char* templateId)
{
	const chrTemplate_t* chrTemplate;
	const teamDef_t* teamDef = chr->teamDef;

	if (multiplayer && teamDef->team == TEAM_PHALANX)
		/* @todo Hard coded template id, remove when possible */
		templateId = "soldier_mp";

	if (!Q_strnull(templateId)) {
		chrTemplate = CHRSH_GetTemplateByID(teamDef, templateId);
		if (!chrTemplate)
			Sys_Error("CHRSH_CharGenAbilitySkills: Character template not found (%s) in %s", templateId, teamDef->id);
	} else if (teamDef->characterTemplates[0]) {
		if (teamDef->numTemplates > 1) {
			float sumRate = 0.0f;
			for (int i = 0; i < teamDef->numTemplates; i++) {
				chrTemplate = teamDef->characterTemplates[i];
				sumRate += chrTemplate->rate;
			}
			if (sumRate > 0.0f) {
				const float soldierRoll = frand();
				float curRate = 0.0f;
				for (chrTemplate = teamDef->characterTemplates[0]; chrTemplate->id; chrTemplate++) {
					curRate += chrTemplate->rate;
					if (curRate && soldierRoll <= (curRate / sumRate))
						break;
				}
			} else {
				/* No rates or all set to 0 default to first template */
				chrTemplate = teamDef->characterTemplates[0];
			}
		} else {
			/* Only one template */
			chrTemplate = teamDef->characterTemplates[0];
		}
	} else {
		Sys_Error("CHRSH_CharGenAbilitySkills: No character template for team %s!", teamDef->id);
	}

	assert(chrTemplate);
	const int (*skillsTemplate)[2] = chrTemplate->skills;

	/* Abilities and skills -- random within the range */
	for (int i = 0; i < SKILL_NUM_TYPES; i++) {
		const int abilityWindow = skillsTemplate[i][1] - skillsTemplate[i][0];
		/* Reminder: In case if abilityWindow==0 the ability will be set to the lower limit. */
		const int temp = (frand() * abilityWindow) + skillsTemplate[i][0];
		chr->score.skills[i] = temp;
		chr->score.initialSkills[i] = temp;
	}

	/* Health. */
	const int abilityWindow = skillsTemplate[SKILL_NUM_TYPES][1] - skillsTemplate[SKILL_NUM_TYPES][0];
	const int temp = (frand() * abilityWindow) + skillsTemplate[SKILL_NUM_TYPES][0];
	chr->score.initialSkills[SKILL_NUM_TYPES] = temp;
	chr->maxHP = temp;
	chr->HP = temp;

	/* Morale */
	chr->morale = GET_MORALE(chr->score.skills[ABILITY_MIND]);
	if (chr->morale >= MAX_SKILL)
		chr->morale = MAX_SKILL;

	/* Initialize the experience values */
	for (int i = 0; i <= SKILL_NUM_TYPES; i++) {
		chr->score.experience[i] = 0;
	}
}

/**
 * @brief Returns the body model for the soldiers for armoured and non armoured soldiers
 * @param[in] chr Pointer to character struct
 * @sa CHRSH_CharGetBody
 * @return the character body model (from a static buffer)
 */
const char* CHRSH_CharGetBody (const character_t* const chr)
{
	static char returnModel[MAX_VAR];

	/* models of UGVs don't change - because they are already armoured */
	if (chr->inv.getArmour() && !CHRSH_IsTeamDefRobot(chr->teamDef)) {
		const objDef_t* od = chr->inv.getArmour()->def();
		const char* id = od->armourPath;
		if (!od->isArmour())
			Sys_Error("CHRSH_CharGetBody: Item is no armour");

		Com_sprintf(returnModel, sizeof(returnModel), "%s%s/%s", chr->path, id, chr->body);
	} else
		Com_sprintf(returnModel, sizeof(returnModel), "%s/%s", chr->path, chr->body);
	return returnModel;
}

/**
 * @brief Returns the head model for the soldiers for armoured and non armoured soldiers
 * @param[in] chr Pointer to character struct
 * @sa CHRSH_CharGetBody
 */
const char* CHRSH_CharGetHead (const character_t* const chr)
{
	static char returnModel[MAX_VAR];

	/* models of UGVs don't change - because they are already armoured */
	if (chr->inv.getArmour() && !chr->teamDef->robot) {
		const objDef_t* od = chr->inv.getArmour()->def();
		const char* id = od->armourPath;
		if (!od->isArmour())
			Sys_Error("CHRSH_CharGetBody: Item is no armour");

		Com_sprintf(returnModel, sizeof(returnModel), "%s%s/%s", chr->path, id, chr->head);
	} else
		Com_sprintf(returnModel, sizeof(returnModel), "%s/%s", chr->path, chr->head);
	return returnModel;
}

BodyData::BodyData (void) :
		_totalBodyArea(0.0f), _numBodyParts(0)
{
}

short BodyData::getRandomBodyPart (void) const
{
	const float rnd = frand() * _totalBodyArea;
	float currentArea = 0.0f;
	short bodyPart;

	for (bodyPart = 0; bodyPart < _numBodyParts; ++bodyPart) {
		currentArea += getArea(bodyPart);
		if (rnd <= currentArea)
			break;
	}
	if (bodyPart >= _numBodyParts) {
		bodyPart = 0;
		Com_DPrintf(DEBUG_SHARED, "Warning: No bodypart hit, defaulting to %s!\n", name(bodyPart));
	}
	return bodyPart;
}

const char* BodyData::id (void) const
{
	return _id;
}

const char* BodyData::id (const short bodyPart) const
{
	return _bodyParts[bodyPart].id;
}

const char* BodyData::name (const short bodyPart) const
{
	return _bodyParts[bodyPart].name;
}

float BodyData::penalty (const short bodyPart, const modifier_types_t type) const
{
	return _bodyParts[bodyPart].penalties[type] * 0.01f;
}

float BodyData::bleedingFactor (const short bodyPart) const
{
	return _bodyParts[bodyPart].bleedingFactor * 0.01f;
}

float BodyData::woundThreshold (const short bodyPart) const
{
	return _bodyParts[bodyPart].woundThreshold * 0.01f;
}

short BodyData::numBodyParts (void) const
{
	return _numBodyParts;
}

void BodyData::setId (const char* id)
{
	Q_strncpyz(_id, id, sizeof(_id));
}

void BodyData::addBodyPart (const BodyPartData& bodyPart)
{
	_bodyParts[_numBodyParts] = bodyPart;
	_totalBodyArea += getArea(_numBodyParts++);
}

short BodyData::getHitBodyPart (const byte direction, const float height) const
{
	const float rnd = frand();
	short bodyPart;
	float curRand = 0;

	for (bodyPart = 0; bodyPart < _numBodyParts; ++bodyPart) {
		vec4_t shape;
		Vector4Copy(_bodyParts[bodyPart].shape, shape);
		if (height <= shape[3] || height > shape[2] + shape[3])
			continue;
		curRand += (direction < 2 ? shape[0] : (direction < 4 ? shape[1] : (shape[0] + shape[1]) * 0.5f));
		if (rnd <= curRand)
			break;
	}
	if (bodyPart >= _numBodyParts) {
		bodyPart = 0;
		Com_DPrintf(DEBUG_SHARED, "Warning: No bodypart hit, defaulting to %s!\n", name(bodyPart));
	}
	return bodyPart;
}

float BodyData::getArea(const short bodyPart) const
{
	return (_bodyParts[bodyPart].shape[0] + _bodyParts[bodyPart].shape[1]) * 0.5f * _bodyParts[bodyPart].shape[2];
}
