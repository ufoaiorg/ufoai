/*

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

#ifndef BASEMANGEMENT_DEFINED
#define BASEMANGEMENT_DEFINED 1

#define	BSFS(x)	(int)&(((building_t *)0)->x)

#define	PRODFS(x)	(int)&(((production_t *)0)->x)

#define REPAIRTIME 14
#define REPAIRCOSTS 10

#define UPGRADECOSTS 100
#define UPGRADETIME  2

#define MAX_PRODUCTIONS		256

#define MAX_LIST_CHAR		1024
#define MAX_BUILDINGS		256
#define MAX_BASES		8
#define MAX_DESC		256

#define SCROLLSPEED		1000

// this is not best - but better than hardcoded every time i used it
#define RELEVANT_X		187.0
#define RELEVANT_Y		280.0
// take the values from scriptfile
#define BASEMAP_SIZE_X		778.0
#define BASEMAP_SIZE_Y		672.0

#define BASE_SIZE		5
#define MAX_BASE_LEVELS		1

// max values for employee-management
#define MAX_EMPLOYEES 1024
#define MAX_EMPLOYEES_IN_BUILDING 64
// TODO:
// MAX_EMPLOYEES_IN_BUILDING should be redefined by a config variable that is lab/workshop/quarters-specific
// e.g.:
// if ( !maxEmployeesInQuarter ) maxEmployeesInQuarter = MAX_EMPLOYEES_IN_BUILDING;
// if ( !maxEmployeesWorkersInLab ) maxEmployeesWorkersInLab = MAX_EMPLOYEES_IN_BUILDING;
// if ( !maxEmployeesInWorkshop ) maxEmployeesInWorkshop = MAX_EMPLOYEES_IN_BUILDING;

// allocate memory for menuText[TEXT_STANDARD] contained the information about a building
char	buildingText[MAX_LIST_CHAR];

// The types of employees
typedef enum
{
	EMPL_UNDEF,
	EMPL_SOLDIER,
	EMPL_SCIENTIST,
	EMPL_WORKER,		// unused right now
	EMPL_MEDIC,		// unused right now
	EMPL_ROBOT,		// unused right now
	MAX_EMPL			// for counting over all available enums
} employeeType_t;

// The definition of an employee
typedef struct employee_s
{
	int idx;				// self link in global employee-list.
	employeeType_t type;		// What profession does this employee has.

	char speed;			// Speed of this Worker/Scientist at research/construction.

	int base_idx;	// what base this employee is in.
	int quarters;	// The quarter this employee is assigned to. (all except EMPL_ROBOT)
	int lab;		// The lab this scientist is working in. (only EMPL_SCIENTIST)
	int workshop;	// The lab this worker is working in. (only EMPL_WORKER)
	//int sickbay;	// The sickbay this employee is medicaly treated in. (all except EMPL_ROBOT ... since all can get injured.)
	//int training_room;	// The room this soldier is training in in. (only EMPL_SOLDIER)

	struct character_s *combat_stats;	// Soldier stats (scis/workers/etc... as well ... e.g. if the base is attacked)
} employee_t;


// Struct to be used in building definition - List of employees.
typedef struct employees_s
{
	int assigned[MAX_EMPLOYEES_IN_BUILDING];	// List of employees (links to global list).
	int numEmployees;					// Current number of employees.
	int maxEmployees;					// Max. number of employees (from config file)
	float cost_per_employee;				// Costs per employee that are added to toom-total-costs-
} employees_t;

typedef enum
{
	BASE_NOT_USED,
	BASE_UNDER_ATTACK,
	BASE_WORKING
} baseStatus_t;

typedef enum
{
	B_STATUS_NOT_SET, // not build yet
	B_STATUS_UNDER_CONSTRUCTION, // right now under construction
	B_STATUS_CONSTRUCTION_FINISHED, // construction finished - no workers assigned
				 // and building needs workers
	B_STATUS_WORKING, // working normal (or workers assigned when needed)
	B_STATUS_DOWN // totally damaged
} buildingStatus_t;

typedef enum
{
	B_MISC,
	B_LAB,
	B_QUARTERS,
	B_WORKSHOP,
	B_HANGAR
} buildingType_t;

typedef struct building_s
{
	int	idx;		// self link in "buildingTypes" OR "buildings" list.
	int	base_idx;	// Number/index of base this building is located in.

	char	id[MAX_VAR];
	char	name[MAX_VAR];
	// needs determines the second building part
	char	*image, *mapPart, *pedia;
	char *needs;	// if the buildign has a second part
	float	fixCosts, varCosts;

	int	timeStart, buildTime;

	// A list of employees assigned to this building.
	struct employees_s assigned_employees;

	//if we can build more than one building of the same type:
	buildingStatus_t	buildingStatus; //[BASE_SIZE*BASE_SIZE];

	byte	visible;
	// needed for baseassemble
	// when there are two tiles (like hangar) - we only load the first tile
	int	used;

	// event handler functions
	char	onConstruct[MAX_VAR];
	char	onAttack[MAX_VAR];
	char	onDestroy[MAX_VAR];
	char	onUpgrade[MAX_VAR];
	char	onRepair[MAX_VAR];
	char	onClick[MAX_VAR];

	//more than one building of the same type allowed?
	int	moreThanOne;

	//position of autobuild
	vec2_t	pos;

	//autobuild when base is set up
	byte	autobuild;

	//autobuild when base is set up
	byte	firstbase;

	//this way we can rename the buildings without loosing the control
	buildingType_t	buildingType;

	int tech;			// link to the building-technology
	int dependsBuilding;	// if the building needs another one to work (= to be buildable) .. links to gd.buildingTypes
} building_t;

#define MAX_AIRCRAFT	256
#define MAX_CRAFTUPGRADES	64
#define LINE_MAXSEG 64
#define LINE_MAXPTS (LINE_MAXSEG+2)
#define LINE_DPHI	(M_PI/LINE_MAXSEG)

typedef struct mapline_s
{
	int    n;
	float  dist;
	vec2_t p[LINE_MAXPTS];
} mapline_t;

typedef enum
{
	AIRCRAFT_TRANSPORTER,
	AIRCRAFT_INTERCEPTOR,
	AIRCRAFT_UFO
} aircraftType_t;

typedef enum
{
	CRAFTUPGRADE_WEAPON,
	CRAFTUPGRADE_ENGINE,
	CRAFTUPGRADE_ARMOR
} craftupgrade_type_t;

typedef struct craftupgrade_s
{
	/* some of this informations defined here overwrite the ones in the aircraft_t struct if given. */

	/* general */
	char	id[MAX_VAR];		// Short (unique) id/name.
	int	idx;				// Self-link in the global list
	char	name[MAX_VAR];		// Full name of this upgrade
	craftupgrade_type_t type;	// weapon/engine/armor
	int	pedia;			// -pedia link

	/* armor related */
	float armor_kinetic;		// maybe using (k)Newtons here?
	float armor_shield;			// maybe using (k)Newtons here?

	/* weapon related */
	int ammo;				// index of the 'ammo' craftupgrade entry.
	struct weapontype_s *weapontype;	// e.g beam/particle/missle ( do we already have something like that?)
	int num_ammo;
	float    damage_kinetic;		// maybe using (k)Newtons here?
	float    damage_shield;		// maybe using (k)Newtons here?
	float    range;				// meters (only for non-beam weapons. i.e projectiles, missles, etc...)

	/* drive related */
	int    speed;				// the maximum speed the craft can fly with this upgrade
	int    fuelSize;				// the maximum fuel size

	/* representation/display */
	char    model[MAX_QPATH];	// 3D model
	char    image[MAX_VAR];		// image
} craftupgrade_t;

typedef struct aircraft_s
{
	int idx;			// Self-link in the global list
	char	id[MAX_VAR];	// translateable name
	char	name[MAX_VAR];	// internal id
	char	image[MAX_VAR];	// image on geoscape
	aircraftType_t	type;
	int		status;	// see aircraftStatus_t
	float		speed;
	int	price;
	int	fuel;	// actual fuel
	int	fuelSize;	// max fuel
	int	size;	// how many soldiers max
	vec2_t	pos;	// actual pos on geoscape
	int		point;
	int		time;
	int	idx_base;	// id in base
	int	*teamSize;	// how many soldiers on board
				// pointer to base->numOnTeam[AIRCRAFT_ID]
	// TODO
	// xxx teamShape;    // TODO: shape of the soldier-area onboard.
	char	model[MAX_QPATH];
	char	weapon_string[MAX_VAR];
	technology_t*	weapon;
	char	shield_string[MAX_VAR];
	technology_t*	shield;
	mapline_t route;
	void*	homebase;	// pointer to homebase

	char    building[MAX_VAR];	// id of the building needed as hangar

	craftupgrade_t    *upgrades[MAX_CRAFTUPGRADES];	// TODO replace armor/weapon/engine definitions from above with this.
	int    numUpgrades;
	struct aircraft_s *next; // just for linking purposes - not needed in general
} aircraft_t;

typedef struct base_s
{
	int	idx;	// self link
	char	name[MAX_VAR];
	int	map[BASE_SIZE][BASE_SIZE];

	qboolean founded;
	vec2_t pos;

	// to decide which actions are available in the basemenu
	byte	hasHangar;
	byte	hasLab;	// TODO: still needed.

	//this is here to allocate the needed memory for the buildinglist
	char	allBuildingsList[MAX_LIST_CHAR];

	//mapChar indicated which map to load (gras, desert, arctic,...)
	//d=desert, a=arctic, g=gras
	char	mapChar;

	// all aircraft in this base
	// FIXME: make me a linked list (see cl_market.c aircraft selling)
	aircraft_t	aircraft[MAX_AIRCRAFT];
	int 	numAircraftInBase;
	int	aircraftCurrent;

	int	posX[BASE_SIZE][BASE_SIZE];
	int	posY[BASE_SIZE][BASE_SIZE];

	int	condition;

	baseStatus_t	baseStatus;

	int		hiredMask; // hired mask for all soldiers in base
	int		teamMask[MAX_AIRCRAFT]; // assigned to a specific aircraft
	int		deathMask;

	int		numHired;
	int		numOnTeam[MAX_AIRCRAFT];
	int		numWholeTeam; // available soldiers in this base

	// the onconstruct value of the buliding
	// building_radar increases the sensor width
	int	sensorWidth; // radar radius
	qboolean	drawSensor; // ufo in range?

	inventory_t	teamInv[MAX_WHOLETEAM];
	inventory_t	equipment;

	character_t	wholeTeam[MAX_WHOLETEAM];
	character_t	curTeam[MAX_ACTIVETEAM];
	character_t	*curChr;

	int		equipType;
	int		nextUCN; // unified character id (base dependent)

	// needed if there is another buildingpart to build (link to gd.buildingTypes)
	int buildingToBuild;

	struct building_s *buildingCurrent;
} base_t;

extern	base_t	*baseCurrent;							// Currently displayed/accessed base.

craftupgrade_t    *craftupgrades[MAX_CRAFTUPGRADES];		// This it the global list of all available craft-upgrades
int    numCraftUpgrades;								// The global number of entries in the "craftupgrades" list.

void B_SetSensor( void );
void B_InitEmployees ( void );
void B_UpdateBaseData( void );
base_t* B_GetBase ( int id );
void B_UpgradeBuilding( building_t* b );
void B_RepairBuilding( building_t* b );
int B_CheckBuildingConstruction ( building_t* b, int baseID );
int B_GetNumOnTeam ( void );
building_t * B_GetUnusedLab( int base_id );
int B_GetUnusedLabs( int base_id );
void B_ClearBuilding( building_t *building );
int B_EmployeesInBase2 ( int base_id, employeeType_t employee_type, byte free_only );
byte B_RemoveEmployee ( building_t *building );
byte B_AssignEmployee ( building_t *building_dest, employeeType_t employee_type );
void B_SaveBases( sizebuf_t *sb );
void B_LoadBases( sizebuf_t *sb, int version );
void B_ParseBuildings( char *id, char **text, qboolean link );
void B_ParseBases( char *title, char **text );
void B_BuildingInit( void );
void B_AssembleMap( void );
void B_BaseAttack ( void );
void B_DrawBuilding( void );
building_t* B_GetBuildingByIdx ( int idx );
building_t* B_GetBuildingType ( char *buildingName );

typedef struct production_s
{
	char    id[MAX_VAR];
	char    name[MAX_VAR];
	char    text[MAX_VAR]; //short description
	int	amount;
	char	menu[MAX_VAR];
	technology_t* tech;
} production_t;

extern vec2_t newBasePos;

void B_BuildingInit( void );
int B_GetCount ( void );
void B_SetUpBase ( void );

void B_ParseProductions( char *title, char **text );
void B_SetBuildingByClick ( int row, int col );
void B_ResetBaseManagement( void );
void B_ClearBase( base_t *base );
void B_NewBases( void );

#endif /* BASEMANGEMENT_DEFINED */

