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

char *svc_strings[256] =
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
char *ev_format[] =
{
	"",					/* EV_NULL */
	"bb",				/* EV_RESET */
	"",					/* EV_START */
	"b",				/* EV_ENDROUND */

	"bb***",			/* EV_RESULTS */
	"g",				/* EV_CENTERVIEW */

	"sbg",				/* EV_ENT_APPEAR */
	"s",				/* EV_ENT_PERISH */

	"!sbbgbbbssbsb",	/* EV_ACTOR_APPEAR; beware of the '!' */
	"s",				/* EV_ACTOR_START_MOVE */
	"sb",				/* EV_ACTOR_TURN */
	"!s*",				/* EV_ACTOR_MOVE; beware of the '!' */
	"sbgg",			    /* EV_ACTOR_START_SHOOT */
	"sbbppb",			/* EV_ACTOR_SHOOT; the last 'b' cannot be 'd' */
	"bb",				/* EV_ACTOR_SHOOT_HIDDEN */
	"sbbpp",			/* EV_ACTOR_THROW */
	"ss",				/* EV_ACTOR_DIE */
	"!sbbbbb",			/* EV_ACTOR_STATS; beware of the '!' */
	"ss",				/* EV_ACTOR_STATECHANGE */

	"s*",				/* EV_INV_ADD */
	"sbbb",				/* EV_INV_DEL */
	"sbbbbb",			/* EV_INV_AMMO */

	"s",				/* EV_MODEL_PERISH */
	"s",				/* EV_MODEL_EXPLODE */
	"sg*"				/* EV_SPAWN_PARTICLE */
};

char *ev_names[] =
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

	"EV_MODEL_PERISH",
	"EV_MODEL_EXPLODE",

	"EV_SPAWN_PARTICLE"
};

void CL_Reset( sizebuf_t *sb );
void CL_StartGame( sizebuf_t *sb );
void CL_CenterView( sizebuf_t *sb );
void CL_EntAppear( sizebuf_t *sb );
void CL_EntPerish( sizebuf_t *sb );
void CL_ActorDoStartMove( sizebuf_t *sb );
void CL_ActorAppear( sizebuf_t *sb );
void CL_ActorStats( sizebuf_t *sb );
void CL_ActorStateChange( sizebuf_t *sb );
void CL_ActorShootHidden( sizebuf_t *sb );
void CL_InvAdd( sizebuf_t *sb );
void CL_InvDel( sizebuf_t *sb );
void CL_InvAmmo( sizebuf_t *sb );

void (*ev_func[])( sizebuf_t *sb ) =
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

	LM_Perish,
	LM_Explode,

	CL_ParticleSpawnFromSizeBuf
};

#define EV_STORAGE_SIZE		32768
#define EV_TIMES			4096

sizebuf_t	evStorage;
byte		evBuf[EV_STORAGE_SIZE];
byte		*evWp;

typedef struct evTimes_s
{
	int		start;
	int		pos;
	struct	evTimes_s *next;
} evTimes_t;

evTimes_t	evTimes[EV_TIMES];
evTimes_t	*etUnused, *etCurrent;

qboolean blockEvents;

int nextTime;
int shootTime;
int impactTime;

/*============================================================================= */

/*
======================
CL_RegisterSounds
======================
*/
void CL_RegisterSounds (void)
{
	int		i, j;

	S_BeginRegistration ();

	/* load game sounds */
	for (i=1 ; i<MAX_SOUNDS ; i++) {
		if (!cl.configstrings[CS_SOUNDS+i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
		/* pump message loop */
		Sys_SendKeyEvents ();
	}
	/* load weapon sounds */
	for ( i = 0; i < csi.numODs; i++ )
		for ( j = 0; j < 2; j++ ) {
			if ( csi.ods[i].fd[j].fireSound[0] )
				S_RegisterSound( csi.ods[i].fd[j].fireSound );
			if ( csi.ods[i].fd[j].impactSound[0] )
				S_RegisterSound( csi.ods[i].fd[j].impactSound );
			/* pump message loop */
			Sys_SendKeyEvents ();
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
void CL_ParseServerData (void)
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
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf ("%c%s\n", 2, str);
		/* need to prep refresh at next oportunity */
		cl.refresh_prepped = qfalse;
	}
}

/**
 * @brief
 * @sa PF_Configstring
 */
void CL_ParseConfigString (void)
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

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
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


/*
=====================
CL_Reset
=====================
*/
void CL_Reset( sizebuf_t *sb )
{
	evTimes_t	*last, *et;
	int		i;

	/* clear local entities */
	numLEs = 0;
	selActor = NULL;
	cl.numTeamList = 0;
	Cbuf_AddText( "numonteam1\n" );

	/* reset events */
	for ( i = 0, et = evTimes; i < EV_TIMES-1; i++ ) {
		last = et++;
		et->next = last;
	}
	etUnused = et;
/*	Com_Printf("et: etUnused = %p\n", etUnused);*/
	etCurrent = NULL;
	cl.eventTime = 0;
	nextTime = 0;
	shootTime = 0;
	impactTime = 0;
	blockEvents = qfalse;

	/* set the active player */
	MSG_ReadFormat(sb, ev_format[EV_RESET], &cls.team, &cl.actTeam);
	Com_Printf( "(player %i) It's team %i's round\n", cl.pnum, cl.actTeam );
}


/*
=====================
CL_StartGame
=====================
*/
void CL_StartGame( sizebuf_t *sb )
{
	/* init camera position and angles */
	memset( &cl.cam, 0, sizeof( camera_t ) );
	VectorSet( cl.cam.angles, 60.0, 60.0, 0.0 );
	VectorSet( cl.cam.omega, 0.0, 0.0, 0.0 );
	cl.cam.zoom = 1.0;
	camera_mode = CAMERA_MODE_REMOTE;

	/* center on first actor */
	cl_worldlevel->modified = qtrue;
	if ( cl.numTeamList ) {
		le_t	*le;
		le = cl.teamList[0];
		VectorCopy( le->origin, cl.cam.reforg );
		Cvar_SetValue( "cl_worldlevel", le->pos[2] );
	}

	/* activate the renderer */
	cls.state = ca_active;

	/* activate hud */
	MN_PushMenu( mn_hud->string );
	Cvar_Set( "mn_active", mn_hud->string );
}


/*
=====================
CL_CenterView
=====================
*/
void CL_CenterView( sizebuf_t *sb )
{
	pos3_t pos;

	MSG_ReadFormat(sb, ev_format[EV_CENTERVIEW], &pos);
	V_CenterView(pos);
}


/*
=====================
CL_EntAppear
=====================
*/
void CL_EntAppear( sizebuf_t *sb )
{
	le_t	*le;
	int		entnum, type;
	pos3_t	pos;

	MSG_ReadFormat(sb, ev_format[EV_ENT_APPEAR], &entnum, &type, &pos);

	/* check if the ent is already visible */
	le = LE_Get(entnum);
	if ( !le )
		le = LE_Add(entnum);
	else
		Com_Printf("Entity appearing already visible... overwriting the old one\n");

	le->type = type;
	le->pos[0] = pos[0]; /* how to write this more elegantly? */
	le->pos[1] = pos[1];
	le->pos[2] = pos[2];
	Grid_PosToVec( &clMap, le->pos, le->origin );
}


/**
 * @brief Called whenever an entity dies
 */
void CL_EntPerish( sizebuf_t *sb )
{
	int		entnum;
	le_t	*le;

	MSG_ReadFormat(sb, ev_format[EV_ENT_PERISH], &entnum);

	le = LE_Get(entnum);

	if ( !le ) {
		Com_Printf( "Delete request ignored... LE not found\n" );
		return;
	}

	/* decrease the count of spotted aliens */
	if ( (le->type == ET_ACTOR || le->type == ET_UGV) && !(le->state & STATE_DEAD) && le->team != cls.team && le->team != TEAM_CIVILIAN )
		cl.numAliensSpotted--;

	if ( le->type == ET_ITEM ) {
		le_t *actor;
		int i;
		
		Com_EmptyContainer(&le->i, csi.idFloor);

		/* search owners (there can be many, some of them dead) */
		for (i = 0, actor = LEs; i < numLEs; i++, actor++)
			if ( actor->inuse 
				 && (actor->type == ET_ACTOR || actor->type == ET_UGV)
				 && VectorCompare(actor->pos, le->pos) ) {
				FLOOR(actor) = NULL;
			}
	} else {
		Com_DestroyInventory(&le->i);
	}

	/* FIXME: Check whether this call is needed */
	/* the actor die event should handle this already - doesn't it? */
#if 0
	if ( le->type == ET_ACTOR || le->type == ET_UGV )
		CL_RemoveActorFromTeamList( le );
#endif

	le->inuse = qfalse;

/*	le->startTime = cl.time; */
/*	le->think = LET_Perish; */
}


/*
=====================
CL_ActorDoStartMove
=====================
*/
le_t	*lastMoving;

void CL_ActorDoStartMove( sizebuf_t *sb )
{
	int	entnum;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_START_MOVE], &entnum);

	lastMoving = LE_Get(entnum);
}


/*
=====================
CL_ActorAppear
=====================
*/
void CL_ActorAppear( sizebuf_t *sb )
{
	qboolean newActor;
	le_t	*le;
	int		entnum, modelnum1, modelnum2;

	/* check if the actor is already visible */
	entnum = MSG_ReadShort( sb );
	le = LE_Get( entnum );

	if ( !le ) {
		le = LE_Add( entnum );
		newActor = qtrue;
	} else {
/*		Com_Printf( "Actor appearing already visible... overwriting the old one\n" ); */
		newActor = qfalse;
	}

	/* get the info */
	MSG_ReadFormat( sb, ev_format[EV_ACTOR_APPEAR],
		&le->team, &le->pnum, &le->pos, &le->dir,
		&le->right, &le->left,
		&modelnum1, &modelnum2, &le->skinnum, &le->state, &le->fieldSize );

	if ( le->fieldSize == ACTOR_SIZE_NORMAL ) {
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
	VectorCopy( player_maxs, le->maxs );

	le->think = LET_StartIdle;

	/* count spotted aliens */
	if ( !(le->state & STATE_DEAD) && newActor && le->team != cls.team && le->team != TEAM_CIVILIAN )
		cl.numAliensSpotted++;

	if ( cls.state == ca_active && !(le->state & STATE_DEAD) ) {
		/* center view (if wanted) */
		if ( (int)cl_centerview->value > 1 || ((int)cl_centerview->value == 1 && cl.actTeam != cls.team) ) {
			VectorCopy( le->origin, cl.cam.reforg );
			Cvar_SetValue( "cl_worldlevel", le->pos[2] );
		}

		/* draw line of sight */
		if ( le->team != cls.team ) {
			if ( cl.actTeam == cls.team && lastMoving ) {
				ptl_t	*ptl;
				ptl = CL_ParticleSpawn( "fadeTracer", 0, lastMoving->origin, le->origin, NULL );
				if ( le->team == TEAM_CIVILIAN )
					VectorSet( ptl->color, 0.2, 0.2, 1 );
			}

			/* message */
			if ( le->team != TEAM_CIVILIAN ) {
				if ( curCampaign )
					CL_DisplayHudMessage( _("Alien spotted!\n"), 2000 );
				else
					CL_DisplayHudMessage( _("Enemy spotted!\n"), 2000 );
			} else
				CL_DisplayHudMessage( _("Civilian spotted!\n"), 2000 );
		}
	}

	/* add team members to the actor list */
	CL_AddActorToTeamList( le );

/*	Com_Printf( "Player at (%i %i %i) -> (%f %f %f)\n", */
/*		le->pos[0], le->pos[1], le->pos[2], le->origin[0], le->origin[1], le->origin[2] ); */
}


/*
=====================
CL_ActorStats
=====================
*/
void CL_ActorStats( sizebuf_t *sb )
{
	le_t	*le;
	int		number;

	number = MSG_ReadShort( sb );
	le = LE_Get( number );

	if ( !le ) {
		Com_Printf( "Stats message ignored... LE not found\n" );
		return;
	}

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_STATS], &le->TU, &le->HP, &le->STUN, &le->AP, &le->morale);

	if ( le->TU > le->maxTU )
		le->maxTU = le->TU;
	if ( le->HP > le->maxHP )
		le->maxHP = le->HP;
	if ( le->morale > le->maxMorale )
		le->maxMorale = le->morale;
}


/*
=====================
CL_ActorStateChange
=====================
*/
void CL_ActorStateChange( sizebuf_t *sb )
{
	le_t	*le;
	int		number, state;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_STATECHANGE], &number, &state);

	le = LE_Get(number);
	if ( !le ) {
		Com_Printf( "StateChange message ignored... LE not found\n" );
		return;
	}

	le->state = state;
	le->think = LET_StartIdle;

	/* killed by the server: no animation is played, etc. */
	if (state & STATE_DEAD)
		CL_RemoveActorFromTeamList(le);
}

/*
=====================
CL_ActorShootHidden
=====================
*/
void CL_ActorShootHidden( sizebuf_t *sb )
{
	fireDef_t	*fd;
	qboolean	first;
	int		type;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_SHOOT_HIDDEN], &first, &type);

	/* get the fire def */
	fd = GET_FIREDEF( type );

	/* start the sound */
	if ( fd->fireSound[0] && ( ( first && fd->soundOnce ) || ( !first && !fd->soundOnce ) ) )
		S_StartLocalSound( fd->fireSound );
}


/*
=====================
CL_BiggestItem
=====================
*/
int CL_BiggestItem( invList_t *ic )
{
	int shape, size, max, maxSize;

	maxSize = 0;
	for ( max = ic->item.t; ic; ic = ic->next ) {
		shape = csi.ods[ic->item.t].shape;
		size = (shape>>24 & 0xF) * (shape>>28 & 0xF);
		if ( size > maxSize ) {
			max = ic->item.t;
			maxSize = size;
		}
	}

	return max;
}


/**
 * @brief
 */
void CL_PlaceItem( le_t *le )
{
	le_t *actor;
	int	biggest;
	int i;

	/* search owners (there can be many, some of them dead) */
	for (i = 0, actor = LEs; i < numLEs; i++, actor++)
		if ( actor->inuse 
			 && (actor->type == ET_ACTOR || actor->type == ET_UGV)
			 && VectorCompare(actor->pos, le->pos) ) {
#if PARANOID
		Com_Printf("CL_PlaceItem: shared container: '%p'\n", FLOOR(le));
#endif
		FLOOR(actor) = FLOOR(le);
		}
	
	if (FLOOR(le)) {
		biggest = CL_BiggestItem(FLOOR(le));
		le->model1 = cl.model_weapons[biggest];
		Grid_PosToVec( &clMap, le->pos, le->origin );
		VectorSubtract( le->origin, csi.ods[biggest].center, le->origin );
		le->origin[2] -= 28;
		le->angles[ROLL] = 90;
/*		le->angles[YAW] = 10*(int)(le->origin[0] + le->origin[1] + le->origin[2]) % 360; */
	}
}


/**
 * @brief
 */
void CL_InvAdd( sizebuf_t *sb )
{
	int		number;
	le_t	*le;

	number = MSG_ReadShort( sb );

	le = LE_Get( number );
	if ( !le ) {
		Com_Printf( "InvAdd message ignored... LE not found\n" );
		return;
	}

	{
		item_t item;
		int container, x, y;
		int nr = MSG_ReadShort(sb) / 6;

		for (; nr-- > 0;) {

			CL_ReceiveItem(sb, &item, &container, &x, &y);

			Com_AddToInventory(&le->i, item, container, x, y);

			if ( container == csi.idRight )
				le->right = item.t;
			else if ( container == csi.idLeft )
				le->left = item.t;
		}
	}

	switch ( le->type ) {
	case ET_ACTOR:
	case ET_UGV:
		le->think = LET_StartIdle;
		break;
	case ET_ITEM:
		CL_PlaceItem( le );
		break;
	}
}


/*
=====================
CL_InvDel
=====================
*/
void CL_InvDel( sizebuf_t *sb )
{
	le_t	*le;
	int		number;
	int 	container, x, y;

	MSG_ReadFormat( sb, ev_format[EV_INV_DEL],
		&number, &container, &x, &y );

	le = LE_Get( number );
	if ( !le ) {
		Com_Printf( "InvDel message ignored... LE not found\n" );
		return;
	}

	Com_RemoveFromInventory( &le->i, container, x, y );
	if ( container == csi.idRight )
		le->right = NONE;
	else if ( container == csi.idLeft )
		le->left = NONE;

	switch ( le->type ) {
	case ET_ACTOR:
	case ET_UGV:
		le->think = LET_StartIdle;
		break;
	case ET_ITEM:
		CL_PlaceItem( le );
		break;
	}
}


/*
=====================
CL_InvAmmo
=====================
*/
void CL_InvAmmo( sizebuf_t *sb )
{
	invList_t	*ic;
	le_t	*le;
	int		number;
	int		ammo, type, container, x, y;

	MSG_ReadFormat( sb, ev_format[EV_INV_AMMO],
		&number, &ammo, &type, &container, &x, &y );

	le = LE_Get( number );
	if ( !le ) {
		Com_Printf( "InvAmmo message ignored... LE not found\n" );
		return;
	}

	if ( le->team != cls.team )
		return;

	ic = Com_SearchInInventory( &le->i, container, x, y );
	if ( !ic )
		return;

	/* if we're reloading and the displaced clip had any remaining */
	/* bullets, store them as loose, unless the removed clip was full */
	if ( curCampaign
		 && le->team == cls.team
		 &&	ammo == csi.ods[ic->item.t].ammo
		 && ic->item.a > 0
		 && ic->item.a != csi.ods[ic->item.t].ammo ) {
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

	if ( ic && csi.ods[ic->item.t].ammo == ammo && le->team != TEAM_ALIEN )
		S_StartLocalSound( "weapons/verschluss.wav" );
}


/*
=====================
CL_LogEvent
=====================
*/
void CL_LogEvent( int num )
{
	FILE *logfile;

	if ( !cl_logevents->value )
		return;

	logfile = fopen( va( "%s/events.log", FS_Gamedir() ), "a" );
	fprintf( logfile, "%10i %s\n", cl.eventTime, ev_names[num] );
	fclose( logfile );
}


/*
=====================
CL_ParseEvent
=====================
*/
void CL_ParseEvent( void )
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

			if ( eType == EV_ACTOR_DIE || eType == EV_MODEL_EXPLODE )
				time = impactTime;
			else if ( eType == EV_ACTOR_SHOOT )
				time = shootTime;
			else
				time = nextTime;

			/* calculate next and shoot time */
			switch ( eType ) {
			case EV_ACTOR_APPEAR:
				if ( cls.state == ca_active && cl.actTeam != cls.team )
					nextTime += 600;
				break;
			case EV_ACTOR_START_SHOOT:
				nextTime += 300;
				shootTime = nextTime;
				break;
			case EV_ACTOR_SHOOT_HIDDEN:
				{
					int flags, type;
					fireDef_t *fd;

					MSG_ReadFormat(&net_message, ev_format[EV_ACTOR_SHOOT_HIDDEN], &flags, &type);

					if ( !flags ) {
						fd = GET_FIREDEF(type);
						if ( fd->rof )
							nextTime += 1000 / fd->rof;
					} else { 
						nextTime += 500;
					}
					shootTime = nextTime;
					break;
				}
			case EV_ACTOR_SHOOT:
				{
					fireDef_t	*fd;
					int		type, flags, dummy;
					vec3_t	muzzle, impact;

					/* read data */
					MSG_ReadFormat(&net_message, ev_format[EV_ACTOR_SHOOT], &dummy, &type, &flags, &muzzle, &impact, &dummy);

					fd = GET_FIREDEF( type );
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
						if ( fd->speed )
							impactTime += 1000 * VectorDist( muzzle, impact ) / fd->speed;
					}
				}
				break;
			case EV_ACTOR_THROW:
				nextTime += MSG_ReadShort( &net_message );
				shootTime = impactTime = nextTime;
				break;
			}

			/* add to timetable */
			last = NULL;
			for ( et = etCurrent; et; et = et->next ) {
				if ( et->start > time )
					break;
				last = et;
			}

			if ( !etUnused )
				Com_Error( ERR_DROP, "CL_ParseEvent: timetable overflow\n" );
			cur = etUnused;
			etUnused = cur->next;
/*			Com_Printf("cur->next: etUnused = %p\n", etUnused);*/

			cur->start = time;
			cur->pos = evWp - evBuf;

			if ( last )
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


/*
=====================
CL_Events
=====================
*/
void CL_Events( void )
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


/*
=====================
CL_InitEvents
=====================
*/
void CL_InitEvents( void )
{
	SZ_Init( &evStorage, evBuf, EV_STORAGE_SIZE );
	evStorage.cursize = EV_STORAGE_SIZE;
	evWp = evBuf;
}


void SHOWNET(char *s)
{
	if (cl_shownet->value>=2)
		Com_Printf ("%3i:%s\n", net_message.readcount-1, s);
}

/*
=====================
CL_ParseServerMessage
=====================
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
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cl_shownet->value>=2) {
			if (!svc_strings[cmd])
				Com_Printf ("%3i:BAD CMD %i\n", net_message.readcount-1,cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}

		/* other commands */
		switch (cmd) {
		case svc_nop:
/*			Com_Printf ("svc_nop\n"); */
			break;

		case svc_disconnect:
			Com_Error (ERR_DISCONNECT,"Server disconnected\n");
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

		default:
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Illegible server message %d\n", cmd);
			break;
		}
	}

	CL_AddNetgraph ();
}
