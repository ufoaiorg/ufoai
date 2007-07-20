/**
 * @file cl_parse.c
 * @brief Parse a message received from the server.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/
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

#include "client.h"
#include "snd_loc.h"

extern cvar_t *fs_gamedir;

#if 0
/**
 * @brief see also svc_ops_e in qcommon.h
 * @note don't change the array size - a NET_ReadByte can
 * return values between 0 and UCHAR_MAX (-1 is not handled here)
 */
static const char *svc_strings[UCHAR_MAX+1] =
{
	"svc_bad",

	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_breaksound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",
	"svc_centerprint",
	"svc_playerinfo",
	"svc_event"
};
#endif

/*============================================================================= */

/* id	| type		| length (bytes) */
/*====================================== */
/* c	| char		| 1 */
/* b	| byte		| 1 */
/* s	| short		| 2 */
/* l	| long		| 4 */
/* p	| pos		| 6 */
/* g	| gpos		| 3 */
/* d	| dir		| 1 */
/* a	| angle		| 1 */
/* !	| do not read the next id | 1 */
/* *	| pascal string type - SIZE+DATA, SIZE can be read from va_arg
		| 2 + sizeof(DATA) */
const char *ev_format[] =
{
	"",					/* EV_NULL */
	"bb",				/* EV_RESET */
	"b",				/* EV_START */
	"b",				/* EV_ENDROUND */

	"bb****",			/* EV_RESULTS */
	"g",				/* EV_CENTERVIEW */

	"sbg",				/* EV_ENT_APPEAR */
	"s",				/* EV_ENT_PERISH */
	"sss",				/* EV_ENT_EDICT */

	"!sbbbbbgbbbssbsbbbs",	/* EV_ACTOR_APPEAR; beware of the '!' */
	"s",				/* EV_ACTOR_START_MOVE */
	"sb",				/* EV_ACTOR_TURN */
	"!s*",				/* EV_ACTOR_MOVE; beware of the '!' */

	"ssbbgg",			/* EV_ACTOR_START_SHOOT */
	"ssbbbppb",			/* EV_ACTOR_SHOOT; the last 'b' cannot be 'd' */
	"bsbb",				/* EV_ACTOR_SHOOT_HIDDEN */
	"ssbbbpp",			/* EV_ACTOR_THROW */

	"ss",				/* EV_ACTOR_DIE */
	"!sbsbbb",			/* EV_ACTOR_STATS; beware of the '!' */
	"ss",				/* EV_ACTOR_STATECHANGE */

	"s*",				/* EV_INV_ADD */
	"sbbb",				/* EV_INV_DEL */
	"sbbbbb",			/* EV_INV_AMMO */
	"sbbbbb",			/* EV_INV_RELOAD */
	"ss",					/* EV_INV_HANDS_CHANGED */

	"s",				/* EV_MODEL_PERISH */
	"ss",				/* EV_MODEL_EXPLODE */
	"sg*",				/* EV_SPAWN_PARTICLE */

	"ssp",				/* EV_DOOR_OPEN */
	"ssp"				/* EV_DOOR_CLOSE */
};

static const char *ev_names[] =
{
	"EV_NULL",
	"EV_RESET",
	"EV_START",
	"EV_ENDROUND",
	"EV_RESULTS",
	"EV_CENTERVIEW",

	"EV_ENT_APPEAR",
	"EV_ENT_PERISH",
	"EV_ENT_EDICT",

	"EV_ACTOR_APPEAR",
	"EV_ACTOR_START_MOVE",
	"EV_ACTOR_TURN",
	"EV_ACTOR_MOVE",
	"EV_ACTOR_START_SHOOT",
	"EV_ACTOR_SHOOT",
	"EV_ACTOR_SHOOT_HIDDEN",
	"EV_ACTOR_THROW",
	"EV_ACTOR_DIE",
	"EV_ACTOR_STATS",
	"EV_ACTOR_STATECHANGE",

	"EV_INV_ADD",
	"EV_INV_DEL",
	"EV_INV_AMMO",
	"EV_INV_RELOAD",
	"EV_INV_HANDS_CHANGED",

	"EV_MODEL_PERISH",
	"EV_MODEL_EXPLODE",

	"EV_SPAWN_PARTICLE",

	"EV_DOOR_OPEN",
	"EV_DOOR_CLOSE"
};

static void CL_Reset(struct dbuffer *msg);
static void CL_StartGame(struct dbuffer *msg);
static void CL_CenterView(struct dbuffer *msg);
static void CL_EntAppear(struct dbuffer *msg);
static void CL_EntPerish(struct dbuffer *msg);
static void CL_EntEdict(struct dbuffer *msg);
static void CL_ActorDoStartMove(struct dbuffer *msg);
static void CL_ActorAppear(struct dbuffer *msg);
static void CL_ActorStats(struct dbuffer *msg);
static void CL_ActorStateChange(struct dbuffer *msg);
static void CL_InvAdd(struct dbuffer *msg);
static void CL_InvDel(struct dbuffer *msg);
static void CL_InvAmmo(struct dbuffer *msg);
static void CL_InvReload(struct dbuffer *msg);

static void (*ev_func[])( struct dbuffer *msg ) =
{
	NULL,
	CL_Reset,		/* EV_RESET */
	CL_StartGame,	/* EV_START */
	CL_DoEndRound,
	CL_ParseResults,
	CL_CenterView,

	CL_EntAppear,
	CL_EntPerish,
	CL_EntEdict,

	CL_ActorAppear,
	CL_ActorDoStartMove,
	CL_ActorDoTurn,
	CL_ActorDoMove,
	CL_ActorStartShoot,
	CL_ActorDoShoot,
	CL_ActorShootHidden,
	CL_ActorDoThrow,
	CL_ActorDie,
	CL_ActorStats,
	CL_ActorStateChange,

	CL_InvAdd,
	CL_InvDel,
	CL_InvAmmo,
	CL_InvReload,
	CL_InvCheckHands,

	LM_Perish,
	LM_Explode,

	CL_ParticleSpawnFromSizeBuf,

	LM_DoorOpen,
	LM_DoorClose
};

typedef struct evTimes_s {
	int when;
	int eType;
	struct dbuffer *msg;
	struct evTimes_s *next;
} evTimes_t;

static evTimes_t *events = NULL;

qboolean blockEvents;	/**< block network events - see CL_Events */

static int nextTime;
static int shootTime;
static int impactTime;
static qboolean parsedDeath = qfalse;

/*============================================================================= */

/**
 * @return true if the file exists, otherwise it attempts to start a download via curl
 */
qboolean CL_CheckOrDownloadFile (const char *filename)
{
	static char lastfilename[MAX_OSPATH] = {0};

	/* r1: don't attempt same file many times */
	if (!strcmp(filename, lastfilename))
		return qtrue;

	strcpy(lastfilename, filename);

	if (strstr (filename, "..")) {
		Com_Printf("Refusing to check a path with .. (%s)\n",  filename);
		return qtrue;
	}

	if (strchr (filename, ' ')) {
		Com_Printf("Refusing to check a path containing spaces (%s)\n", filename);
		return qtrue;
	}

	if (strchr (filename, ':')) {
		Com_Printf("Refusing to check a path containing a colon (%s)\n", filename);
		return qtrue;
	}

	if (filename[0] == '/') {
		Com_Printf("Refusing to check a path starting with / (%s)\n", filename);
		return qtrue;
	}

	if (FS_LoadFile(filename, NULL) != -1) {
		/* it exists, no need to download */
		return qtrue;
	}

#ifdef HAVE_CURL
	if (CL_QueueHTTPDownload(filename))
		return qfalse;
#endif /* HAVE_CURL */
	return qtrue;
}

/**
 * @brief
 */
void CL_RegisterSounds (void)
{
	int i, j, k;

	S_BeginRegistration();

	/* load game sounds */
	for (i = 1; i < MAX_SOUNDS; i++) {
		if (!cl.configstrings[CS_SOUNDS+i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound(cl.configstrings[CS_SOUNDS+i]);
		/* pump message loop */
		Sys_SendKeyEvents();
	}

	/* load weapon sounds */
	for (i = 0; i < csi.numODs; i++) { /* i = obj */
		for (j = 0; j < csi.ods[i].numWeapons; j++) {	/* j = weapon-entry per obj */
			for (k = 0; k < csi.ods[i].numFiredefs[j]; j++) { /* k = firedef per wepaon */
				if (csi.ods[i].fd[j][k].fireSound[0])
					S_RegisterSound(csi.ods[i].fd[j][k].fireSound);
				if (csi.ods[i].fd[j][k].impactSound[0])
					S_RegisterSound(csi.ods[i].fd[j][k].impactSound);
				/* pump message loop */
				Sys_SendKeyEvents();
			}
		}
	}

	S_EndRegistration();
}

/*
=====================================================================
SERVER CONNECTING MESSAGES
=====================================================================
*/

/**
 * @brief Written by SV_New_f in sv_user.c
 */
static void CL_ParseServerData (struct dbuffer *msg)
{
	char *str;
	int i;

	Com_DPrintf("Serverdata packet received.\n");

	/* wipe the client_state_t struct */
	CL_ClearState();
	cls.state = ca_connected;

	/* parse protocol version number */
	i = NET_ReadLong(msg);
	cls.serverProtocol = i;

	/* compare versions */
	if (i != PROTOCOL_VERSION)
		Com_Error(ERR_DROP, "Server returned version %i, not %i", i, PROTOCOL_VERSION);

	cl.servercount = NET_ReadLong(msg);
	cl.attractloop = NET_ReadByte(msg);

	/* game directory */
	str = NET_ReadString(msg);
	Q_strncpyz(cl.gamedir, str, sizeof(cl.gamedir));

	/* set gamedir */
	if ((*str && (!fs_gamedir->string || !*fs_gamedir->string || strcmp(fs_gamedir->string, str))) || (!*str && (fs_gamedir->string || *fs_gamedir->string)))
		Cvar_Set("fs_gamedir", str);

	/* parse player entity number */
	cl.pnum = NET_ReadShort(msg);

	/* get the full level name */
	str = NET_ReadString(msg);

	Com_DPrintf("serverdata: count %d, attractloop %d, gamedir %s, pnum %d, level %s\n",
		cl.servercount, cl.attractloop, cl.gamedir, cl.pnum, str);

	if (cl.pnum >= 0) {
		/* seperate the printfs so the server message can have a color */
/*		Com_Printf("%c%s\n", 2, str);*/
		/* need to prep refresh at next oportunity */
		cl.refresh_prepped = qfalse;
	}
}

/**
 * @brief Stores the client name
 * @sa CL_ParseClientinfo
 */
static void CL_LoadClientinfo (clientinfo_t *ci, const char *s)
{
	Q_strncpyz(ci->cinfo, s, sizeof(ci->cinfo));

	/* isolate the player's name */
	Q_strncpyz(ci->name, s, sizeof(ci->name));
}

/**
 * @brief Parses client names that are displayed on the targeting box for
 * multiplayer games
 * @sa CL_AddTargetingBoX
 */
static void CL_ParseClientinfo (int player)
{
	char *s;
	clientinfo_t *ci;

	s = cl.configstrings[player+CS_PLAYERNAMES];

	ci = &cl.clientinfo[player];

	CL_LoadClientinfo(ci, s);
}

/**
 * @brief
 * @sa PF_Configstring
 */
static void CL_ParseConfigString (struct dbuffer *msg)
{
	int		i;
	char	*s;

	/* which configstring? */
	i = NET_ReadShort(msg);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error(ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	/* value */
	s = NET_ReadString(msg);

	Com_DPrintf("configstring %d: %s\n", i, s);

	/* there may be overflows in i==CS_TILES - but thats ok */
	/* see definition of configstrings and MAX_TILESTRINGS */
	switch (i) {
	case CS_TILES:
	case CS_POSITIONS:
		Q_strncpyz(cl.configstrings[i], s, MAX_TOKEN_CHARS*MAX_TILESTRINGS);
		break;
	default:
		Q_strncpyz(cl.configstrings[i], s, MAX_TOKEN_CHARS);
		break;
	}

	/* do something apropriate */
	if (i >= CS_LIGHTS && i < CS_LIGHTS + MAX_LIGHTSTYLES)
		CL_SetLightstyle(i - CS_LIGHTS);
	else if (i >= CS_MODELS && i < CS_MODELS + MAX_MODELS) {
		if (cl.refresh_prepped) {
			cl.model_draw[i-CS_MODELS] = re.RegisterModel(cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel(cl.configstrings[i]);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}
	} else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_MODELS) {
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound(cl.configstrings[i]);
	} else if (i >= CS_IMAGES && i < CS_IMAGES + MAX_MODELS) {
		if (cl.refresh_prepped)
			cl.image_precache[i-CS_IMAGES] = re.RegisterPic(cl.configstrings[i]);
	} else if (i >= CS_PLAYERNAMES && i < CS_PLAYERNAMES + MAX_CLIENTS) {
		CL_ParseClientinfo(i-CS_PLAYERNAMES);
	}
}


/*
=====================================================================
ACTION MESSAGES
=====================================================================
*/

/**
 * @brief Parsed a server send sound package
 * @sa svc_breaksound
 * @sa SV_BreakSound
 */
static void CL_ParseStartBreakSoundPacket (struct dbuffer *msg)
{
	vec3_t  pos_v;
	float	*pos;
	int 	channel, ent;
	edictMaterial_t material;
	float 	volume;
	float 	attenuation;
	int		flags;
	float	ofs;
	const char	*sound;
	sfx_t *sfx;

	flags = NET_ReadByte(msg);
	material = NET_ReadByte(msg);

	if (flags & SND_VOLUME)
		volume = NET_ReadByte(msg) / 255.0;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (flags & SND_ATTENUATION)
		attenuation = NET_ReadByte(msg) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	if (flags & SND_OFFSET)
		ofs = NET_ReadByte(msg) / 1000.0;
	else
		ofs = 0;

	/* entity reletive */
	if (flags & SND_ENT) {
		channel = NET_ReadShort(msg);
		ent = channel >> 3;
		if (ent > MAX_EDICTS)
			Com_Error(ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	} else {
		ent = 0;
		channel = 0;
	}

	/* positioned in space */
	if (flags & SND_POS) {
		NET_ReadPos(msg, pos_v);

		pos = pos_v;
	} else /* use entity number */
		pos = NULL;

	Com_DPrintf("startbreaksoundpacket: flags %x, material %d, volume %.2ff, attenuation %.2f, ofs %.2f,"
		" channel %d, ent %d, pos %.3f, %.3f, %.3f\n",
		flags, material, volume, attenuation, ofs, channel, ent, pos[0], pos[1], pos[2]);

	switch (material) {
	case MAT_METAL:
		sound = "ambient/breakmetal.wav";
		break;
	case MAT_GLASS:
		sound = "ambient/breakglass.wav";
		break;
	case MAT_ELECTRICAL:
		sound = "ambient/breakeletric.wav";
		break;
	case MAT_WOOD:
		sound = "ambient/breakwood.wav";
		break;
	default:
		if (material >= MAT_MAX)
			Com_Printf("CL_ParseStartBreakSoundPacket: Unknown material\n");
		/* no sound */
		return;
	}

	sfx = S_RegisterSound(sound);

	S_StartSound(pos, ent, channel, sfx, volume, attenuation, ofs);
}

/**
 * @brief Reset the events
 * @note Also sets etUnused - if you get Timetable overflow messages, etUnused is NULL
 */
static void CL_EventReset (void)
{
	evTimes_t *event;
	evTimes_t *next;
	for (event = events; event; event = next) {
		next = event->next;
		free_dbuffer(event->msg);
		free(event);
	}
	events = NULL;
}

/**
 * @brief
 * @sa G_ClientSpawn
 */
static void CL_Reset (struct dbuffer *msg)
{
	/* clear local entities */
	numLEs = 0;
	selActor = NULL;
	cl.numTeamList = 0;
	Cbuf_AddText("numonteam1\n");

	CL_EventReset();
	parsedDeath = qfalse;
	cl.eventTime = 0;
	nextTime = 0;
	shootTime = 0;
	impactTime = 0;
	blockEvents = qfalse;

	/* set the active player */
	NET_ReadFormat(msg, ev_format[EV_RESET], &cls.team, &cl.actTeam);

	Com_Printf("(player %i) It's team %i's round\n", cl.pnum, cl.actTeam);
	/* if in multiplayer spawn the soldiers */
	if (!ccs.singleplayer) {
		/* pop any waiting menu and activate the HUD */
		MN_PopMenu(qtrue);
		MN_PushMenu(mn_hud->string);
		Cvar_Set("mn_active", mn_hud->string);
	}
	if (cls.team == cl.actTeam)
		Cbuf_AddText("startround\n");
	else
		Com_Printf("You lost the coin-toss for first-turn\n");
}


/**
 * @brief
 * @sa G_ClientBegin
 */
static void CL_StartGame (struct dbuffer *msg)
{
	int team_play = NET_ReadByte(msg);

	/* init camera position and angles */
	memset(&cl.cam, 0, sizeof(camera_t));
	VectorSet(cl.cam.angles, 60.0, 60.0, 0.0);
	VectorSet(cl.cam.omega, 0.0, 0.0, 0.0);
	cl.cam.zoom = 1.25;
	CalcFovX();
	camera_mode = CAMERA_MODE_REMOTE;

	SCR_SetLoadingBackground(NULL);

	Com_Printf("Starting the game...\n");

	/* make sure selActor is null (after reconnect or server change this is needed) */
	selActor = NULL;

	/* center on first actor */
	cl_worldlevel->modified = qtrue;
	if (cl.numTeamList) {
		le_t *le;
		le = cl.teamList[0];
		VectorCopy(le->origin, cl.cam.reforg);
		Cvar_SetValue("cl_worldlevel", le->pos[2]);
	}

	/* activate the renderer */
	cls.state = ca_active;

	CL_EventReset();

	/* back to the console */
	MN_PopMenu(qtrue);

	if (!ccs.singleplayer && baseCurrent) {
		if (team_play) {
			MN_PushMenu("multiplayer_selectteam");
			Cvar_Set("mn_active", "multiplayer_selectteam");
		} else {
			Cbuf_AddText("mp_selectteam_init\n");
		}
	} else {
		MN_PushMenu(mn_hud->string);
		Cvar_Set("mn_active", mn_hud->string);
	}
	SCR_EndLoadingPlaque();	/* get rid of loading plaque */
}


/**
 * @brief
 */
static void CL_CenterView (struct dbuffer *msg)
{
	pos3_t pos;

	NET_ReadFormat(msg, ev_format[EV_CENTERVIEW], &pos);
	V_CenterView(pos);
}


/**
 * @brief
 */
static void CL_EntAppear (struct dbuffer *msg)
{
	le_t	*le;
	int		entnum, type;
	pos3_t	pos;

	NET_ReadFormat(msg, ev_format[EV_ENT_APPEAR], &entnum, &type, &pos);

	/* check if the ent is already visible */
	le = LE_Get(entnum);
	if (!le) {
		le = LE_Add(entnum);
	} else {
		Com_DPrintf("Entity appearing already visible... overwriting the old one\n");
		le->inuse = qtrue;
	}

	le->type = type;

	VectorCopy(pos, le->pos);
	Grid_PosToVec(&clMap, le->pos, le->origin);
}


/**
 * @brief Called whenever an entity disappears from view
 */
static void CL_EntPerish (struct dbuffer *msg)
{
	int		entnum, i;
	le_t	*le, *actor;

	NET_ReadFormat(msg, ev_format[EV_ENT_PERISH], &entnum);

	le = LE_Get(entnum);

	if (!le) {
		Com_DPrintf("Delete request ignored... LE not found\n");
		return;
	}

	/* decrease the count of spotted aliens */
	if ((le->type == ET_ACTOR || le->type == ET_UGV) && !(le->state & STATE_DEAD) && le->team != cls.team && le->team != TEAM_CIVILIAN)
		cl.numAliensSpotted--;

	switch (le->type) {
	case ET_ITEM:
		Com_EmptyContainer(&le->i, csi.idFloor);

		/* search owners (there can be many, some of them dead) */
		for (i = 0, actor = LEs; i < numLEs; i++, actor++)
			if (actor->inuse
				 && (actor->type == ET_ACTOR || actor->type == ET_UGV)
				 && VectorCompare(actor->pos, le->pos)) {
				Com_DPrintf("CL_EntPerish: le of type ET_ITEM hidden\n");
				FLOOR(actor) = NULL;
			}
		break;
	case ET_ACTOR:
	case ET_UGV:
		Com_DestroyInventory(&le->i);
		break;
	case ET_BREAKABLE:
	case ET_DOOR:
		break;
	default:
		break;
	}

	if (!(le->team && le->state && le->team == cls.team && !(le->state & STATE_DEAD)))
		le->inuse = qfalse;
}

/**
 * @brief Inits the breakable or other solid objects
 * @note func_breakable, func_door
 * @sa G_SendVisibleEdicts
 */
static void CL_EntEdict (struct dbuffer *msg)
{
	le_t *le;
	int entnum, modelnum1, type;
	char *inline_model_name;
	cBspModel_t *model;

	NET_ReadFormat(msg, ev_format[EV_ENT_EDICT], &type, &entnum, &modelnum1);

	/* check if the ent is already visible */
	le = LE_Get(entnum);
	if (!le) {
		le = LE_Add(entnum);
	} else {
		Com_DPrintf("Entity appearing already visible... overwriting the old one\n");
		le->inuse = qtrue;
	}

	le->invis = qtrue;
	le->type = type;
	le->modelnum1 = modelnum1;
	inline_model_name = va("*%i", le->modelnum1);
	model = CM_InlineModel(inline_model_name);
	if (!model)
		Com_Error(ERR_DROP, "CL_EntEdict: Could not find inline model %i", le->modelnum1);
	VectorCopy(model->origin, le->origin);
	VectorCopy(model->mins, le->mins);
	VectorCopy(model->maxs, le->maxs);

	/* to allow tracing against this le */
	le->contents = CONTENTS_SOLID;
}


static le_t	*lastMoving;

/**
 * @brief Set the lastMoving entity (set to the actor who last
 * walked, turned, crouched or stood up).
 */
void CL_SetLastMoving (le_t *le)
{
	lastMoving = le;
}

/**
 * @brief
 */
static void CL_ActorDoStartMove (struct dbuffer *msg)
{
	int	entnum;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_START_MOVE], &entnum);

	CL_SetLastMoving(LE_Get(entnum));
}


/**
 * @brief
 * @sa CL_AddActorToTeamList
 * @sa G_AppearPerishEvent
 */
static void CL_ActorAppear (struct dbuffer *msg)
{
	qboolean newActor;
	le_t *le;
	char tmpbuf[128];
	int entnum, modelnum1, modelnum2;
	int teamDescID = -1;

	/* check if the actor is already visible */
	entnum = NET_ReadShort(msg);
	le = LE_Get(entnum);

	if (!le) {
		le = LE_Add(entnum);
		newActor = qtrue;
	} else {
/*		Com_Printf("Actor appearing already visible... overwriting the old one\n"); */
		le->inuse = qtrue;
		newActor = qfalse;
	}

	/* get the info */
	NET_ReadFormat(msg, ev_format[EV_ACTOR_APPEAR],
				&le->team, &le->teamDesc, &le->category, &le->gender, &le->pnum, &le->pos,
				&le->dir, &le->right, &le->left,
				&modelnum1, &modelnum2, &le->skinnum,
				&le->state, &le->fieldSize,
				&le->maxTU, &le->maxMorale, &le->maxHP);

	if (le->fieldSize == ACTOR_SIZE_NORMAL) {
		le->addFunc = CL_AddActor;
		le->type = ET_ACTOR;
	} else {
		le->addFunc = CL_AddUGV;
		le->type = ET_UGV;
	}
	le->modelnum1 = modelnum1;
	le->modelnum2 = modelnum2;
	le->model1 = cl.model_draw[modelnum1];
	le->model2 = cl.model_draw[modelnum2];
	Grid_PosToVec(&clMap, le->pos, le->origin);
	le->angles[YAW] = dangle[le->dir];

	le->contents = CONTENTS_ACTOR;
	VectorCopy(player_mins, le->mins);
	if (le->state & STATE_DEAD)
		VectorCopy(player_dead_maxs, le->maxs);
	else
		VectorCopy(player_maxs, le->maxs);

	le->think = LET_StartIdle;

	/* count spotted aliens */
	if (!(le->state & STATE_DEAD) && newActor && le->team != cls.team && le->team != TEAM_CIVILIAN)
		cl.numAliensSpotted++;

	if (cls.state == ca_active && !(le->state & STATE_DEAD)) {
		/* center view (if wanted) */
		if (cl_centerview->integer > 1 || (cl_centerview->integer == 1 && cl.actTeam != cls.team)) {
			VectorCopy(le->origin, cl.cam.reforg);
			Cvar_SetValue("cl_worldlevel", le->pos[2]);
		}

		/* draw line of sight */
		if (le->team != cls.team) {
			if (cl.actTeam == cls.team && lastMoving) {
				ptl_t *ptl;
				vec3_t eyes;
				/* start is the last moving actor's origin */
				VectorCopy(lastMoving->origin, eyes);
				if (lastMoving->state & STATE_CROUCHED)
					eyes[2] += EYE_HT_CROUCH;
				else
					eyes[2] += EYE_HT_STAND;
				ptl = CL_ParticleSpawn("fadeTracer", 0, eyes, le->origin, NULL);
				if (le->team == TEAM_CIVILIAN)
					VectorSet(ptl->color, 0.2, 0.2, 1);
			}

			/* message */
			if (le->team != TEAM_CIVILIAN) {
				if (curCampaign) {
					if (le->teamDesc) {
						teamDescID = le->teamDesc - 1;
						if (RS_IsResearched_idx(RS_GetTechIdxByName(teamDesc[teamDescID].tech))) {
							Com_sprintf(tmpbuf, sizeof(tmpbuf), "%s %s!\n",
							_("Alien spotted:"), _(teamDesc[teamDescID].name));
							CL_DisplayHudMessage(tmpbuf, 2000);
						} else
							CL_DisplayHudMessage(_("Alien spotted!\n"), 2000);
					} else
						CL_DisplayHudMessage(_("Alien spotted!\n"), 2000);
				} else
					CL_DisplayHudMessage(_("Enemy spotted!\n"), 2000);
			} else
				CL_DisplayHudMessage(_("Civilian spotted!\n"), 2000);

			/* update pathing as new actor could block path */
			if (newActor)
				CL_ConditionalMoveCalc(&clMap, selActor, MAX_ROUTE);
		}
	}

	/* add team members to the actor list */
	CL_AddActorToTeamList(le);

/*	Com_Printf("Player at (%i %i %i) -> (%f %f %f)\n", */
/*		le->pos[0], le->pos[1], le->pos[2], le->origin[0], le->origin[1], le->origin[2]); */
}


/**
 * @brief Parses the actor stats that comes from the netchannel
 * @sa CL_ParseEvent
 * @sa G_SendStats
 * @sa ev_func
 */
static void CL_ActorStats (struct dbuffer *msg)
{
	le_t	*le;
	int		number, selActorTU = 0;

	number = NET_ReadShort(msg);
	le = LE_Get(number);

	if (!le || (le->type != ET_ACTOR && le->type != ET_UGV)) {
		Com_Printf("Stats message ignored... LE not found or not an actor\n");
		return;
	}

	if (le == selActor)
		selActorTU = selActor->TU;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_STATS], &le->TU, &le->HP, &le->STUN, &le->AP, &le->morale);

	if (le->TU > le->maxTU)
		le->maxTU = le->TU;
	if (le->HP > le->maxHP)
		le->maxHP = le->HP;
	if (le->morale > le->maxMorale)
		le->maxMorale = le->morale;

	/* if selActor's timeunits have changed, update movelength */
	if (le == selActor)
		if (le->TU != selActorTU) {
			CL_ResetActorMoveLength();
		}
}


/**
 * @brief
 */
static void CL_ActorStateChange (struct dbuffer *msg)
{
	le_t	*le;
	int		number, state;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_STATECHANGE], &number, &state);

	le = LE_Get(number);
	if (!le || (le->type != ET_ACTOR && le->type != ET_UGV)) {
		Com_Printf("StateChange message ignored... LE not found or not an actor\n");
		return;
	}

	/* if standing up or crouching down, set this le as the last moving */
	if ((state & STATE_CROUCHED && !(le->state & STATE_CROUCHED)) ||
		 (!(state & STATE_CROUCHED) && le->state & STATE_CROUCHED))
		CL_SetLastMoving(le);

	/* killed by the server: no animation is played, etc. */
	if (state & STATE_DEAD && !(le->state & STATE_DEAD)) {
		le->state = state;
		FLOOR(le) = NULL;
		le->think = NULL;
		VectorCopy(player_dead_maxs, le->maxs);
		CL_RemoveActorFromTeamList(le);
		CL_ConditionalMoveCalc(&clMap, selActor, MAX_ROUTE);
		return;
	} else {
		le->state = state;
		le->think = LET_StartIdle;
	}

	/* state change may have affected move length */
	if (selActor)
		CL_ResetActorMoveLength();
}


/**
 * @brief Returns the index of the biggest item in the inventory list
 * @note This item is the only one that will be drawn when lying at the floor
 * @sa CL_PlaceItem
 */
static int CL_BiggestItem (invList_t *ic)
{
	uint32_t shape;
	int size, max, maxSize;

	maxSize = 0;
	for (max = ic->item.t; ic; ic = ic->next) {
		shape = csi.ods[ic->item.t].shape;
		size = (shape >> 24 & 0xF) * (shape >> 28 & 0xF);
		if (size > maxSize) {
			max = ic->item.t;
			maxSize = size;
		}
	}

	return max;
}


/**
 * @brief
 * @sa CL_BiggestItem
 */
static void CL_PlaceItem (le_t *le)
{
	le_t *actor;
	int	biggest;
	int i;

	/* search owners (there can be many, some of them dead) */
	for (i = 0, actor = LEs; i < numLEs; i++, actor++)
		if (actor->inuse
			 && (actor->type == ET_ACTOR || actor->type == ET_UGV)
			 && VectorCompare(actor->pos, le->pos) ) {
#if PARANOID
			Com_DPrintf("CL_PlaceItem: shared container: '%p'\n", FLOOR(le));
#endif
			if (FLOOR(le))
				FLOOR(actor) = FLOOR(le);
		}

	if (FLOOR(le)) {
		biggest = CL_BiggestItem(FLOOR(le));
		le->model1 = cl.model_weapons[biggest];
		Grid_PosToVec(&clMap, le->pos, le->origin);
		VectorSubtract(le->origin, csi.ods[biggest].center, le->origin);
		/* fall to ground */
		le->origin[2] -= GROUND_DELTA;
		le->angles[ROLL] = 90;
/*		le->angles[YAW] = 10*(int)(le->origin[0] + le->origin[1] + le->origin[2]) % 360; */
	} else {
		/* If no items in floor inventory, don't draw this le */
		/* DEBUG
		 * This is disabled for now because it'll prevent LE_Add to get a floor-edict when in mid-move.
		 * See g_client.c:G_ClientInvMove
		 */
		/* le->inuse = qfalse; */
	}
}


/**
 * @brief
 * @sa CL_InvDel
 * @sa G_SendInventory
 */
static void CL_InvAdd (struct dbuffer *msg)
{
	int		number, nr;
	int 	container, x, y;
	le_t	*le;
	item_t 	item;

	number = NET_ReadShort(msg);

	le = LE_Get(number);
	if (!le) {
		nr = NET_ReadShort(msg) / 6;
		Com_Printf("InvAdd: message ignored... LE %i not found\n", number);
		for (; nr-- > 0;) {
			CL_NetReceiveItem(msg, &item, &container, &x, &y, qfalse);
			Com_Printf("InvAdd: ignoring:\n");
			Com_PrintItemDescription(item.t);
		}
		return;
	}
	if (!le->inuse)
		Com_DPrintf("InvAdd: warning... LE found but not in-use\n");

	nr = NET_ReadShort(msg) / 6;

	for (; nr-- > 0;) {
		CL_NetReceiveItem(msg, &item, &container, &x, &y, qfalse);

		Com_AddToInventory(&le->i, item, container, x, y);

		if (container == csi.idRight)
			le->right = item.t;
		else if (container == csi.idLeft)
			le->left = item.t;
		else if (container == csi.idExtension)
			le->extension = item.t;
	}

	switch (le->type) {
	case ET_ACTOR:
	case ET_UGV:
		le->think = LET_StartIdle;
		break;
	case ET_ITEM:
		CL_PlaceItem(le);
		break;
	}
}


/**
 * @brief
 * @sa CL_InvAdd
 */
static void CL_InvDel (struct dbuffer *msg)
{
	le_t	*le;
	int		number;
	int 	container, x, y;

	NET_ReadFormat(msg, ev_format[EV_INV_DEL],
		&number, &container, &x, &y);

	le = LE_Get(number);
	if (!le) {
		Com_DPrintf("InvDel message ignored... LE not found\n");
		return;
	}

	Com_RemoveFromInventory(&le->i, container, x, y);
	if (container == csi.idRight)
		le->right = NONE;
	else if (container == csi.idLeft)
		le->left = NONE;
	else if (container == csi.idExtension)
		le->extension = NONE;

	switch (le->type) {
	case ET_ACTOR:
	case ET_UGV:
		le->think = LET_StartIdle;
		break;
	case ET_ITEM:
		CL_PlaceItem(le);
		break;
	}
}


/**
 * @brief
 */
static void CL_InvAmmo (struct dbuffer *msg)
{
	invList_t	*ic;
	le_t	*le;
	int		number;
	int		ammo, type, container, x, y;

	NET_ReadFormat(msg, ev_format[EV_INV_AMMO],
		&number, &ammo, &type, &container, &x, &y);

	le = LE_Get(number);
	if (!le) {
		Com_DPrintf("InvAmmo message ignored... LE not found\n");
		return;
	}

	if (le->team != cls.team)
		return;

	ic = Com_SearchInInventory(&le->i, container, x, y);
	if (!ic)
		return;

	/* set new ammo */
	ic->item.a = ammo;
	ic->item.m = type;
}


/**
 * @brief
 */
static void CL_InvReload (struct dbuffer *msg)
{
	invList_t	*ic;
	le_t	*le;
	int		number;
	int		ammo, type, container, x, y;

	NET_ReadFormat(msg, ev_format[EV_INV_RELOAD],
		&number, &ammo, &type, &container, &x, &y);

	S_StartLocalSound("weapons/reload");

	le = LE_Get(number);
	if (!le) {
		Com_DPrintf("CL_InvReload: only sound played\n");
		return;
	}

	if (le->team != cls.team)
		return;

	ic = Com_SearchInInventory(&le->i, container, x, y);
	if (!ic)
		return;

	/* if the displaced clip had any remaining bullets */
	/* store them as loose, unless the removed clip was full */
	if (curCampaign
		 && ic->item.a > 0
		 && ic->item.a != csi.ods[ic->item.t].ammo) {
		assert(ammo == csi.ods[ic->item.t].ammo);
		ccs.eMission.num_loose[ic->item.m] += ic->item.a;
		/* Accumulate loose ammo into clips (only accessible post-mission) */
		if (ccs.eMission.num_loose[ic->item.m] >= csi.ods[ic->item.t].ammo) {
			ccs.eMission.num_loose[ic->item.m] -= csi.ods[ic->item.t].ammo;
			ccs.eMission.num[ic->item.m]++;
		}
	}

	/* set new ammo */
	ic->item.a = ammo;
	ic->item.m = type;
}

/**
 * @brief
 * @sa CL_Events
 */
static void CL_LogEvent (int num)
{
	FILE *logfile;

	if (!cl_logevents->integer)
		return;

	logfile = fopen(va("%s/events.log", FS_Gamedir()), "a");
	fprintf(logfile, "%10i %s\n", cl.eventTime, ev_names[num]);
	fclose(logfile);
}

static void schedule_do_event(void);

static void do_event (int now, void *data)
{
	while (events && !blockEvents) {
		evTimes_t *event = events;

		if (event->when > cl.eventTime) {
			schedule_do_event();
			return;
		}

		events = event->next;

		Com_DPrintf("event(dispatching): %s %p\n", ev_names[event->eType], event);
		CL_LogEvent(event->eType);
		ev_func[event->eType](event->msg);

		free_dbuffer(event->msg);
		free(event);
	}
}

static void schedule_do_event (void)
{
	/* We need to schedule the first event in the queue. Unfortunately,
	 events don't run on the master timer (yet - this should change),
	 so we have to convert from one timescale to the other */
	int timescale_delta = Sys_Milliseconds() - cl.eventTime;
	if (!events)
		return;
	Schedule_Event(events->when + timescale_delta, &do_event, NULL);
}

void CL_BlockEvents (void)
{
	blockEvents = qtrue;
}

void CL_UnblockEvents (void)
{
	blockEvents = qfalse;
	schedule_do_event();
}

/**
 * @brief Called in case a svc_event was send via the network buffer
 * @sa CL_ParseServerMessage
 */
static void CL_ParseEvent (struct dbuffer *msg)
{
	evTimes_t *cur;
	qboolean now;
	int eType = NET_ReadByte(msg);
	if (eType == 0)
		return;

	/* check instantly flag */
	if (eType & INSTANTLY) {
		now = qtrue;
		eType &= ~INSTANTLY;
	} else
		now = qfalse;

	/* check if eType is valid */
	if (eType < 0 || eType >= EV_NUM_EVENTS)
		Com_Error(ERR_DROP, "CL_ParseEvent: invalid event %s\n", ev_names[eType]);

	if (!ev_func[eType])
		Com_Error(ERR_DROP, "CL_ParseEvent: no handling function for event %i\n", eType);

	if (now) {
		/* log and call function */
		CL_LogEvent(eType);
		Com_DPrintf("event(now): %s\n", ev_names[eType]);
		ev_func[eType](msg);
	} else {
		struct dbuffer *event_msg = dbuffer_dup(msg);
		int event_time;

		/* get event time */
		if (nextTime < cl.eventTime)
			nextTime = cl.eventTime;
		if (impactTime < cl.eventTime)
			impactTime = cl.eventTime;

		if (eType == EV_ACTOR_DIE)
			parsedDeath = qtrue;

		if (eType == EV_ACTOR_DIE || eType == EV_MODEL_EXPLODE)
			event_time = impactTime;
		else if (eType == EV_ACTOR_SHOOT || eType == EV_ACTOR_SHOOT_HIDDEN)
			event_time = shootTime;
		else
			event_time = nextTime;

		if ((eType == EV_ENT_APPEAR || eType == EV_INV_ADD)) {
			if (parsedDeath) { /* drop items after death (caused by impact) */
				event_time = impactTime + 400;
				/* EV_INV_ADD messages are the last event sent after a death */
				if (eType == EV_INV_ADD)
					parsedDeath = qfalse;
			} else if (impactTime > cl.eventTime) { /* item thrown on the ground */
				event_time = impactTime + 75;
			}
		}

		/* calculate time interval before the next event */
		switch (eType) {
		case EV_ACTOR_APPEAR:
			if (cls.state == ca_active && cl.actTeam != cls.team)
				nextTime += 600;
			break;
		case EV_INV_RELOAD:
			/* let the reload sound play */
			nextTime += 600;
			break;
		case EV_ACTOR_START_SHOOT:
			nextTime += 300;
			shootTime = nextTime;
			break;
		case EV_ACTOR_SHOOT_HIDDEN:
			{
				fireDef_t *fd;
				int first;
				int obj_idx;
				int weap_fds_idx, fd_idx;

				NET_ReadFormat(msg, ev_format[EV_ACTOR_SHOOT_HIDDEN], &first, &obj_idx, &weap_fds_idx, &fd_idx);

				if (first) {
					nextTime += 500;
					impactTime = shootTime = nextTime;
				} else {
#ifdef DEBUG
					GET_FIREDEFDEBUG(obj_idx, weap_fds_idx, fd_idx)
#endif
					fd = GET_FIREDEF(obj_idx, weap_fds_idx, fd_idx);
#if 0
					@todo: not needed? and SF_BOUNCED?
					if (fd->speed)
						impactTime = shootTime + 1000 * VectorDist( muzzle, impact ) / fd->speed;
					else
#endif
						impactTime = shootTime;
					nextTime = shootTime + 1400;
					if (fd->rof)
						shootTime += 1000 / fd->rof;
				}
				parsedDeath = qfalse;
			}
			break;
		case EV_ACTOR_SHOOT:
			{
				fireDef_t	*fd;
				int flags, dummy;
				int obj_idx;
				int weap_fds_idx, fd_idx;
				vec3_t	muzzle, impact;

				/* read data */
				NET_ReadFormat(msg, ev_format[EV_ACTOR_SHOOT], &dummy, &obj_idx, &weap_fds_idx, &fd_idx, &flags, &muzzle, &impact, &dummy);

#ifdef DEBUG
				GET_FIREDEFDEBUG(obj_idx, weap_fds_idx, fd_idx)
#endif
				fd = GET_FIREDEF(obj_idx, weap_fds_idx, fd_idx);

				if (!(flags & SF_BOUNCED)) {
					/* shooting */
					if (fd->speed)
						impactTime = shootTime + 1000 * VectorDist(muzzle, impact) / fd->speed;
					else
						impactTime = shootTime;
					if (cl.actTeam != cls.team)
						nextTime = shootTime + 1400;
					else
						nextTime = shootTime + 400;
					if (fd->rof)
						shootTime += 1000 / fd->rof;
				} else {
					/* only a bounced shot */
					event_time = impactTime;
					if (fd->speed) {
						impactTime += 1000 * VectorDist(muzzle, impact) / fd->speed;
						nextTime = impactTime;
					}
				}
				parsedDeath = qfalse;
			}
			break;
		case EV_ACTOR_THROW:
			nextTime += NET_ReadShort(msg);
			impactTime = shootTime = nextTime;
			parsedDeath = qfalse;
			break;
		default:
/*			nextTime += 300;*/ /* mattn - for testing */
			break;
		}

		cur = malloc(sizeof(*cur));
		cur->when = event_time;
		cur->eType = eType;
		cur->msg = event_msg;

		if (!events || (events)->when > event_time) {
			cur->next = events;
			events = cur;
			schedule_do_event();
		} else {
			evTimes_t *e = events;
			while (e->next && e->next->when <= event_time)
				e = e->next;
			cur->next = e->next;
			e->next = cur;
		}

		Com_DPrintf("event(at %d): %s %p\n", event_time, ev_names[eType], cur);
	}
}

/**
 * @brief
 * @sa CL_ReadPackets
 */
void CL_ParseServerMessage (int cmd, struct dbuffer *msg)
{
	char		*s;
	int			i;

#if 0
	/* if recording demos, copy the message out */
	if (cl_shownet->integer == 1)
		Com_Printf("%i ", dbuffer_len(msg));
	else if (cl_shownet->integer >= 2)
		Com_Printf("------------------\n");
#endif

	/* parse the message */
	if (cmd == -1)
		return;

#if 0
	if (cl_shownet->integer >= 2) {
		if (!svc_strings[cmd])
			Com_Printf("%3i:BAD CMD %i\n", net_message.readcount-1,cmd);
		else
			CL_ShowNet(svc_strings[cmd]);
	}
#endif

		/* other commands */
	switch (cmd) {
	case svc_nop:
/*		Com_Printf("svc_nop\n"); */
		break;

	case svc_disconnect:
		Com_Error(ERR_DISCONNECT, "Server disconnected. Not attempting to reconnect.\n");
		break;

	case svc_reconnect:
		Com_Printf("Server disconnected, reconnecting\n");
		cls.state = ca_connecting;
		cls.connect_time = -99999;	/* CL_CheckForResend() will fire immediately */
		break;

	case svc_print:
		i = NET_ReadByte(msg);
		s = NET_ReadString(msg);
		switch (i) {
		case PRINT_CHAT:
			S_StartLocalSound("misc/talk");
			MN_AddChatMessage(s);
			/* skip format strings */
			if (*s == '^')
				s += 2;
			/* also print to console */
			break;
		case PRINT_HUD:
			/* all game lib messages or server messages should be printed
			 * untranslated with bprintf or cprintf */
			/* see src/po/OTHER_STRINGS */
			CL_DisplayHudMessage(_(s), 2000);
			/* this is utf-8 - so no console print please */
			return;
		default:
			break;
		}
		Com_DPrintf("svc_print(%d): %s\n", i, s);
					Com_Printf("%s", s);
		break;

	case svc_centerprint:
		s = NET_ReadString(msg);
					Com_DPrintf("svc_centerprint: %s\n", s);
		SCR_CenterPrint(s);
		break;

	case svc_stufftext:
		s = NET_ReadString(msg);
		Com_DPrintf("stufftext: %s\n", s);
		Cbuf_AddText(s);
		break;

	case svc_serverdata:
		Cbuf_Execute();		/* make sure any stuffed commands are done */
		CL_ParseServerData(msg);
		break;

	case svc_configstring:
		CL_ParseConfigString(msg);
		break;

	case svc_breaksound:
		CL_ParseStartBreakSoundPacket(msg);
		break;

	case svc_event:
		CL_ParseEvent(msg);
		break;

	case svc_bad:
		Com_Printf("CL_ParseServerMessage: bad server message %d\n", cmd);
		break;

	default:
		Com_Error(ERR_DROP,"CL_ParseServerMessage: Illegible server message %d\n", cmd);
		break;
	}
}
