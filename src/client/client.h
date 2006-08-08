/**
 * @file client.h
 * @brief Primary header for client.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

Changes:
15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

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
#include "input.h"
#include "keys.h"
#include "console.h"
#include "cdaudio.h"
#include "cl_market.h"

/*============================================================================= */
typedef struct tutorial_s {
	char name[MAX_VAR];
	char sequence[MAX_VAR];
} tutorial_t;

#define MAX_TUTORIALS 16

extern tutorial_t tutorials[MAX_TUTORIALS];

/*============================================================================= */

#define MAX_TEAMLIST	8

#define RIGHT(e) ((e)->i.c[csi.idRight])
#define LEFT(e)  ((e)->i.c[csi.idLeft])
#define FLOOR(e) ((e)->i.c[csi.idFloor])

typedef struct {
	int serverframe;			/* if not current, this ent isn't in the frame */

	int trailcount;				/* for diminishing grenade trails */
	vec3_t lerp_origin;			/* for trails (variable hz) */

	int fly_stoptime;
} centity_t;

#define MAX_CLIENTWEAPONMODELS		20	/* PGM -- upped from 16 to fit the chainfist vwep */

typedef struct {
	char name[MAX_QPATH];
	char cinfo[MAX_QPATH];
	struct image_s *skin;
	struct image_s *icon;
	char iconname[MAX_QPATH];
	struct model_s *model;
	struct model_s *weaponmodel[MAX_CLIENTWEAPONMODELS];
} clientinfo_t;

extern char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
extern int num_cl_weaponmodels;

#define	CMD_BACKUP		64		/* allow a lot of command backups for very fast systems */

#define FOV				60.0

typedef struct {
	vec3_t reforg;
	vec3_t camorg;
	vec3_t speed;
	vec3_t angles;
	vec3_t omega;
	vec3_t axis[3];				/* set when refdef.angles is set */

	float lerplevel;
	float zoom;
	float fpzoom;
} camera_t;

typedef enum { CAMERA_MODE_REMOTE, CAMERA_MODE_FIRSTPERSON } camera_mode_t;

camera_mode_t camera_mode;

/* don't mess with the order!!! */
typedef enum {
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

/* the client_state_t structure is wiped completely at every */
/* server map change */
typedef struct {
	int timeoutcount;

	int timedemo_frames;
	int timedemo_start;

	qboolean refresh_prepped;	/* false if on new level or new ref dll */
	qboolean sound_prepped;		/* ambient sounds can start */
	qboolean force_refdef;		/* vid has changed, so we can't use a paused refdef */

	int surpressCount;			/* number of messages rate supressed */

	int parse_entities;			/* index (not anded off) into cl_parse_entities[] */

	int time;					/* this is the time value that the client */
	/* is rendering at.  always <= cls.realtime */
	int eventTime;				/* similar to time, but not counting if blockEvents is set */

	camera_t cam;

	refdef_t refdef;

	int cmode;
	int oldcmode;
	int oldstate;

	int msgTime;
	char msgText[256];

	struct le_s *teamList[MAX_TEAMLIST];
	int numTeamList;
	int numAliensSpotted;

	/* transient data from server */
	char layout[1024];			/* general 2D overlay */

	/* non-gameserver infornamtion */
	/* FIXME: move this cinematic stuff into the cin_t structure */
	FILE *cinematic_file;
	int cinematictime;			/* cls.realtime for first cinematic frame */
	int cinematicframe;
	char cinematicpalette[768];
	qboolean cinematicpalette_active;

	/* server state information */
	qboolean attractloop;		/* running the attract loop, any key will menu */
	int servercount;			/* server identification for prespawns */
	char gamedir[MAX_QPATH];
	int pnum;
	int actTeam;

	char configstrings[MAX_CONFIGSTRINGS][MAX_TOKEN_CHARS];

	/* locally derived information from server state */
	struct model_s *model_draw[MAX_MODELS];
	struct cmodel_s *model_clip[MAX_MODELS];
	struct model_s *model_weapons[MAX_OBJDEFS];

	struct sfx_s *sound_precache[MAX_SOUNDS];
	struct image_s *image_precache[MAX_IMAGES];

	clientinfo_t clientinfo[MAX_CLIENTS];
	clientinfo_t baseclientinfo;
} client_state_t;

extern client_state_t cl;

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/

typedef enum {
	ca_uninitialized,
	ca_disconnected,			/* not talking to a server */
	ca_sequence,				/* rendering a sequence */
	ca_connecting,				/* sending request packets to the server */
	ca_connected,				/* netchan_t established, waiting for svc_serverdata */
	ca_active					/* game views should be displayed */
} connstate_t;

typedef enum { key_game, key_console, key_message } keydest_t;

typedef struct {
	connstate_t state;
	keydest_t key_dest;

	int framecount;
	int realtime;				/* always increasing, no clamping, etc */
	float frametime;			/* seconds since last frame */

	int framedelta;
	float framerate;

	/* screen rendering information */
	float disable_screen;		/* showing loading plaque between levels */
	/* or changing rendering dlls */
	/* if time gets > 30 seconds ahead, break it */
	int disable_servercount;	/* when we receive a frame and cl.servercount */
	/* > cls.disable_servercount, clear disable_screen */

	/* connection information */
	char servername[MAX_OSPATH];	/* name of server from original connect */
	float connect_time;			/* for connection retransmits */

	int quakePort;				/* a 16 bit value that allows quake servers */
	/* to work around address translating routers */
	netchan_t netchan;
	int serverProtocol;			/* in case we are doing some kind of version hack */

	int challenge;				/* from the server to use for connecting */

	/* demo recording info must be here, so it isn't cleared on level change */
	qboolean demorecording;
	qboolean demowaiting;		/* don't record until a non-delta message is received */
	FILE *demofile;

	/* needs to be here, because server can be shutdown, before we see the ending screen */
	int team;
} client_static_t;

extern client_static_t cls;

/*============================================================================= */

/* cvars */
#ifdef HAVE_GETTEXT
extern cvar_t *s_language;
#endif
extern cvar_t *cl_stereo_separation;
extern cvar_t *cl_stereo;

extern cvar_t *cl_aviFrameRate;
extern cvar_t *cl_aviMotionJpeg;

extern cvar_t *cl_add_blend;
extern cvar_t *cl_add_lights;
extern cvar_t *cl_add_particles;
extern cvar_t *cl_add_entities;
extern cvar_t *cl_predict;
extern cvar_t *cl_noskins;
extern cvar_t *cl_autoskins;
extern cvar_t *cl_markactors;

extern cvar_t *cl_camrotspeed;
extern cvar_t *cl_camrotaccel;
extern cvar_t *cl_cammovespeed;
extern cvar_t *cl_cammoveaccel;
extern cvar_t *cl_camyawspeed;
extern cvar_t *cl_campitchspeed;
extern cvar_t *cl_camzoomquant;

extern cvar_t *cl_run;

extern cvar_t *cl_anglespeedkey;

extern cvar_t *cl_fps;
extern cvar_t *cl_shownet;
extern cvar_t *cl_show_tooltips;

extern cvar_t *lookspring;
extern cvar_t *lookstrafe;
extern cvar_t *sensitivity;

extern cvar_t *m_pitch;
extern cvar_t *m_yaw;
extern cvar_t *m_forward;
extern cvar_t *m_side;

extern cvar_t *freelook;

extern cvar_t *cl_logevents;

extern cvar_t *cl_paused;
extern cvar_t *cl_timedemo;

extern cvar_t *cl_centerview;

extern cvar_t *cl_worldlevel;
extern cvar_t *cl_selected;

extern cvar_t *cl_numnames;

extern cvar_t *difficulty;

extern cvar_t *mn_active;
extern cvar_t *mn_main;
extern cvar_t *mn_sequence;
extern cvar_t *mn_hud;
extern cvar_t *mn_lastsave;

extern cvar_t *snd_ref;

extern cvar_t *confirm_actions;

extern cvar_t *cl_precachemenus;

typedef struct {
	int key;					/* so entities can reuse same entry */
	vec3_t color;
	vec3_t origin;
	float radius;
	float die;					/* stop lighting after this time */
	float decay;				/* drop this each second */
	float minlight;				/* don't add when contributing less */
} cdlight_t;

extern centity_t cl_entities[MAX_EDICTS];
extern cdlight_t cl_dlights[MAX_DLIGHTS];

/*============================================================================= */

extern netadr_t net_from;

void CL_AddNetgraph(void);

/*================================================= */
/* shader stuff */
/* ================================================= */

void CL_ShaderList_f(void);
void CL_ParseShaders(char *title, char **text);
extern int r_numshaders;
extern shader_t r_shaders[MAX_SHADERS];

/* ================================================= */

/* ======== */
/* PGM */
typedef struct particle_s {
	struct particle_s *next;

	float time;

	vec3_t org;
	vec3_t vel;
	vec3_t accel;

	float color;
	float colorvel;
	float alpha;
	float alphavel;
} cparticle_t;


#define	PARTICLE_GRAVITY	40
#define BLASTER_PARTICLE_COLOR		0xe0

void CL_ClearEffects(void);
void CL_ClearTEnts(void);

int CL_ParseEntityBits(unsigned *bits);

void CL_ParseTEnt(void);
void CL_ParseConfigString(void);

void CL_SetLightstyle(int i);
void CL_RunLightStyles(void);
void CL_AddLightStyles(void);

void CL_AddEntities(void);
void CL_AddTEnts(void);

/*================================================= */

void CL_PrepRefresh(void);
void CL_RegisterSounds(void);
void CL_RegisterLocalModels(void);

void CL_Quit_f(void);

void IN_Accumulate(void);

void CL_ParseLayout(void);

/* cl_sequence.c (avi stuff) */
qboolean CL_OpenAVIForWriting(char *filename);
void CL_TakeVideoFrame(void);
void CL_WriteAVIVideoFrame(const byte * imageBuffer, int size);
void CL_WriteAVIAudioFrame(const byte * pcmBuffer, int size);
qboolean CL_CloseAVI(void);
qboolean CL_VideoRecording(void);
void CL_StopVideo_f(void);
void CL_Video_f(void);

/* cl_main */
/* interface to refresh lib */
extern refexport_t re;

void CL_Init(void);
void CL_ReadSinglePlayerData( void );

void CL_FixUpGender(void);
void CL_Disconnect(void);
void CL_Disconnect_f(void);
void CL_GetChallengePacket(void);
void CL_PingServers_f(void);
void CL_Snd_Restart_f(void);
void CL_ParseMedalsAndRanks( char *title, char **text, byte parserank );
void CL_ParseUGVs(char *title, char **text);
char* CL_ToDifficultyName(int difficulty);

/* cl_input */
typedef struct {
	int down[2];				/* key nums holding it down */
	unsigned downtime;			/* msec timestamp */
	unsigned msec;				/* msec down this frame */
	int state;
} kbutton_t;

typedef enum {
	MS_NULL,
	MS_MENU,
	MS_DRAG,
	MS_ROTATE,
	MS_SHIFTMAP,
	MS_ZOOMMAP,
	MS_SHIFT3DMAP,
	MS_ZOOM3DMAP,
	MS_WORLD
} mouseSpace_t;

extern int mouseSpace;
extern int mx, my;
extern int dragFrom, dragFromX, dragFromY;
extern item_t dragItem;
extern float *rotateAngles;

extern kbutton_t in_mlook, in_klook;
extern kbutton_t in_strafe;
extern kbutton_t in_speed;
extern float MIN_ZOOM, MAX_ZOOM;

void CL_InitInput(void);
void CL_CameraMove(void);
void CL_CameraRoute(pos3_t from, pos3_t target);
void CL_ParseInput(void);

void CL_ClearState(void);

void CL_ReadPackets(void);

int CL_ReadFromServer(void);

float CL_KeyState(kbutton_t * key);
char *Key_KeynumToString(int keynum);

void CL_CameraMode(void);
void CL_CameraModeChange(camera_mode_t newcameramode);

void CL_Sniper(void);
void CL_SniperModeSet(void);
void CL_SniperModeUnset(void);

/* cl_le.c */
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
	int STUN;					/* if stunned - state STATE_STUN */
	int AP;
	int state;

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

	/* is called every frame */
	void (*think) (struct le_s * le);

	/* various think function vars */
	byte path[32];
	int pathLength, pathPos;
	int startTime, endTime;
	int speed;

	/* gfx */
	animState_t as;
	ptl_t *ptl;
	char *ref1, *ref2;
	inventory_t i;
	int left, right;
	int fieldSize;				/* ACTOR_SIZE_* */

	/* is called before adding a le to scene */
	qboolean(*addFunc) (struct le_s * le, entity_t * ent);
} le_t;							/* local entity */

#define MAX_LOCALMODELS		512
#define MAX_MAPPARTICLES	1024

#define LMF_LIGHTFIXED		1
#define LMF_NOSMOOTH		2

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
	int levelflags;
	float sunfrac;

	struct model_s *model;
} lm_t;							/* local models */

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

static const vec3_t player_mins = { -PLAYER_WIDTH, -PLAYER_WIDTH, -PLAYER_MIN };
static const vec3_t player_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND };
/*extern vec3_t player_mins;
extern vec3_t player_maxs;*/

void LE_Status(void);
void LE_Think(void);
char *LE_GetAnim(char *anim, int right, int left, int state);

void LE_AddProjectile(fireDef_t * fd, int flags, vec3_t muzzle, vec3_t impact, int normal);
void LE_AddGrenade(fireDef_t * fd, int flags, vec3_t muzzle, vec3_t v0, int dt);

void LET_StartIdle(le_t * le);
void LET_Appear(le_t * le);
void LET_Perish(le_t * le);
void LET_StartPathMove(le_t * le);

void LM_Perish(sizebuf_t * sb);
void LM_Explode(sizebuf_t * sb);

void LM_AddToScene(void);
void LE_AddToScene(void);
le_t *LE_Add(int entnum);
le_t *LE_Get(int entnum);
le_t *LE_Find(int type, pos3_t pos);
trace_t CL_Trace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, le_t * passle, int contentmask);

lm_t *CL_AddLocalModel(char *model, char *particle, vec3_t origin, vec3_t angles, int num, int levelflags);
void CL_AddMapParticle(char *particle, vec3_t origin, vec2_t wait, char *info, int levelflags);
void CL_ParticleCheckRounds(void);
void CL_ParticleSpawnFromSizeBuf (sizebuf_t* sb);

/* cl_actor.c */
extern le_t *selActor;
extern int actorMoveLength;
extern invList_t invList[MAX_INVLIST];

extern byte *fb_list[MAX_FB_LIST];
extern int fb_length;

void CL_CharacterCvars(character_t * chr);
void CL_UGVCvars(character_t *chr);
void CL_ActorUpdateCVars(void);

void CL_DisplayHudMessage(char *text, int time);

qboolean CL_ActorSelect(le_t * le);
qboolean CL_ActorSelectList(int num);
void CL_AddActorToTeamList(le_t * le);
void CL_RemoveActorFromTeamList(le_t * le);
void CL_ActorSelectMouse(void);
void CL_ActorReload(int hand);
void CL_ActorTurnMouse(void);
void CL_ActorDoTurn(sizebuf_t * sb);
void CL_ActorStandCrouch(void);
void CL_ActorStun(void);
void CL_ActorToggleReaction(void);
void CL_BuildForbiddenList(void);
void CL_ActorStartMove(le_t * le, pos3_t to);
void CL_ActorShoot(le_t * le, pos3_t at);
void CL_ActorDoMove(sizebuf_t * sb);
void CL_ActorDoShoot(sizebuf_t * sb);
void CL_ActorDoThrow(sizebuf_t * sb);
void CL_ActorStartShoot(sizebuf_t * sb);
void CL_ActorDie(sizebuf_t * sb);

void CL_ActorActionMouse(void);

void CL_NextRound(void);
void CL_DoEndRound(sizebuf_t * sb);

void CL_ResetMouseLastPos(void);
void CL_ActorMouseTrace(void);

qboolean CL_AddActor(le_t * le, entity_t * ent);
qboolean CL_AddUGV(le_t * le, entity_t * ent);

void CL_AddTargeting(void);

/* cl_team.c */
#define MAX_ACTIVETEAM	8
#define MAX_WHOLETEAM	32
#define NUM_BUYTYPES	4
#define NUM_TEAMSKINS	4

extern char *teamSkinNames[NUM_TEAMSKINS];

void CL_SendItem(sizebuf_t * buf, item_t item, int container, int x, int y);
void CL_ResetTeams(void);
void CL_ParseResults(sizebuf_t * buf);
void CL_SendTeamInfo(sizebuf_t * buf, character_t * team, int num);
void CL_SendCurTeamInfo(sizebuf_t * buf, character_t ** team, int num);
void CL_CheckInventory(equipDef_t * equip);
void CL_ParseCharacterData(sizebuf_t *buf, qboolean updateCharacter);

/* cl_radar.c */
#define MAX_UFOONGEOSCAPE	8
struct aircraft_s;
struct menuNode_s;

typedef struct radar_s {
	int range;						/* Range of radar */
	int ufos[MAX_UFOONGEOSCAPE];	/* Ufos id sensored by radar (gd.ufos[id]) */
	int numUfos;					/* Num ufos sensored by radar */
} radar_t;

extern void RADAR_DrawInMap(const struct menuNode_s* node, const radar_t* radar, int x, int y, vec2_t pos);
extern void RADAR_RemoveUfo(radar_t* radar, const struct aircraft_s* ufo);
extern void Radar_NotifyUfoRemoved(radar_t* radar, const struct aircraft_s* ufo);
extern void RADAR_ChangeRange(radar_t* radar, int change);
extern void Radar_Initialise(radar_t* radar, int range);
extern qboolean RADAR_CheckUfoSensored(radar_t* radar, vec2_t posRadar,
	const struct aircraft_s* ufo, qboolean wasUfoSensored);

/* cl_research.c */
#include "cl_research.h"

/* cl_produce.c */
#include "cl_produce.h"

/* SAVEGAMES */
#ifndef SAVE_FILE_VERSION
#define SAVE_FILE_VERSION 4
#endif							/* SAVE_FILE_VERSION */

#include "cl_aircraft.h"

/* cl_basemanagment.c */
/* needs the MAX_ACTIVETEAM definition from above. */
#include "cl_basemanagement.h"

/* cl_employee.c */
#include "cl_employee.h"

/* MISC */
/* TODO: needs to be sorted (e.g what file is it defined?) */
#define MAX_TEAMDATASIZE	32768
#define MAX_GAMESAVESIZE	MAX_TEAMDATASIZE + 16384 + sizeof(globalData_t)
#define MAX_COMMENTLENGTH	32

void CL_LoadTeam(sizebuf_t *sb, base_t *base, int version);
void CL_UpdateHireVar(void);

void CL_ResetCharacters(base_t *base);
void CL_GenerateCharacter(char *team, base_t *base, int type);

void MN_BuildNewBase(vec2_t pos);

void MN_GetMaps_f(void);
void CL_ListMaps_f(void);
void MN_NextMap(void);
void MN_PrevMap(void);

/* END MISC */

/* stats */

/* beware - if you add new stuff to stats_t */
/* the loading of an older savegame will not work */
typedef struct stats_s {
	int missionsWon;
	int missionsLost;
	int basesBuild;
	int basesAttacked;
	int interceptions;
	int soldiersLost;
	int soldiersNew;			/* new recruits */
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
	MSG_DEBUG,
	MSG_STANDARD,
	MSG_RESEARCH,
	MSG_CONSTRUCTION,
	MSG_UFOSPOTTED,
	MSG_TERRORSITE,
	MSG_BASEATTACK,
	MSG_TRANSFERFINISHED,
	MSG_PROMOTION,
	MSG_PRODUCTION,
	MSG_DEATH
} messagetype_t;

#define MAX_MESSAGE_TEXT 1024
typedef struct message_s {
	char title[MAX_VAR];
	char text[MAX_MESSAGE_TEXT];
	messagetype_t type;
	technology_t *pedia;		/* link to ufopedia if a research has finished. */
	struct message_s *next;
	int d, m, y, h, min, s;
} message_t;

char messageBuffer[MAX_MESSAGE_TEXT];

message_t *messageStack;

message_t *MN_AddNewMessage(const char *title, const char *text, qboolean popup, messagetype_t type, technology_t * pedia);
void MN_RemoveMessage(char *title);
void CL_InitMessageSystem(void);

#include "cl_campaign.h"

/* cl_menu.c */
#define MAX_MENUS			64
#define MAX_MENUNODES		4096
#define MAX_MENUACTIONS		4096
#define MAX_MENUSTACK		16
#define MAX_MENUMODELS		128

#define MAX_MENU_COMMAND	32
#define MAX_MENU_PICLINK	64

/* one unit in the containers is 25x25 */
#define C_UNIT				25
#define C_UNDEFINED			0xFE

#define MAX_MENUTEXTLEN		4096


typedef struct menuModel_s {
	char id[MAX_VAR];
	char need[MAX_VAR];
	char anim[MAX_VAR];
	int skin;
	char model[MAX_QPATH];
	animState_t animState;
	vec3_t origin, scale, angles, center;
	vec4_t color;				/* rgba */
	struct menuModel_s *next;
} menuModel_t;

typedef struct menuAction_s {
	int type;
	void *data;
	struct menuAction_s *next;
} menuAction_t;

typedef struct menuNode_s {
	void *data[6];				/* needs to be first */
	char name[MAX_VAR];
	int type;
	vec3_t origin, scale, angles, center;
	vec2_t pos, size, texh, texl;
	menuModel_t *menuModel;
	byte state;
	byte align;
	byte invis, blend;
	int mousefx;
	int textScroll;				/* textfields - current scroll position */
	int timeOut;				/* ms value until invis is set (see cl.time) */
	int num, height;			/* textfields - num: menutexts-id; height: max. rows to show */
	vec4_t color;				/* rgba */
	vec4_t bgcolor;				/* rgba */
	menuAction_t *click, *rclick, *mclick, *mouseIn, *mouseOut;
	menuDepends_t depends;
	struct menuNode_s *next;
} menuNode_t;

typedef struct menu_s {
	char name[MAX_VAR];
	menuNode_t *firstNode, *initNode, *closeNode, *renderNode, *popupNode, *hoverNode;
} menu_t;

typedef enum {
	TEXT_STANDARD,
	TEXT_LIST,
	TEXT_UFOPEDIA,
	TEXT_BUILDINGS,
	TEXT_BUILDING_INFO,
	TEXT_RESEARCH = 5,
	TEXT_RESEARCH_INFO,
	TEXT_POPUP,
	TEXT_POPUP_INFO,
	TEXT_AIRCRAFT_LIST,
	TEXT_AIRCRAFT = 10,
	TEXT_AIRCRAFT_INFO,
	TEXT_MESSAGESYSTEM,			/* just a dummy for messagesystem - we use the stack */
	TEXT_CAMPAIGN_LIST,
	TEXT_MULTISELECTION,
	TEXT_PRODUCTION_LIST = 15,
	TEXT_PRODUCTION_AMOUNT,
	TEXT_PRODUCTION_INFO,
	TEXT_EMPLOYEE,

	MAX_MENUTEXTS
} texts_t;

qboolean MN_CursorOnMenu(int x, int y);
void MN_Click(int x, int y);
void MN_RightClick(int x, int y);
void MN_MiddleClick(int x, int y);
void MN_SetViewRect(const menu_t* menu);
void MN_DrawMenus(void);
void MN_DrawItem(vec3_t org, item_t item, int sx, int sy, int x, int y, vec3_t scale, vec4_t color);
void MN_ShutdownMessageSystem(void);
void MN_UnHideNode ( menuNode_t* node );
void MN_HideNode ( menuNode_t* node );
menuNode_t* MN_GetNodeFromCurrentMenu(char*name);
void MN_SetNewNodePos (menuNode_t* node, int x, int y);
menuNode_t *MN_GetNode(menu_t * menu, char *name);
menu_t *MN_GetMenu(char *name);

void MN_ResetMenus(void);
void MN_Shutdown(void);
void MN_ParseMenu(char *name, char **text);
void MN_ParseMenuModel(char *name, char **text);
menu_t* MN_PushMenu(char *name);
void MN_PopMenu(qboolean all);
menu_t* MN_ActiveMenu(void);
void MN_Popup(const char *title, const char *text);
void MN_ParseTutorials(char *title, char **text);

void B_DrawBase(menuNode_t * node);

extern inventory_t *menuInventory;
extern char *menuText[MAX_MENUTEXTS];

/* this is the function where all the sdl_ttf fonts are parsed */
void CL_ParseFont(char *name, char **text);

/* here they get reinit after a vid_restart */
void CL_InitFonts(void);

typedef struct font_s {
	char name[MAX_VAR];
	int size;
	char style[MAX_VAR];
	char path[MAX_QPATH];
} font_t;

extern font_t *fontSmall;
extern font_t *fontBig;

/* will return the size and the path for each font */
void CL_GetFontData(char *name, int *size, char *path);

/* cl_particle.c */
void CL_ParticleRegisterArt(void);
void CL_ResetParticles(void);
void CL_ParticleRun(void);
void CL_RunMapParticles(void);
void CL_ParseParticle(char *name, char **text);
ptl_t *CL_ParticleSpawn(char *name, int levelFlags, vec3_t s, vec3_t v, vec3_t a);

extern ptlArt_t ptlArt[MAX_PTL_ART];
extern ptl_t ptl[MAX_PTLS];

extern int numPtlArt;
extern int numPtls;


/* cl_parse.c */
extern char *svc_strings[256];
extern char *ev_format[128];
extern void (*ev_func[128]) (sizebuf_t * sb);
extern qboolean blockEvents;

void CL_ParseServerMessage(void);
void CL_LoadClientinfo(clientinfo_t * ci, char *s);
void SHOWNET(char *s);
void CL_ParseClientinfo(int player);
void CL_InitEvents(void);
void CL_Events(void);

/* cl_view.c */
extern sun_t map_sun;
extern int map_maxlevel;
extern cvar_t *map_dropship;
extern vec3_t map_dropship_coord;

void V_Init(void);
void V_RenderView(float stereo_separation);
void V_AddEntity(entity_t * ent);
void V_AddLight(vec3_t org, float intensity, float r, float g, float b);
void V_AddLightStyle(int style, float r, float g, float b);
entity_t *V_GetEntity(void);
void V_CenterView(pos3_t pos);

/* cl_sequence.c */
void CL_SequenceRender(void);
void CL_Sequence2D(void);
void CL_SequenceStart_f(void);
void CL_SequenceEnd_f(void);
void CL_ResetSequences(void);
void CL_ParseSequence(char *name, char **text);

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
extern qboolean MAP_MapToScreen(const menuNode_t* node, const vec2_t pos, int *x, int *y);
extern void MAP_MapCalcLine(const vec2_t start, const vec2_t end, mapline_t* line);
extern void MAP_DrawMap(const menuNode_t* node, qboolean map3D);
extern void MAP_MapClick(const menuNode_t * node, int x, int y);
extern void MAP_3DMapClick(const menuNode_t* node, int x, int y);
extern void MAP_ResetAction(void);
extern void MAP_SelectAircraft(aircraft_t* aircraft);
extern void MAP_SelectMission(actMis_t* mission);
extern void MAP_NotifyMissionRemoved(const actMis_t* mission);
extern void MAP_NotifyUfoRemoved(const aircraft_t* ufo);
extern void MAP_NotifyUfoDisappear(const aircraft_t* ufo);
extern void MAP_GameInit(void);

/* cl_ufo.c */
extern void UFO_CampaignRunUfos(int dt);
extern void UFO_CampaignCheckEvents(void);
extern void UFO_Reset(void);
extern void UFO_RemoveUfoFromGeoscape(aircraft_t* ufo);

/* cl_popup.c */
extern void CL_PopupInit(void);
extern void CL_PopupNotifyMIssionRemoved(const actMis_t* mission);
extern void CL_PopupNotifyUfoRemoved(const aircraft_t* ufo);
extern void CL_PopupNotifyUfoDisappeared(const aircraft_t* ufo);
extern void CL_DisplayPopupAircraft(const aircraft_t* aircraft);
extern void CL_DisplayPopupIntercept(struct actMis_s* mission, aircraft_t* ufo);

#endif /* CLIENT_CLIENT_H */
