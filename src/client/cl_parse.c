/**
 * @file cl_parse.c
 * @brief Parse a message received from the server.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

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

/** @brief see also svc_ops_e in qcommon.h */
static const char *svc_strings[256] =
{
	"svc_bad",

	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",
	"svc_centerprint",
	"svc_playerinfo",
	"svc_event"
};

/*============================================================================= */

/* id	| type		| length (bytes) */
/*====================================== */
/* c	| char		| 1 */
/* b	| byte		| 1 */
/* s	| short		| 2 */
/* l	| long		| 4 */
/* f	| float		| 4 */
/* p	| pos		| 6 */
/* g	| gpos		| 3 */
/* d	| dir		| 1 */
/* a	| angle		| 1 */
/* !	| do not read	| 1 */
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

	"!sbbbgbbbssbsbbbs",		/* EV_ACTOR_APPEAR; beware of the '!' */
	"s",				/* EV_ACTOR_START_MOVE */
	"sb",				/* EV_ACTOR_TURN */
	"!s*",				/* EV_ACTOR_MOVE; beware of the '!' */
	"sbgg",			    /* EV_ACTOR_START_SHOOT */
	"sbbppb",			/* EV_ACTOR_SHOOT; the last 'b' cannot be 'd' */
	"bb",				/* EV_ACTOR_SHOOT_HIDDEN */
	/* Replace the above formats with these when changing firemdode code
	"ssbbgg",			EV_ACTOR_START_SHOOT
	"ssbbbppb",			EV_ACTOR_SHOOT; the last 'b' cannot be 'd'
	"bsbb",				EV_ACTOR_SHOOT_HIDDEN
	*/
	"sbbpp",			/* EV_ACTOR_THROW */
	"ss",				/* EV_ACTOR_DIE */
	"!sbsbbb",		    /* EV_ACTOR_STATS; beware of the '!' */
	"ss",				/* EV_ACTOR_STATECHANGE */

	"s*",				/* EV_INV_ADD */
	"sbbb",				/* EV_INV_DEL */
	"sbbbbb",			/* EV_INV_AMMO */
	"sbbbbb",			/* EV_INV_RELOAD */

	"s",				/* EV_MODEL_PERISH */
	"s",				/* EV_MODEL_EXPLODE */
	"sg*"				/* EV_SPAWN_PARTICLE */
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

	"EV_MODEL_PERISH",
	"EV_MODEL_EXPLODE",

	"EV_SPAWN_PARTICLE"
};

static void CL_Reset( sizebuf_t *sb );
static void CL_StartGame( sizebuf_t *sb );
static void CL_CenterView( sizebuf_t *sb );
static void CL_EntAppear( sizebuf_t *sb );
static void CL_EntPerish( sizebuf_t *sb );
static void CL_ActorDoStartMove( sizebuf_t *sb );
static void CL_ActorAppear( sizebuf_t *sb );
static void CL_ActorStats( sizebuf_t *sb );
static void CL_ActorStateChange( sizebuf_t *sb );
static void CL_InvAdd( sizebuf_t *sb );
static void CL_InvDel( sizebuf_t *sb );
static void CL_InvAmmo( sizebuf_t *sb );
static void CL_InvReload( sizebuf_t *sb );

static void (*ev_func[])( sizebuf_t *sb ) =
{
	NULL,
	CL_Reset,
	CL_StartGame,
	CL_DoEndRound,
	CL_ParseResults,
	CL_CenterView,

	CL_EntAppear,
	CL_EntPerish,

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

	LM_Perish,
	LM_Explode,

	CL_ParticleSpawnFromSizeBuf
};

#define EV_STORAGE_SIZE		32768
#define EV_TIMES			4096

static sizebuf_t	evStorage;
static byte		evBuf[EV_STORAGE_SIZE];
static byte		*evWp;

typedef struct evTimes_s
{
	int		start;
	int		pos;
	struct	evTimes_s *next;
} evTimes_t;

static evTimes_t	evTimes[EV_TIMES];
static evTimes_t	*etUnused, *etCurrent;

qboolean blockEvents;

static int nextTime;
static int shootTime;
static int impactTime;
static qboolean parsedDeath = qfalse;

/*============================================================================= */

/**
 * @brief
 */
extern void CL_RegisterSounds (void)
{
	int i, j, k;

	S_BeginRegistration ();

	/* load game sounds */
	for ( i=1; i < MAX_SOUNDS; i++ ) {
		if (!cl.configstrings[CS_SOUNDS+i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
		/* pump message loop */
		Sys_SendKeyEvents ();
	}

	/* load weapon sounds */
	for ( i = 0; i < csi.numODs; i++ ) { /* i = obj */
		for (j = 0; j < csi.ods[i].numWeapons; j++ ) {	/* j = weapon-entry per obj */
			for ( k = 0; k < csi.ods[i].numFiredefs[j]; j++ ) { /* k = firedef per wepaon */
				if ( csi.ods[i].fd[j][k].fireSound[0] )
					S_RegisterSound( csi.ods[i].fd[j][k].fireSound );
				if ( csi.ods[i].fd[j][k].impactSound[0] )
					S_RegisterSound( csi.ods[i].fd[j][k].impactSound );
				/* pump message loop */
				Sys_SendKeyEvents ();
			}
		}
	}

	S_EndRegistration ();
}

/*
=====================================================================
SERVER CONNECTING MESSAGES
=====================================================================
*/

/**
 * @brief Written by SV_New_f in sv_user.c
 */
static void CL_ParseServerData (void)
{
	extern cvar_t	*fs_gamedirvar;
	char	*str;
	int		i;

	Com_DPrintf ("Serverdata packet received.\n");

	/* wipe the client_state_t struct */
	CL_ClearState ();
	cls.state = ca_connected;

	/* parse protocol version number */
	i = MSG_ReadLong (&net_message);
	cls.serverProtocol = i;

	/* compare versions */
	if (i != PROTOCOL_VERSION)
		Com_Error (ERR_DROP,"Server returned version %i, not %i", i, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong (&net_message);
	cl.attractloop = MSG_ReadByte (&net_message);

	/* game directory */
	str = MSG_ReadString (&net_message);
	Q_strncpyz (cl.gamedir, str, MAX_QPATH);

	/* set gamedir */
	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str))) || (!*str && (fs_gamedirvar->string || *fs_gamedirvar->string)))
		Cvar_Set("game", str);

	/* parse player entity number */
	cl.pnum = MSG_ReadShort (&net_message);

	/* get the full level name */
	str = MSG_ReadString (&net_message);

	if (cl.pnum >= 0) {
		/* seperate the printfs so the server message can have a color */
/*		Com_Printf ("%c%s\n", 2, str);*/
		/* need to prep refresh at next oportunity */
		cl.refresh_prepped = qfalse;
	}
}

/**
 * @brief
 * @sa PF_Configstring
 */
static void CL_ParseConfigString (void)
{
	int		i;
	char	*s;

	/* which configstring? */
	i = MSG_ReadShort (&net_message);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	/* value */
	s = MSG_ReadString(&net_message);

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
	if (i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES)
		CL_SetLightstyle (i - CS_LIGHTS);
	else if (i == CS_CDTRACK) {
		if (cl.refresh_prepped)
			CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), qtrue);
	} else if (i >= CS_MODELS && i < CS_MODELS+MAX_MODELS) {
		if (cl.refresh_prepped) {
			cl.model_draw[i-CS_MODELS] = re.RegisterModel (cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel (cl.configstrings[i]);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}
	} else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_MODELS) {
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound (cl.configstrings[i]);
	} else if (i >= CS_IMAGES && i < CS_IMAGES+MAX_MODELS) {
		if (cl.refresh_prepped)
			cl.image_precache[i-CS_IMAGES] = re.RegisterPic (cl.configstrings[i]);
	}
}


/*
=====================================================================
ACTION MESSAGES
=====================================================================
*/

/**
 * @brief Parsed a server send sound package
 * @sa svc_sound
 * @sa SV_StartSound
 */
static void CL_ParseStartSoundPacket (void)
{
	vec3_t  pos_v;
	float	*pos;
	int 	channel, ent;
	int 	sound_num;
	float 	volume;
	float 	attenuation;
	int		flags;
	float	ofs;

	flags = MSG_ReadByte (&net_message);
	sound_num = MSG_ReadByte (&net_message);

	if (flags & SND_VOLUME)
		volume = MSG_ReadByte (&net_message) / 255.0;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte (&net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	if (flags & SND_OFFSET)
		ofs = MSG_ReadByte (&net_message) / 1000.0;
	else
		ofs = 0;

	/* entity reletive */
	if (flags & SND_ENT) {
		channel = MSG_ReadShort(&net_message);
		ent = channel>>3;
		if (ent > MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	} else {
		ent = 0;
		channel = 0;
	}

	/* positioned in space */
	if (flags & SND_POS) {
		MSG_ReadPos (&net_message, pos_v);

		pos = pos_v;
	} else /* use entity number */
		pos = NULL;

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound (pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, ofs);
}

/**
 * @brief Reset the events
 * @note Also sets etUnused - if you get Timetable overflow messages, etUnused is NULL
 */
static void CL_EventReset (void)
{
	evTimes_t	*last, *et;
	int		i;

	/* reset events */
	for ( i = 0, et = evTimes; i < EV_TIMES-1; i++ ) {
		last = et++;
		et->next = last;
	}
	etUnused = et;
	/*	Com_Printf("et: etUnused = %p\n", etUnused);*/
	etCurrent = NULL;
}

/**
 * @brief
 * @sa G_ClientSpawn
 */
static void CL_Reset (sizebuf_t *sb)
{
	/* clear local entities */
	numLEs = 0;
	selActor = NULL;
	cl.numTeamList = 0;
	Cbuf_AddText( "numonteam1\n" );

	CL_EventReset();
	parsedDeath = qfalse;
	cl.eventTime = 0;
	nextTime = 0;
	shootTime = 0;
	impactTime = 0;
	blockEvents = qfalse;

	/* set the active player */
	MSG_ReadFormat(sb, ev_format[EV_RESET], &cls.team, &cl.actTeam);
	Com_Printf( "(player %i) It's team %i's round\n", cl.pnum, cl.actTeam );
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
static void CL_StartGame( sizebuf_t *sb )
{
	qboolean team_play = MSG_ReadByte(sb);

	/* init camera position and angles */
	memset(&cl.cam, 0, sizeof(camera_t));
	VectorSet(cl.cam.angles, 60.0, 60.0, 0.0);
	VectorSet(cl.cam.omega, 0.0, 0.0, 0.0);
	cl.cam.zoom = 1.25;
	CalcFovX();
	camera_mode = CAMERA_MODE_REMOTE;

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
			Cbuf_AddText("selectteam_init\n");
		}
	} else {
		MN_PushMenu(mn_hud->string);
		Cvar_Set("mn_active", mn_hud->string);
	}
}


/**
 * @brief
 */
static void CL_CenterView (sizebuf_t *sb)
{
	pos3_t pos;

	MSG_ReadFormat(sb, ev_format[EV_CENTERVIEW], &pos);
	V_CenterView(pos);
}


/**
 * @brief
 */
static void CL_EntAppear (sizebuf_t *sb)
{
	le_t	*le;
	int		entnum, type;
	pos3_t	pos;

	MSG_ReadFormat(sb, ev_format[EV_ENT_APPEAR], &entnum, &type, &pos);

	/* check if the ent is already visible */
	le = LE_Get(entnum);
	if (!le) {
		le = LE_Add(entnum);
	} else {
		Com_DPrintf("Entity appearing already visible... overwriting the old one\n");
		le->inuse = qtrue;
	}

	le->type = type;
	le->pos[0] = pos[0]; /* how to write this more elegantly? */
	le->pos[1] = pos[1];
	le->pos[2] = pos[2];
	Grid_PosToVec( &clMap, le->pos, le->origin );
}


/**
 * @brief Called whenever an entity disappears from view
 */
static void CL_EntPerish (sizebuf_t *sb)
{
	int		entnum, i;
	le_t	*le, *actor;

	MSG_ReadFormat(sb, ev_format[EV_ENT_PERISH], &entnum);

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
				 && VectorCompare(actor->pos, le->pos) ) {
				Com_DPrintf("CL_EntPerish: le of type ET_ITEM hidden\n");
				FLOOR(actor) = NULL;
			}
		break;
	case ET_ACTOR:
	case ET_UGV:
		Com_DestroyInventory(&le->i);
		break;
	case ET_BREAKABLE:
		break;
	default:
		break;
	}

	if (!(le->team && le->state && le->team == cls.team && !(le->state & STATE_DEAD)))
		le->inuse = qfalse;

/*	le->startTime = cl.time; */
/*	le->think = LET_Perish; */
}


static le_t	*lastMoving;

/**
 * @brief Set the lastMoving entity (set to the actor who last
 * walked, turned, crouched or stood up).
 */
extern void CL_SetLastMoving (le_t *le)
{
	lastMoving = le;
}

/**
 * @brief
 */
static void CL_ActorDoStartMove (sizebuf_t *sb)
{
	int	entnum;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_START_MOVE], &entnum);

	CL_SetLastMoving(LE_Get(entnum));
}


/**
 * @brief
 */
static void CL_ActorAppear (sizebuf_t *sb)
{
	qboolean newActor;
	le_t *le;
	char tmpbuf[128];
	int entnum, modelnum1, modelnum2;
	int teamDescID = -1;

	/* check if the actor is already visible */
	entnum = MSG_ReadShort(sb);
	le = LE_Get(entnum);

	if (!le) {
		le = LE_Add(entnum);
		newActor = qtrue;
	} else {
/*		Com_Printf( "Actor appearing already visible... overwriting the old one\n" ); */
		le->inuse = qtrue;
		newActor = qfalse;
	}

	/* get the info */
	MSG_ReadFormat(sb, ev_format[EV_ACTOR_APPEAR],
				&le->team, &le->teamDesc, &le->pnum, &le->pos,
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
	le->model1 = cl.model_draw[ modelnum1 ];
	le->model2 = cl.model_draw[ modelnum2 ];
	Grid_PosToVec( &clMap, le->pos, le->origin );
	le->angles[YAW] = dangle[le->dir];

	le->contents = CONTENTS_ACTOR;
	VectorCopy( player_mins, le->mins );
	if (le->state & STATE_DEAD)
		VectorCopy( player_dead_maxs, le->maxs );
	else
		VectorCopy( player_maxs, le->maxs );

	le->think = LET_StartIdle;

	/* count spotted aliens */
	if (!(le->state & STATE_DEAD) && newActor && le->team != cls.team && le->team != TEAM_CIVILIAN)
		cl.numAliensSpotted++;

	if (cls.state == ca_active && !(le->state & STATE_DEAD)) {
		/* center view (if wanted) */
		if (cl_centerview->integer> 1 || (cl_centerview->integer == 1 && cl.actTeam != cls.team)) {
			VectorCopy( le->origin, cl.cam.reforg );
			Cvar_SetValue( "cl_worldlevel", le->pos[2] );
		}

		/* draw line of sight */
		if ( le->team != cls.team ) {
			if ( cl.actTeam == cls.team && lastMoving ) {
				ptl_t	*ptl;
				vec3_t eyes;
				VectorCopy(lastMoving->origin, eyes);
				if (lastMoving->state & STATE_CROUCHED)
					eyes[2] += EYE_HT_CROUCH;
				else
					eyes[2] += EYE_HT_STAND;
				ptl = CL_ParticleSpawn( "fadeTracer", 0, eyes, le->origin, NULL );
				if ( le->team == TEAM_CIVILIAN )
					VectorSet( ptl->color, 0.2, 0.2, 1 );
			}

			/* message */
			if ( le->team != TEAM_CIVILIAN ) {
				if (curCampaign) {
					if (le->teamDesc) {
						teamDescID = le->teamDesc - 1;
						if (RS_IsResearched_idx(RS_GetTechIdxByName(teamDesc[teamDescID].tech))) {
							Com_sprintf(tmpbuf, sizeof(tmpbuf), "%s %s!\n",
							_("Alien spotted:"), _(teamDesc[teamDescID].name));
							CL_DisplayHudMessage( tmpbuf, 2000 );
						} else
							CL_DisplayHudMessage( _("Alien spotted!\n"), 2000 );
					} else
						CL_DisplayHudMessage( _("Alien spotted!\n"), 2000 );
				} else
					CL_DisplayHudMessage( _("Enemy spotted!\n"), 2000 );
			} else
				CL_DisplayHudMessage( _("Civilian spotted!\n"), 2000 );

			/* update pathing as new actor could block path */
			if (newActor)
				CL_ConditionalMoveCalc(&clMap, selActor, MAX_ROUTE);
		}
	}

	/* add team members to the actor list */
	CL_AddActorToTeamList( le );

/*	Com_Printf( "Player at (%i %i %i) -> (%f %f %f)\n", */
/*		le->pos[0], le->pos[1], le->pos[2], le->origin[0], le->origin[1], le->origin[2] ); */
}


/**
 * @brief
 * @sa CL_ParseEvent
 */
static void CL_ActorStats (sizebuf_t *sb)
{
	le_t	*le;
	int		number, selActorTU;

	number = MSG_ReadShort(sb);
	le = LE_Get(number);

	if (!le || (le->type != ET_ACTOR && le->type != ET_UGV)) {
		Com_Printf( "Stats message ignored... LE not found or not an actor\n" );
		return;
	}

	if (le == selActor)
		selActorTU = selActor->TU;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_STATS], &le->TU, &le->HP, &le->STUN, &le->AP, &le->morale);

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
static void CL_ActorStateChange (sizebuf_t *sb)
{
	le_t	*le;
	int		number, state;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_STATECHANGE], &number, &state);

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
	int shape, size, max, maxSize;

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
		Grid_PosToVec( &clMap, le->pos, le->origin );
		VectorSubtract( le->origin, csi.ods[biggest].center, le->origin );
		/* fall to ground */
		le->origin[2] -= 28;
		le->angles[ROLL] = 90;
/*		le->angles[YAW] = 10*(int)(le->origin[0] + le->origin[1] + le->origin[2]) % 360; */
	} else {
		/* if no items in floor inventory, don't draw this le */
		le->inuse = qfalse;
	}
}


/**
 * @brief
 * @sa CL_InvDel
 */
static void CL_InvAdd (sizebuf_t *sb)
{
	int		number, nr, i;
	int 	container, x, y;
	le_t	*le;
	item_t 	item;

	number = MSG_ReadShort( sb );

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->entnum == number)
           	break;

	if (le->entnum != number) {
		nr = MSG_ReadShort(sb) / 6;
		Com_Printf( "InvAdd: message ignored... LE %i not found (type: %i)\n", number, le->type );
		for (; nr-- > 0;) {
			CL_ReceiveItem(sb, &item, &container, &x, &y);
			Com_Printf("InvAdd: ignoring:\n");
			Com_PrintItemDescription(item.t);
		}
		return;
	}
	if (!le->inuse)
		Com_DPrintf( "InvAdd: warning... LE found but not in-use\n" );

	nr = MSG_ReadShort(sb) / 6;

	for (; nr-- > 0;) {
		CL_ReceiveItem(sb, &item, &container, &x, &y);

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
static void CL_InvDel (sizebuf_t *sb)
{
	le_t	*le;
	int		number;
	int 	container, x, y;

	MSG_ReadFormat( sb, ev_format[EV_INV_DEL],
		&number, &container, &x, &y );

	le = LE_Get(number);
	if (!le) {
		Com_DPrintf( "InvDel message ignored... LE not found\n" );
		return;
	}

	Com_RemoveFromInventory( &le->i, container, x, y );
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
		CL_PlaceItem( le );
		break;
	}
}


/**
 * @brief
 */
static void CL_InvAmmo (sizebuf_t *sb)
{
	invList_t	*ic;
	le_t	*le;
	int		number;
	int		ammo, type, container, x, y;

	MSG_ReadFormat(sb, ev_format[EV_INV_AMMO],
		&number, &ammo, &type, &container, &x, &y);

	le = LE_Get(number);
	if (!le) {
		Com_DPrintf( "InvAmmo message ignored... LE not found\n" );
		return;
	}

	if (le->team != cls.team)
		return;

	ic = Com_SearchInInventory( &le->i, container, x, y );
	if ( !ic )
		return;

	/* set new ammo */
	ic->item.a = ammo;
	ic->item.m = type;
}


/**
 * @brief
 */
static void CL_InvReload (sizebuf_t *sb)
{
	invList_t	*ic;
	le_t	*le;
	int		number;
	int		ammo, type, container, x, y;

	MSG_ReadFormat( sb, ev_format[EV_INV_RELOAD],
		&number, &ammo, &type, &container, &x, &y );

	S_StartLocalSound( "weapons/verschluss.wav" );

	le = LE_Get( number );
	if (!le) {
		Com_DPrintf( "CL_InvReload: only sound played\n" );
		return;
	}

	if (le->team != cls.team)
		return;

	ic = Com_SearchInInventory( &le->i, container, x, y );
	if (!ic)
		return;

	/* if the displaced clip had any remaining bullets */
	/* store them as loose, unless the removed clip was full */
	if (curCampaign
		 && ic->item.a > 0
		 && ic->item.a != csi.ods[ic->item.t].ammo) {
		assert (ammo == csi.ods[ic->item.t].ammo);
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

	if (!cl_logevents->value)
		return;

	logfile = fopen( va( "%s/events.log", FS_Gamedir() ), "a" );
	fprintf( logfile, "%10i %s\n", cl.eventTime, ev_names[num] );
	fclose( logfile );
}


/**
 * @brief Called in case a svc_event was send via the network buffer
 * @sa CL_ParseServerMessage
 */
static void CL_ParseEvent (void)
{
	evTimes_t *et, *last, *cur;
	qboolean now;
	int oldCount, length;
	int eType;
	int time;

	while ( ( eType = MSG_ReadByte( &net_message ) ) != 0 ) {
		if (net_message.readcount > net_message.cursize) {
			Com_Error (ERR_DROP,"CL_ParseEvent: Bad event message");
			break;
		}

		/* check instantly flag */
		if ( eType & INSTANTLY ) {
			now = qtrue;
			eType &= ~INSTANTLY;
		} else
			now = qfalse;

		/* check if eType is valid */
		if ( eType < 0 || eType >= EV_NUM_EVENTS )
			Com_Error( ERR_DROP, "CL_ParseEvent: invalid event %s\n", ev_names[eType] );

		if ( !ev_func[eType] )
			Com_Error( ERR_DROP, "CL_ParseEvent: no handling function for event %i\n", eType );

		oldCount = net_message.readcount;
		length = MSG_LengthFormat( &net_message, ev_format[eType] );

		if ( now ) {
			/* log and call function */
			CL_LogEvent( eType );
			ev_func[eType]( &net_message );
		} else {
			if ( evWp-evBuf + length+2 >= EV_STORAGE_SIZE )
				evWp = evBuf;

			/* get event time */
			if ( nextTime < cl.eventTime )
				nextTime = cl.eventTime;
			if ( impactTime < cl.eventTime )
				impactTime = cl.eventTime;

			if (eType == EV_ACTOR_DIE)
				parsedDeath = qtrue;

			if ( eType == EV_ACTOR_DIE || eType == EV_MODEL_EXPLODE )
				time = impactTime;
			else if ( eType == EV_ACTOR_SHOOT || eType == EV_ACTOR_SHOOT_HIDDEN )
				time = shootTime;
			else
				time = nextTime;

			if ((eType == EV_ENT_APPEAR || eType == EV_INV_ADD)) {
				if (parsedDeath) { /* drop items after death (caused by impact) */
					time = impactTime + 400;
					/* EV_INV_ADD messages are the last event sent after a death */
					if (eType == EV_INV_ADD)
						parsedDeath = qfalse;
				} else if (impactTime > cl.eventTime) { /* item thrown on the ground */
					time = impactTime + 75;
				}
			}

			/* calculate time interval before the next event */
			switch ( eType ) {
			case EV_ACTOR_APPEAR:
				if ( cls.state == ca_active && cl.actTeam != cls.team )
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
					qboolean first;
					int type;
					/* int obj_idx; byte weap_idx, fd_idx; */

					MSG_ReadFormat(&net_message, ev_format[EV_ACTOR_SHOOT_HIDDEN], &first, &type);
					/* MSG_ReadFormat(&net_message, ev_format[EV_ACTOR_SHOOT_HIDDEN], &first, &obj_idx, &weap_idx, &fd_idx); */

					if (first) {
						nextTime += 500;
						impactTime = shootTime = nextTime;
					} else {
						fd = GET_FIREDEF(type);
						/* fd = GET_FIREDEF(obj_idx,weap_idx,fd_idx); */
/*
						TODO: not needed? and SF_BOUNCED?
						if ( fd->speed )
							impactTime = shootTime + 1000 * VectorDist( muzzle, impact ) / fd->speed;
						else
*/
							impactTime = shootTime;
						nextTime = shootTime + 1400;
						if ( fd->rof )
							shootTime += 1000 / fd->rof;
					}
					parsedDeath = qfalse;
				}
				break;
			case EV_ACTOR_SHOOT:
				{
					fireDef_t	*fd;
					int		flags, dummy;
					int		type;
					/* int obj_idx; byte weap_idx, fd_idx; */
					vec3_t	muzzle, impact;

					/* read data */
					MSG_ReadFormat(&net_message, ev_format[EV_ACTOR_SHOOT], &dummy, &type, &flags, &muzzle, &impact, &dummy);
					/* MSG_ReadFormat(&net_message, ev_format[EV_ACTOR_SHOOT], &dummy, &obj_idx, &weap_idx, &fd_idx, &flags, &muzzle, &impact, &dummy); */

					fd = GET_FIREDEF( type );
					/* fd = GET_FIREDEF(obj_idx,weap_idx,fd_idx); */

					if ( !(flags & SF_BOUNCED) ) {
						/* shooting */
						if ( fd->speed )
							impactTime = shootTime + 1000 * VectorDist( muzzle, impact ) / fd->speed;
						else
							impactTime = shootTime;
						if ( cl.actTeam != cls.team )
							nextTime = shootTime + 1400;
						else
							nextTime = shootTime + 400;
						if ( fd->rof )
							shootTime += 1000 / fd->rof;
					} else {
						/* only a bounced shot */
						time = impactTime;
						if ( fd->speed ) {
							impactTime += 1000 * VectorDist( muzzle, impact ) / fd->speed;
							nextTime = impactTime;
						}
					}
					parsedDeath = qfalse;
				}
				break;
			case EV_ACTOR_THROW:
				nextTime += MSG_ReadShort( &net_message );
				impactTime = shootTime = nextTime;
				parsedDeath = qfalse;
				break;
			}

			/* add to timetable */
			last = NULL;
			for (et = etCurrent; et; et = et->next) {
				if (et->start > time)
					break;
				last = et;
			}

			if (!etUnused)
				Com_Error(ERR_DROP, "CL_ParseEvent: timetable overflow - no EV_RESET event?\n");

			cur = etUnused;
			etUnused = cur->next;
/*			Com_Printf("cur->next: etUnused = %p\n", etUnused);*/

			cur->start = time;
			cur->pos = evWp - evBuf;

			if (last)
				last->next = cur;
			else
				etCurrent = cur;
			cur->next = et;

			/* copy data */
			*evWp++ = eType;
			memcpy( evWp, net_message.data + oldCount, length );
			evWp += length;
		}
		if (net_message.readcount - oldCount != length) {
			int correct;

			if (now) {
				correct = 0;
			} else {
				switch ( eType ) {
				case EV_ACTOR_SHOOT_HIDDEN:
				case EV_ACTOR_SHOOT:
					correct = 0;
					break;
				case EV_ACTOR_THROW:
					correct = (net_message.readcount == oldCount + 2);
					break;
				default:
					correct = (net_message.readcount == oldCount);
				}
			}
			if (!correct)
				Com_Printf ("Warning: message for event %s has wrong lenght %i, should be %i.\n", ev_names[eType], net_message.readcount - oldCount, length);
			net_message.readcount = oldCount + length;
		}
	}
}


/**
 * @brief
 * @sa CL_Frame
 * @sa CL_LogEvent
 */
extern void CL_Events (void)
{
	evTimes_t	*last;
	byte		eType;

	if ( cls.state < ca_connected )
		return;

	while ( !blockEvents && etCurrent && cl.eventTime >= etCurrent->start ) {
		/* get event type */
		evStorage.readcount = etCurrent->pos;
		eType = MSG_ReadByte( &evStorage );

#if 0
		/* check if eType is valid */
		if ( eType >= EV_NUM_EVENTS )
			Com_Error( ERR_DROP, "CL_Events: invalid event %i\n", eType );
#endif

		/* free timetable entry */
		last = etCurrent;
		etCurrent = etCurrent->next;
		last->next = etUnused;
		etUnused = last;
/*		Com_Printf("last: etUnused = %p\n", etUnused);*/

		/* call function */
		CL_LogEvent( eType );
		ev_func[eType]( &evStorage );
	}
}


/**
 * @brief
 */
extern void CL_InitEvents (void)
{
	SZ_Init( &evStorage, evBuf, EV_STORAGE_SIZE );
	evStorage.cursize = EV_STORAGE_SIZE;
	evWp = evBuf;
}


/**
 * @brief
 */
static void CL_ShowNet (const char *s)
{
	if (cl_shownet->value >= 2)
		Com_Printf ("%3i:%s\n", net_message.readcount-1, s);
}

/**
 * @brief
 * @sa CL_ReadPackets
 */
void CL_ParseServerMessage (void)
{
	int			cmd;
	char		*s;
	int			i;

	/* if recording demos, copy the message out */
	if (cl_shownet->value == 1)
		Com_Printf ("%i ",net_message.cursize);
	else if (cl_shownet->value >= 2)
		Com_Printf ("------------------\n");

	/* parse the message */
	while (1) {
		if (net_message.readcount > net_message.cursize) {
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1) {
			CL_ShowNet("END OF MESSAGE");
			break;
		}

		if (cl_shownet->value >= 2) {
			if (!svc_strings[cmd])
				Com_Printf("%3i:BAD CMD %i\n", net_message.readcount-1,cmd);
			else
				CL_ShowNet(svc_strings[cmd]);
		}

		/* other commands */
		switch (cmd) {
		case svc_nop:
/*			Com_Printf ("svc_nop\n"); */
			break;

		case svc_disconnect:
			Com_Error (ERR_DISCONNECT,"Server disconnected. Not attempting to reconnect.\n");
			break;

		case svc_reconnect:
			Com_Printf ("Server disconnected, reconnecting\n");
			cls.state = ca_connecting;
			cls.connect_time = -99999;	/* CL_CheckForResend() will fire immediately */
			break;

		case svc_print:
			i = MSG_ReadByte (&net_message);
			if (i == PRINT_CHAT) {
				S_StartLocalSound ("misc/talk.wav");
				con.ormask = 128;
			}
			Com_Printf ("%s", MSG_ReadString (&net_message));
			con.ormask = 0;
			break;

		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString (&net_message));
			break;

		case svc_stufftext:
			s = MSG_ReadString (&net_message);
			Com_DPrintf ("stufftext: %s\n", s);
			Cbuf_AddText (s);
			break;

		case svc_serverdata:
			Cbuf_Execute ();		/* make sure any stuffed commands are done */
			CL_ParseServerData ();
			break;

		case svc_configstring:
			CL_ParseConfigString ();
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			break;

		case svc_event:
			CL_ParseEvent();
			break;

		case svc_bad:
			Com_Printf("CL_ParseServerMessage: bad server message %d\n", cmd);
			break;

		default:
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Illegible server message %d\n", cmd);
			break;
		}
	}

	CL_AddNetgraph ();
}
