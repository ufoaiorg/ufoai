/**
 * @file
 * @brief common object-, inventory-, container- and firemode-related functions headers.
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

#pragma once

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
#define MAX_OBJDEFS		130
#define MAX_IMPLANTS	16
#define MAX_MAPDEFS		256
#define MAX_WEAPONS_PER_OBJDEF 4
#define MAX_AMMOS_PER_OBJDEF 4
#define MAX_FIREDEFS_PER_WEAPON 8
#define WEAPON_BALANCE 0.5f
#define SKILL_BALANCE 1.0f

typedef int32_t containerIndex_t;
#define CID_RIGHT		0
#define CID_LEFT		1
#define CID_IMPLANT		2
#define CID_HEADGEAR	3
#define CID_BACKPACK	4
#define CID_BELT		5
#define CID_HOLSTER		6
#define CID_ARMOUR		7
#define CID_FLOOR		8
#define CID_EQUIP		9
#define CID_MAX			10

inline bool isValidContId(const containerIndex_t id)
{
	return (id >= 0 && id < CID_MAX);
}

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

typedef enum {
	EFFECT_ACTIVE,
	EFFECT_INACTIVE,
	EFFECT_OVERDOSE,
	EFFECT_STRENGTHEN,
	EFFECT_MAX
} effectStages_t;

typedef struct itemEffect_s {
	bool isPermanent;	/**< @c true if this effect does not expire */
	int duration;		/**< the turns that the effect is active */
	int period;			/**< Frequency to add attribute. This is for effects that are handled outside of the battlescape. */

	/* all in percent */
	float accuracy;
	float TUs;
	float power;
	float mind;
	float morale;
} itemEffect_t;

typedef struct implantDef_s {
	const char* id;
	int idx;
	const struct objDef_s* item;	/**< the assigned implant effect */
	int installationTime;	/**< the time that is needed in order to install the implant */
	int removeTime;			/**< the time that is needed in order to remove the implant */
} implantDef_t;

/** @brief this is a fire definition for our weapons/ammo */
typedef struct fireDef_s {
	const char* name;			/**< fire definition name (translatable) */
	const char* projectile;		/**< projectile particle */
	const char* impact;			/**< impact particle */
	const char* impactSound;	/**< the sound that is played on impact */
	const char* hitBody;		/**< hit the body particles */
	const char* hitBodySound;	/**< the sound that is played on hitting a body */
	const char* fireSound;		/**< the sound when a recruits fires */
	const char* bounceSound;	/**< bouncing sound */

	float fireAttenuation;		/**< attenuation of firing (less louder over distance), see S_PlaySample() */
	float impactAttenuation;	/**< attenuation of impact (less louder over distance), see S_PlaySample() */

	/* These values are created in Com_ParseItem and Com_AddObjectLinks.
	 * They are used for self-referencing the firedef. */
	const struct objDef_s* obj;		/**< The weapon/ammo item this fd is located in. */
	weaponFireDefIndex_t weapFdsIdx;	/**< The index of the "weapon_mod" entry (objDef_t->fd[weapFdsIdx]) this fd is located in.
						 ** Depending on this value you can find out via objDef_t->weapIdx[weapFdsIdx] what weapon this firemode is used for.
						 ** This does _NOT_ equal the index of the weapon object in ods.
						 */
	fireDefIndex_t fdIdx;		/**< Self link of the fd in the objDef_t->fd[][fdIdx] array. */
	itemEffect_t* activeEffect;
	itemEffect_t* deactiveEffect;
	itemEffect_t* overdoseEffect;

	bool soundOnce;		/**< when set, firing sound is played only once, see CL_ActorDoThrow() and CL_ActorShootHidden() */
	bool gravity;		/**< Does gravity have any influence on this item? */
	bool launched;		/**< used for calculating parabolas in Com_GrenadeTarget() */
	bool rolled;		/**< Can it be rolled - e.g. grenades - used in "Roll" firemodes, see Com_GrenadeTarget() */
	bool reaction;		/**< This firemode can be used/selected for reaction fire.*/
	bool irgoggles;		/**< Is this an irgoggle? */
	byte dmgweight;		/**< used in G_Damage() to apply damagetype effects - redundant with obj->dmgtype */
	int throughWall;	/**< allow the shooting through a wall */
	float speed;		/**< projectile-related; zero value means unlimited speed (most of the cases).
							 for that unlimited speed we use special particle (which cannot work with speed non-zero valued. */
	vec2_t shotOrg;		/**< This can shift a muzzle vertically (first value) and horizontally (second value) for the trace that is done on the server side. */
	vec2_t spread;		/**< used for accuracy calculations (G_ShootGrenade(), G_ShootSingle()) */
	int delay;			/**< applied on grenades and grenade launcher. If no delay is set, a touch with an actor will lead to
						 * an explosion or a hit of the projectile. If a delay is set, the (e.g. grenade) may bounce away again. */
	int bounce;			/**< amount of max possible bounces, for example grenades */
	float bounceFac;	/**< used in G_ShootGrenade() to apply VectorScale() effect */
	float crouch;		/**< used for accuracy calculations - must be greater than 0.0 */
	float range;		/**< range of the weapon ammunition, defined per firemode */
	int shots;			/**< how many shots this firemode will produce */
	int ammo;			/**< how many ammo this firemode will use */
	float delayBetweenShots;	/**< delay between shots (sounds and particles) for this firemode;
						 the higher the value, the less the delay (1000/delay) */
	int time;			/**< amount of TU used for this firemode */
	vec2_t damage;		/**< G_Damage(), damage[0] is min value of damage, damage[1] is used for randomized calculations
						 of additional damage; damage[0] < 0 means healing, not applying damage */
	vec2_t spldmg;		/**< G_SplashDamage(), currently we use only first value (spldmg[0]) for apply splashdamage effect */
	float splrad;		/**< splash damage radius */
	int weaponSkill;	/**< What weapon skill is needed to fire this weapon. */
	int rounds;			/**< e.g. for incendiary grenades */

	void getShotOrigin (const vec3_t from, const vec3_t dir, bool crouching, vec3_t shotOrigin) const;
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
	aircraftItemType_t type;	/**< The type of the aircraft item. */
	float stats[AIR_STATS_MAX];	/**< All coefficient that can affect aircraft->stats */
	float weaponDamage;			/**< The base damage inflicted by an ammo */
	float weaponSpeed;			/**< The speed of the projectile on geoscape */
	float weaponDelay;			/**< The minimum delay between 2 shots */
	int installationTime;		/**< The time needed to install/remove the item on an aircraft */
	bool bullets;				/**< create bullets for the projectiles */
	bool beam;					/**< create (laser/particle) beam particles for the projectiles */
	vec4_t beamColor;
} craftItem;

#define MAX_DAMAGETYPES 64

/**
 * @brief Defines all attributes of objects used in the inventory.
 * @note See also http://ufoai.org/wiki/index.php/UFO-Scripts/weapon_%2A.ufo
 */
typedef struct objDef_s {
	/* Common */
	int idx;				/**< Index of the objDef in the global item list (ods). */
	const char* name;		/**< Item name taken from scriptfile. */
	const char* id;			/**< Identifier of the item being item definition in scriptfile. */
	const char* model;		/**< Model name - relative to game dir. */
	const char* image;		/**< Object image file - relative to game dir. */
	const char* type;		/**< melee, rifle, ammo, armour. e.g. used in the ufopedia */
	const char* armourPath;
	uint32_t shape;			/**< The shape in inventory. */

	float scale;			/**< scale value for images? and models */
	vec3_t center;			/**< origin for models */
	bool weapon;			/**< This item is a weapon or ammo. */
	bool holdTwoHanded;		/**< The soldier needs both hands to hold this object. */
	bool fireTwoHanded;		/**< The soldier needs both hands to fire using object. */
	bool headgear;			/**< This is a headgear. */
	bool thrown;			/**< This item can be thrown. */
	bool implant;			/**< This item can be implanted */
	itemEffect_t* strengthenEffect;
	bool isVirtual;			/**< virtual equipment don't show up in menus, if it's an ammo no item needed for reload */
	bool isPrimary;
	bool isSecondary;
	bool isHeavy;
	bool isMisc;
	bool isUGVitem;
	bool isDummy;

	/* Weapon specific. */
	int ammo;			/**< This value is set for weapon and it says how many bullets currently loaded clip would
					     have, if that clip would be full. In other words, max bullets for this type of
					     weapon with currently loaded type of ammo. */
	int _reload;			/**< Time units (TUs) for reloading the weapon. */
	const char* reloadSound;	/**< Sound played when weapon is reloaded */
	float reloadAttenuation;	/**< Attenuation of reload sound - less louder over distance - */
	bool oneshot;		/**< This weapon contains its own ammo (it is loaded in the base).
						 * "int ammo" of objDef_s defines the amount of ammo used in oneshoot weapons. */
	bool deplete;		/**< This weapon is useless after all ("oneshot") ammo is used up.
						 * If true this item will not be collected on mission-end. (see INV_CollectinItems). */

	int useable;		/**< Defines which team can use this item: TEAM_*.
						 * Used in checking the right team when filling the containers with available armour. */

	const struct objDef_s* ammos[MAX_AMMOS_PER_OBJDEF];		/**< List of ammo-object pointers that can be used in this one. */
	int numAmmos;		/**< Number of ammos this weapon can be used with, which is <= MAX_AMMOS_PER_OBJDEF. */

	/* Firemodes (per weapon). */
	const struct objDef_s* weapons[MAX_WEAPONS_PER_OBJDEF];	/**< List of weapon-object pointers where this item can be used in.
															 * Correct index for this array can be get from fireDef_t.weapFdsIdx. or
															 * getFiredefs(). */
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
	craftItem craftitem;
	int price;				/**< Price for this item. */
	int productionCost;		/**< Production costs for this item. */
	int size;				/**< Size of an item, used in storage capacities. */
	int weight;			/**< The weight of the item */
	bool notOnMarket;		/**< True if this item should not be available on market. */


	uint32_t getShapeRotated () const;
	bool isCraftItem () const;
	bool isBaseDefenceItem () const;
	bool isLoadableInWeapon (const objDef_s* weapon) const;
	inline bool isAmmo () const {
		return Q_streq(this->type, "ammo");
	}
	inline bool isArmour ()	const {
		return Q_streq(this->type, "armour");
	}
	inline int getReloadTime () const {
		return _reload;
	}
	inline bool isReloadable () const {
		return getReloadTime() > 0;
	}

} objDef_t;

/**
 * @brief Return values for canHoldItem.
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
	bool single;	/**< Just a single item can be stored in this container. */
	bool armour;	/**< Only armour can be stored in this container. */
	bool implant;	/**< Only implants can be stored in this container. */
	bool headgear;	/**< Only headgear items can be stored in this container. */
	bool all;		/**< Every item type can be stored in this container. */
	bool unique;	/**< Does not allow to put the same item more than once into the container */
	bool temp;		/**< This is only a pointer to another inventory definitions. */
	/** Scroll information. @sa Inventory */
	bool scroll;	/**< Set if the container is scrollable */
	uint32_t shape[SHAPE_BIG_MAX_HEIGHT];	/**< The inventory form/shape. */
	int in, out;	/**< parsed: TU costs for moving items in and out. */

	bool isArmourDef () const;
	bool isFloorDef () const;
	bool isRightDef () const;
	bool isLeftDef () const;
	bool isEquipDef () const;
} invDef_t;

#define MAX_CONTAINERS	MAX_INVDEFS

/**
 * @brief item instance data, with linked list capability
 * @note the item and ammo indices are transfered as shorts over the net - a value of NONE means
 * that there is no item - e.g. a value of NONE for the ammo means, that there is no
 * ammo loaded or assigned to this weapon
 */
class Item {
	const objDef_t* _itemDef;	/**< The weapon definition. */
	const objDef_t* _ammoDef;	/**< Pointer to ammo definition. */
	Item* _next;				/**< Next entry in this list. */
	int _x, _y;					/**< Position (aka origin location) of the item in the container/invlist.
								 * @note ATTENTION Do not use this to get an item by comparing it against a x/y value.
								 * The shape as defined in the Item may be empty at this location! */
	int _amount;				/**< The amount of items of this type on the same x and y location in the container */
	int _ammoLeft;				/**< Number of ammo rounds left - see NONE_AMMO */
public:
	int rotated;	/**< If the item is currently displayed rotated (true or 1) or not (false or 0)
					 * @note don't change this to anything smaller than 4 bytes - the network
					 * parsing functions are expecting this to be at least 4 bytes */


	/*==================
	 *		ctors
	 *==================*/
	Item ();
	Item (const objDef_t* _itemDef, const objDef_t* ammo = nullptr, int ammoLeft = 0);

	/*==================
	 *		setters
	 *==================*/
	inline void setNext (Item* nx) {
		_next = nx;
	}
	inline void setX (const int val) {
		_x = val;
	}
	inline void setY (const int val) {
		_y = val;
	}
	inline void setAmmoDef (const objDef_t* od) {
		_ammoDef = od;
	}
	inline void setAmount (int value) {
		_amount = value;
	}
	inline void setAmmoLeft (int value) {
		_ammoLeft = value;
	}
	inline void setDef(const objDef_t* objDef) {
		_itemDef = objDef;
	}

	/*==================
	 *		getters
	 *==================*/
	inline Item* getNext () const {
		return _next;
	}
	inline int getX () const {
		return _x;
	}
	inline int getY () const {
		return _y;
	}
	inline const objDef_t* ammoDef (void) const {
		return _ammoDef;
	}
	inline int getAmount () const {
		return _amount;
	}
	inline int getAmmoLeft () const {
		return _ammoLeft;
	}
	inline const objDef_t* def (void) const {
		return _itemDef;
	}

	/*==================
	 *		checkers
	 *==================*/
	inline bool isHeldTwoHanded() const {
		return _itemDef->holdTwoHanded;
	}
	inline bool isReloadable() const {
		return _itemDef->getReloadTime() > 0;
	}
	/** @note !mustReload() is *not* equivalent to 'usable' e.g. medikits are not reloadable, but they do have ammo */
	inline bool mustReload() const {
		return isReloadable() && getAmmoLeft() <= 0;
	}
	inline bool isWeapon() const {
		return _itemDef->weapon;
	}
	inline bool isArmour() const {
		return _itemDef->isArmour();
	}
	bool isSameAs (const Item* const other) const;

	/*==================
	 *	manipulators
	 *==================*/
	inline void addAmount (int value) {
		_amount += value;
	}


	int getWeight() const;
	void getFirstShapePosition(int* const x, int* const y) const;
	const objDef_t* getReactionFireWeaponType() const;
	const fireDef_t* getFiredefs() const;
	int getNumFiredefs() const;
	const fireDef_t* getSlowestFireDef() const;
	const fireDef_t* getFastestFireDef() const;
};

class Container
{
	const invDef_t* _def;	/* container attributes (invDef_t) */
public:
	int id;
	Item* _invList;	/* start of the list of items */

	Container();
	const invDef_t* def () const;
	Item* getNextItem (const Item* prev) const;
	int countItems () const;
};

/** @brief inventory definition with all its containers */
class Inventory {
protected:
	Container _containers[MAX_CONTAINERS];

	/** @todo: convert into iterator */
	const Container* _getNextCont (const Container* prev) const;

public:
	Inventory ();
	virtual ~Inventory() {}
	void init ();

	inline const Container& getContainer (const containerIndex_t idx) const {
		return _containers[idx];
	}

	/**
	 * @note temporary naming while migrating !!
	 * getContainer2 will return an item, while
	 * @todo this should return a reference - can't be null
	 */
	inline Item* getContainer2 (const containerIndex_t idx) const {
		return getContainer(idx)._invList;
	}

	inline void setContainer (const containerIndex_t idx, Item* cont) {
		_containers[idx]._invList = cont;
	}

	inline void resetContainer (const containerIndex_t idx) {
		_containers[idx]._invList = nullptr;
	}
	inline void resetTempContainers () {
		const Container* cont = nullptr;
		while ((cont = getNextCont(cont, true))) {
			/* CID_FLOOR and CID_EQUIP are temp */
			if (cont->def()->temp)
				resetContainer(cont->id);
		}
	}

	/**
	 * @brief Searches if there is a specific item already in the inventory&container.
	 * @param[in] contId Container in the inventory.
	 * @param[in] item The item to search for.
	 * @return true if there already is at least one item of this type, otherwise false.
	 */
	inline bool containsItem (const containerIndex_t contId, const Item* const item) const {
		return findInContainer(contId, item) ? true : false;
	}

	Item* getArmour () const;
	Item* getHeadgear() const;
	/** @todo this should return a reference - can't be null */
	Item* getRightHandContainer() const;
	/** @todo this should return a reference - can't be null */
	Item* getLeftHandContainer () const;
	/** @todo this should return a reference - can't be null */
	Item* getHolsterContainer() const;
	/** @todo this should return a reference - can't be null */
	Item* getEquipContainer () const;
	Item* getImplantContainer () const;
	/** @todo this should return a reference - can't be null */
	Item* getFloorContainer() const;
	void setFloorContainer(Item* cont);

	void findSpace (const invDef_t* container, const Item* item, int* const px, int* const py, const Item* ignoredItem) const;
	Item* findInContainer (const containerIndex_t contId, const Item*  const item) const;
	Item* getItemAtPos (const invDef_t* container, const int x, const int y) const;
	int getWeight () const;
	int canHoldItem (const invDef_t* container, const objDef_t* od, const int x, const int y, const Item* ignoredItem) const;
	bool canHoldItemWeight (containerIndex_t from, containerIndex_t to, const Item& item, int maxWeight) const;
	bool holdsReactionFireWeapon () const;
	/** @todo convert into iterator */
	const Container* getNextCont (const Container* prev, bool inclTemp = false) const;
	int countItems () const;
};

#define MAX_EQUIPDEFS   64

typedef struct equipDef_s {
	char id[MAX_VAR];			/**< script id of the equipment definition */
	const char* name;			/**< translatable name of the equipment definition */
	int numItems[MAX_OBJDEFS];	/**< Number of item for each item type (see equipment_missions.ufo for more info) */
	byte numItemsLoose[MAX_OBJDEFS];	/**< currently only used for weapon ammo */
	int numAircraft[AIRCRAFTTYPE_MAX];
	int minInterest;			/**< Minimum overall interest to use this equipment definition (only for alien) */
	int maxInterest;			/**< Maximum overall interest to use this equipment definition (only for alien) */

	void addClip(const Item* item);	/** Combine the rounds of partially used clips. */
} equipDef_t;

/* Maximum number of alien teams per alien group */
#define MAX_TEAMS_PER_MISSION MAX_TEAMDEFS

typedef struct damageType_s {
	char id[MAX_VAR];
	bool showInMenu;	/**< true for values that contains a translatable id */
} damageType_t;

/** @todo remove this and use the container id - not every unit must have two hands */
typedef enum {
	ACTOR_HAND_NOT_SET = 0,
	ACTOR_HAND_RIGHT = 1,
	ACTOR_HAND_LEFT = 2,

	ACTOR_HAND_ENSURE_32BIT = 0x7FFFFFFF
} actorHands_t;

#define foreachhand(hand) for (int hand##__loop = 0; hand##__loop < 2; ++hand##__loop) \
	if (hand = (hand##__loop == 0 ? ACTOR_HAND_RIGHT : ACTOR_HAND_LEFT), false) {} else


/* ================================ */
/*  INVENTORY MANAGEMENT FUNCTIONS  */
/* ================================ */

void INVSH_InitCSI(const struct csi_s* import) __attribute__((nonnull));

const objDef_t* INVSH_GetItemByID(const char* id);
const objDef_t* INVSH_GetItemByIDX(int index);
const objDef_t* INVSH_GetItemByIDSilent(const char* id);

const implantDef_t* INVSH_GetImplantForObjDef(const objDef_t* od);
const implantDef_t* INVSH_GetImplantByID(const char* id);
const implantDef_t* INVSH_GetImplantByIDSilent(const char* id);

const invDef_t* INVSH_GetInventoryDefinitionByID(const char* id);

#define THIS_FIREMODE(fm, HAND, fdIdx)	((fm)->getHand() == (HAND) && (fm)->getFmIdx() == (fdIdx))

/* =============================== */
/*  FIREMODE MANAGEMENT FUNCTIONS  */
/* =============================== */

const fireDef_t* FIRESH_GetFiredef(const objDef_t* obj, const weaponFireDefIndex_t weapFdsIdx, const fireDefIndex_t fdIdx);
#define FIRESH_IsMedikit(firedef) ((firedef)->damage[0] < 0)
void INVSH_MergeShapes(uint32_t* shape, const uint32_t itemShape, const int x, const int y);
bool INVSH_CheckShape(const uint32_t* shape, const int x, const int y);
int INVSH_ShapeSize(const uint32_t shape);
