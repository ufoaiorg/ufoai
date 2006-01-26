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
// cl_parse.c  -- parse a message received from the server

#include "client.h"

char *svc_strings[256] =
{
	"svc_bad",

	"svc_layout",
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
	"svc_download",
	"svc_playerinfo",
	"svc_event"
};

//=============================================================================

char *ev_format[] =
{
	"",					// EV_NULL
	"bb",				// EV_RESET
	"",					// EV_START
	"b",				// EV_ENDROUND
	"bb&",				// EV_RESULTS
	"g",				// EV_CENTERVIEW

	"!sbg",				// EV_ENT_APPEAR
	"!s",				// EV_ENT_PERISH

	"!sbbgbbbssbs",		// EV_ACTOR_APPEAR
	"!s",				// EV_ACTOR_START_MOVE
	"!sb",				// EV_ACTOR_TURN
	"!s*",				// EV_ACTOR_MOVE
	"!sbgg",			// EV_ACTOR_START_SHOOT
	"!sbbppb",			// EV_ACTOR_SHOOT
	"bb",				// EV_ACTOR_SHOOT_HIDDEN
	"sbbpp",			// EV_ACTOR_THROW
	"!sb",				// EV_ACTOR_DIE
	"!sbbb",			// EV_ACTOR_STATS
	"!ss",				// EV_ACTOR_STATECHANGE

	"!s&",				// EV_INV_ADD
	"sbbb",				// EV_INV_DEL
	"sbbbb",			// EV_INV_AMMO

	"s",				// EV_MODEL_PERISH
	"s"					// EV_MODEL_EXPLODE
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
	"EV_MODEL_EXPLODE"
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
	LM_Explode
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

//=============================================================================

void CL_DownloadFileName(char *dest, int destlen, char *fn)
{
	if (strncmp(fn, "players", 7) == 0)
		Com_sprintf (dest, destlen, "%s/%s", BASEDIRNAME, fn);
	else
		Com_sprintf (dest, destlen, "%s/%s", FS_Gamedir(), fn);
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean	CL_CheckOrDownloadFile (char *filename)
{
//	FILE *fp;
//	char	name[MAX_OSPATH];

	if (strstr (filename, ".."))
	{
		Com_Printf (_("Refusing to download a path with ..\n"));
		return true;
	}

	if (FS_LoadFile (filename, NULL) != -1)
	{	// it exists, no need to download
		return true;
	}

/*
	strcpy (cls.downloadname, filename);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted

	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

//ZOID
	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

//	FS_CreatePath (name);

	fp = fopen (name, "r+b");
	if (fp) { // it exists
		int len;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		cls.download = fp;

		// give the server an offset to start the download
		Com_Printf ("Resuming %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
			va("download %s %i", cls.downloadname, len));
	} else {
		Com_Printf ("Downloading %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
			va("download %s", cls.downloadname));
	}

	cls.downloadnumber++;

	return false;*/

	return true;
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void	CL_Download_f (void)
{
	char filename[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: download <filename>\n");
		return;
	}

	Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));

	if (strstr (filename, ".."))
	{
		Com_Printf (_("Refusing to download a path with ..\n"));
		return;
	}

	if (FS_LoadFile (filename, NULL) != -1)
	{	// it exists, no need to download
		Com_Printf(_("File already exists.\n"));
		return;
	}

	strcpy (cls.downloadname, filename);
	Com_Printf (_("Downloading %s\n"), cls.downloadname);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message,
		va("download %s", cls.downloadname));

	cls.downloadnumber++;
}

/*
======================
CL_RegisterSounds
======================
*/
void CL_RegisterSounds (void)
{
	int		i, j;

	S_BeginRegistration ();

	// load game sounds
	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!cl.configstrings[CS_SOUNDS+i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	// load weapon sounds
	for ( i = 0; i < csi.numODs; i++ )
		for ( j = 0; j < 2; j++ )
		{
			if ( csi.ods[i].fd[j].fireSound[0] )   S_RegisterSound( csi.ods[i].fd[j].fireSound );
			if ( csi.ods[i].fd[j].impactSound[0] ) S_RegisterSound( csi.ods[i].fd[j].impactSound );
			Sys_SendKeyEvents ();	// pump message loop
		}

	S_EndRegistration ();
}


/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload (void)
{
	int		size, percent;
	char	name[MAX_OSPATH];
	int		r;

	// read the data
	size = MSG_ReadShort (&net_message);
	percent = MSG_ReadByte (&net_message);
	if (size == -1)
	{
		Com_Printf (_("Server does not have this file.\n"));
		if (cls.download)
		{
			// if here, we tried to resume a file but the server said no
			fclose (cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download)
	{
		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

		FS_CreatePath (name);

		cls.download = fopen (name, "wb");
		if (!cls.download)
		{
			net_message.readcount += size;
			Com_Printf (_("Failed to open %s\n"), cls.downloadtempname);
			CL_RequestNextDownload ();
			return;
		}
	}

	fwrite (net_message.data + net_message.readcount, 1, size, cls.download);
	net_message.readcount += size;

	if (percent != 100)
	{
		// request next block
		// change display routines by zoid
#if 0
		Com_Printf (".");
		if (10*(percent/10) != cls.downloadpercent)
		{
			cls.downloadpercent = 10*(percent/10);
			Com_Printf ("%i%%", cls.downloadpercent);
		}
#endif
		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	}
	else
	{
		char	oldn[MAX_OSPATH];
		char	newn[MAX_OSPATH];

//		Com_Printf ("100%%\n");

		fclose (cls.download);

		// rename the temp file to it's final name
		CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
		CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);
		r = rename (oldn, newn);
		if (r)
			Com_Printf (_("failed to rename.\n"));

		cls.download = NULL;
		cls.downloadpercent = 0;

		// get another file if needed

		CL_RequestNextDownload ();
	}
}


/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData (void)
{
	extern cvar_t	*fs_gamedirvar;
	char	*str;
	int		i;
	
	Com_DPrintf (_("Serverdata packet received.\n"));
	//
	// wipe the client_state_t struct
	//
	CL_ClearState ();
	cls.state = ca_connected;

	// parse protocol version number
	i = MSG_ReadLong (&net_message);
	cls.serverProtocol = i;

	// compare versions
	if (i != PROTOCOL_VERSION)
		Com_Error (ERR_DROP,_("Server returned version %i, not %i"), i, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong (&net_message);
	cl.attractloop = MSG_ReadByte (&net_message);

	// game directory
	str = MSG_ReadString (&net_message);
	strncpy (cl.gamedir, str, sizeof(cl.gamedir)-1);

	// set gamedir
	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str))) || (!*str && (fs_gamedirvar->string || *fs_gamedirvar->string)))
		Cvar_Set("game", str);

	// parse player entity number
	cl.pnum = MSG_ReadShort (&net_message);

	// get the full level name
	str = MSG_ReadString (&net_message);

	if (cl.pnum == -1)
	{	// playing a cinematic or showing a pic, not a level
		SCR_PlayCinematic (str);
	}
	else
	{
		// seperate the printfs so the server message can have a color
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf ("%c%s\n", 2, str);

		// need to prep refresh at next oportunity
		cl.refresh_prepped = false;
	}
}

/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString (void)
{
	int		i;
	char	*s;
	char	olds[MAX_QPATH];

	i = MSG_ReadShort (&net_message);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	s = MSG_ReadString(&net_message);

	strncpy (olds, cl.configstrings[i], sizeof(olds));
	olds[sizeof(olds) - 1] = 0;

	strcpy (cl.configstrings[i], s);

	// do something apropriate 

	if (i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES)
		CL_SetLightstyle (i - CS_LIGHTS);
	else if (i == CS_CDTRACK)
	{
		if (cl.refresh_prepped)
			CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
	}
	else if (i >= CS_MODELS && i < CS_MODELS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
		{
			cl.model_draw[i-CS_MODELS] = re.RegisterModel (cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel (cl.configstrings[i]);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}
	}
	else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound (cl.configstrings[i]);
	}
	else if (i >= CS_IMAGES && i < CS_IMAGES+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.image_precache[i-CS_IMAGES] = re.RegisterPic (cl.configstrings[i]);
	}
	/*else if (i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		if (cl.refresh_prepped && strcmp(olds, s))
			CL_ParseClientinfo (i-CS_PLAYERSKINS);
	}*/
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

	if (flags & SND_ENT)
	{	// entity reletive
		channel = MSG_ReadShort(&net_message); 
		ent = channel>>3;
		if (ent > MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}

	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{	// positioned in space
		MSG_ReadPos (&net_message, pos_v);
 
		pos = pos_v;
	}
	else	// use entity number
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

	// clear local entities
	numLEs = 0;
	selActor = NULL;
	cl.numTeamList = 0;
	Cbuf_AddText( "numonteam1\n" );

	// reset events
	for ( i = 0, et = evTimes; i < EV_TIMES-1; i++ )
	{
		last = et++;
		et->next = last;
	}
	etUnused = et;
	etCurrent = NULL;
	cl.eventTime = 0;
	nextTime = 0;
	shootTime = 0;
	impactTime = 0;
	blockEvents = false;

	// set the active player
	cls.team = MSG_ReadByte( sb );
	cl.actTeam = MSG_ReadByte( sb );
	Com_Printf( _("(player %i) It's team %i's round\n"), cl.pnum, cl.actTeam );
}


/*
=====================
CL_StartGame
=====================
*/
void CL_StartGame( sizebuf_t *sb )
{
	// init camera position and angles
	memset( &cl.cam, 0, sizeof( camera_t ) );
	VectorSet( cl.cam.angles, 60.0, 60.0, 0.0 );
	VectorSet( cl.cam.omega, 0.0, 0.0, 0.0 );
	cl.cam.zoom = 1.0;

	// center on first actor
	cl_worldlevel->modified = true;
	if ( cl.numTeamList )
	{
		le_t	*le;
		le = cl.teamList[0];
		VectorCopy( le->origin, cl.cam.reforg );
		Cvar_SetValue( "cl_worldlevel", le->pos[2] );
	}

	// activate the renderer
	cls.state = ca_active;

	// activate hud
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
	pos3_t	pos;
//	vec3_t	vec;

	MSG_ReadGPos( sb, pos );
	V_CenterView( pos );
}


/*
=====================
CL_EntAppear
=====================
*/
void CL_EntAppear( sizebuf_t *sb )
{
	le_t	*le;
	int		entnum;

	// check if the ent is already visible
	entnum = MSG_ReadShort( sb );
	le = LE_Get( entnum );

	if ( !le )
		le = LE_Add( entnum );
	else
		Com_Printf( _("Entity appearing already visible... overwriting the old one\n") );

	le->type = MSG_ReadByte( sb );
	MSG_ReadGPos( sb, le->pos );
	Grid_PosToVec( &clMap, le->pos, le->origin );
}


/*
=====================
CL_EntPerish
=====================
*/
void CL_EntPerish( sizebuf_t *sb )
{
	le_t	*le;

	le = LE_Get( MSG_ReadShort( sb ) );

	if ( !le ) 
	{
		Com_Printf( _("Delete request ignored... LE not found\n") );
		return;
	}

	// count spotted aliens
	if ( le->type == ET_ACTOR && !(le->state & STATE_DEAD) && le->team != cls.team && le->team != TEAM_CIVILIAN ) 
		cl.numAliensSpotted--;

	Com_DestroyInventory( &le->i );
	if ( le->type == ET_ITEM ) 
	{
		le_t *actor;
		actor = LE_Find( ET_ACTOR, le->pos );
		if ( actor ) actor->i.c[csi.idFloor] = NULL;
	}

	if ( le->type == ET_ACTOR )
		CL_RemoveActorFromTeamList( le );

	le->inuse = false;

//	le->startTime = cl.time;
//	le->think = LET_Perish;
}


/*
=====================
CL_ActorDoStartMove
=====================
*/
le_t	*lastMoving;

void CL_ActorDoStartMove( sizebuf_t *sb )
{
	lastMoving = LE_Get( MSG_ReadShort( sb ) );
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

	// check if the actor is already visible
	entnum = MSG_ReadShort( sb );
	le = LE_Get( entnum );

	if ( !le )
	{
		le = LE_Add( entnum );
		newActor = true;
	} else {		
//		Com_Printf( "Actor appearing already visible... overwriting the old one\n" );
		newActor = false;
	}

	// get the info
	MSG_ReadFormat( sb, ev_format[EV_ACTOR_APPEAR], 
		&le->team, &le->pnum, &le->pos, &le->dir, 
		&le->right, &le->left, 
		&modelnum1, &modelnum2, &le->skinnum, &le->state );	
	
	le->type = ET_ACTOR;
	le->modelnum1 = modelnum1;
	le->modelnum2 = modelnum2;
	le->model1 = cl.model_draw[ modelnum1 ];
	le->model2 = cl.model_draw[ modelnum2 ];
	Grid_PosToVec( &clMap, le->pos, le->origin );
	le->angles[YAW] = dangle[le->dir];

	le->contents = CONTENTS_ACTOR;
	VectorCopy( player_mins, le->mins );
	VectorCopy( player_maxs, le->maxs );

	le->addFunc = CL_AddActor;
	le->think = LET_StartIdle;

	// count spotted aliens
	if ( !(le->state & STATE_DEAD) && newActor && le->team != cls.team && le->team != TEAM_CIVILIAN ) 
		cl.numAliensSpotted++;

	if ( cls.state == ca_active && !(le->state & STATE_DEAD) ) 
	{
		// center view (if wanted)
		if ( (int)cl_centerview->value > 1 || ((int)cl_centerview->value == 1 && cl.actTeam != cls.team) )
		{
			VectorCopy( le->origin, cl.cam.reforg );
			Cvar_SetValue( "cl_worldlevel", le->pos[2] );
		}

		// draw line of sight
		if ( le->team != cls.team )
		{
			if ( cl.actTeam == cls.team && lastMoving )
			{
				ptl_t	*ptl;
				ptl = CL_ParticleSpawn( "fadeTracer", 0, lastMoving->origin, le->origin, NULL );
				if ( le->team == TEAM_CIVILIAN )
					VectorSet( ptl->color, 0.2, 0.2, 1 );
			}

			// message
			cl.msgTime = cl.time + 2000;
			if ( le->team != TEAM_CIVILIAN ) 
			{
				if ( curCampaign ) strcpy( cl.msgText, _("Alien spotted!\n") );
				else strcpy( cl.msgText, _("Enemy spotted!\n") );
			}
			else strcpy( cl.msgText, _("Civilian spotted!\n") );
		}
	}

	// add team members to the actor list
	CL_AddActorToTeamList( le );

//	Com_Printf( "Player at (%i %i %i) -> (%f %f %f)\n", 
//		le->pos[0], le->pos[1], le->pos[2], le->origin[0], le->origin[1], le->origin[2] );
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

	if ( !le ) 
	{
		Com_Printf( _("Stats message ignored... LE not found\n") );
		return;
	}

	le->TU = MSG_ReadByte( sb );
	le->HP = MSG_ReadByte( sb );
	le->morale = MSG_ReadByte( sb );
	if ( le->TU > le->maxTU ) le->maxTU = le->TU;
	if ( le->HP > le->maxHP ) le->maxHP = le->HP;
	if ( le->morale > le->maxMorale ) le->maxMorale = le->morale;
}


/*
=====================
CL_ActorStateChange
=====================
*/
void CL_ActorStateChange( sizebuf_t *sb )
{
	le_t	*le;
	int		number;

	number = MSG_ReadShort( sb );
	le = LE_Get( number );

	if ( !le ) 
	{
		Com_Printf( _("StateChange message ignored... LE not found\n") );
		return;
	}

	le->state = MSG_ReadShort( sb );
	le->think = LET_StartIdle;
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

	first = MSG_ReadByte( sb );
	type = MSG_ReadByte( sb );

	// get the fire def
	fd = GET_FIREDEF( type );

	// start the sound
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
	for ( max = ic->item.t; ic; ic = ic->next )
	{
		shape = csi.ods[ic->item.t].shape;
		size = (shape>>24 & 0xF) * (shape>>28 & 0xF);
		if ( size > maxSize ) 
		{
			max = ic->item.t;
			maxSize = size;
		}
	}

	return max;
}


/*
=====================
CL_PlaceItem
=====================
*/
void CL_PlaceItem( le_t *le )
{
	le_t *actor;
	int	biggest;			

//		if ( !le->state ) 
	{
		// search an owner
		actor = LE_Find( ET_ACTOR, le->pos );
		if ( actor ) actor->i.c[csi.idFloor] = le->i.c[csi.idFloor];
	}
	if ( le->i.c[csi.idFloor] ) 
	{
		biggest = CL_BiggestItem( le->i.c[csi.idFloor] );
		le->model1 = cl.model_weapons[biggest];
		Grid_PosToVec( &clMap, le->pos, le->origin );
		VectorSubtract( le->origin, csi.ods[biggest].center, le->origin );
		le->origin[2] -= 28;
		le->angles[ROLL] = 90;
//		le->angles[YAW] = 10*(int)(le->origin[0] + le->origin[1] + le->origin[2]) % 360;
	}
}


/*
=====================
CL_InvAdd
=====================
*/
void CL_InvAdd( sizebuf_t *sb )
{
	item_t	item;
	le_t	*le;
	int		number;
	byte	container, x, y;

	number = MSG_ReadShort( sb );
	le = LE_Get( number );
	if ( !le ) 
	{
		Com_Printf( _("InvAdd message ignored... LE not found\n") );
		return;
	}

	for ( item.t = MSG_ReadByte( sb ); item.t != NONE; item.t = MSG_ReadByte( sb ) )
	{
		item.a = MSG_ReadByte( sb );
		item.m = MSG_ReadByte( sb );
		container = MSG_ReadByte( sb );
		x = MSG_ReadByte( sb );
		y = MSG_ReadByte( sb );
		Com_AddToInventory( &le->i, item, container, x, y );
		if ( container == csi.idRight ) le->right = item.t;
		else if ( container == csi.idLeft ) le->left = item.t;
	}

	switch ( le->type )
	{
	case ET_ACTOR:
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
	if ( !le ) 
	{
		Com_Printf( _("InvDel message ignored... LE not found\n") );
		return;
	}

	Com_RemoveFromInventory( &le->i, container, x, y );
	if ( container == csi.idRight ) le->right = NONE;
	else if ( container == csi.idLeft ) le->left = NONE;

	switch ( le->type )
	{
	case ET_ACTOR:
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
	int		ammo, container, x, y;
	int		*pAmmo;

	MSG_ReadFormat( sb, ev_format[EV_INV_AMMO], 
		&number, &ammo, &container, &x, &y );			

	le = LE_Get( number );
	if ( !le ) 
	{
		Com_Printf( _("InvAmmo message ignored... LE not found\n") );
		return;
	}

	if ( le->team != cls.team ) 
		return;

	ic = Com_SearchInInventory( &le->i, container, x, y );
	if ( !ic ) return;

	pAmmo = &ic->item.a;

	if ( curCampaign && le->team == cls.team && *pAmmo == csi.ods[ic->item.t].ammo )
	{
		// started to use up a magazine
		if ( ic->item.m != NONE && ccs.eMission.num[ic->item.m] )
			ccs.eMission.num[ic->item.m]--;
	}

	// set new ammo
	*pAmmo = ammo;

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
	evTimes_t	*et, *last, *cur;
	int			oldCount, eType;
	int			time;
	int			next;
	qboolean	now;

	while ( ( eType = MSG_ReadByte( &net_message ) ) ) 
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error (ERR_DROP,_("CL_ParseEvent: Bad event message"));
			break;
		}

		// check instantly flag
		if ( eType & INSTANTLY )
		{
			now = true;
			eType &= ~INSTANTLY;
		}
		else now = false;

		// check if eType is valid
		if ( eType < 0 || eType >= EV_NUM_EVENTS )
			Com_Error( ERR_DROP, _("CL_ParseEvent: invalid event %i\n"), eType );

		if ( !ev_func[eType] )
			Com_Error( ERR_DROP, _("CL_ParseEvent: no handling function for event %i\n"), eType );

		if ( now )
		{
			// check if eType is valid
			if ( eType < 0 || eType >= EV_NUM_EVENTS )
				Com_Error( ERR_DROP, _("CL_Events: invalid event %i\n"), eType );

			// log and call function
			CL_LogEvent( eType );
			next = net_message.readcount + MSG_LengthFormat( &net_message, ev_format[eType] );
			ev_func[eType]( &net_message );
			net_message.readcount = next;
		}
		else
		{
			int		length;

			// store data
			length = MSG_LengthFormat( &net_message, ev_format[eType] );

			if ( evWp-evBuf + length+2 >= EV_STORAGE_SIZE )
				evWp = evBuf;

			// get event time
			if ( nextTime < cl.eventTime ) nextTime = cl.eventTime;

			if ( eType == EV_ACTOR_DIE || eType == EV_MODEL_EXPLODE ) time = impactTime;
			else if ( eType == EV_ACTOR_SHOOT ) time = shootTime;
			else time = nextTime;

			// store old readcount
			oldCount = net_message.readcount;

			// calculate next and shoot time
			switch ( eType )
			{
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
				int flags;
				flags = MSG_ReadByte( &net_message );
				if ( !flags ) 
				{
					fireDef_t *fd;
					fd = GET_FIREDEF( MSG_ReadByte( &net_message ) );
					if ( fd->rof ) nextTime += 1000 / fd->rof;
				}
				else nextTime += 500;
				shootTime = nextTime;
				break;
				}
			case EV_ACTOR_SHOOT:
				{
					fireDef_t	*fd;
					int		type, flags;
					vec3_t	muzzle, impact;

					// read data
					MSG_ReadShort( &net_message );
					type = MSG_ReadByte( &net_message );
					flags = MSG_ReadByte( &net_message );
					MSG_ReadPos( &net_message, muzzle );
					MSG_ReadPos( &net_message, impact );

					fd = GET_FIREDEF( type );
					if ( !(flags & SF_BOUNCED) )
					{
						// shooting
						if ( fd->speed ) impactTime = shootTime + 1000 * VectorDist( muzzle, impact ) / fd->speed;
						else impactTime = shootTime;
						if ( cl.actTeam != cls.team ) nextTime = shootTime + 1400;
						else nextTime = shootTime + 400;
						if ( fd->rof ) shootTime += 1000 / fd->rof;
					} else {
						// only a bounced shot
						time = impactTime;
						if ( fd->speed ) impactTime += 1000 * VectorDist( muzzle, impact ) / fd->speed;
					}
				}
				break;
			case EV_ACTOR_THROW:
				nextTime += MSG_ReadShort( &net_message );
				shootTime = impactTime = nextTime;
				break;
			}

			// add to timetable
			last = NULL;
			for ( et = etCurrent; et; et = et->next )
			{
				if ( et->start > time ) break;
				last = et;
			}

			if ( !etUnused ) Com_Error( ERR_DROP, _("CL_ParseEvent: timetable overflow\n") );
			cur = etUnused;
			etUnused = cur->next;

			cur->start = time;
			cur->pos = evWp - evBuf;

			if ( last ) last->next = cur;
			else etCurrent = cur;
			cur->next = et;

			// copy data
			*evWp++ = eType;
			memcpy( evWp, net_message.data + oldCount, length );
			evWp += length;
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

	while ( !blockEvents && etCurrent && cl.eventTime >= etCurrent->start )
	{
		// get event type
		evStorage.readcount = etCurrent->pos;
		eType = MSG_ReadByte( &evStorage );

#if 0
		// check if eType is valid
		if ( eType < 0 || eType >= EV_NUM_EVENTS )
			Com_Error( ERR_DROP, _("CL_Events: invalid event %i\n"), eType );
#endif

		// free timetable entry
		last = etCurrent;
		etCurrent = etCurrent->next;
		last->next = etUnused;
		etUnused = last;

		// call function
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

	//
	// if recording demos, copy the message out
	//
	if (cl_shownet->value == 1)
		Com_Printf ("%i ",net_message.cursize);
	else if (cl_shownet->value >= 2)
		Com_Printf ("------------------\n");


	//
	// parse the message
	//
	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error (ERR_DROP,_("CL_ParseServerMessage: Bad server message"));
			break;
		}

		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cl_shownet->value>=2)
		{
			if (!svc_strings[cmd])
				Com_Printf ("%3i:BAD CMD %i\n", net_message.readcount-1,cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}
	
	// other commands
		switch (cmd)
		{
		default:
			Com_Error (ERR_DROP,_("CL_ParseServerMessage: Illegible server message\n"));
			break;
			
		case svc_nop:
//			Com_Printf ("svc_nop\n");
			break;
			
		case svc_disconnect:
			Com_Error (ERR_DISCONNECT,_("Server disconnected\n"));
			break;

		case svc_reconnect:
			Com_Printf (_("Server disconnected, reconnecting\n"));
			if (cls.download) {
				//ZOID, close download
				fclose (cls.download);
				cls.download = NULL;
			}
			cls.state = ca_connecting;
			cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
			break;

		case svc_print:
			i = MSG_ReadByte (&net_message);
			if (i == PRINT_CHAT)
			{
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
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CL_ParseServerData ();
			break;
			
		case svc_configstring:
			CL_ParseConfigString ();
			break;
			
		case svc_sound:
			CL_ParseStartSoundPacket();
			break;
			
		case svc_download:
			CL_ParseDownload ();
			break;

		case svc_layout:
			s = MSG_ReadString (&net_message);
			strncpy (cl.layout, s, sizeof(cl.layout)-1);
			break;

		case svc_event:
			CL_ParseEvent ();
			break;
		}
	}

	CL_AddNetgraph ();
}


