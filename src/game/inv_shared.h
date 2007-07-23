/**
 * @file inv_shared.h
 * @brief common object-, inventory-, container- and firemode-related functions headers.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

/* this is the absolute max for now */
#define MAX_OBJDEFS     128		/* Remember to adapt the "NONE" define (and similar) if this gets changed. */
#define MAX_WEAPONS_PER_OBJDEF 4
#define MAX_AMMOS_PER_OBJDEF 4
#define MAX_FIREDEFS_PER_WEAPON 8
#define MAX_DAMAGETYPES 32

/* #define GET_FIREDEF(type)   (&csi.ods[type & 0x7F].fd[0][!!(type & 0x80)]) @todo remove me */
/* @todo: might need some changes so the correct weapon (i.e. not 0) is used for the fd */

#define GET_FIREDEFDEBUG(obj_idx,weap_fds_idx,fd_idx) \
	if (obj_idx < 0 || obj_idx >= MAX_OBJDEFS) \
		Sys_Error("GET_FIREDEF: obj_idx out of bounds [%i] (%s: %i)\n", obj_idx, __FILE__, __LINE__); \
	if (weap_fds_idx < 0 || weap_fds_idx >= MAX_WEAPONS_PER_OBJDEF) \
		Sys_Error("GET_FIREDEF: weap_fds_idx out of bounds [%i] (%s: %i)\n", weap_fds_idx, __FILE__, __LINE__); \
	if (fd_idx < 0 || fd_idx >= MAX_FIREDEFS_PER_WEAPON) \
		Sys_Error("GET_FIREDEF: fd_idx out of bounds [%i] (%s: %i)\n", fd_idx, __FILE__, __LINE__);

#define GET_FIREDEF(obj_idx,weap_fds_idx,fd_idx) \
	(&csi.ods[obj_idx & (MAX_OBJDEFS-1)].fd[weap_fds_idx & (MAX_WEAPONS_PER_OBJDEF-1)][fd_idx & (MAX_FIREDEFS_PER_WEAPON-1)])

/** @brief this is a fire definition for our weapons/ammo */
typedef struct fireDef_s {
	char name[MAX_VAR];			/**< script id */
	char projectile[MAX_VAR];	/**< projectile particle */
	char impact[MAX_VAR];		/**< impact particle */
	char hitBody[MAX_VAR];		/**< hit the body particles */
	char fireSound[MAX_VAR];	/**< the sound when a recruits fires */
	char impactSound[MAX_VAR];	/**< the sound that is played on impact */
	char hitBodySound[MAX_VAR];	/**< the sound that is played on hitting a body */
	char bounceSound[MAX_VAR];	/**< bouncing sound */

	/* These values are created in Com_ParseItem and Com_AddObjectLinks.
	 * They are used for self-referencing the firedef. */
	int obj_idx;		/**< The weapon/ammo (csi.ods[obj_idx]) this fd is located in.
						 ** So you can get the containing object by acceessing e.g. csi.ods[obj_idx]. */
	int weap_fds_idx;	/**< The index of the "weapon_mod" entry (objDef_t->fd[weap_fds_idx]) this fd is located in.
						 ** Depending on this value you can find out via objDef_t->weap_idx[weap_fds_idx] what weapon this firemode is used for.
						 ** This does _NOT_ equal the index of the weapon object in ods.
						 */
	int fd_idx;		/**< Self link of the fd in the objDef_t->fd[][fd_idx] array. */

	qboolean soundOnce;
	qboolean gravity;			/**< Does gravity has any influence on this item? */
	qboolean launched;
	qboolean rolled;			/**< Can it be rolled - e.g. grenades */
	qboolean reaction;			/**< This firemode can be used/selected for reaction fire.*/
	int throughWall;		/**< allow the shooting through a wall */
	byte dmgtype;
	float speed;
	vec2_t shotOrg;
	vec2_t spread;
	float modif;
	int delay;
	int bounce;				/**< Is this item bouncing? e.g. grenades */
	float bounceFac;
	float crouch;
	float range;			/**< range of the weapon ammunition */
	int shots;
	int ammo;
	float rof;
	int time;
	vec2_t damage, spldmg;
	float splrad;			/**< splash damage radius */
	int weaponSkill;		/**< What weapon skill is needed to fire this weapon. */
	int irgoggles;			/**< Is this an irgoogle? */
} fireDef_t;

/**
 * @brief The max width and height of an item-shape
 * @note these values depend on the the usage of an uint32_t that has 32 bits and a width of 8bit => 4 rows
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
 * @brief Defines all attributes of obejcts used in the inventory.
 * @todo Document the various (and mostly not obvious) varables here. The documentation in the .ufo file(s) might be a good starting point.
 * @note See also http://ufoai.ninex.info/wiki/index.php/UFO-Scripts/weapon_%2A.ufo
 */
typedef struct objDef_s {
	/* Common */
	char name[MAX_VAR];	/**< item name/scriptfile id */
	char id[MAX_VAR];	/**< short identifier */
	char model[MAX_VAR];	/**< model name - relative to game dir */
	char image[MAX_VAR];	/**< object image file - relative to game dir */
	char type[MAX_VAR];	/**< melee, rifle, ammo, armor */
	char extends_item[MAX_VAR];
	uint32_t shape;			/**< the shape in our inventory */

	byte sx, sy;		/**< Size in x and y direction. */
	float scale;		/**< scale value for images? and models */
	vec3_t center;		/**< origin for models */
	char category;		/**<  The animation index for the character with the weapon.
						 * Don't confuse this with buytype. */
	qboolean weapon;		/**< This item is a weapon or ammo. */
	qboolean holdtwohanded;	/**< The soldier needs both hands to hold this object.
				 		 * Influences model-animations and which hands are blocked int he inventory screen.*/
	qboolean firetwohanded;	/**< The soldier needs both hands to fire this object.
						  * Influences model-animations. */
	qboolean extension;		/**< Boolean: Is an extension. (may not be headgear, too) */
	qboolean headgear;		/**< Boolean: Is a headgear. (may not be extension, too) */
	qboolean thrown;		/**< This item is thrown. */
	int price;		/**< the price for this item */
	int size;		/**< Size of an item, used in storage capacities. */
	int buytype;		/**< In which category of the buy menu is this item listed.
						 * see equipment_buytypes_t */
	qboolean notonmarket;		/**< True if this item should not be available on market. */

	/* Weapon specific */
	int ammo;			/**< How much can we load into this weapon at once. */
	int reload;			/**< Time units (TUs) for reloading the weapon. */
	qboolean oneshot;			/**< This weapon contains its own ammo (it is loaded in the base).
								 ** Don't confuse "oneshot" with "only one shot is possible",
								 ** the number of the "ammo" value above defines how many shots are possible. */
	qboolean deplete;			/**< Is this weapon useless after all ("oneshot") ammo is used up? If true this item will not be collected on mission-end. (see CL_CollectItems) */
	int useable;			/**< Defines which team can use this item: 0 - human, 1 - alien. */
	int ammo_idx[MAX_AMMOS_PER_OBJDEF];			/**< List of ammo-object indices. The index of the ammo in csi.ods[xxx]. */
	int numAmmos;							/**< Number of ammos this weapon can be used with.
											 ** it's <= MAX_AMMOS_PER_OBJDEF) */

	/* Firemodes (per weapon) */
	char weap_id[MAX_WEAPONS_PER_OBJDEF][MAX_VAR];	/**< List of weapon ids as parsed from the ufo file "weapon_mod <id>" */
	int weap_idx[MAX_WEAPONS_PER_OBJDEF];			/**< List of weapon-object indices. The index of the weapon in csi.ods[xxx].
												 ** You can get the correct index for this array from e.g. fireDef_t.weap_fds_idx. or with FIRESH_FiredefsIDXForWeapon. */
	fireDef_t fd[MAX_WEAPONS_PER_OBJDEF][MAX_FIREDEFS_PER_WEAPON];	/**< List of firemodes per weapon (the ammo can be used in). */
	int numFiredefs[MAX_WEAPONS_PER_OBJDEF];	/**< Number of firemodes per weapon. (How many of the firemodes-per-weapons list is used.)
												 ** Maximum value for fireDef_t.fd_idx and it't <= MAX_FIREDEFS_PER_WEAPON */
	int numWeapons;							/**< Number of weapons this ammo can be used in.
											 ** Maximum value for fireDef_t.weap_fds_idx and it's <= MAX_WEAPONS_PER_OBJDEF) */

	/* Technology link */
	void *tech;		/**< Technology link to item to use this extension for (if this is an extension) */
	/** @todo: Can be used for menu highlighting and in ufopedia */
	void *extension_tech;

	/* Armor specific */
	short protection[MAX_DAMAGETYPES];
	short hardness[MAX_DAMAGETYPES];
} objDef_t;

#define MAX_INVDEFS     16

/** @brief inventory definition for our menus */
typedef struct invDef_s {
	char name[MAX_VAR];	/**< id from script files */
	qboolean single, armor, all, temp, extension, headgear;	/**< type of this container or inventory */
	uint32_t shape[SHAPE_BIG_MAX_HEIGHT];	/**< the inventory form/shape */
	int in, out;	/**< TU costs */
} invDef_t;

#define MAX_CONTAINERS  MAX_INVDEFS
#define MAX_INVLIST     1024

/** @brief item definition */
typedef struct item_s {
	int a;	/**< number of ammo rounds left - see NONE_AMMO */
	int m;	/**< unique index of ammo type on csi->ods - see NONE */
	int t;	/**< unique index of weapon in csi.ods array - see NONE */
	int rotated; /**< If the item is curerntly displayed rotated (1) or not (0) */
} item_t;

/** @brief container/inventory list (linked list) with items */
typedef struct invList_s {
	item_t item;	/**< which item */
	int x, y;		/**< position of the item */
	struct invList_s *next;		/**< next entry in this list */
} invList_t;

/** @brief inventory defintion with all its containers */
typedef struct inventory_s {
	invList_t *c[MAX_CONTAINERS];
} inventory_t;

#define MAX_EQUIPDEFS   64

typedef struct equipDef_s {
	char name[MAX_VAR];
	int num[MAX_OBJDEFS];
	byte num_loose[MAX_OBJDEFS];
} equipDef_t;

/**
 * @brief The csi structure is the client-server-information structure
 * which contains all the UFO info needed by the server and the client.
 */
typedef struct csi_s {
	/** Object definitions */
	objDef_t ods[MAX_OBJDEFS];
	int numODs;

	/** Inventory definitions */
	invDef_t ids[MAX_INVDEFS];
	int numIDs;

	/** Special container ids */
	int idRight, idLeft, idExtension;
	int idHeadgear, idBackpack, idBelt, idHolster;
	int idArmor, idFloor, idEquip;

	/** Damage type ids */
	int damNormal, damBlast, damFire, damShock;
	/** Damage type ids */
	int damLaser, damPlasma, damParticle, damStun;

	/** Equipment definitions */
	equipDef_t eds[MAX_EQUIPDEFS];
	int numEDs;

	/** Damage types */
	char dts[MAX_DAMAGETYPES][MAX_VAR];
	int numDTs;
} csi_t;


/** @todo Medals. Still subject to (major) changes. */

#define MAX_SKILL           100

#define GET_HP_HEALING( ab ) (1 + (ab) * 10/MAX_SKILL)
#define GET_HP( ab )        (min((80 + (ab) * 90/MAX_SKILL), 255))
#define GET_ACC( ab, sk )   ((1 - ((float)(ab)/MAX_SKILL + (float)(sk)/MAX_SKILL) / 2)) /**@todo Skill-influence needs some balancing. */
#define GET_TU( ab )        (min((27 + (ab) * 20/MAX_SKILL), 255))
#define GET_MORALE( ab )        (min((100 + (ab) * 150/MAX_SKILL), 255))

typedef enum {
	KILLED_ALIENS,
	KILLED_CIVILIANS,
	KILLED_TEAM,

	KILLED_NUM_TYPES
} killtypes_t;

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


#define MAX_UGV         8
typedef struct ugv_s {
	char id[MAX_VAR];
	char weapon[MAX_VAR];
	char armor[MAX_VAR];
	int size;
	int tu;
} ugv_t;

#define MAX_RANKS           32

/** @brief Describes a rank that a recruit can gain */
typedef struct rank_s {
	char *id;		/**< Index in gd.ranks */
	char name[MAX_VAR];	/**< Rank name (Captain, Squad Leader) */
	char shortname[8];		/**< Rank shortname (Cpt, Sqd Ldr) */
	char *image;		/**< Image to show in menu */
	int type;			/**< employeeType_t */
	int mind;			/**< character mind attribute needed */
	int killed_enemies;		/**< needed amount of enemies killed */
	int killed_others;		/**< needed amount of other actors killed */
} rank_t;

extern rank_t ranks[MAX_RANKS]; /**< Global list of all ranks defined in medals.ufo. */
extern int numRanks;            /**< The number of entries in the list above. */

/** @brief Structure of scores being counted after soldier kill or stun. */
typedef struct chrScore_s {
	int alienskilled;	/**< Killed aliens. */
	int aliensstunned;	/**< Stunned aliens. */
	int civilianskilled;	/**< Killed civilians. @todo: use me. */
	int civiliansstunned;	/**< Stunned civilians. @todo: use me. */
	int teamkilled;		/**< Killed teammates. @todo: use me. */
	int teamstunned;	/**< Stunned teammates. @todo: use me. */
	int closekills;		/**< Aliens killed by CLOSE. */
	int heavykills;		/**< Aliens killed by HEAVY. */
	int assaultkills;	/**< Aliens killed by ASSAULT. */
	int sniperkills;	/**< Aliens killed by SNIPER. */
	int explosivekills;	/**< Aliens killed by EXPLOSIVE. */
	int accuracystat;	/**< Aliens kills or stuns counted to ACCURACY. */
	int powerstat;		/**< Aliens kills or stuns counted to POWER. */
	int survivedmissions;	/**< Missions survived. @todo: use me. */
} chrScore_t;

/** @brief Describes a character with all its attributes */
typedef struct character_s {
	int ucn;
	char name[MAX_VAR];			/**< Character name (as in: soldier name). */
	char path[MAX_VAR];			/**< @todo: document me. */
	char body[MAX_VAR];			/**< @todo: document me. */
	char head[MAX_VAR];			/**< @todo: document me. */
	int skin;				/**< Index of skin. */

	int skills[SKILL_NUM_TYPES];		/**< Array of skills and abilities. */

	int HP;					/**< Health points (current ones). */
	int maxHP;				/**< Maximum health points (as in: 100% == fully healed). */
	int STUN, AP, morale;			/**< @todo: document me. */

	chrScore_t chrscore;			/**< Array of scores. */

	int kills[KILLED_NUM_TYPES];		/**< Array of kills (@todo: integrate me with chrScore_t chrscore). */
	int assigned_missions;			/**< Assigned missions (@todo: integrate me with chrScore_t chrscore). */

	int rank;				/**< Index of rank (in gd.ranks). */

	int fieldSize;				/**< ACTOR_SIZE_* */

	inventory_t *inv;			/**< Inventory definition. */

	int empl_idx;				/**< Backlink to employee-struct - global employee index. */
	int empl_type;				/**< Employee type. */

	qboolean armor;				/**< Able to use armour. */
	qboolean weapons;			/**< Able to use weapons. */

	int teamDesc;				/**< Index in teamDesc array. */
	int category;				/**< nameCategory index in nameCat. */
	int gender;				/**< Gender index. */

/* @todo: remove me, if this is not needed. */
/*  int     destroyed_objects; */
/*  int     hit_ratio; */
/*  int     inflicted_damage; */
/*  int     damage_taken; */
/*  int     crossed_distance; */
	/* date     joined_edc; */
	/* date     died; */
} character_t;

/** @brief The types of employees. */
/** @note If you will change order, make sure personel transfering still works. */
typedef enum {
	EMPL_SOLDIER,
	EMPL_SCIENTIST,
	EMPL_WORKER,
	EMPL_MEDIC,
	EMPL_ROBOT,
	MAX_EMPL					/**< for counting over all available enums */
} employeeType_t;

#define MAX_CAMPAIGNS	16

/* ================================ */
/*  CHARACTER GENERATING FUNCTIONS  */
/* ================================ */

/* These are min and max values for all teams can be defined via campaign script. */
extern int CHRSH_skillValues[MAX_CAMPAIGNS][MAX_TEAMS][MAX_EMPL][2];
extern int CHRSH_abilityValues[MAX_CAMPAIGNS][MAX_TEAMS][MAX_EMPL][2];

void CHRSH_SetGlobalCampaignID(int campaignID);
int Com_StringToTeamNum(const char* teamString) __attribute__((nonnull));
void CHRSH_CharGenAbilitySkills(character_t * chr, int team) __attribute__((nonnull));
char *CHRSH_CharGetBody(character_t* const chr) __attribute__((nonnull));
char *CHRSH_CharGetHead(character_t* const chr) __attribute__((nonnull));

/* ================================ */
/*  INVENTORY MANAGEMENT FUNCTIONS  */
/* ================================ */

void INVSH_InitCSI(csi_t * import) __attribute__((nonnull));
void INVSH_InitInventory(invList_t * invChain) __attribute__((nonnull));
int Com_CheckToInventory(const inventory_t* const i, const int item, const int container, int x, int y);
invList_t *Com_SearchInInventory(const inventory_t* const i, int container, int x, int y) __attribute__((nonnull(1)));
invList_t *Com_AddToInventory(inventory_t* const i, item_t item, int container, int x, int y) __attribute__((nonnull(1)));
qboolean Com_RemoveFromInventory(inventory_t* const i, int container, int x, int y) __attribute__((nonnull(1)));
qboolean Com_RemoveFromInventoryIgnore(inventory_t* const i, int container, int x, int y, qboolean ignore_type) __attribute__((nonnull(1)));
int Com_MoveInInventory(inventory_t* const i, int from, int fx, int fy, int to, int tx, int ty, int *TU, invList_t ** icp) __attribute__((nonnull(1)));
int Com_MoveInInventoryIgnore(inventory_t* const i, int from, int fx, int fy, int to, int tx, int ty, int *TU, invList_t ** icp, qboolean ignore_type) __attribute__((nonnull(1)));
void Com_EmptyContainer(inventory_t* const i, const int container) __attribute__((nonnull(1)));
void Com_DestroyInventory(inventory_t* const i) __attribute__((nonnull(1)));
void Com_FindSpace(const inventory_t* const inv, item_t *item, const int container, int * const px, int * const py) __attribute__((nonnull(1)));
int Com_TryAddToInventory(inventory_t* const inv, item_t item, int container) __attribute__((nonnull(1)));
int Com_TryAddToBuyType(inventory_t* const inv, item_t item, int container) __attribute__((nonnull(1)));
void INVSH_EquipActor(inventory_t* const inv, const int equip[MAX_OBJDEFS], const char *name, character_t* chr) __attribute__((nonnull(1)));
void INV_PrintToConsole(inventory_t* const i);

void INVSH_PrintItemDescription(int i);
int INVSH_GetItemByID(const char *id);
qboolean INVSH_LoadableInWeapon(objDef_t *od, int weapon_idx);

/* =============================== */
/*  FIREMODE MANAGEMENT FUNCTIONS  */
/* =============================== */

int FIRESH_FiredefsIDXForWeapon(objDef_t *od, int weapon_idx);
int FIRESH_GetDefaultReactionFire(objDef_t *ammo, int weapon_fds_idx);

void Com_MergeShapes(uint32_t *shape, uint32_t itemshape, int x, int y);
qboolean Com_CheckShape(const uint32_t *shape, int x, int y);
int Com_ShapeUsage(uint32_t shape);
uint32_t Com_ShapeRotate(uint32_t shape);
#ifdef DEBUG
void Com_ShapePrint(uint32_t shape);
#endif

/** @brief Buytype categories in the various equipment screens (buy/sell, equip, etc...)
 ** Do not mess with the order (especially BUY_AIRCRAFT and BUY_MULTI_AMMO is/will be used for max-check in normal equipment screens)
 ** @sa scripts.c:buytypeNames
 ** @note Be sure to also update all usages of the buy_type" console function (defined in cl_market.c and mostly used there and in menu_buy.ufo) when changing this.
 **/
typedef enum {
	BUY_WEAP_PRI,	/**< All 'Primary' weapons and their ammo for soldiers. */
	BUY_WEAP_SEC,	/**< All 'Secondary' weapons and their ammo for soldiers. */
	BUY_MISC,	/**< Misc sodldier equipment. */
	BUY_ARMOUR,	/**< Armour for soldiers. */
	BUY_MULTI_AMMO, /**< Ammo (and other stuff) that is used in both Pri/Sec weapons. */
	/* MAX_SOLDIER_EQU_BUYTYPES ... possible better solution? */
	BUY_AIRCRAFT,	/**< Aircraft and craft-equipment. */
	BUY_DUMMY,	/**< Everything that is not equipment for soldiers. */
	MAX_BUYTYPES
} equipment_buytypes_t;

#define BUY_PRI(type)	( (((type) == BUY_WEAP_PRI) || ((type) == BUY_MULTI_AMMO)) ) /** < Checks if "type" is displayable/usable in the primary category. */
#define BUY_SEC(type)	( (((type) == BUY_WEAP_SEC) || ((type) == BUY_MULTI_AMMO)) ) /** < Checks if "type" is displayable/usable in the secondary category. */
#define BUYTYPE_MATCH(type1,type2) (\
	(  ((((type1) == BUY_WEAP_PRI) || ((type1) == BUY_WEAP_SEC)) && ((type2) == BUY_MULTI_AMMO)) \
	|| ((((type2) == BUY_WEAP_PRI) || ((type2) == BUY_WEAP_SEC)) && ((type1) == BUY_MULTI_AMMO)) \
	|| ((type1) == (type2)) ) \
	) /**< Check if the 2 buytypes (type1 and type2) are compatible) */

/** @brief Number of bytes that is read and written via inventory transfer functions */
#define INV_INVENTORY_BYTES 7

#endif /* GAME_INV_SHARED_H */


