/**
 * @file client.h
 * @brief Primary header for client.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/client.h
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

#ifndef CLIENT_CLIENT_H
#define CLIENT_CLIENT_H

#include "ref.h"
#include "vid.h"
#include "screen.h"
#include "sound.h"
#include "cl_input.h"
#include "cl_keys.h"
#include "cl_console.h"
#include "cl_market.h"
#include "cl_event.h"
#ifdef HAVE_OPENAL
	#ifndef DEDICATED_ONLY
		#include "qal.h"
		#include "snd_openal.h"
	#endif
#endif

#ifndef _MSC_VER
#include <zlib.h>
#else
#include "../ports/win32/zlib.h"
#endif

#ifdef HAVE_CURL
#define CURL_STATICLIB
#include <curl/curl.h>
#include "cl_http.h"
#endif /* HAVE_CURL */

/*============================================================================= */

#define MAX_TEAMLIST	8

/* Macros for faster access to the inventory-container. */
#define RIGHT(e) ((e)->i.c[csi.idRight])
#define LEFT(e)  ((e)->i.c[csi.idLeft])
#define FLOOR(e) ((e)->i.c[csi.idFloor])

typedef struct {
	int serverframe;			/**< if not current, this ent isn't in the frame */

	int trailcount;				/**< for diminishing grenade trails */
	vec3_t lerp_origin;			/**< for trails (variable hz) */

	int fly_stoptime;
} centity_t;

typedef struct {
	char name[MAX_QPATH];
	char cinfo[MAX_QPATH];
} clientinfo_t;

#define	CMD_BACKUP		64		/* allow a lot of command backups for very fast systems */

#define FOV				75.0
#define FOV_FPS			90.0
#define CAMERA_START_DIST   600
#define CAMERA_START_HEIGHT UNIT_HEIGHT*1.5
#define CAMERA_LEVEL_HEIGHT 64

typedef struct {
	vec3_t reforg;
	vec3_t camorg;
	vec3_t speed;
	vec3_t angles;
	vec3_t omega;
	vec3_t axis[3];				/**< set when refdef.angles is set */

	float lerplevel;
	float zoom;
	float fpzoom;
} camera_t;

typedef enum { CAMERA_MODE_REMOTE, CAMERA_MODE_FIRSTPERSON } camera_mode_t;

camera_mode_t camera_mode;

typedef enum {
	M_MOVE,		/**< We are currently in move-mode (destination selection). */
	M_FIRE_R,	/**< We are currently in fire-mode for the right weapon (target selection). */
	M_FIRE_L,	/**< We are currently in fire-mode for the left weapon (target selection). */
	M_PEND_MOVE,	/**< A move target has been selected, we are waiting for player-confirmation. */
	M_PEND_FIRE_R,	/**< A fire target (right weapon) has been selected, we are waiting for player-confirmation. */
	M_PEND_FIRE_L	/**< A fire target (left weapon) has been selected, we are waiting for player-confirmation. */
} cmodes_t;

#define IS_MODE_FIRE_RIGHT(x)	((x) == M_FIRE_R || (x) == M_PEND_FIRE_R)
#define IS_MODE_FIRE_LEFT(x)		((x) == M_FIRE_L || (x) == M_PEND_FIRE_L)

/**
 * @brief the client_state_t structure is wiped completely at every server map change
 * @note the client_static_t structure is persistant through an arbitrary
 * number of server connections
 */
typedef struct {
	int timeoutcount;

	int timedemo_frames;
	int timedemo_start;

	qboolean refresh_prepped;	/**< false if on new level or new ref dll */
	qboolean sound_prepped;		/**< ambient sounds can start */
	qboolean force_refdef;		/**< vid has changed, so we can't use a paused refdef */

	int surpressCount;			/**< number of messages rate supressed */

	int parse_entities;			/**< index (not anded off) into cl_parse_entities[] */

	int time;					/**< this is the time value that the client */
	/** is rendering at.  always <= cls.realtime */
	int eventTime;				/**< similar to time, but not counting if blockEvents is set */

	camera_t cam;

	refdef_t refdef;

	cmodes_t cmode;
	int cfiremode;
	cmodes_t oldcmode;
	int oldstate;

	int msgTime;
	char msgText[256];

	struct le_s *teamList[MAX_TEAMLIST];
	int numTeamList;
	int numAliensSpotted;

	/** server state information */
	qboolean attractloop;		/**< running the attract loop, any key will menu */
	int servercount;			/**< server identification for prespawns */
	char gamedir[MAX_QPATH];
	int pnum;
	int actTeam;

	char configstrings[MAX_CONFIGSTRINGS][MAX_TOKEN_CHARS];

	/** locally derived information from server state */
	struct model_s *model_draw[MAX_MODELS];
	struct cmodel_s *model_clip[MAX_MODELS];
	struct model_s *model_weapons[MAX_OBJDEFS];

	struct sfx_s *sound_precache[MAX_SOUNDS];
	struct image_s *image_precache[MAX_IMAGES];

	clientinfo_t clientinfo[MAX_CLIENTS];
	clientinfo_t baseclientinfo;
} client_state_t;

extern client_state_t cl;

typedef enum {
	ca_uninitialized,
	ca_disconnected,			/**< not talking to a server */
	ca_sequence,				/**< rendering a sequence */
	ca_connecting,				/**< sending request packets to the server */
	ca_connected,				/**< netchan_t established, waiting for svc_serverdata */
	ca_ptledit,					/**< particles should be rendered */
	ca_active					/**< game views should be displayed */
} connstate_t;

typedef enum { key_game, key_input, key_console, key_message } keydest_t;

typedef struct {
	connstate_t state;
	keydest_t key_dest;

	int framecount;
	int realtime;				/**< always increasing, no clamping, etc */
	float frametime;			/**< seconds since last frame */

	int framedelta;
	float framerate;

	/* screen rendering information */
	float disable_screen;		/* showing loading plaque between levels */
	/* or changing rendering dlls */
	/* if time gets > 30 seconds ahead, break it */
	int disable_servercount;	/* when we receive a frame and cl.servercount */
	/* > cls.disable_servercount, clear disable_screen */

	/* connection information */
	char servername[MAX_OSPATH];	/**< name of server from original connect */
	float connect_time;			/**< for connection retransmits */

	int ufoPort;				/* a 16 bit value that allows ufo servers */
								/* to work around address translating routers */
	netchan_t netchan;
	int serverProtocol;			/**< in case we are doing some kind of version hack */

	int challenge;				/**< from the server to use for connecting */

	/* demo recording info must be here, so it isn't cleared on level change */
	qboolean demorecording;
	qboolean demowaiting;		/**< don't record until a non-delta message is received */
	qFILE demofile;

	/** needs to be here, because server can be shutdown, before we see the ending screen */
	int team;

	char downloadtempname[MAX_OSPATH];
	char downloadname[MAX_OSPATH];
	size_t downloadposition;
	int downloadpercent;

#ifdef HAVE_CURL
	dlqueue_t downloadQueue;	/**< queue of paths we need */
	dlhandle_t HTTPHandles[4];	/**< actual download handles */
	char downloadServer[512];	/**< base url prefix to download from */
	char downloadReferer[32];	/**< libcurl requires a static string :( */
#endif
} client_static_t;

extern client_static_t cls;

/*============================================================================= */

/* cvars */
#ifdef HAVE_GETTEXT
extern cvar_t *s_language;
#endif /* HAVE_GETTEXT */
#ifdef HAVE_CURL
extern cvar_t *cl_http_downloads;
extern cvar_t *cl_http_filelists;
extern cvar_t *cl_http_proxy;
extern cvar_t *cl_http_max_connections;
#endif /* HAVE_CURL */
extern cvar_t *cl_isometric;
extern cvar_t *cl_stereo_separation;
extern cvar_t *cl_stereo;

extern cvar_t *cl_aviForceDemo;
extern cvar_t *cl_aviFrameRate;
extern cvar_t *cl_aviMotionJpeg;

extern cvar_t *cl_markactors;

extern cvar_t *cl_camrotspeed;
extern cvar_t *cl_camrotaccel;
extern cvar_t *cl_cammovespeed;
extern cvar_t *cl_cammoveaccel;
extern cvar_t *cl_camyawspeed;
extern cvar_t *cl_campitchmax;
extern cvar_t *cl_campitchmin;
extern cvar_t *cl_campitchspeed;
extern cvar_t *cl_camzoomquant;
extern cvar_t *cl_camzoommax;
extern cvar_t *cl_camzoommin;

extern cvar_t *cl_mapzoommax;
extern cvar_t *cl_mapzoommin;

extern cvar_t *cl_anglespeedkey;

extern cvar_t *cl_fps;
extern cvar_t *cl_shownet;
extern cvar_t *cl_show_tooltips;
extern cvar_t *cl_show_cursor_tooltips;

extern cvar_t *sensitivity;

extern cvar_t *cl_logevents;

extern cvar_t *cl_paused;
extern cvar_t *cl_timedemo;

extern cvar_t *cl_centerview;

extern cvar_t *cl_worldlevel;
extern cvar_t *cl_selected;

extern cvar_t *cl_3dmap;
extern cvar_t *gl_3dmapradius;

extern cvar_t *cl_numnames;

extern cvar_t *difficulty;
extern cvar_t *cl_start_employees;
extern cvar_t *cl_initial_equipment;
extern cvar_t *cl_start_buildings;

extern cvar_t *mn_serverlist;

extern cvar_t *mn_active;
extern cvar_t *mn_main;
extern cvar_t *mn_sequence;
extern cvar_t *mn_hud;
extern cvar_t *mn_lastsave;

extern cvar_t *snd_ref;

extern cvar_t *confirm_actions;
extern cvar_t *confirm_movement;

extern cvar_t *cl_precachemenus;

extern cvar_t *mn_inputlength;

/** limit the input for cvar editing (base name, save slots and so on) */
#define MAX_CVAR_EDITING_LENGTH 256 /* MAXCMDLINE */

typedef struct {
	int key;					/**< so entities can reuse same entry */
	vec3_t color;
	vec3_t origin;
	float radius;
	float die;					/**< stop lighting after this time */
	float decay;				/**< drop this each second */
	float minlight;				/**< don't add when contributing less */
} cdlight_t;

extern centity_t cl_entities[MAX_EDICTS];
extern cdlight_t cl_dlights[MAX_DLIGHTS];

/*============================================================================= */

extern netadr_t net_from;

void CL_AddNetgraph(void);

/*
=================================================
shader stuff
=================================================
*/

void CL_ShaderList_f(void);
void CL_ParseShaders(const char *name, char **text);
extern int r_numshaders;
extern shader_t r_shaders[MAX_SHADERS];

/* ================================================= */

void CL_ClearEffects(void);

void CL_SetLightstyle(int i);
void CL_RunLightStyles(void);
void CL_AddLightStyles(void);

void CL_AddEntities(void);

/*================================================= */

void CL_PrepRefresh(void);
void CL_RegisterSounds(void);
void CL_RegisterLocalModels(void);

void IN_Accumulate(void);

/* cl_save.c */
void SAV_Init(void);

/* cl_sequence.c (avi stuff) */
qboolean CL_OpenAVIForWriting(const char *filename);
void CL_TakeVideoFrame(void);
void CL_WriteAVIVideoFrame(const byte * imageBuffer, size_t size);
void CL_WriteAVIAudioFrame(const byte * pcmBuffer, size_t size);
qboolean CL_CloseAVI(void);
qboolean CL_VideoRecording(void);
void CL_StopVideo_f(void);
void CL_Video_f(void);

/* cl_main.c */
/* interface to refresh lib */
extern refexport_t re;

void CL_Init(void);
void CL_ReadSinglePlayerData(void);
void CL_RequestNextDownload(void);
void CL_StartSingleplayer(qboolean singleplayer);
void CL_Disconnect(void);
void CL_GetChallengePacket(void);
void CL_Snd_Restart_f(void);
void S_ModifySndRef_f(void);
void CL_ParseMedalsAndRanks(const char *name, char **text, byte parserank);
void CL_ParseUGVs(const char *name, char **text);
void CL_UpdateCharacterSkills(character_t *chr);	/* cl_team.c */
char* CL_ToDifficultyName(int difficulty);

extern qboolean loadingMessage;
extern char loadingMessages[96];
extern float loadingPercent;

/* cl_input */
typedef struct {
	int down[2];				/**< key nums holding it down */
	unsigned downtime;			/**< msec timestamp */
	unsigned msec;				/**< msec down this frame */
	int state;
} kbutton_t;

typedef enum {
	MS_NULL,
	MS_MENU,	/**< we are over some menu node */
	MS_DRAG,	/**< we are dragging some stuff / equipment */
	MS_ROTATE,	/**< we are rotating models (ufopedia) */
	MS_SHIFTMAP,	/**< we move the geoscape map */
	MS_ZOOMMAP,		/**< we zoom the geoscape map (also possible via mousewheels)*/
	MS_SHIFT3DMAP,	/**< we rotate the 3d globe */
	MS_WORLD		/**< we are in tactical mode */
} mouseSpace_t;

extern int mouseSpace;
extern int mx, my;
extern int dragFrom, dragFromX, dragFromY;
extern item_t dragItem;
extern float *rotateAngles;

extern kbutton_t in_mlook, in_klook;
extern const float MIN_ZOOM, MAX_ZOOM;

void CL_InitInput(void);
void CL_CameraMove(void);
void CL_CameraRoute(pos3_t from, pos3_t target);
void CL_ParseInput(void);

void CL_ClearState(void);

float CL_KeyState(kbutton_t * key);
char *Key_KeynumToString(int keynum);

void CL_CameraModeChange(camera_mode_t newcameramode);

void CL_Sniper(void);
void CL_SniperModeSet(void);
void CL_SniperModeUnset(void);

/* cl_le.c */

/** @brief a local entity */
typedef struct le_s {
	qboolean inuse;
	qboolean invis;
	qboolean selected;
	int type;
	int entnum;

	vec3_t origin, oldOrigin;
	pos3_t pos, oldPos;
	int dir;

	int TU, maxTU;
	int morale, maxMorale;
	int HP, maxHP;
	int STUN;					/**< if stunned - state STATE_STUN */
	int AP;
	int state;
	int reaction_minhit;

	float angles[3];
	float sunfrac;
	float alpha;

	int team;
	int pnum;

	int contents;
	vec3_t mins, maxs;

	int modelnum1, modelnum2, skinnum;
	struct model_s *model1, *model2;

/* 	character_t	*chr; */

	/** is called every frame */
	void (*think) (struct le_s * le);

	/** various think function vars */
	byte path[32];
	int pathLength, pathPos;
	int startTime, endTime;
	int speed;

	/** gfx */
	animState_t as;
	ptl_t *ptl;
	char *ref1, *ref2;
	inventory_t i;
	int left, right, extension;
	int fieldSize;				/**< ACTOR_SIZE_* */
	int teamDesc;
	int category; 	/**< nameCategory_t index */
	int gender;

	/** is called before adding a le to scene */
	qboolean(*addFunc) (struct le_s * le, entity_t * ent);
} le_t;							/* local entity */

#define MAX_LOCALMODELS		512
#define MAX_MAPPARTICLES	1024

#define LMF_LIGHTFIXED		1
#define LMF_NOSMOOTH		2

/** @brief local models */
typedef struct lm_s {
	char name[MAX_VAR];
	char particle[MAX_VAR];

	vec3_t origin;
	vec3_t angles;
	vec4_t lightorigin;
	vec4_t lightcolor;
	vec4_t lightambient;

	int num;
	int skin;
	int flags;
	int frame;	/**< which frame to show */
	char animname[MAX_QPATH];	/**< is this an animated model */
	int levelflags;
	float sunfrac;
	animState_t as;

	struct model_s *model;
} lm_t;							/* local models */

/** @brief map particles */
typedef struct mp_s {
	char ptl[MAX_QPATH];
	char *info;
	vec3_t origin;
	vec2_t wait;
	int nextTime;
	int levelflags;
} mp_t;							/* mapparticles */

extern lm_t LMs[MAX_LOCALMODELS];
extern mp_t MPs[MAX_MAPPARTICLES];

extern int numLMs;
extern int numMPs;

extern le_t LEs[MAX_EDICTS];
extern int numLEs;

static const vec3_t player_mins = { -PLAYER_WIDTH, -PLAYER_WIDTH, PLAYER_MIN };
static const vec3_t player_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND };
static const vec3_t player_dead_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_DEAD };

void LE_Think(void);
char *LE_GetAnim(const char *anim, int right, int left, int state);

void LE_AddProjectile(fireDef_t * fd, int flags, vec3_t muzzle, vec3_t impact, int normal);
void LE_AddGrenade(fireDef_t * fd, int flags, vec3_t muzzle, vec3_t v0, int dt);

void LET_StartIdle(le_t * le);
void LET_Appear(le_t * le);
void LET_StartPathMove(le_t * le);

void LM_Perish(sizebuf_t * sb);
void LM_Explode(sizebuf_t * sb);
void LM_DoorOpen(sizebuf_t * sb);
void LM_DoorClose(sizebuf_t * sb);

void LM_AddToScene(void);
void LE_AddToScene(void);
le_t *LE_Add(int entnum);
le_t *LE_Get(int entnum);
le_t *LE_Find(int type, pos3_t pos);
void LE_Cleanup(void);
trace_t CL_Trace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, le_t * passle, le_t * passle2, int contentmask);

lm_t *CL_AddLocalModel(char *model, char *particle, vec3_t origin, vec3_t angles, int num, int levelflags);
void CL_AddMapParticle(char *particle, vec3_t origin, vec2_t wait, char *info, int levelflags);
void CL_ParticleCheckRounds(void);
void CL_ParticleSpawnFromSizeBuf (sizebuf_t* sb);
void CL_ParticleFree(ptl_t *p);

/* cl_actor.c */
/* vertical cursor offset */
#define CURSOR_OFFSET UNIT_HEIGHT * 0.4
/* distance from vertical center of grid-point to head when standing */
#define EYE_HT_STAND  UNIT_HEIGHT * 0.25
/* distance from vertical center of grid-point to head when crouched */
#define EYE_HT_CROUCH UNIT_HEIGHT * 0.06
/* time-unit costs to reserve reaction fire and crouch/stand. Must match defines in game/g_local.h */
#define TU_CROUCH			1
#define TU_REACTION_SINGLE		7
#define TU_REACTION_MULTI		14

/* reaction fire toggle state, don't mess with the order!!! */
typedef enum {
	R_FIRE_OFF,
	R_FIRE_ONCE,
	R_FIRE_MANY
} reactionmode_t;

typedef enum {
	BT_RIGHT_PRIMARY,
	BT_RIGHT_PRIMARY_REACTION,
	BT_RIGHT_SECONDARY,
	BT_RIGHT_SECONDARY_REACTION, /* unused */
	BT_LEFT_PRIMARY,
	BT_LEFT_PRIMARY_REACTION,
	BT_LEFT_SECONDARY,
	BT_LEFT_SECONDARY_REACTION, /* unused */
	BT_RIGHT_RELOAD,
	BT_LEFT_RELOAD,
	BT_STAND,
	BT_CROUCH,
	BT_NUM_TYPES
} button_types_t;

extern le_t *selActor;
extern int actorMoveLength;
extern invList_t invList[MAX_INVLIST];

extern byte *fb_list[MAX_EDICTS];
extern int fb_length;

void MSG_Write_PA(player_action_t player_action, int num, ...);

void CL_CharacterCvars(character_t * chr);
void CL_UGVCvars(character_t *chr);
void CL_ActorUpdateCVars(void);
qboolean CL_CheckMenuAction(int time, invList_t *weapon, int mode);

void CL_DisplayHudMessage(const char *text, int time);
void CL_ResetWeaponButtons(void);
void CL_SetDefaultReactionFiremode(int actor_idx, char hand);
void CL_DisplayFiremodes_f(void);
void CL_FireWeapon_f(void);
void CL_SelectReactionFiremode_f(void);

void CL_ConditionalMoveCalc(struct routing_s *map, le_t *le, int distance);
qboolean CL_ActorSelect(le_t * le);
qboolean CL_ActorSelectList(int num);
qboolean CL_ActorSelectNext();
void CL_AddActorToTeamList(le_t * le);
void CL_RemoveActorFromTeamList(le_t * le);
void CL_ActorSelectMouse(void);
void CL_ActorReload(int hand);
void CL_ActorTurnMouse(void);
void CL_ActorDoTurn(sizebuf_t * sb);
void CL_ActorStandCrouch_f(void);
void CL_ActorToggleReaction_f(void);
void CL_ActorStartMove(le_t * le, pos3_t to);
void CL_ActorShoot(le_t * le, pos3_t at);
void CL_InvCheckHands(sizebuf_t * sb);
void CL_ActorDoMove(sizebuf_t * sb);
void CL_ActorDoShoot(sizebuf_t * sb);
void CL_ActorShootHidden(sizebuf_t *sb);
void CL_ActorDoThrow(sizebuf_t * sb);
void CL_ActorStartShoot(sizebuf_t * sb);
void CL_ActorDie(sizebuf_t * sb);
void CL_PlayActorSound(int category, int gender, actorSound_t soundType);

void CL_ActorActionMouse(void);

void CL_NextRound_f(void);
void CL_DoEndRound(sizebuf_t * sb);

void CL_ResetMouseLastPos(void);
void CL_ResetActorMoveLength(void);
void CL_ActorMouseTrace(void);

qboolean CL_AddActor(le_t * le, entity_t * ent);
qboolean CL_AddUGV(le_t * le, entity_t * ent);

void CL_AddTargeting(void);

/* cl_team.c */
#define MAX_ACTIVETEAM	8
#define MAX_WHOLETEAM	32
/* if you increase this, you also have to change the aircraft buy/sell menu scripts */
#define NUM_TEAMSKINS	4

void CL_SendItem(sizebuf_t * buf, item_t item, int container, int x, int y, qboolean save);
void CL_SendInventory(sizebuf_t * buf, inventory_t * i, qboolean save);
void CL_ReceiveItem(sizebuf_t * buf, item_t * item, int * container, int * x, int * y, qboolean save);
void CL_ReceiveInventory(sizebuf_t * buf, inventory_t * i, qboolean save);
void CL_ResetTeams(void);
void CL_ParseResults(sizebuf_t * buf);
void CL_SendCurTeamInfo(sizebuf_t * buf, character_t ** team, int num);
void CL_AddCarriedToEq(equipDef_t * equip);
void CL_ParseCharacterData(sizebuf_t *buf, qboolean updateCharacter);
qboolean CL_SoldierInAircraft(int employee_idx, int aircraft_idx);
void CL_RemoveSoldierFromAircraft(int employee_idx, int aircraft_idx);
void CL_RemoveSoldiersFromAircraft(int aircraft_idx, int base_idx);

/* cl_radar.c */
#define MAX_UFOONGEOSCAPE	8
struct aircraft_s;
struct menuNode_s;

typedef struct radar_s {
	int range;						/**< Range of radar */
	int ufos[MAX_UFOONGEOSCAPE];	/**< Ufos id sensored by radar (gd.ufos[id]) */
	int numUfos;					/**< Num ufos sensored by radar */
} radar_t;

extern void RADAR_DrawCoverage(const struct menuNode_s* node, const radar_t* radar, vec2_t pos, qboolean globe);
extern void RADAR_DrawInMap(const struct menuNode_s* node, const radar_t* radar, int x, int y, vec2_t pos, qboolean globe);
extern void RADAR_RemoveUfo(radar_t* radar, const struct aircraft_s* ufo);
extern void Radar_NotifyUfoRemoved(radar_t* radar, const struct aircraft_s* ufo);
extern void RADAR_ChangeRange(radar_t* radar, int change);
extern void Radar_Initialise(radar_t* radar, int range);
extern qboolean RADAR_CheckUfoSensored(radar_t* radar, vec2_t posRadar,
const struct aircraft_s* ufo, qboolean wasUfoSensored);

/* audio cd headers */
int CDAudio_Init(void);
void CDAudio_Shutdown(void);
void CDAudio_Play(int track, qboolean looping);
void CDAudio_Stop(void);
void CDAudio_Update(void);
void CDAudio_Activate (qboolean active);
void CDAudio_RandomPlay(void);

/* cl_hunk.c */
#include "cl_hunk.h"

/* cl_research.c */
#include "cl_research.h"

/* cl_produce.c */
#include "cl_produce.h"

/* cl_aliencont.c */
#include "cl_aliencont.h"

/* cl_basemanagment.c */
/* we need this in cl_aircraft.h */
#define MAX_EMPLOYEES 256

#include "cl_aircraft.h"

/* cl_basemanagment.c */
/* needs the MAX_ACTIVETEAM definition from above. */
#include "cl_basemanagement.h"

/* cl_inventory.c */
#include "cl_inventory.h"

/* cl_employee.c */
#include "cl_employee.h"

/* cl_hospital.c */
#include "cl_hospital.h"

/* cl_transfer.c */
#include "cl_transfer.h"

/* MISC */
/* @todo: needs to be sorted (e.g what file is it defined?) */
#define MAX_TEAMDATASIZE	32768

/* cl_team.c: CL_UpdateHireVar(), CL_ReloadAndRemoveCarried() */
/* cl_team.c should have own header file afterall 24042007 Zenerka */
qboolean CL_SoldierAwayFromBase(employee_t *soldier);
void CL_UpdateHireVar(aircraft_t *aircraft);
void CL_ReloadAndRemoveCarried(aircraft_t *aircraft, equipDef_t * equip);
void CL_CleanTempInventory(base_t* base);

void CL_ResetCharacters(base_t* const base);
void CL_ResetTeamInBase(void);
void CL_GenerateCharacter(employee_t *employee, const char *team, employeeType_t employeeType);
char* CL_GetTeamSkinName(int id);

void MN_BuildNewBase(vec2_t pos);

/* END MISC */

/* stats */

typedef struct stats_s {
	int missionsWon;
	int missionsLost;
	int basesBuild;
	int basesAttacked;
	int interceptions;
	int soldiersLost;
	int soldiersNew;			/**< new recruits */
	int killedAliens;
	int rescuedCivilians;
	int researchedTechnologies;
	int moneyInterceptions;
	int moneyBases;
	int moneyResearch;
	int moneyWeapons;
} stats_t;

extern stats_t stats;

/* message systems */
typedef enum {
	MSG_DEBUG,			/**< only save them in debug mode */
	MSG_INFO,			/**< don't save these messages */
	MSG_STANDARD,
	MSG_RESEARCH_PROPOSAL,
	MSG_RESEARCH_FINISHED,
	MSG_CONSTRUCTION,
	MSG_UFOSPOTTED,
	MSG_TERRORSITE,
	MSG_BASEATTACK,
	MSG_TRANSFERFINISHED,
	MSG_PROMOTION,
	MSG_PRODUCTION,
	MSG_NEWS,
	MSG_DEATH,

	MSG_MAX
} messagetype_t;

/** @brief also used for chat message buffer */
#define MAX_MESSAGE_TEXT 1024
#define TIMESTAMP_TEXT 21
typedef struct message_s {
	char title[MAX_VAR];
	char *text;
	messagetype_t type;
	technology_t *pedia;		/**< link to ufopedia if a research has finished. */
	struct message_s *next;
	int d, m, y, h, min, s;
} message_t;

char messageBuffer[MAX_MESSAGE_TEXT];

extern message_t *messageStack;

message_t *MN_AddNewMessage(const char *title, const char *text, qboolean popup, messagetype_t type, technology_t * pedia);
void MN_RemoveMessage(char *title);
void CL_InitMessageSystem(void);

void MN_AddChatMessage(const char *text);

#include "cl_campaign.h"

#include "cl_menu.h"

void CL_SpawnSoldiers(void);

void B_DrawBase(menuNode_t * node);

/* cl_particle.c */
void CL_ParticleRegisterArt(void);
void CL_ResetParticles(void);
void CL_ParticleRun(void);
void CL_RunMapParticles(void);
int CL_ParseParticle(const char *name, char **text);
void CL_InitParticles(void);
ptl_t *CL_ParticleSpawn(const char *name, int levelFlags, const vec3_t s, const vec3_t v, const vec3_t a);
void PE_RenderParticles(void);

extern ptlArt_t ptlArt[MAX_PTL_ART];
extern ptl_t ptl[MAX_PTLS];

extern int numPtlArt;
extern int numPtls;


/* cl_parse.c */
extern const char *ev_format[128];
extern qboolean blockEvents;

void CL_SetLastMoving(le_t *le);
void CL_ParseServerMessage(void);
void CL_InitEvents(void);
void CL_Events(void);
qboolean CL_CheckOrDownloadFile(const char *filename);

/* cl_view.c */
extern sun_t map_sun;
extern int map_maxlevel;
extern int map_maxlevel_base;
extern cvar_t *map_dropship;
extern vec3_t map_dropship_coord;

void V_Init(void);
void V_RenderView(float stereo_separation);
void V_UpdateRefDef(void);
void V_AddEntity(entity_t * ent);
void V_AddLight(vec3_t org, float intensity, float r, float g, float b);
void V_AddLightStyle(int style, float r, float g, float b);
entity_t *V_GetEntity(void);
void V_CenterView(pos3_t pos);
void CalcFovX(void);

/* cl_sequence.c */
void CL_SequenceRender(void);
void CL_Sequence2D(void);
void CL_SequenceClick_f(void);
void CL_SequenceStart_f(void);
void CL_SequenceEnd_f(void);
void CL_ResetSequences(void);
void CL_ParseSequence(const char *name, char **text);

/* cl_fx.c */
cdlight_t *CL_AllocDlight(int key);
void CL_AddParticles(void);


#if id386
void x86_TimerStart(void);
void x86_TimerStop(void);
void x86_TimerInit(unsigned long smallest, unsigned longest);
unsigned long *x86_TimerGetHistogram(void);
#endif /* id386 */

/* cl_map.c */
#define GLOBE_ROTATE -90
extern nation_t* MAP_GetNation(const vec2_t pos);
extern qboolean MAP_MapToScreen(const menuNode_t* node, const vec2_t pos, int *x, int *y);
extern qboolean MAP_3DMapToScreen(const menuNode_t* node, const vec2_t pos, int *x, int *y, int *z);
extern void MAP_MapCalcLine(const vec2_t start, const vec2_t end, mapline_t* line);
extern void MAP_DrawMap(const menuNode_t* node, qboolean map3D);
extern void MAP_MapClick(const menuNode_t * node, int x, int y, qboolean globe);
extern void MAP_ResetAction(void);
extern void MAP_SelectAircraft(aircraft_t* aircraft);
extern void MAP_SelectMission(actMis_t* mission);
extern void MAP_NotifyMissionRemoved(const actMis_t* mission);
extern void MAP_NotifyUfoRemoved(const aircraft_t* ufo);
extern void MAP_NotifyUfoDisappear(const aircraft_t* ufo);
extern void MAP_GameInit(void);
extern const char* MAP_GetZoneType(byte* color);

/* cl_ufo.c */
extern void UFO_CampaignRunUfos(int dt);
extern void UFO_CampaignCheckEvents(void);
extern void UFO_Reset(void);
extern void UFO_RemoveUfoFromGeoscape(aircraft_t* ufo);
extern void UFO_PrepareRecovery(base_t *base);
extern void UFO_Recovery(void);

/* cl_popup.c */
extern void CL_PopupInit(void);
extern void CL_PopupNotifyMIssionRemoved(const actMis_t* mission);
extern void CL_PopupNotifyUfoRemoved(const aircraft_t* ufo);
extern void CL_PopupNotifyUfoDisappeared(const aircraft_t* ufo);
extern void CL_DisplayPopupAircraft(const aircraft_t* aircraft);
extern void CL_DisplayPopupIntercept(struct actMis_s* mission, aircraft_t* ufo);

/* keys.c */
extern char *keybindings[K_LAST_KEY];

/* os specific video functions */
void VID_Error(int err_level, const char *fmt, ...) __attribute__((noreturn));

#endif /* CLIENT_CLIENT_H */
