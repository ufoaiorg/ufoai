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
#define MAX_MISSIONS	255
#define MAX_MISFIELDS	8
#define MAX_REQMISSIONS	4
#define MAX_ACTMISSIONS	16
#define MAX_SETMISSIONS	16
#define MAX_CAMPAIGNS	16

#define MAX_STAGESETS	256
#define MAX_STAGES		64

// check for water
// blue value is 64
#define MapIsWater(color) (color[0] == 0 && color[1] == 0 && color[2] == 64)
#define MapIsArctic(color) (color[0] == 128 && color[1] == 255 && color[2] == 255)
#define MapIsDesert(color) (color[0] == 255 && color[1] == 128 && color[2] == 0)
// others:
// red 255, 0, 0
// yellow 255, 255, 0
// green 128, 255, 0
// violet 128, 0, 128
// blue (not water) 128, 128, 255
// blue (not water, too) 0, 0, 255

#define MAX_UFOONGEOSCAPE 8

typedef struct mission_s
{
	char	*text;
	char	name[MAX_VAR];
	char	map[MAX_VAR];
	char	param[MAX_VAR];
	char	music[MAX_VAR];
	char	alienTeam[MAX_VAR];
	char	alienEquipment[MAX_VAR];
	char	civTeam[MAX_VAR];
	int	ugv; // uncontrolled ground units (entity: info_ugv_start)
	qboolean	active; // aircraft at place?
	vec2_t	pos;
	byte	mask[4];
	int		aliens, civilians;
	int		recruits;
	int		cr_win, cr_alien, cr_civilian;
} mission_t;

typedef struct stageSet_s
{
	char	name[MAX_VAR];
	char	needed[MAX_VAR];
	char	nextstage[MAX_VAR];
	char	endstage[MAX_VAR];
	char	cmds[MAX_VAR];
	// play a sequence when entering a new stage?
	char	sequence[MAX_VAR];
	date_t	delay;
	date_t	frame;
	date_t	expire;
	int		number;
	int		quota;
	byte	numMissions;
	int		missions[MAX_SETMISSIONS];
} stageSet_t;

typedef struct stage_s
{
	char	name[MAX_VAR];
	int		first, num;
} stage_t;

typedef struct setState_s
{
	stageSet_t *def;
	stage_t *stage;
	byte	active;
	date_t	start;
	date_t	event;
	int		num;
	int		done;
} setState_t;

typedef struct stageState_s
{
	stage_t *def;
	byte	active;
	date_t	start;
} stageState_t;

typedef struct actMis_s
{
	mission_t  *def;
	setState_t *cause;
	date_t	expire;
	vec2_t	realPos;
} actMis_t;

typedef struct campaign_s
{
	char	name[MAX_VAR];
	char	team[MAX_VAR];
	char	equipment[MAX_VAR];
	char	market[MAX_VAR];
	char	campaignName[MAX_VAR];
	char	text[MAX_VAR]; // placeholder for gettext stuff
	char	map[MAX_VAR]; // geoscape map
	char	firststage[MAX_VAR];
	int		soldiers;
	int		credits;
	int		num;
	qboolean	visible;
	date_t	date;
	qboolean	finished;
} campaign_t;

extern aircraft_t	aircraft[MAX_AIRCRAFT];
extern int		numAircraft;
extern int		interceptAircraft;
extern aircraft_t*	ufoOnGeoscape[MAX_UFOONGEOSCAPE];

typedef struct nation_s
{
	char	id[MAX_VAR];
	char	name[MAX_VAR];
	int	funding;
	vec4_t	color;
	float	alienFriendly;
	int	soldiers;
	int	scientists;
	char	names[MAX_VAR];
} nation_t;

#define MAX_NATIONS 8

void MN_MapCalcLine( vec2_t start, vec2_t end, mapline_t *line );
void CL_SelectAircraft_f ( void );
void CL_OpenAircraft_f ( void );
void CL_BuildingAircraftList_f ( void );
void CL_MapActionReset( void );
aircraft_t* CL_GetAircraft ( char* name );

typedef struct ccs_s
{
	equipDef_t		eCampaign, eMission, eMarket;

	stageState_t	stage[MAX_STAGES];
	setState_t		set[MAX_STAGESETS];
	actMis_t		mission[MAX_ACTMISSIONS];
	int		numMissions;
	int 		numUfoOnGeoscape;

	qboolean	singleplayer;

	int		credits;
	int		reward;
	date_t	date;
	float	timer;

	vec2_t	center;
	float	zoom;

	int	actualBaseID;
} ccs_t;

typedef enum mapAction_s
{
	MA_NONE,
	MA_NEWBASE,	// build a new base
	MA_INTERCEPT,	// intercept TODO:
	MA_BASEATTACK,	// base attacking
	MA_UFORADAR	// ufos are in our radar
} mapAction_t;

typedef enum aircraftStatus_s
{
	AIR_UFOMOVE = -1,	// a moving ufo
	AIR_NONE = 0,
	AIR_REFUEL,	// refill fuel
	AIR_HOME,	// in homebase
	AIR_IDLE,	// just sit there on geoscape
	AIR_TRANSIT,	// moving
	AIR_DROP,	// ready to drop down
	AIR_INTERCEPT,	// ready to intercept
	AIR_TRANSPORT,	// transporting from one base to another
	AIR_RETURNING	// returning to homebase
} aircraftStatus_t;

extern	mission_t	missions[MAX_MISSIONS];
extern	int			numMissions;
extern	actMis_t	*selMis;

extern	campaign_t	campaigns[MAX_CAMPAIGNS];
extern	int			numCampaigns;

extern	stageSet_t	stageSets[MAX_STAGESETS];
extern	stage_t		stages[MAX_STAGES];
extern	int			numStageSets;
extern	int			numStages;

extern	campaign_t	*curCampaign;
extern	ccs_t		ccs;

extern	int			mapAction;

void AIR_SaveAircraft( sizebuf_t *sb, base_t* base );
void AIR_LoadAircraft ( sizebuf_t *sb, base_t* base, int version );
aircraft_t* AIR_FindAircraft ( char* aircraftName );

char* CL_AircraftStatusToName ( aircraft_t* air );
qboolean CL_MapIsNight( vec2_t pos );
void CL_ResetCampaign( void );
void CL_DateConvert( date_t *date, int *day, int *month );
char* CL_DateGetMonthName ( int month );
void CL_CampaignRun( void );
void CL_GameTimeStop( void );
byte *CL_GetmapColor( vec2_t pos );
qboolean CL_NewBase( vec2_t pos );
void CL_ParseMission( char *name, char **text );
void CL_ParseStage( char *name, char **text );
void CL_ParseCampaign( char *name, char **text );
void CL_ParseAircraft( char *name, char **text );
void CL_ParseNations( char *name, char **text );
void CL_AircraftSelect( void );
void CL_NewAircraft_f ( void );
void CL_NewAircraft ( base_t* base, char* name );
void CL_AircraftInit( void );
void CL_CollectAliens( mission_t* mission );
void CL_CollectItems( int won );
void CL_UpdateCharacterStats ( int won );
void CL_UpdateCredits ( int credits );
qboolean CL_OnBattlescape( void );
