/**
 * @file chr_shared.h
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

#ifndef GAME_CHR_SHARED_H
#define GAME_CHR_SHARED_H

typedef enum {
	KILLED_ENEMIES,		/**< Killed enemies */
	KILLED_CIVILIANS,	/**< Civilians, animals */
	KILLED_TEAM,		/**< Friendly fire, own team, partner-teams. */

	KILLED_NUM_TYPES
} killtypes_t;

/** @note Changing order/entries also changes network-transmission and savegames! */
typedef enum {
	ABILITY_POWER,
	ABILITY_SPEED,
	ABILITY_ACCURACY,
	ABILITY_MIND,

	SKILL_CLOSE,
	SKILL_HEAVY,
	SKILL_ASSAULT,
	SKILL_SNIPER,
	SKILL_EXPLOSIVE,
	SKILL_NUM_TYPES
} abilityskills_t;

#define ABILITY_NUM_TYPES SKILL_CLOSE

typedef struct chrTemplate_s {
	char id[MAX_VAR];					/** short name of the template */
	float rate;							/**< rate of this template relative to total */
	int skills[SKILL_NUM_TYPES + 1][2];	/** ability and skill min and max */
} chrTemplate_t;

/**
 * @brief Structure of all stats collected in a mission.
 * @note More general Info: http://ufoai.org/wiki/index.php/Proposals/Attribute_Increase
 * @note Mostly collected in g_client.c and not used anywhere else (at least that's the plan ;)).
 * The result is parsed into chrScoreGlobal_t which is stored in savegames.
 * @note BTAxis about "hit" count:
 * "But yeah, what we want is a counter per skill. This counter should start at 0
 * every battle, and then be increased by 1 everytime:
 * - a direct fire weapon hits (or deals damage, same thing) the actor the weapon
 *   was fired at. If it wasn't fired at an actor, nothing should happen.
 * - a splash weapon deals damage to any enemy actor. If multiple actors are hit,
 *   increase the counter multiple times."
 */
typedef struct chrScoreMission_s {
	/* Movement counts. */
	int movedNormal;
	int movedCrouched;

	/* Kills & stuns */
	/** @todo use existing code */
	int kills[KILLED_NUM_TYPES];	/**< Count of kills (aliens, civilians, teammates) */
	int stuns[KILLED_NUM_TYPES];	/**< Count of stuns (aliens, civilians, teammates) */

	/* Hits/Misses */
	int fired[SKILL_NUM_TYPES];				/**< Count of fired "firemodes" (i.e. the count of how many times the soldier started shooting) */
	int firedTUs[SKILL_NUM_TYPES];				/**< Count of TUs used for the fired "firemodes". (direct hits only)*/
	qboolean firedHit[KILLED_NUM_TYPES];	/** Temporarily used for shot-stats calculations and status-tracking. Not used in stats.*/
	int hits[SKILL_NUM_TYPES][KILLED_NUM_TYPES];	/**< Count of hits (aliens, civilians or, teammates) per skill.
													 * It is a sub-count of "fired".
													 * It's planned to be increased by 1 for each series of shots that dealt _some_ damage. */
	int firedSplash[SKILL_NUM_TYPES];	/**< Count of fired splash "firemodes". */
	int firedSplashTUs[SKILL_NUM_TYPES];				/**< Count of TUs used for the fired "firemodes" (splash damage only). */
	qboolean firedSplashHit[KILLED_NUM_TYPES];	/** Same as firedHit but for Splash damage. */
	int hitsSplash[SKILL_NUM_TYPES][KILLED_NUM_TYPES];	/**< Count of splash hits. */
	int hitsSplashDamage[SKILL_NUM_TYPES][KILLED_NUM_TYPES];	/**< Count of dealt splash damage (aliens, civilians or, teammates).
																 * This is counted in overall damage (healthpoint).*/
	/** @todo Check HEALING of others. */
	int skillKills[SKILL_NUM_TYPES];	/**< Number of kills related to each skill. */

	int heal;	/**< How many hitpoints has this soldier received trough healing in battlescape. */
} chrScoreMission_t;

/**
 * @brief Structure of all stats collected for an actor over time.
 * @note More general Info: http://ufoai.org/wiki/index.php/Proposals/Attribute_Increase
 * @note This information is stored in savegames (in contract to chrScoreMission_t).
 * @note WARNING: if you change something here you'll have to make sure all the network and savegame stuff is updated as well!
 * Additionally you have to check the size of the network-transfer in G_SendCharacterData and GAME_CP_Results
 */
typedef struct chrScoreGlobal_s {
	int experience[SKILL_NUM_TYPES + 1]; /**< Array of experience values for all skills, and health. @todo What are the mins and maxs for these values */

	int skills[SKILL_NUM_TYPES];		/**< Array of skills and abilities. This is the total value. */
	int initialSkills[SKILL_NUM_TYPES + 1];		/**< Array of initial skills and abilities. This is the value generated at character generation time. */

	/* Kills & Stuns */
	int kills[KILLED_NUM_TYPES];	/**< Count of kills (aliens, civilians, teammates) */
	int stuns[KILLED_NUM_TYPES];	/**< Count of stuns(aliens, civilians, teammates) */

	int assignedMissions;		/**< Number of missions this soldier was assigned to. */

	int rank;					/**< Index of rank (in ccs.ranks). */
} chrScoreGlobal_t;

typedef struct chrFiremodeSettings_s {
	actorHands_t hand;	/**< Stores the used hand */
	fireDefIndex_t fmIdx;	/**< Stores the used firemode index. Max. number is MAX_FIREDEFS_PER_WEAPON -1=undef*/
	const objDef_t *weapon;
} chrFiremodeSettings_t;

/**
 * @brief How many TUs (and of what type) did a player reserve for a unit?
 * @sa CL_ActorUsableTUs
 * @sa CL_ActorReservedTUs
 * @sa CL_ActorReserveTUs
 */
typedef struct chrReservations_s {
	/* Reaction fire reservation (for current turn and next enemy turn) */
	int reaction;	/**< Did the player activate RF with a usable firemode?
					 * (And at the same time storing the TU-costs of this firemode) */

	/* Crouch reservation (for current turn)	*/
	int crouch;	/**< Did the player reserve TUs for crouching (or standing up)? Depends exclusively on TU_CROUCH. */

	/* Shot reservation (for current turn) */
	int shot;	/**< If non-zero we reserved a shot in this turn. */
	chrFiremodeSettings_t shotSettings;	/**< Stores what type of firemode & weapon
										 * (and hand) was used for "shot" reservation. */
} chrReservations_t;

typedef enum {
	RES_REACTION,
	RES_CROUCH,
	RES_SHOT,
	RES_ALL,
	RES_ALL_ACTIVE,
	RES_TYPES /**< Max. */
} reservation_types_t;

typedef int32_t actorSizeEnum_t;

/** @brief Types of actor sounds being issued by CL_ActorPlaySound(). */
typedef enum {
	SND_DEATH,	/**< Sound being played on actor death. */
	SND_HURT,	/**< Sound being played when an actor is being hit. */

	SND_MAX
} actorSound_t;

/* team definitions */

#define MAX_UGV					8
#define MAX_TEAMDEFS			64
#define MAX_CHARACTER_TEMPLATES	24
#define MAX_TEMPLATES_PER_TEAM	16

typedef enum {
	NAME_NEUTRAL,
	NAME_FEMALE,
	NAME_MALE,

	NAME_LAST,
	NAME_FEMALE_LAST,
	NAME_MALE_LAST,

	NAME_NUM_TYPES
} nametypes_t;

/**
 * @brief Different races of actors used in game
 * @todo add different robot races.
 */
typedef enum {
	RACE_PHALANX_HUMAN,		/**< Phalanx team */
	RACE_CIVILIAN,			/**< Civilian team */

	RACE_ROBOT,				/**< Robot */

	RACE_TAMAN,				/**< Alien: taman race */
	RACE_ORTNOK,			/**< Alien: ortnok race */
	RACE_BLOODSPIDER,		/**< Alien: bloodspider race */
	RACE_SHEVAAR			/**< Alien: shevaar race */
} racetypes_t;

/** @brief Defines a type of UGV/Robot */
typedef struct ugv_s {
	char *id;
	int idx;
	char weapon[MAX_VAR];
	char armour[MAX_VAR];
	int tu;
	char actors[MAX_VAR];
	int price;
} ugv_t;

typedef struct teamDef_s {
	int idx;			/**< The index in the teamDef array. */
	char id[MAX_VAR];	/**< id from script file. */
	char name[MAX_VAR];	/**< Translatable name. */
	char tech[MAX_VAR];	/**< technology_t id from research.ufo */

	linkedList_t *names[NAME_NUM_TYPES];	/**< Names list per gender. */
	int numNames[NAME_NUM_TYPES];	/**< Amount of names in this list for all different genders. */

	linkedList_t *models[NAME_LAST];	/**< Models list per gender. */
	int numModels[NAME_LAST];	/**< Amount of models in this list for all different genders. */

	linkedList_t *sounds[SND_MAX][NAME_LAST];	/**< Sounds list per gender and per sound type. */
	int numSounds[SND_MAX][NAME_LAST];	/**< Amount of sounds in this list for all different genders and soundtypes. */

	racetypes_t race;	/**< What is the race of this team?*/

	qboolean armour;	/**< Does this team use armour. */
	qboolean weapons;	/**< Does this team use weapons. */
	const struct objDef_s *onlyWeapon;	/**< ods[] index - If this team is not able to use 'normal' weapons, we have to assign a weapon to it
							 * The default value is NONE for every 'normal' actor - but e.g. bloodspiders only have
							 * the ability to melee attack their victims. They get a weapon assigned with several
							 * bloodspider melee attack firedefinitions */

	actorSizeEnum_t size;	/**< What size is this unit on the field (1=1x1 or 2=2x2)? */
	char hitParticle[MAX_VAR]; /**< Particle id of what particle effect should be spawned if a unit of this type is hit. */
	char deathTextureName[MAX_VAR];	/**< texture name for death of any member of this team */

	short resistance[MAX_DAMAGETYPES]; /**< Resistance to damage */

	const chrTemplate_t *characterTemplates[MAX_TEMPLATES_PER_TEAM];
	int numTemplates;
} teamDef_t;

/** @brief Describes a character with all its attributes */
typedef struct character_s {
	int ucn;					/**< unique character number */
	char name[MAX_VAR];			/**< Character name (as in: soldier name). */
	char path[MAX_VAR];
	char body[MAX_VAR];
	char head[MAX_VAR];
	int skin;					/**< Index of skin. */

	int HP;						/**< Health points (current ones). */
	int minHP;					/**< Minimum hp during combat */
	int maxHP;					/**< Maximum health points (as in: 100% == fully healed). */
	int STUN;
	int morale;

	int state;					/**< a character can request some initial states when the team is spawned (like reaction fire) */

	chrScoreGlobal_t score;		/**< Array of scores/stats the soldier/unit collected over time. */
	chrScoreMission_t *scoreMission;		/**< Array of scores/stats the soldier/unit collected in a mission - only used in battlescape (server side). Otherwise it's NULL. */

	actorSizeEnum_t fieldSize;

	inventory_t i;			/**< Inventory definition. */

	const teamDef_t *teamDef;			/**< Pointer to team definition. */
	int gender;				/**< Gender index. */
	chrReservations_t reservedTus;	/** < Stores the reserved TUs for actions. @sa See chrReserveSettings_t for more. */
	chrFiremodeSettings_t RFmode;	/** < Stores the firemode to be used for reaction fire (if the fireDef allows that) See also reaction_firemode_type_t */
} character_t;

/* ================================ */
/*  CHARACTER GENERATING FUNCTIONS  */
/* ================================ */

void CHRSH_CharGenAbilitySkills(character_t * chr, qboolean multiplayer) __attribute__((nonnull));
const char *CHRSH_CharGetBody(const character_t* const chr) __attribute__((nonnull));
const char *CHRSH_CharGetHead(const character_t* const chr) __attribute__((nonnull));
qboolean CHRSH_IsTeamDefAlien(const teamDef_t* const td) __attribute__((nonnull));
qboolean CHRSH_IsTeamDefRobot(const teamDef_t* const td) __attribute__((nonnull));
qboolean CHRSH_IsArmourUseableForTeam(const objDef_t *od, const teamDef_t *teamDef);

#endif
