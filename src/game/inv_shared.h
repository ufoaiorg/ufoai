/**
 * @file inv_shared.h
 * @brief common object-, inventory-, container- and firemode-related functions headers.
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

#ifndef GAME_INV_SHARED_H
#define GAME_INV_SHARED_H

struct csi_s;

/** @todo doesn't belong here, but AIRCRAFTTYPE_MAX is needed for the equipment definitions */
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
#define MAX_WEAPONS_PER_OBJDEF 4
#define MAX_AMMOS_PER_OBJDEF 4
#define MAX_FIREDEFS_PER_WEAPON 8
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
	const char *name;			/**< fire defintion name (translatable) */
	const char *projectile;	/**< projectile particle */
	const char *impact;		/**< impact particle */
	const char *impactSound;	/**< the sound that is played on impact */
	const char *hitBody;		/**< hit the body particles */
	const char *hitBodySound;	/**< the sound that is played on hitting a body */
	const char *fireSound;	/**< the sound when a recruits fires */
	const char *bounceSound;	/**< bouncing sound */

	float fireAttenuation;		/**< attenuation of firing (less louder over distance), see S_PlaySample() */
	float impactAttenuation;		/**< attenuation of impact (less louder over distance), see S_PlaySample() */

	/* These values are created in Com_ParseItem and Com_AddObjectLinks.
	 * They are used for self-referencing the firedef. */
	const struct objDef_s *obj;		/**< The weapon/ammo item this fd is located in. */
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
	vec2_t shotOrg;			/**< This can shift a muzzle vertically (first value) and horizontally (second value) for the trace that is done on the server side. */
	vec2_t spread;			/**< used for accuracy calculations (G_ShootGrenade(), G_ShootSingle()) */
	int delay;			/**< applied on grenades and grenade launcher. If no delay is set, a touch with an actor will lead to
						 * an explosion or a hit of the projectile. If a delay is set, the (e.g. grenade) may bounce away again. */
	int bounce;			/**< amount of max possible bounces, for example grenades */
	float bounceFac;		/**< used in G_ShootGrenade() to apply VectorScale() effect */
	float crouch;			/**< used for accuracy calculations - must be greater than 0.0 */
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
	int irgoggles;			/**< Is this an irgoggle? */
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
 * @todo move into campaign only structure
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
 * @todo move into campaign only structure
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

	AIR_STATS_MAX
} aircraftParams_t;

/**
 * @brief Aircraft items.
 * @note This is a part of objDef, only filled for aircraft items (weapons, shield, electronics).
 * @sa objDef_t
 * @todo move into campaign only structure
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

#define MAX_DAMAGETYPES 64

/**
 * @brief Defines all attributes of objects used in the inventory.
 * @note See also http://ufoai.org/wiki/index.php/UFO-Scripts/weapon_%2A.ufo
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
	uint32_t shape;			/**< The shape in inventory. */

	float scale;			/**< scale value for images? and models */
	vec3_t center;			/**< origin for models */
	qboolean weapon;		/**< This item is a weapon or ammo. */
	qboolean holdTwoHanded;		/**< The soldier needs both hands to hold this object. */
	qboolean fireTwoHanded;		/**< The soldier needs both hands to fire using object. */
	qboolean extension;		/**< This is an extension. (may not be headgear, too). */
	qboolean headgear;		/**< This is a headgear. (may not be extension, too). */
	qboolean thrown;		/**< This item can be thrown. */
	qboolean isVirtual;	/**< virtual equipment don't show up in menus, if it's an ammo no item needed for reload */
	qboolean isPrimary;
	qboolean isSecondary;
	qboolean isHeavy;
	qboolean isMisc;
	qboolean isUGVitem;
	qboolean isDummy;

	/* Weapon specific. */
	int ammo;			/**< This value is set for weapon and it says how many bullets currently loaded clip would
					     have, if that clip would be full. In other words, max bullets for this type of
					     weapon with currently loaded type of ammo. */
	int reload;			/**< Time units (TUs) for reloading the weapon. */
	char reloadSound[MAX_VAR];	/**< Sound played when weapon is reloaded */
	float reloadAttenuation;		/**< Attenuation of reload sound - less louder over distance - */
	qboolean oneshot;	/**< This weapon contains its own ammo (it is loaded in the base).
						 * "int ammo" of objDef_s defines the amount of ammo used in oneshoot weapons. */
	qboolean deplete;	/**< This weapon useless after all ("oneshot") ammo is used up.
						 * If true this item will not be collected on mission-end. (see INV_CollectinItems). */

	int useable;		/**< Defines which team can use this item: TEAM_*.
						 * Used in checking the right team when filling the containers with available armour. */

	const struct objDef_s *ammos[MAX_AMMOS_PER_OBJDEF];		/**< List of ammo-object pointers that can be used in this one. */
	int numAmmos;			/**< Number of ammos this weapon can be used with, which is <= MAX_AMMOS_PER_OBJDEF. */

	/* Firemodes (per weapon). */
	const struct objDef_s *weapons[MAX_WEAPONS_PER_OBJDEF];		/**< List of weapon-object pointers where this item can be used in.
															 * Correct index for this array can be get from fireDef_t.weapFdsIdx. or
															 * FIRESH_FiredefForWeapon. */
	fireDef_t fd[MAX_WEAPONS_PER_OBJDEF][MAX_FIREDEFS_PER_WEAPON];	/**< List of firemodes per weapon (the ammo can be used in). */
	fireDefIndex_t numFiredefs[MAX_WEAPONS_PER_OBJDEF];	/**< Number of firemodes per weapon.
												 * Maximum value for fireDef_t.fdIdx <= MAX_FIREDEFS_PER_WEAPON. */
	int numWeapons;		/**< Number of weapons this ammo can be used in.
						 * Maximum value for fireDef_t.weapFdsIdx <= MAX_WEAPONS_PER_OBJDEF. */


	/* Armour specific */
	short protection[MAX_DAMAGETYPES];	/**< Protection values for each armour and every damage type. */
	short ratings[MAX_DAMAGETYPES];		/**< Rating values for each armour and every damage type to display in the menus. */

	byte dmgtype;
	byte sx, sy;			/**< Size in x and y direction. */
	char animationIndex;	/**< The animation index for the character with the weapon. */

	/**** @todo move into campaign only structure ****/
	/* Aircraft specific */
	craftitem_t craftitem;
	int price;			/**< Price for this item. */
	int productionCost;	/**< Production costs for this item. */
	int size;			/**< Size of an item, used in storage capacities. */
	int weight;			/**< The weight of the item */
	qboolean notOnMarket;		/**< True if this item should not be available on market. */
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

/**
 * @brief item definition
 * @note m and t are transfered as shorts over the net - a value of NONE means
 * that there is no item - e.g. a value of NONE for m means, that there is no
 * ammo loaded or assigned to this weapon
 */
typedef struct item_s {
	int a;			/**< Number of ammo rounds left - see NONE_AMMO */
	const objDef_t *m;	/**< Pointer to ammo type. */
	const objDef_t *t;	/**< Pointer to weapon. */
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

/** @brief inventory definition with all its containers */
typedef struct inventory_s {
	invList_t *c[MAX_CONTAINERS];
} inventory_t;

#define MAX_EQUIPDEFS   64

typedef struct equipDef_s {
	char id[MAX_VAR];		/**< script id of the equipment definition */
	const char *name;		/**< translatable name of the equipment definition */
	int numItems[MAX_OBJDEFS];	/**< Number of item for each item type (see equipment_missions.ufo for more info) */
	byte numItemsLoose[MAX_OBJDEFS];	/**< currently only used for weapon ammo */
	int numAircraft[AIRCRAFTTYPE_MAX];
	int minInterest;		/**< Minimum overall interest to use this equipment definition (only for alien) */
	int maxInterest;		/**< Maximum overall interest to use this equipment definition (only for alien) */
} equipDef_t;

/* Maximum number of alien teams per alien group */
#define MAX_TEAMS_PER_MISSION 4
#define MAX_TERRAINS 8
#define MAX_CULTURES 8
#define MAX_POPULATIONS 8

typedef struct damageType_s {
	char id[MAX_VAR];
	qboolean showInMenu;	/**< true for values that contains a translatable id */
} damageType_t;

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

/* ================================ */
/*  INVENTORY MANAGEMENT FUNCTIONS  */
/* ================================ */

invList_t* INVSH_HasArmour(const inventory_t *inv);
void INVSH_InitCSI(const struct csi_s * import) __attribute__((nonnull));
int INVSH_CheckToInventory(const inventory_t* const i, const objDef_t *ob, const invDef_t * container, const int x, const int y, const invList_t *ignoredItem);
qboolean INVSH_CompareItem(const item_t *const item1, const item_t *const item2);
void INVSH_GetFirstShapePosition(const invList_t *ic, int* const x, int* const y);
qboolean INVSH_ExistsInInventory(const inventory_t* const inv, const invDef_t * container, const item_t * item);
invList_t *INVSH_FindInInventory(const inventory_t* const inv, const invDef_t * container, const item_t * const );
invList_t *INVSH_SearchInInventory(const inventory_t* const i, const invDef_t * container, const int x, const int y) __attribute__((nonnull(1)));
invList_t *INVSH_SearchInInventoryByItem(const inventory_t* const i, const invDef_t * container, const objDef_t *item);
void INVSH_FindSpace(const inventory_t* const inv, const item_t *item, const invDef_t * container, int * const px, int * const py, const invList_t *ignoredItem) __attribute__((nonnull(1)));
qboolean INV_IsCraftItem(const objDef_t *obj);
qboolean INV_IsBaseDefenceItem(const objDef_t *item);

const objDef_t *INVSH_GetItemByID(const char *id);
const objDef_t *INVSH_GetItemByIDX(int index);
const objDef_t *INVSH_GetItemByIDSilent(const char *id);
qboolean INVSH_LoadableInWeapon(const objDef_t *od, const objDef_t *weapon);

const invDef_t *INVSH_GetInventoryDefinitionByID(const char *id);

#define THIS_FIREMODE(fm, HAND, fdIdx)	((fm)->hand == (HAND) && (fm)->fmIdx == (fdIdx))
#define SANE_FIREMODE(fm)	(((fm)->hand > ACTOR_HAND_NOT_SET && (fm)->fmIdx >= 0 && (fm)->fmIdx < MAX_FIREDEFS_PER_WEAPON && (fm)->weapon != NULL))

#define INV_IsArmour(od)	(Q_streq((od)->type, "armour"))
#define INV_IsAmmo(od)		(Q_streq((od)->type, "ammo"))

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

#endif /* GAME_INV_SHARED_H */
