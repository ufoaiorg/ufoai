/**
 * @file inv_shared.h
 * @brief common object-, inventory-, container- and firemode-related functions headers.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef GAME_INV_SHARED_H
#define GAME_INV_SHARED_H

#include "q_shared.h"
#include "lua/lua.h"
#include "../common/filesys.h"

typedef enum {
	DROPSHIP_FIREBIRD,
	DROPSHIP_HERAKLES,
	DROPSHIP_RAPTOR,

	INTERCEPTOR_STILETTO,
	INTERCEPTOR_SARACEN,
	INTERCEPTOR_DRAGON,
	INTERCEPTOR_STARCHASER,
	INTERCEPTOR_STINGRAY,

	AIRCRAFTTYPE_MAX
} humanAircraftType_t;

/* this is the absolute max for now */
#define MAX_OBJDEFS		128		/* Remember to adapt the "NONE" define (and similar) if this gets changed. */
#define MAX_MAPDEFS		128
#define MAX_UGV			8
#define MAX_WEAPONS_PER_OBJDEF 4
#define MAX_AMMOS_PER_OBJDEF 4
#define MAX_FIREDEFS_PER_WEAPON 8
#define MAX_DAMAGETYPES 64
#define WEAPON_BALANCE 0.5f
#define SKILL_BALANCE 1.0f
#define INJURY_BALANCE 0.2f
#define INJURY_THRESHOLD 0.5f /* HP / maxHP > INJURY_THRESHOLD no penalty is incurred */

typedef int32_t containerIndex_t;

/** @brief Possible inventory actions for moving items between containers */
typedef enum {
	IA_NONE,			/**< no move possible */

	IA_MOVE,			/**< normal inventory item move */
	IA_ARMOUR,			/**< move or swap armour */
	IA_RELOAD,			/**< reload weapon */
	IA_RELOAD_SWAP,		/**< switch loaded ammo */

	IA_NOTIME,			/**< not enough TUs to make this inv move */
	IA_NORELOAD			/**< not loadable or already fully loaded */
} inventory_action_t;

typedef int32_t weaponFireDefIndex_t;
typedef int32_t fireDefIndex_t;

/** @brief this is a fire definition for our weapons/ammo */
typedef struct fireDef_s {
	char name[MAX_VAR];			/**< fire defintion name (translatable) */
	char projectile[MAX_VAR];	/**< projectile particle */
	char impact[MAX_VAR];		/**< impact particle */
	char hitBody[MAX_VAR];		/**< hit the body particles */
	char fireSound[MAX_VAR];	/**< the sound when a recruits fires */
	char impactSound[MAX_VAR];	/**< the sound that is played on impact */
	char hitBodySound[MAX_VAR];	/**< the sound that is played on hitting a body */
	int fireAttenuation;		/**< attenuation of firing (less louder over distance), see S_PlaySample() */
	int impactAttenuation;		/**< attenuation of impact (less louder over distance), see S_PlaySample() */
	char bounceSound[MAX_VAR];	/**< bouncing sound */

	/* These values are created in Com_ParseItem and Com_AddObjectLinks.
	 * They are used for self-referencing the firedef. */
	struct objDef_s *obj;		/**< The weapon/ammo item this fd is located in. */
	weaponFireDefIndex_t weapFdsIdx;	/**< The index of the "weapon_mod" entry (objDef_t->fd[weapFdsIdx]) this fd is located in.
						 ** Depending on this value you can find out via objDef_t->weapIdx[weapFdsIdx] what weapon this firemode is used for.
						 ** This does _NOT_ equal the index of the weapon object in ods.
						 */
	fireDefIndex_t fdIdx;		/**< Self link of the fd in the objDef_t->fd[][fdIdx] array. */

	qboolean soundOnce;		/**< when set, firing sound is played only once, see CL_ActorDoThrow() and CL_ActorShootHidden() */
	qboolean gravity;		/**< Does gravity has any influence on this item? */
	qboolean launched;		/**< used for calculating parabolas in Com_GrenadeTarget() */
	qboolean rolled;		/**< Can it be rolled - e.g. grenades - used in "Roll" firemodes, see Com_GrenadeTarget() */
	qboolean reaction;		/**< This firemode can be used/selected for reaction fire.*/
	int throughWall;		/**< allow the shooting through a wall */
	byte dmgweight;			/**< used in G_Damage() to apply damagetype effects - redundant with obj->dmgtype */
	float speed;			/**< projectile-related; zero value means unlimited speed (most of the cases).
					     for that unlimited speed we use special particle (which cannot work with speed non-zero valued. */
	vec2_t shotOrg;			/**< not set for any firedefinition, but called in CL_TargetingGrenade() and G_GetShotOrigin() */
	vec2_t spread;			/**< used for accuracy calculations (G_ShootGrenade(), G_ShootSingle()) */
	int delay;			/**< applied on grenades and grenade launcher. If no delay is set, a touch with an actor will lead to
						 * an explosion or a hit of the projectile. If a delay is set, the (e.g. grenade) may bounce away again. */
	int bounce;			/**< amount of max possible bounces, for example grenades */
	float bounceFac;		/**< used in G_ShootGrenade() to apply VectorScale() effect */
	float crouch;			/**< used for accuracy calculations (G_ShootGrenade(), G_ShootSingle()) */
	float range;			/**< range of the weapon ammunition, defined per firemode */
	int shots;			/**< how many shots this firemode will produce */
	int ammo;			/**< how many ammo this firemode will use */
	float delayBetweenShots;	/**< delay between shots (sounds and particles) for this firemode;
					     the higher the value, the less the delay (1000/delay) */
	int time;			/**< amount of TU used for this firemode */
	vec2_t damage;			/**< G_Damage(), damage[0] is min value of damage, damage[1] is used for randomized calculations
					     of additional damage; damage[0] < 0 means healing, not applying damage */
	vec2_t spldmg;			/**< G_SplashDamage(), currently we use only first value (spldmg[0]) for apply splashdamage effect */
	float splrad;			/**< splash damage radius */
	int weaponSkill;		/**< What weapon skill is needed to fire this weapon. */
	int irgoggles;			/**< Is this an irgoogle? */
} fireDef_t;

/**
 * @brief The max width and height of an item-shape
 * @note these values depend on the usage of an uint32_t that has 32 bits and a width of 8bit => 4 rows
 * @sa SHAPE_BIG_MAX_HEIGHT
 * @sa SHAPE_BIG_MAX_WIDTH
 * @note This is also used for bit shifting, so please don't change this until
 * you REALLY know what you are doing
 */
#define SHAPE_SMALL_MAX_WIDTH 8
#define SHAPE_SMALL_MAX_HEIGHT 4

/**
 * @brief defines the max height of an inventory container
 * @note the max width is 32 - because uint32_t has 32 bits and we are
 * using a bitmask for the x values
 * @sa SHAPE_SMALL_MAX_WIDTH
 * @sa SHAPE_SMALL_MAX_HEIGHT
 * @note This is also used for bit shifting, so please don't change this until
 * you REALLY know what you are doing
 */
#define SHAPE_BIG_MAX_HEIGHT 16
/** @brief 32 bit mask */
#define SHAPE_BIG_MAX_WIDTH 32

/**
 * @brief All different types of craft items.
 * @note must begin with weapons and end with ammos
 */
typedef enum {
	/* weapons */
	AC_ITEM_BASE_MISSILE,	/**< base weapon */
	AC_ITEM_BASE_LASER,		/**< base weapon */
	AC_ITEM_WEAPON,			/**< aircraft weapon */

	/* misc */
	AC_ITEM_SHIELD,			/**< aircraft shield */
	AC_ITEM_ELECTRONICS,	/**< aircraft electronics */

	/* Crew */
	AC_ITEM_PILOT,

	/* ammos */
	AC_ITEM_AMMO,			/**< aircraft ammos */
	AC_ITEM_AMMO_MISSILE,	/**< base ammos */
	AC_ITEM_AMMO_LASER,		/**< base ammos */

	MAX_ACITEMS
} aircraftItemType_t;

/**
 * @brief Aircraft parameters.
 * @note This is a list of all aircraft parameters that depends on aircraft items.
 * those values doesn't change with shield or weapon assigned to aircraft
 * @note AIR_STATS_WRANGE must be the last parameter (see AII_UpdateAircraftStats)
 */
typedef enum {
	AIR_STATS_SPEED,	/**< Aircraft cruising speed. */
	AIR_STATS_MAXSPEED,	/**< Aircraft max speed. */
	AIR_STATS_SHIELD,	/**< Aircraft shield. */
	AIR_STATS_ECM,		/**< Aircraft electronic warfare level. */
	AIR_STATS_DAMAGE,	/**< Aircraft damage points (= hit points of the aircraft). */
	AIR_STATS_ACCURACY,	/**< Aircraft accuracy - base accuracy (without weapon). */
	AIR_STATS_FUELSIZE,	/**< Aircraft fuel capacity. */
	AIR_STATS_WRANGE,	/**< Aircraft weapon range - the maximum distance aircraft can open fire.
						 * for aircraft, the value is in millidegree (because this is an int) */
	AIR_STATS_ANTIMATTER,	/**< amount of antimatter needed for a full refill. */

	AIR_STATS_MAX,
} aircraftParams_t;

/**
 * @brief Aircraft items.
 * @note This is a part of objDef, only filled for aircraft items (weapons, shield, electronics).
 * @sa objDef_t
 */
typedef struct craftitem_s {
	aircraftItemType_t type;		/**< The type of the aircraft item. */
	float stats[AIR_STATS_MAX];	/**< All coefficient that can affect aircraft->stats */
	float weaponDamage;			/**< The base damage inflicted by an ammo */
	float weaponSpeed;			/**< The speed of the projectile on geoscape */
	float weaponDelay;			/**< The minimum delay between 2 shots */
	int installationTime;		/**< The time needed to install/remove the item on an aircraft */
	qboolean bullets;			/**< create bullets for the projectiles */
	qboolean beam;				/**< create (laser/partivle) beam particles for the projectiles */
	vec4_t beamColor;
} craftitem_t;

/**
 * @brief Defines all attributes of objects used in the inventory.
 * @note See also http://ufoai.ninex.info/wiki/index.php/UFO-Scripts/weapon_%2A.ufo
 */
typedef struct objDef_s {
	/* Common */
	int idx;	/**< Index of the objDef in the global item list (ods). */
	char name[MAX_VAR];		/**< Item name taken from scriptfile. */
	char id[MAX_VAR];		/**< Identifier of the item being item definition in scriptfile. */
	char model[MAX_VAR];		/**< Model name - relative to game dir. */
	char image[MAX_VAR];		/**< Object image file - relative to game dir. */
	char type[MAX_VAR];		/**< melee, rifle, ammo, armour. e.g. used in the ufopedia */
	char armourPath[MAX_VAR];
	char extends_item[MAX_VAR];
	uint32_t shape;			/**< The shape in inventory. */

	byte sx, sy;			/**< Size in x and y direction. */
	float scale;			/**< scale value for images? and models */
	vec3_t center;			/**< origin for models */
	char animationIndex;	/**< The animation index for the character with the weapon. */
	qboolean weapon;		/**< This item is a weapon or ammo. */
	qboolean holdTwoHanded;		/**< The soldier needs both hands to hold this object. */
	qboolean fireTwoHanded;		/**< The soldier needs both hands to fire using object. */
	qboolean extension;		/**< This is an extension. (may not be headgear, too). */
	qboolean headgear;		/**< This is a headgear. (may not be extension, too). */
	qboolean thrown;		/**< This item can be thrown. */

	int price;			/**< Price for this item. */
	int size;			/**< Size of an item, used in storage capacities. */

	qboolean isVirtual;	/**< virtual equipment don't show up in menus, if it's an ammo no item needed for reload */
	/** Item type used to check against buytypes.
	 * @sa type=="armour", type=="ammo"			equals "isAmmo"
	 * @sa obj.craftitem.type == MAX_ACITEMS	equals "isCraftitem" */
	qboolean isPrimary;
	qboolean isSecondary;
	qboolean isHeavy;
	qboolean isMisc;
	qboolean isUGVitem;
	qboolean isDummy;

	qboolean notOnMarket;		/**< True if this item should not be available on market. */

	/* Weapon specific. */
	int ammo;			/**< This value is set for weapon and it says how many bullets currently loaded clip would
					     have, if that clip would be full. In other words, max bullets for this type of
					     weapon with currently loaded type of ammo. */
	int reload;			/**< Time units (TUs) for reloading the weapon. */
	qboolean oneshot;	/**< This weapon contains its own ammo (it is loaded in the base).
						 * "int ammo" of objDef_s defines the amount of ammo used in oneshoot weapons. */
	qboolean deplete;	/**< This weapon useless after all ("oneshot") ammo is used up.
						 * If true this item will not be collected on mission-end. (see INV_CollectinItems). */

	int useable;		/**< Defines which team can use this item: TEAM_*.
						 * Used in checking the right team when filling the containers with available armour. */

	struct objDef_s *ammos[MAX_AMMOS_PER_OBJDEF];		/**< List of ammo-object pointers that can be used in this one. */
	int numAmmos;			/**< Number of ammos this weapon can be used with, which is <= MAX_AMMOS_PER_OBJDEF. */

	/* Firemodes (per weapon). */
	struct objDef_s *weapons[MAX_WEAPONS_PER_OBJDEF];		/**< List of weapon-object pointers where this item can be used in.
															 * Correct index for this array can be get from fireDef_t.weapFdsIdx. or
															 * FIRESH_FiredefForWeapon. */
	fireDef_t fd[MAX_WEAPONS_PER_OBJDEF][MAX_FIREDEFS_PER_WEAPON];	/**< List of firemodes per weapon (the ammo can be used in). */
	fireDefIndex_t numFiredefs[MAX_WEAPONS_PER_OBJDEF];	/**< Number of firemodes per weapon.
												 * Maximum value for fireDef_t.fdIdx <= MAX_FIREDEFS_PER_WEAPON. */
	int numWeapons;		/**< Number of weapons this ammo can be used in.
						 * Maximum value for fireDef_t.weapFdsIdx <= MAX_WEAPONS_PER_OBJDEF. */

	struct technology_s *tech;	/**< Technology link to item. */

	/* Armour specific */
	short protection[MAX_DAMAGETYPES];	/**< Protection values for each armour and every damage type. */
	short ratings[MAX_DAMAGETYPES];		/**< Rating values for each armour and every damage type to display in the menus. */

	/* Aircraft specific */
	byte dmgtype;
	craftitem_t craftitem;
} objDef_t;

/**
 * @brief Return values for INVSH_CheckToInventory.
 * @sa INVSH_CheckToInventory
 */
enum {
	INV_DOES_NOT_FIT		= 0,	/**< Item does not fit. */
	INV_FITS 				= 1,	/**< The item fits without rotation (only) */
	INV_FITS_ONLY_ROTATED	= 2,	/**< The item fits only when rotated (90! to the left) */
	INV_FITS_BOTH			= 3		/**< The item fits either rotated or not. */
};

#define MAX_INVDEFS	16

/** @brief inventory definition for our menus */
typedef struct invDef_s {
	char name[MAX_VAR];	/**< id from script files. */
	containerIndex_t id;				/**< Special container id. See csi_t for the values to compare it with. */
	/** Type of this container or inventory. */
	qboolean single;	/**< Just a single item can be stored in this container. */
	qboolean armour;	/**< Only armour can be stored in this container. */
	qboolean extension;	/**< Only extension items can be stored in this container. */
	qboolean headgear;	/**< Only headgear items can be stored in this container. */
	qboolean all;		/**< Every item type can be stored in this container. */
	qboolean temp;		/**< This is only a pointer to another inventory definitions. */
	uint32_t shape[SHAPE_BIG_MAX_HEIGHT];	/**< The inventory form/shape. */
	int in, out;	/**< parsed: TU costs for moving items in and out. */

	/** Scroll information. @sa inventory_t */
	qboolean scroll;	/**< Set if the container is scrollable */
} invDef_t;

#define MAX_CONTAINERS	MAX_INVDEFS
#define MAX_INVLIST		1024

/**
 * @brief item definition
 * @note m and t are transfered as shorts over the net - a value of NONE means
 * that there is no item - e.g. a value of NONE for m means, that there is no
 * ammo loaded or assigned to this weapon
 */
typedef struct item_s {
	int a;			/**< Number of ammo rounds left - see NONE_AMMO */
	objDef_t *m;	/**< Pointer to ammo type. */
	objDef_t *t;	/**< Pointer to weapon. */
	int amount;		/**< The amount of items of this type on the same x and y location in the container */
	int rotated;	/**< If the item is currently displayed rotated (qtrue or 1) or not (qfalse or 0)
					 * @note don't change this to anything smaller than 4 bytes - the network
					 * parsing functions are expecting this to be at least 4 bytes */
} item_t;

/** @brief container/inventory list (linked list) with items. */
typedef struct invList_s {
	item_t item;	/**< Which item */
	int x, y;		/**< Position (aka origin location) of the item in the container/invlist.
					 * @note ATTENTION Do not use this to get an item by comparing it against a x/y value.
					 * The shape as defined in the item_t may be empty at this location! */
	struct invList_s *next;		/**< Next entry in this list. */
} invList_t;

/** @brief inventory defintion with all its containers */
typedef struct inventory_s {
	invList_t *c[MAX_CONTAINERS];
} inventory_t;

#define MAX_EQUIPDEFS   64

typedef struct equipDef_s {
	char name[MAX_VAR];		/**< Name of the equipment definition */
	int numItems[MAX_OBJDEFS];	/**< Number of item for each item type (see equipment_missions.ufo for more info) */
	byte numItemsLoose[MAX_OBJDEFS];	/**< currently only used for weapon ammo */
	int numAircraft[AIRCRAFTTYPE_MAX];
	int numUGVs[MAX_UGV];
	int minInterest;		/**< Minimum overall interest to use this equipment definition (only for alien) */
	int maxInterest;		/**< Maximum overall interest to use this equipment definition (only for alien) */
} equipDef_t;

/* Maximum number of alien teams per alien group */
#define MAX_TEAMS_PER_MISSION 4
#define MAX_TERRAINS 8
#define MAX_CULTURES 8
#define MAX_POPULATIONS 8

typedef struct mapDef_s {
	/* general */
	char *id;				/**< script file id */
	char *map;				/**< bsp or ump base filename (without extension and day or night char) */
	char *param;			/**< in case of ump file, the assembly to use */
	char *description;		/**< the description to show in the menus */
	char *size;				/**< small, medium, big */

	/* multiplayer */
	qboolean multiplayer;	/**< is this map multiplayer ready at all */
	int teams;				/**< multiplayer teams */
	linkedList_t *gameTypes;	/**< gametype strings this map is useable for */

	/* singleplayer */
	int maxAliens;				/**< Number of spawning points on the map */
	qboolean hurtAliens;		/**< hurt the aliens on spawning them - e.g. for ufocrash missions */

	linkedList_t *terrains;		/**< terrain strings this map is useable for */
	linkedList_t *populations;	/**< population strings this map is useable for */
	linkedList_t *cultures;		/**< culture strings this map is useable for */
	qboolean storyRelated;		/**< Is this a mission story related? */
	int timesAlreadyUsed;		/**< Number of times the map has already been used */
	linkedList_t *ufos;			/**< Type of allowed UFOs on the map */
	linkedList_t *aircraft;		/**< Type of allowed aircraft on the map */

	/** @note Don't end with ; - the trigger commands get the base index as
	 * parameter - see CP_ExecuteMissionTrigger - If you don't need the base index
	 * in your trigger command, you can seperate more commands with ; of course */
	char onwin[MAX_VAR];		/**< trigger command after you've won a battle */
	char onlose[MAX_VAR];		/**< trigger command after you've lost a battle */
} mapDef_t;

typedef struct damageType_s {
	char id[MAX_VAR];
	qboolean showInMenu;	/**< true for values that contains a translatable id */
} damageType_t;

/**
 * @brief The csi structure is the client-server-information structure
 * which contains all the UFO info needed by the server and the client.
 * @sa ccs_t
 * @note Most of this comes from the script files
 */
typedef struct csi_s {
	/** Object definitions */
	objDef_t ods[MAX_OBJDEFS];
	int numODs;

	/** Inventory definitions */
	invDef_t ids[MAX_INVDEFS];
	int numIDs;

	/** Map definitions */
	mapDef_t mds[MAX_MAPDEFS];
	int numMDs;
	mapDef_t *currentMD;	/**< currently selected mapdef */

	/** Special container ids */
	containerIndex_t idRight, idLeft, idExtension;
	containerIndex_t idHeadgear, idBackpack, idBelt, idHolster;
	containerIndex_t idArmour, idFloor, idEquip;

	/** Damage type ids */
	int damNormal, damBlast, damFire;
	int damShock;	/**< Flashbang-type 'damage' (i.e. Blinding). */

	/** Damage type ids */
	int damLaser, damPlasma, damParticle;
	int damStunGas;		/**< Stun gas attack (only effective against organic targets).
						 * @todo Maybe even make a differentiation between aliens/humans here? */
	int damStunElectro;	/**< Electro-Shock attack (effective against organic and robotic targets). */

	/** Equipment definitions */
	equipDef_t eds[MAX_EQUIPDEFS];
	int numEDs;

	/** Damage types */
	damageType_t dts[MAX_DAMAGETYPES];
	int numDTs;

	/** team definitions */
	teamDef_t teamDef[MAX_TEAMDEFS];
	int numTeamDefs;

	/** the current assigned teams for this mission */
	const teamDef_t* alienTeams[MAX_TEAMS_PER_MISSION];
	int numAlienTeams;
} csi_t;

/** @todo remove this and use the container id - not every unit must have two hands */
typedef enum {
	ACTOR_HAND_NOT_SET = 0,
	ACTOR_HAND_RIGHT = 1,
	ACTOR_HAND_LEFT = 2,

	ACTOR_HAND_ENSURE_32BIT = 0x7FFFFFFF
} actorHands_t;

#define ACTOR_GET_INV(actor, hand) (((hand) == ACTOR_HAND_RIGHT) ? RIGHT(actor) : (((hand) == ACTOR_HAND_LEFT) ? LEFT(actor) : NULL))
/** @param[in] hand Hand index (ACTOR_HAND_RIGHT, ACTOR_HAND_LEFT) */
#define ACTOR_SWAP_HAND(hand) ((hand) == ACTOR_HAND_RIGHT ? ACTOR_HAND_LEFT : ACTOR_HAND_RIGHT)

qboolean INV_IsFloorDef(const invDef_t* invDef);
qboolean INV_IsRightDef(const invDef_t* invDef);
qboolean INV_IsLeftDef(const invDef_t* invDef);
qboolean INV_IsEquipDef(const invDef_t* invDef);
qboolean INV_IsArmourDef(const invDef_t* invDef);

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

/**
 * @brief Structure of all stats collected in a mission.
 * @note More general Info: http://ufoai.ninex.info/wiki/index.php/Proposals/Attribute_Increase
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
	int stuns[KILLED_NUM_TYPES];	/**< Count of stuns(aliens, civilians, teammates) */

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
 * @note More general Info: http://ufoai.ninex.info/wiki/index.php/Proposals/Attribute_Increase
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
	/* Reaction fire reservation (for current round and next enemy round) */
	int reaction;	/**< Did the player activate RF with a usable firemode?
					 * (And at the same time storing the TU-costs of this firemode) */

	/* Crouch reservation (for current round)	*/
	int crouch;	/**< Did the player reserve TUs for crouching (or standing up)? Depends exclusively on TU_CROUCH. */

	/* Shot reservation (for current round) */
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

/** @brief Artificial intelligence of a character
 * @todo doesn't belong here  */
typedef struct AI_s {
	char type[MAX_QPATH];	/**< Lua file used by the AI. */
	char subtype[MAX_VAR];	/**< Subtype to be used by AI. */
	lua_State* L;			/**< The lua state used by the AI. */
} AI_t;

/** @brief Describes a character with all its attributes
 * @todo doesn't belong here */
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

	/** @sa memcpy in Grid_CheckForbidden */
	actorSizeEnum_t fieldSize;				/**< @sa ACTOR_SIZE_**** */

	inventory_t i;			/**< Inventory definition. */

	teamDef_t *teamDef;			/**< Pointer to team definition. */
	int gender;				/**< Gender index. */
	chrReservations_t reservedTus;	/** < Stores the reserved TUs for actions. @sa See chrReserveSettings_t for more. */
	chrFiremodeSettings_t RFmode;	/** < Stores the firemode to be used for reaction fire (if the fireDef allows that) See also reaction_firemode_type_t */

	AI_t AI; /**< The character's artificial intelligence */
} character_t;

#define THIS_FIREMODE(fm, HAND, fdIdx)	((fm)->hand == (HAND) && (fm)->fmIdx == (fdIdx))
#define SANE_FIREMODE(fm)	(((fm)->hand > ACTOR_HAND_NOT_SET && (fm)->fmIdx >= 0 && (fm)->fmIdx < MAX_FIREDEFS_PER_WEAPON))

#define INV_IsArmour(od)	(!strcmp((od)->type, "armour"))
#define INV_IsAmmo(od)		(!strcmp((od)->type, "ammo"))

/* ================================ */
/*  CHARACTER GENERATING FUNCTIONS  */
/* ================================ */

void CHRSH_CharGenAbilitySkills(character_t * chr, qboolean multiplayer) __attribute__((nonnull));
const char *CHRSH_CharGetBody(const character_t* const chr) __attribute__((nonnull));
const char *CHRSH_CharGetHead(const character_t* const chr) __attribute__((nonnull));
qboolean CHRSH_IsTeamDefAlien(const teamDef_t* const td) __attribute__((nonnull));

/* ================================ */
/*  INVENTORY MANAGEMENT FUNCTIONS  */
/* ================================ */

void INVSH_InitCSI(csi_t * import) __attribute__((nonnull));
int INVSH_CheckToInventory(const inventory_t* const i, const objDef_t *ob, const invDef_t * container, const int x, const int y, const invList_t *ignoredItem);
qboolean INVSH_CompareItem(item_t *item1, item_t *item2);
void INVSH_GetFirstShapePosition(const invList_t *ic, int* const x, int* const y);
qboolean INVSH_ExistsInInventory(const inventory_t* const inv, const invDef_t * container, item_t item);
invList_t *INVSH_FindInInventory(const inventory_t* const inv, const invDef_t * container, item_t item);
invList_t *INVSH_SearchInInventory(const inventory_t* const i, const invDef_t * container, const int x, const int y) __attribute__((nonnull(1)));
void INVSH_FindSpace(const inventory_t* const inv, const item_t *item, const invDef_t * container, int * const px, int * const py, const invList_t *ignoredItem) __attribute__((nonnull(1)));
qboolean INV_IsCraftItem(const objDef_t *obj);
qboolean INV_IsBaseDefenceItem(const objDef_t *item);

objDef_t *INVSH_GetItemByID(const char *id);
objDef_t *INVSH_GetItemByIDX(int index);
objDef_t *INVSH_GetItemByIDSilent(const char *id);
qboolean INVSH_LoadableInWeapon(const objDef_t *od, const objDef_t *weapon);
qboolean INVSH_UseableForTeam(const objDef_t *od, const int team);

invDef_t *INVSH_GetInventoryDefinitionByID(const char *id);

/* =============================== */
/*  FIREMODE MANAGEMENT FUNCTIONS  */
/* =============================== */

const fireDef_t* FIRESH_GetFiredef(const objDef_t *obj, const weaponFireDefIndex_t weapFdsIdx, const fireDefIndex_t fdIdx);
const fireDef_t *FIRESH_FiredefForWeapon(const item_t *item);
#define FIRESH_IsMedikit(firedef) ((firedef)->damage[0] < 0)
void INVSH_MergeShapes(uint32_t *shape, const uint32_t itemShape, const int x, const int y);
qboolean INVSH_CheckShape(const uint32_t *shape, const int x, const int y);
int INVSH_ShapeSize(const uint32_t shape);
uint32_t INVSH_ShapeRotate(const uint32_t shape);

const objDef_t* INVSH_HasReactionFireEnabledWeapon(const invList_t *invList);

/** @brief Number of bytes that is read and written via inventory transfer functions */
#define INV_INVENTORY_BYTES 11

#endif /* GAME_INV_SHARED_H */
