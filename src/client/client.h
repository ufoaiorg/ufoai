/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// client.h -- primary header for client

//define	PARANOID			// speed sapping error checking


#ifndef CLIENT_DEFINED
#define CLIENT_DEFINED 1
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

// i18n support via gettext
// needs to be activated via -DHAVE_GETTEXT
#ifdef HAVE_GETTEXT
#ifdef MACOS_X
#include <intl/libintl.h>
#elif defined(_WIN32)
#define snprintf _snprintf
#include "../win32/libintl.h"
#else
#include <libintl.h>
#endif

#include <locale.h>
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)
#else //HAVE_GETTEXT
// no gettext support
#define _(String) String
#endif

#include "ref.h"
#include "vid.h"
#include "screen.h"
#include "sound.h"
#include "input.h"
#include "keys.h"
#include "console.h"
#include "cdaudio.h"
#include "cl_basemanagement.h"
#include "cl_ufopedia.h"
#include "cl_research.h"

//=============================================================================

#define MAX_TEAMLIST	8

#define RIGHT(e) ((e)->i.c[csi.idRight])
#define LEFT(e)  ((e)->i.c[csi.idLeft])
#define FLOOR(e) ((e)->i.c[csi.idFloor])

typedef struct
{
	int			serverframe;		// if not current, this ent isn't in the frame

	int			trailcount;			// for diminishing grenade trails
	vec3_t		lerp_origin;		// for trails (variable hz)

	int			fly_stoptime;
} centity_t;

#define MAX_CLIENTWEAPONMODELS		20		// PGM -- upped from 16 to fit the chainfist vwep

typedef struct
{
	char	name[MAX_QPATH];
	char	cinfo[MAX_QPATH];
	struct image_s	*skin;
	struct image_s	*icon;
	char	iconname[MAX_QPATH];
	struct model_s	*model;
	struct model_s	*weaponmodel[MAX_CLIENTWEAPONMODELS];
} clientinfo_t;

extern char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
extern int num_cl_weaponmodels;

#define	CMD_BACKUP		64	// allow a lot of command backups for very fast systems

//
//
#define FOV				60.0

typedef struct
{
	vec3_t	reforg;
	vec3_t	camorg;
	vec3_t	speed;
	vec3_t	angles;
	vec3_t	omega;
	vec3_t	axis[3];	// set when refdef.angles is set

	float	lerplevel;
	float	zoom;
	float fpzoom;
} camera_t;

typedef enum { CAMERA_MODE_REMOTE, CAMERA_MODE_FIRSTPERSON } camera_mode_t;

camera_mode_t camera_mode;

// don't mess with the order!!!
typedef enum
{
	M_MOVE,
	M_FIRE_PR,
	M_FIRE_SR,
	M_FIRE_PL,
	M_FIRE_SL,
	M_PEND_MOVE,
	M_PEND_FIRE_PR,
	M_PEND_FIRE_SR,
	M_PEND_FIRE_PL,
	M_PEND_FIRE_SL
} cmodes_t;

//
// the client_state_t structure is wiped completely at every
// server map change
//
typedef struct
{
	int			timeoutcount;

	int			timedemo_frames;
	int			timedemo_start;

	qboolean	refresh_prepped;	// false if on new level or new ref dll
	qboolean	sound_prepped;		// ambient sounds can start
	qboolean	force_refdef;		// vid has changed, so we can't use a paused refdef

	int			surpressCount;		// number of messages rate supressed

	int			parse_entities;		// index (not anded off) into cl_parse_entities[]

	int			time;			// this is the time value that the client
								// is rendering at.  always <= cls.realtime
	int			eventTime;		// similar to time, but not counting if blockEvents is set

	camera_t	cam;

	refdef_t	refdef;

	int			cmode;
	int			oldcmode;
	int			oldstate;

	int			msgTime;
	char		msgText[256];

	struct le_s	*teamList[MAX_TEAMLIST];
	int			numTeamList;
	int			numAliensSpotted;

	//
	// transient data from server
	//
	char		layout[1024];		// general 2D overlay

	//
	// non-gameserver infornamtion
	// FIXME: move this cinematic stuff into the cin_t structure
	FILE		*cinematic_file;
	int			cinematictime;		// cls.realtime for first cinematic frame
	int			cinematicframe;
	char		cinematicpalette[768];
	qboolean	cinematicpalette_active;

	//
	// server state information
	//
	qboolean	attractloop;		// running the attract loop, any key will menu
	int			servercount;	// server identification for prespawns
	char		gamedir[MAX_QPATH];
	int			pnum;
	int			actTeam;

	char		configstrings[MAX_CONFIGSTRINGS][MAX_TOKEN_CHARS];

	//
	// locally derived information from server state
	//
	struct model_s	*model_draw[MAX_MODELS];
	struct cmodel_s	*model_clip[MAX_MODELS];
	struct model_s	*model_weapons[MAX_OBJDEFS];

	struct sfx_s	*sound_precache[MAX_SOUNDS];
	struct image_s	*image_precache[MAX_IMAGES];

	clientinfo_t	clientinfo[MAX_CLIENTS];
	clientinfo_t	baseclientinfo;
} client_state_t;

extern	client_state_t	cl;

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/

typedef enum {
	ca_uninitialized,
	ca_disconnected, 	// not talking to a server
	ca_sequence,		// rendering a sequence
	ca_connecting,		// sending request packets to the server
	ca_connected,		// netchan_t established, waiting for svc_serverdata
	ca_active			// game views should be displayed
} connstate_t;

typedef enum {
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_single
} dltype_t;		// download type

typedef enum {key_game, key_console, key_message} keydest_t;

typedef struct
{
	connstate_t	state;
	keydest_t	key_dest;

	int			framecount;
	int			realtime;			// always increasing, no clamping, etc
	float		frametime;			// seconds since last frame

	int			framedelta;
	float		framerate;

	// screen rendering information
	float		disable_screen;		// showing loading plaque between levels
									// or changing rendering dlls
									// if time gets > 30 seconds ahead, break it
	int			disable_servercount;	// when we receive a frame and cl.servercount
									// > cls.disable_servercount, clear disable_screen

	// connection information
	char		servername[MAX_OSPATH];	// name of server from original connect
	float		connect_time;		// for connection retransmits

	int			quakePort;			// a 16 bit value that allows quake servers
									// to work around address translating routers
	netchan_t	netchan;
	int			serverProtocol;		// in case we are doing some kind of version hack

	int			challenge;			// from the server to use for connecting

	FILE		*download;			// file transfer from server
	char		downloadtempname[MAX_OSPATH];
	char		downloadname[MAX_OSPATH];
	int			downloadnumber;
	dltype_t	downloadtype;
	int			downloadpercent;

	// demo recording info must be here, so it isn't cleared on level change
	qboolean	demorecording;
	qboolean	demowaiting;	// don't record until a non-delta message is received
	FILE		*demofile;

	// needs to be here, because server can be shutdown, before we see the ending screen
	int			team;
} client_static_t;

extern client_static_t	cls;

//=============================================================================

//
// cvars
//
extern	cvar_t	*cl_stereo_separation;
extern	cvar_t	*cl_stereo;

extern	cvar_t	*cl_aviFrameRate;
extern	cvar_t	*cl_aviMotionJpeg;

extern	cvar_t	*cl_gun;
extern	cvar_t	*cl_add_blend;
extern	cvar_t	*cl_add_lights;
extern	cvar_t	*cl_add_particles;
extern	cvar_t	*cl_add_entities;
extern	cvar_t	*cl_predict;
extern	cvar_t	*cl_footsteps;
extern	cvar_t	*cl_noskins;
extern	cvar_t	*cl_autoskins;
extern	cvar_t	*cl_markactors;

extern	cvar_t	*cl_camrotspeed;
extern	cvar_t	*cl_camrotaccel;
extern	cvar_t	*cl_cammovespeed;
extern	cvar_t	*cl_cammoveaccel;
extern	cvar_t	*cl_camyawspeed;
extern	cvar_t	*cl_campitchspeed;
extern	cvar_t	*cl_camzoomquant;

extern	cvar_t	*cl_run;

extern	cvar_t	*cl_anglespeedkey;

extern	cvar_t	*cl_fps;
extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_showmiss;
extern	cvar_t	*cl_showclamp;
extern	cvar_t	*cl_show_tooltips;

extern	cvar_t	*lookspring;
extern	cvar_t	*lookstrafe;
extern	cvar_t	*sensitivity;

extern	cvar_t	*m_pitch;
extern	cvar_t	*m_yaw;
extern	cvar_t	*m_forward;
extern	cvar_t	*m_side;

extern	cvar_t	*freelook;

extern	cvar_t	*cl_logevents;

extern	cvar_t	*cl_lightlevel;	// FIXME HACK

extern	cvar_t	*cl_paused;
extern	cvar_t	*cl_timedemo;

extern	cvar_t	*cl_vwep;

extern	cvar_t	*cl_centerview;

extern	cvar_t	*cl_worldlevel;
extern	cvar_t	*cl_selected;

extern	cvar_t	*cl_numnames;

extern	cvar_t	*difficulty;

extern	cvar_t	*mn_active;
extern	cvar_t	*mn_main;
extern	cvar_t	*mn_sequence;
extern	cvar_t	*mn_hud;
extern	cvar_t	*mn_lastsave;
//the new soundsystem cvar 0-3 by now
#ifndef _WIN32
extern	cvar_t	*s_system;
#endif

extern	cvar_t	*confirm_actions;

extern cvar_t	*cl_precachemenus;

typedef struct
{
	int		key;				// so entities can reuse same entry
	vec3_t	color;
	vec3_t	origin;
	float	radius;
	float	die;				// stop lighting after this time
	float	decay;				// drop this each second
	float	minlight;			// don't add when contributing less
} cdlight_t;

extern	centity_t	cl_entities[MAX_EDICTS];
extern	cdlight_t	cl_dlights[MAX_DLIGHTS];

//=============================================================================

extern	netadr_t	net_from;
//extern	sizebuf_t	net_message;

void DrawString (int x, int y, char *s);
void DrawAltString (int x, int y, char *s);	// toggle high bit
qboolean	CL_CheckOrDownloadFile (char *filename);

void CL_AddNetgraph (void);

void CL_ParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, int color, int count);
// RAFAEL
void CL_ParticleEffect3 (vec3_t org, vec3_t dir, int color, int count);

//=================================================
// shader stuff
// =================================================

void CL_ShaderList_f ( void );
void CL_ParseShaders( char *title, char **text );
extern int	r_numshaders;
extern shader_t	r_shaders[MAX_SHADERS];

// =================================================

// ========
// PGM
typedef struct particle_s
{
	struct particle_s	*next;

	float		time;

	vec3_t		org;
	vec3_t		vel;
	vec3_t		accel;

	float		color;
	float		colorvel;
	float		alpha;
	float		alphavel;
} cparticle_t;


#define	PARTICLE_GRAVITY	40
#define BLASTER_PARTICLE_COLOR		0xe0
// PMM
#define INSTANT_PARTICLE	-10000.0
// PGM
// ========

void CL_ClearEffects (void);
void CL_ClearTEnts (void);
void CL_BlasterTrail (vec3_t start, vec3_t end);
void CL_QuadTrail (vec3_t start, vec3_t end);
void CL_RailTrail (vec3_t start, vec3_t end);
void CL_BubbleTrail (vec3_t start, vec3_t end);
void CL_FlagTrail (vec3_t start, vec3_t end, float color);

// RAFAEL
void CL_IonripperTrail (vec3_t start, vec3_t end);

int CL_ParseEntityBits (unsigned *bits);

void CL_ParseTEnt (void);
void CL_ParseConfigString (void);
void CL_ParseMuzzleFlash (void);
void CL_ParseMuzzleFlash2 (void);
void SmokeAndFlash(vec3_t origin);

void CL_SetLightstyle (int i);

void CL_RunDLights (void);
void CL_RunLightStyles (void);

void CL_AddEntities (void);
void CL_AddDLights (void);
void CL_AddTEnts (void);
void CL_AddLightStyles (void);

//=================================================

void CL_PrepRefresh (void);
void CL_RegisterSounds (void);
void CL_RegisterLocalModels ( void );

void CL_Quit_f (void);

void IN_Accumulate (void);

void CL_ParseLayout (void);

//
// cl_sequence.c (avi stuff)
//
qboolean CL_OpenAVIForWriting( char *filename );
void CL_TakeVideoFrame( void );
void CL_WriteAVIVideoFrame( const byte *imageBuffer, int size );
void CL_WriteAVIAudioFrame( const byte *pcmBuffer, int size );
qboolean CL_CloseAVI( void );
qboolean CL_VideoRecording( void );
void CL_StopVideo_f( void );
void CL_Video_f( void );

//
// cl_main
//
extern	refexport_t	re;		// interface to refresh .dll

void CL_Init (void);

void CL_FixUpGender(void);
void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_GetChallengePacket (void);
void CL_PingServers_f (void);
void CL_Snd_Restart_f (void);
void CL_RequestNextDownload (void);

//
// cl_input
//

typedef struct
{
	int			down[2];		// key nums holding it down
	unsigned	downtime;		// msec timestamp
	unsigned	msec;			// msec down this frame
	int			state;
} kbutton_t;

typedef enum
{
	MS_NULL,
	MS_MENU,
	MS_DRAG,
	MS_ROTATE,
	MS_SHIFTMAP,
	MS_ZOOMMAP,
	MS_SHIFT3DMAP,
	MS_ZOOM3DMAP,
	MS_SHIFTBASEMAP,
	MS_ZOOMBASEMAP,
	MS_WORLD
} mouseSpace_t;

extern	int			mouseSpace;
extern	int			mx, my;
extern	int			dragFrom, dragFromX, dragFromY;
extern	item_t		dragItem;
extern	float		*rotateAngles;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;
extern  float    MIN_ZOOM, MAX_ZOOM;

void CL_InitInput (void);
void CL_CameraMove (void);
void CL_CameraRoute( pos3_t from, pos3_t target );
void CL_ParseInput (void);

void CL_ClearState (void);

void CL_ReadPackets (void);

int  CL_ReadFromServer (void);

float CL_KeyState (kbutton_t *key);
char *Key_KeynumToString (int keynum);

void CL_CameraMode(void);
void CL_CameraModeChange(camera_mode_t newcameramode);

void CL_Sniper(void);
void CL_SniperModeSet(void);
void CL_SniperModeUnset(void);

//
// cl_le.c
//
typedef struct le_s {
	qboolean	inuse;
	qboolean	invis;
	qboolean	selected;
	int			type;
	int			entnum;

	vec3_t		origin, oldOrigin;
	pos3_t		pos, oldPos;
	int 		dir;

	int 		TU, maxTU;
	int 		morale, maxMorale;
	int 		HP, maxHP;
	int 		state;

	float		angles[3];
	float		sunfrac;
	float		alpha;

	int			team;
	int			pnum;

	int			contents;
	vec3_t		mins, maxs;

	int			modelnum1, modelnum2, skinnum;
	struct model_s	*model1, *model2;

// 	character_t	*chr;

	// is called every frame
	void		(*think)(struct le_s *le);

	// various think function vars
	byte		path[32];
	int			pathLength, pathPos;
	int			startTime, endTime;
	int			speed;

	// gfx
	animState_t	as;
	ptl_t		*ptl;
	char		*ref1, *ref2;
	inventory_t	i;
	int			left, right;

	// is called before adding a le to scene
	qboolean	(*addFunc)(struct le_s *le, entity_t *ent);
} le_t;

#define MAX_LOCALMODELS		512
#define MAX_MAPPARTICLES	1024

#define LMF_LIGHTFIXED		1
#define LMF_NOSMOOTH		2

typedef struct lm_s {
	char		name[MAX_VAR];
	char		particle[MAX_VAR];

	vec3_t		origin;
	vec3_t		angles;
	vec4_t		lightorigin;
	vec4_t		lightcolor;
	vec4_t		lightambient;

	int			num;
	int			skin;
	int			flags;
	int			levelflags;
	float		sunfrac;

	struct model_s	*model;
} lm_t; /* local models */

typedef struct mp_s {
	char		ptl[MAX_QPATH];
	char		*info;
	vec3_t		origin;
	vec2_t		wait;
	int			nextTime;
	int			levelflags;
} mp_t; /* mapparticles */

extern lm_t LMs[MAX_LOCALMODELS];
extern mp_t MPs[MAX_MAPPARTICLES];

extern int	numLMs;
extern int	numMPs;

extern le_t LEs[MAX_EDICTS];
extern int	numLEs;

extern vec3_t player_mins;
extern vec3_t player_maxs;

void LE_Status ( void );
void LE_Think( void );
char *LE_GetAnim( char *anim, int right, int left, int state );

void LE_AddProjectile( fireDef_t *fd, int flags, vec3_t muzzle, vec3_t impact, int normal );
void LE_AddGrenade( fireDef_t *fd, int flags, vec3_t muzzle, vec3_t v0, int dt );

void LET_StartIdle( le_t *le );
void LET_Appear( le_t *le );
void LET_Perish( le_t *le );
void LET_StartPathMove( le_t *le );

void LM_Perish( sizebuf_t *sb );
void LM_Explode( sizebuf_t *sb );

void LM_AddToScene( void );
void LE_AddToScene( void );
le_t *LE_Add( int entnum );
le_t *LE_Get( int entnum );
le_t *LE_Find( int type, pos3_t pos );
trace_t CL_Trace (vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, le_t *passle, int contentmask);

lm_t *CL_AddLocalModel (char *model, char *particle, vec3_t origin, vec3_t angles, int num, int levelflags);
void CL_AddMapParticle (char *particle, vec3_t origin, vec2_t wait, char *info);

//
// cl_actor.c
//

extern	le_t		*selActor;
extern	int			actorMoveLength;
extern	invList_t	invList[MAX_INVLIST];

extern	byte	*fb_list[MAX_FB_LIST];
extern	int		fb_length;

void CL_CharacterCvars( character_t *chr );
void CL_ActorUpdateCVars( void );

qboolean CL_ActorSelect( le_t *le );
qboolean CL_ActorSelectList( int num );
void CL_AddActorToTeamList( le_t *le );
void CL_RemoveActorFromTeamList( le_t *le );
void CL_ActorSelectMouse( void );
void CL_ActorReload( int hand );
void CL_ActorTurnMouse( void );
void CL_ActorDoTurn( sizebuf_t *sb );
void CL_ActorStandCrouch( void );
void CL_ActorToggleReaction( void );
void CL_BuildForbiddenList( void );
void CL_ActorStartMove( le_t *le, pos3_t to );
void CL_ActorShoot( le_t *le, pos3_t at );
void CL_ActorDoMove( sizebuf_t *sb );
void CL_ActorDoShoot( sizebuf_t *sb );
void CL_ActorDoThrow( sizebuf_t *sb );
void CL_ActorStartShoot( sizebuf_t *sb );
void CL_ActorDie( sizebuf_t *sb );

void CL_ActorActionMouse( void );

void CL_NextRound( void );
void CL_DoEndRound( sizebuf_t *sb );

void CL_ResetMouseLastPos( void );
void CL_ActorMouseTrace( void );

qboolean CL_AddActor( le_t *le, entity_t *ent );

void CL_AddTargeting( void );

//
// cl_team.c
//
#define MAX_ACTIVETEAM	8
#define MAX_WHOLETEAM	32
#define NUM_BUYTYPES	4
#define NUM_TEAMSKINS	4

extern inventory_t teamInv[MAX_WHOLETEAM];
extern inventory_t equipment;
extern character_t wholeTeam[MAX_WHOLETEAM];
extern character_t curTeam[MAX_ACTIVETEAM];
extern int numWholeTeam;
extern int numOnTeam;
extern int teamMask;
extern int deathMask;
extern int equipType;
extern int numHired;
extern int hiredMask;

extern char *teamSkinNames[NUM_TEAMSKINS];

void CL_ResetTeams( void );
void CL_ParseResults( sizebuf_t *buf );
void CL_SendTeamInfo( sizebuf_t *buf, character_t *team, int num );
void CL_CheckInventory( equipDef_t *equip );
void CL_ItemDescription( int item );

//
// cl_research.c
//
void RS_InitTree( void );
void RS_MarkCollected ( void );
void RS_MarkResearchable ( void );

//
// cl_basemanagment.c
//
#define MAX_LIST_CHAR		1024
#define MAX_BUILDINGS		256
#define MAX_BASES		8
#define MAX_DESC		256

#define BUILDINGCONDITION	100

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
// MAX_EMPLOYEES_IN_BUILDING should be redefined by a config variable that is lab/workshop/quaters-specific
// e.g.:
// if ( !maxEmployeesInQuater ) maxEmployeesInQuater = MAX_EMPLOYEES_IN_BUILDING;
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
	employeeType_t type;		// What profession does this employee has.

	char speed;			// Speed of this Worker/Scientist at research/construction.

	struct building_s *quaters;	// The quater this employee is assigned to. (all except EMPL_ROBOT)
	struct building_s *lab;		// The lab this scientist is working in. (only EMPL_SCIENTIST)
	struct building_s *workshop;	// The lab this worker is working in. (only EMPL_WORKER)
	//struct building_s *sickbay;	// The sickbay this employee is medicaly treated in. (all except EMPL_ROBOT ... since all can get injured.)
	//struct building_s *training_room;	// The room this soldier is training in in. (only EMPL_SOLDIER)

	struct character_s *combat_stats;	// Soldier stats (scis/workers/etc... as well ... e.g. if the base is attacked)
} employee_t;


// Struct to be used in building definition - List of employees.
typedef struct employees_s
{
	struct employee_s *assigned[MAX_EMPLOYEES_IN_BUILDING];	// List of employees.
	int numEmployees;								// Current number of employees.
	int maxEmployees;								// Max. number of employees (from config file)
	float cost_per_employee;							// Costs per employee that are added to toom-total-costs-
} employees_t;

typedef enum
{
	BASE_NOT_USED,
	BASE_UNDER_ATTACK,
	BASE_WORKING
} baseStatus_t;

typedef enum
{
	B_NOT_SET,
	B_UNDER_CONSTRUCTION,
	B_CONSTRUCTION_FINISHED,
	B_UPGRADE,
	B_WORKING_120,
	B_WORKING_100,
	B_WORKING_50,
	B_MAINTENANCE,
	B_REPAIRING,
	B_DOWN
} buildingStatus_t;

typedef enum
{
	B_MISC,
	B_LAB,
	B_QUATERS,
	B_WORKSHOP,
	B_HANGAR
} buildingType_t;

typedef struct building_s
{
	// FIXME: This can be a char* name and char* base.... don't it? See bmData in basemanagement.c
	char	name[MAX_VAR];
	char	base[MAX_VAR];
	char	title[MAX_VAR];
	// needs determines the second building part
	char	*text, *image, *needs, *depends, *mapPart, *produceType, *pedia;
	float	energy, workerCosts, produceTime, fixCosts, varCosts;
	int	production, level, id, timeStart, buildTime, techLevel, notUpOn;

	int maxWorkers, minWorkers, assignedWorkers; // TODO: remove these and replace them with the employee_s struct
	// A list of employees assigned to this building.
	struct employees_s assigned_employees;

	//if we can build more than one building of the same type:
	buildingStatus_t	buildingStatus[BASE_SIZE*BASE_SIZE];
	int	condition[BASE_SIZE*BASE_SIZE];

	vec2_t	size;
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

	//how many buildings are there of the same type?
	//depends on the value of moreThanOne ^^
	int	howManyOfThisType;

	//position of autobuild
	vec2_t	pos;

	//autobuild when base is set up
	byte	autobuild;

	//autobuild when base is set up
	byte	firstbase;

	//this way we can rename the buildings without loosing the control
	buildingType_t	buildingType;

	struct building_s *dependsBuilding;
} building_t;

#define MAX_AIRCRAFT	256
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

typedef struct aircraft_s
{
	char	name[MAX_VAR];	// translateable name
	char	title[MAX_VAR];	// internal id
	char	image[MAX_VAR];	// image on geoscape
	aircraftType_t	type;
	int		status;	// see aircraftStatus_t
	float		speed;
	int	fuel;	// actual fuel
	int	fuelSize;	// max fuel
	int	size;	// how many soldiers max
	vec2_t	pos;	// actual pos on geoscape
	int		point;
	int		time;
	int	teamSize;	// how many soldiers on board
	char	model[MAX_VAR];
	char	model_top[MAX_VAR];
	char	model_glass[MAX_VAR];
	char	model_wings[MAX_VAR];
	mapline_t route;
	void*	homebase;	// pointer to homebase
} aircraft_t;

typedef struct base_s
{
	//the internal base-id
	int	id;
	char	title[MAX_VAR];
	int	map[BASE_SIZE][BASE_SIZE][MAX_BASE_LEVELS];

	qboolean founded;
	vec2_t pos;

	// to decide which actions are available in the basemenu
	byte	hasHangar;
	byte	hasLab;

	//this is here to allocate the needed memory for the buildinglist
	char	allBuildingsList[MAX_LIST_CHAR];

	//mapChar indicated which map to load (gras, desert, arctic,...)
	//d=desert, a=arctic, g=gras
	char	mapChar;

	// all aircraft in this base
	aircraft_t	aircraft[MAX_AIRCRAFT];
	int 	numAircraftInBase;
	void*	aircraftCurrent;

	int	posX[BASE_SIZE][BASE_SIZE][MAX_BASE_LEVELS];
	int	posY[BASE_SIZE][BASE_SIZE][MAX_BASE_LEVELS];

	//FIXME: change building condition to base condition
	float	condition;

	baseStatus_t	baseStatus;

	// which level to display?
	int	baseLevel;

	int		hiredMask;
	int		teamMask;
	int		deathMask;

	int		numHired;
	int		numOnTeam;
	int		numWholeTeam;

	inventory_t	teamInv[MAX_WHOLETEAM];
	inventory_t	equipment;

	character_t	wholeTeam[MAX_WHOLETEAM];
	character_t	curTeam[MAX_ACTIVETEAM];
	character_t	*curChr;

	int		equipType;
	int		nextUCN;

	// needed if there is another buildingpart to build
	struct building_s *buildingToBuild;

	struct building_s *buildingCurrent;
} base_t;

extern	base_t	bmBases[MAX_BASES];					// A global list of _all_ bases.
extern	base_t	*baseCurrent;						// Currently displayed/accessed base.
extern	building_t	bmBuildings[MAX_BASES][MAX_BUILDINGS];	// A global list of _all_ buildings (even unbuilt) in all bases.
extern	int numBuildings;								// The global number of entries in the bmBuildings list.

extern	employee_t	employees[MAX_EMPLOYEES];			// This it the global list of employees.
extern	int   numEmployees;							// The global number of enris in the "employees" list.

#ifndef SAVE_FILE_VERSION
#define SAVE_FILE_VERSION 4
#endif

#define MAX_TEAMDATASIZE	32768
#define MAX_GAMESAVESIZE	MAX_TEAMDATASIZE + 16384
#define MAX_COMMENTLENGTH	32

void CL_UpdateBaseData( void );
int B_HowManyPeopleInBase2 ( base_t *base, int location );
base_t* B_GetBase ( int id );
int B_GetNumOnTeam ( void );
void CL_LoadTeam( sizebuf_t *sb, base_t* base, int version );
void CL_UpdateHireVar ( void );

void MN_GetFreeBuilding( buildingType_t type, building_t *building );

byte MN_RemoveEmployee ( building_t *building );
byte MN_AssignEmployee ( building_t *building_dest, employeeType_t employee_type );

void CL_ResetCharacters( base_t* base );
void CL_GenerateCharacter( char *team, base_t* base );

// needed to calculate the chosen building in cl_menu.c
int picWidth, picHeight;

void MN_ClearBase( base_t *base );
void MN_BuildNewBase( vec2_t pos );
void MN_NewBases( void );
void B_SaveBases( sizebuf_t *sb );
void B_LoadBases( sizebuf_t *sb, int version );
void MN_ResetBasemanagement( void );
void MN_ParseBuildings( char *title, char **text );
void MN_ParseBases( char *title, char **text );
void MN_BuildingInit( void );
void B_AssembleMap( void );
void B_BaseAttack ( void );
void MN_GetMaps_f ( void );
void CL_ListMaps_f ( void );
void MN_NextMap ( void );
void MN_PrevMap ( void );
building_t* B_GetBuildingByID ( int id );
building_t* B_GetBuilding ( char *buildingName );

//
// cl_campaign.c
//
#define MAX_MISSIONS	255
#define MAX_MISFIELDS	8
#define MAX_REQMISSIONS	4
#define MAX_ACTMISSIONS	16
#define MAX_SETMISSIONS	16
#define MAX_CAMPAIGNS	16

#define MAX_STAGESETS	256
#define MAX_STAGES		64

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
	char	firststage[MAX_VAR];
	int		soldiers;
	int		credits;
	int		num;
	date_t	date;
	qboolean	finished;
} campaign_t;

extern aircraft_t	aircraft[MAX_AIRCRAFT];
extern int		numAircraft;
extern aircraft_t	*interceptAircraft;

void MN_MapCalcLine( vec2_t start, vec2_t end, mapline_t *line );
void CL_SelectAircraft_f ( void );
void CL_OpenAircraft_f ( void );
void CL_BuildingAircraftList_f ( void );
void CL_MapActionReset( void );

typedef struct ccs_s
{
	equipDef_t		eCampaign, eMission, eMarket;

	stageState_t	stage[MAX_STAGES];
	setState_t		set[MAX_STAGESETS];
	actMis_t		mission[MAX_ACTMISSIONS];
	int		numMissions;

	int		numBases;
	qboolean	singleplayer;

	int		credits;
	int		reward;
	date_t	date;
	float	timer;

	vec2_t	center;
	float	zoom;

	//basemanagement
	vec2_t	basecenter;
	float	basezoom;
	vec2_t	baseField;
	int	actualBaseID;
} ccs_t;

typedef enum mapAction_s
{
	MA_NONE,
	MA_NEWBASE,	// build a new base
	MA_INTERCEPT,	// intercept TODO:
	MA_BASEATTACK	// base attacking
} mapAction_t;

typedef enum aircraftStatus_s
{
	AIR_NONE,
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
void CL_CampaignRun( void );
void CL_GameTimeStop( void );
qboolean CL_NewBase( vec2_t pos );
void CL_ParseMission( char *name, char **text );
void CL_ParseStage( char *name, char **text );
void CL_ParseCampaign( char *name, char **text );
void CL_ParseAircraft( char *name, char **text );
void CL_AircraftSelect( void );
void CL_NewAircraft_f ( void );
void CL_NewAircraft ( base_t* base, char* name );
void CL_AircraftInit( void );
void CL_CollectItems( int won );
void CL_UpdateCharacterStats ( int won );
void CL_UpdateCredits ( int credits );
//
// cl_menu.c
//

//#define MAX_MENUTEXTS		16
#define MAX_MENUTEXTLEN		512

typedef enum
{
	TEXT_STANDARD,
	TEXT_LIST,
	TEXT_UFOPEDIA,
	TEXT_BUILDINGS,
	TEXT_BUILDING_INFO,
	TEXT_RESEARCH = 5,
	TEXT_RESEARCH_INFO,
	TEXT_POPUP,
	TEXT_POPUP_INFO,
	TEXT_INTERCEPT_LIST,
	TEXT_AIRCRAFT = 10,
	TEXT_AIRCRAFT_INFO,

	MAX_MENUTEXTS
} texts_t;

qboolean MN_CursorOnMenu( int x, int y );
void MN_Click( int x, int y );
void MN_RightClick( int x, int y );
void MN_MiddleClick( int x, int y );
void MN_SetViewRect( void );
void MN_Command( void );
void MN_DrawMenus( void );
void MN_DrawItem( vec3_t org, item_t item, int sx, int sy, int x, int y, vec3_t scale, vec4_t color );

void MN_ResetMenus( void );
void MN_Shutdown( void );
void MN_ParseMenu( char *name, char **text );
void MN_PushMenu( char *name );
void MN_PopMenu( qboolean all );
void MN_Popup (char* title, char* text);

extern int numMenus;
extern inventory_t *menuInventory;
extern char *menuText[MAX_MENUTEXTS];

//
// cl_particle.c
//
void CL_ParticleRegisterArt( void );
void CL_ResetParticles( void );
void CL_ParticleRun( void );
void CL_RunMapParticles( void );
void CL_ParseParticle( char *name, char **text );
ptl_t *CL_ParticleSpawn( char *name, int levelFlags, vec3_t s, vec3_t v, vec3_t a );

extern ptlArt_t		ptlArt[MAX_PTL_ART];
extern ptl_t		ptl[MAX_PTLS];

extern int		numPtlArt;
extern int		numPtls;


//
// cl_parse.c
//
extern	char *svc_strings[256];
extern	char *ev_format[128];
extern	void (*ev_func[128])( sizebuf_t *sb );
extern	qboolean blockEvents;

void CL_ParseServerMessage (void);
void CL_LoadClientinfo (clientinfo_t *ci, char *s);
void SHOWNET(char *s);
void CL_ParseClientinfo (int player);
void CL_Download_f (void);
void CL_InitEvents (void);
void CL_Events (void);

//
// cl_view.c
//
extern sun_t	map_sun;
extern int		map_maxlevel;

void V_Init (void);
void V_RenderView( float stereo_separation );
void V_AddEntity (entity_t *ent);
void V_AddParticle (vec3_t org, int color, float alpha);
void V_AddLight (vec3_t org, float intensity, float r, float g, float b);
void V_AddLightStyle (int style, float r, float g, float b);
entity_t *V_GetEntity( void );
void V_CenterView( pos3_t pos );

//
// cl_sequence.c
//
void CL_SequenceRender( void );
void CL_Sequence2D( void );
void CL_SequenceStart_f( void );
void CL_SequenceEnd_f( void );
void CL_ResetSequences( void );
void CL_ParseSequence( char *name, char **text );

//
// cl_tent.c
//
void CL_RegisterTEntSounds (void);
void CL_RegisterTEntModels (void);
void CL_SmokeAndFlash(vec3_t origin);


//
// cl_pred.c
//
void CL_InitPrediction (void);
void CL_PredictMove (void);
void CL_CheckPredictionError (void);

//
// cl_fx.c
//
cdlight_t *CL_AllocDlight (int key);
void CL_BigTeleportParticles (vec3_t org);
void CL_RocketTrail (vec3_t start, vec3_t end, centity_t *old);
void CL_DiminishingTrail (vec3_t start, vec3_t end, centity_t *old, int flags);
void CL_FlyEffect (centity_t *ent, vec3_t origin);
void CL_BfgParticles (entity_t *ent);
void CL_AddParticles (void);
// RAFAEL
void CL_TrapParticles (entity_t *ent);

//
// menus
//
/*void M_Init (void);
void M_Keydown (int key);
void M_Draw (void);
void M_Menu_Main_f (void);
void M_ForceMenuOff (void);
void M_AddToServerList (netadr_t adr, char *info);*/



#if id386
void x86_TimerStart( void );
void x86_TimerStop( void );
void x86_TimerInit( unsigned long smallest, unsigned longest );
unsigned long *x86_TimerGetHistogram( void );
#endif

#endif

