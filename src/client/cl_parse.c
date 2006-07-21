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

char *svc_strings[256] = {
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
/* *	| read from va	| */
/* &	| read bytes 	| 1 */
/* n	| pascal string type - SIZE+DATA | 2 + sizeof(DATA) */
/*	| until NONE	| */
char *ev_format[] = {
	"",							/* EV_NULL */
	"bb",						/* EV_RESET */
	"",							/* EV_START */
	"b",						/* EV_ENDROUND */

	/* TODO: data for '&' MUST NOT have values == NONE (0xFF). */
	"bb&",						/* EV_RESULTS */
	"g",						/* EV_CENTERVIEW */

	"!sbg",						/* EV_ENT_APPEAR */
	"!s",						/* EV_ENT_PERISH */

	"!sbbgbbbssbsb",			/* EV_ACTOR_APPEAR */
	"!s",						/* EV_ACTOR_START_MOVE */
	"!sb",						/* EV_ACTOR_TURN */
	"!s*",						/* EV_ACTOR_MOVE */
	"!sbgg",					/* EV_ACTOR_START_SHOOT */
	"!sbbppb",					/* EV_ACTOR_SHOOT */
	"bb",						/* EV_ACTOR_SHOOT_HIDDEN */
	"sbbpp",					/* EV_ACTOR_THROW */
	"!ss",						/* EV_ACTOR_DIE */
	"!sbbbb",					/* EV_ACTOR_STATS */
	"!ss",						/* EV_ACTOR_STATECHANGE */

	"n",						/* EV_INV_ADD */
	"sbbb",						/* EV_INV_DEL */
	"sbbbb",					/* EV_INV_AMMO */

	"s",						/* EV_MODEL_PERISH */
	"s"							/* EV_MODEL_EXPLODE */
};

char *ev_names[] = {
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

void CL_Reset(sizebuf_t * sb);
void CL_StartGame(sizebuf_t * sb);
void CL_CenterView(sizebuf_t * sb);
void CL_EntAppear(sizebuf_t * sb);
void CL_EntPerish(sizebuf_t * sb);
void CL_ActorDoStartMove(sizebuf_t * sb);
void CL_ActorAppear(sizebuf_t * sb);
void CL_ActorStats(sizebuf_t * sb);
void CL_ActorStateChange(sizebuf_t * sb);
void CL_ActorShootHidden(sizebuf_t * sb);
void CL_InvAdd(sizebuf_t * sb);
void CL_InvDel(sizebuf_t * sb);
void CL_InvAmmo(sizebuf_t * sb);

void (*ev_func[]) (sizebuf_t * sb) = {
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
		CL_ActorShootHidden, CL_ActorDoThrow, CL_ActorDie, CL_ActorStats, CL_ActorStateChange, CL_InvAdd, CL_InvDel, CL_InvAmmo, LM_Perish, LM_Explode};

#define EV_STORAGE_SIZE		32768
#define EV_TIMES			4096

sizebuf_t evStorage;
byte evBuf[EV_STORAGE_SIZE];
byte *evWp;

typedef struct evTimes_s {
	int start;
	int pos;
	struct evTimes_s *next;
} evTimes_t;

evTimes_t evTimes[EV_TIMES];
evTimes_t *etUnused, *etCurrent;

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
void CL_RegisterSounds(void)
{
	int i, j;

	S_BeginRegistration();

	/* load game sounds */
	for (i = 1; i < MAX_SOUNDS; i++) {
		if (!cl.configstrings[CS_SOUNDS + i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound(cl.configstrings[CS_SOUNDS + i]);
		/* pump message loop */
		Sys_SendKeyEvents();
	}
	/* load weapon sounds */
	for (i = 0; i < csi.numODs; i++)
		for (j = 0; j < 2; j++) {
			if (csi.ods[i].fd[j].fireSound[0])
				S_RegisterSound(csi.ods[i].fd[j].fireSound);
			if (csi.ods[i].fd[j].impactSound[0])
				S_RegisterSound(csi.ods[i].fd[j].impactSound);
			/* pump message loop */
			Sys_SendKeyEvents();
		}

	S_EndRegistration();
}

/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData

Written by SV_New_f in sv_user.c
==================
*/
void CL_ParseServerData(void)
{
	extern cvar_t *fs_gamedirvar;
	char *str;
	int i;

	Com_DPrintf("Serverdata packet received.\n");

	/* wipe the client_state_t struct */
	CL_ClearState();
	cls.state = ca_connected;

	/* parse protocol version number */
	i = MSG_ReadLong(&net_message, NULL);
	cls.serverProtocol = i;

	/* compare versions */
	if (i != PROTOCOL_VERSION)
		Com_Error(ERR_DROP, "Server returned version %i, not %i", i, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong(&net_message, NULL);
	cl.attractloop = MSG_ReadByte(&net_message, NULL);

	/* game directory */
	str = MSG_ReadString(&net_message, NULL);
	Q_strncpyz(cl.gamedir, str, MAX_QPATH);

	/* set gamedir */
	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str)))
		|| (!*str && (fs_gamedirvar->string || *fs_gamedirvar->string)))
		Cvar_Set("game", str);

	/* parse player entity number */
	cl.pnum = MSG_ReadShort(&net_message, NULL);

	/* get the full level name */
	str = MSG_ReadString(&net_message, NULL);

	if (cl.pnum >= 0) {
		/* seperate the printfs so the server message can have a color */
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf("%c%s\n", 2, str);
		/* need to prep refresh at next oportunity */
		cl.refresh_prepped = qfalse;
	}
}

/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString(void)
{
	int16_t i;
	char *s;

	/* which configstring? */
	i = MSG_ReadShort(&net_message, NULL);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error(ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	/* value */
	s = MSG_ReadString(&net_message, NULL);

	/* there may be overflows in i==CS_TILES - but thats ok */
	/* see definition of configstrings and MAX_TILESTRINGS */
	strcpy(cl.configstrings[i], s);

	/* do something apropriate */
	if (i >= CS_LIGHTS && i < CS_LIGHTS + MAX_LIGHTSTYLES)
		CL_SetLightstyle(i - CS_LIGHTS);
	else if (i == CS_CDTRACK) {
		if (cl.refresh_prepped)
			CDAudio_Play(atoi(cl.configstrings[CS_CDTRACK]), qtrue);
	} else if (i >= CS_MODELS && i < CS_MODELS + MAX_MODELS) {
		if (cl.refresh_prepped) {
			cl.model_draw[i - CS_MODELS] = re.RegisterModel(cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i - CS_MODELS] = CM_InlineModel(cl.configstrings[i]);
			else
				cl.model_clip[i - CS_MODELS] = NULL;
		}
	} else if (i >= CS_SOUNDS && i < CS_SOUNDS + MAX_MODELS) {
		if (cl.refresh_prepped)
			cl.sound_precache[i - CS_SOUNDS] = S_RegisterSound(cl.configstrings[i]);
	} else if (i >= CS_IMAGES && i < CS_IMAGES + MAX_MODELS) {
		if (cl.refresh_prepped)
			cl.image_precache[i - CS_IMAGES] = re.RegisterPic(cl.configstrings[i]);
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
	vec3_t pos_v;
	float *pos;
	int ent;
	int16_t channel;
	float volume, attenuation, ofs;
	uint8_t flags, sound_num;

	flags = MSG_ReadByte(&net_message, NULL);
	sound_num = MSG_ReadByte(&net_message, NULL);

	if (flags & SND_VOLUME)
		volume = MSG_ReadByte(&net_message, NULL) / 255.0f;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte(&net_message, NULL) / 64.0f;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	if (flags & SND_OFFSET)
		ofs = MSG_ReadByte(&net_message, NULL) / 1000.0f;
	else
		ofs = 0.0f;

	/* entity reletive */
	if (flags & SND_ENT) {
		channel = MSG_ReadShort(&net_message, NULL);
		ent = channel >> 3;
		if (ent > MAX_EDICTS)
			Com_Error(ERR_DROP, "CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	} else {
		ent = 0;
		channel = 0;
	}

	/* positioned in space */
	if (flags & SND_POS) {
		MSG_ReadPos(&net_message, NULL, pos_v);

		pos = pos_v;
	} else						/* use entity number */
		pos = NULL;

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound(pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, ofs);
}


/*
=====================
CL_Reset
=====================
*/
void CL_Reset(sizebuf_t * sb)
{
	evTimes_t *last, *et;
	int i;

	/* clear local entities */
	numLEs = 0;
	selActor = NULL;
	cl.numTeamList = 0;
	Cbuf_AddText("numonteam1\n");

	/* reset events */
	for (i = 0, et = evTimes; i < EV_TIMES - 1; i++) {
		last = et++;
		et->next = last;
	}
	etUnused = et;
	etCurrent = NULL;
	cl.eventTime = 0;
	nextTime = 0;
	shootTime = 0;
	impactTime = 0;
	blockEvents = qfalse;

	/* set the active player */
	cls.team = MSG_ReadByte(sb, NULL);
	cl.actTeam = MSG_ReadByte(sb, NULL);
	Com_Printf("(player %i) It's team %i's round\n", cl.pnum, cl.actTeam);
}


/*
=====================
CL_StartGame
=====================
*/
void CL_StartGame(sizebuf_t * sb)
{
	/* init camera position and angles */
	memset(&cl.cam, 0, sizeof(camera_t));
	VectorSet(cl.cam.angles, 60.0, 60.0, 0.0);
	VectorSet(cl.cam.omega, 0.0, 0.0, 0.0);
	cl.cam.zoom = 1.0;
	camera_mode = CAMERA_MODE_REMOTE;

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

	/* activate hud */
	MN_PushMenu(mn_hud->string);
	Cvar_Set("mn_active", mn_hud->string);
}


/*
=====================
CL_CenterView
=====================
*/
void CL_CenterView(sizebuf_t * sb)
{
	pos3_t pos;

	MSG_ReadGPos(sb, NULL, pos);
	V_CenterView(pos);
}


/*
=====================
CL_EntAppear
=====================
*/
void CL_EntAppear(sizebuf_t * sb)
{
	le_t *le;
	int16_t entnum;

	/* check if the ent is already visible */
	entnum = MSG_ReadShort(sb, NULL);
	le = LE_Get(entnum);

	if (!le)
		le = LE_Add(entnum);
	else
		Com_Printf("Entity appearing already visible... overwriting the old one\n");

	le->type = MSG_ReadByte(sb, NULL);
	MSG_ReadGPos(sb, NULL, le->pos);
	Grid_PosToVec(&clMap, le->pos, le->origin);
}


/**
 * @brief Called whenever an entity dies
 */
void CL_EntPerish(sizebuf_t * sb)
{
	le_t *le;

	le = LE_Get(MSG_ReadShort(sb, NULL));

	if (!le) {
		Com_Printf("Delete request ignored... LE not found\n");
		return;
	}

	/* count spotted aliens */
	if ((le->type == ET_ACTOR || le->type == ET_UGV) && !(le->state & STATE_DEAD) && le->team != cls.team && le->team != TEAM_CIVILIAN)
		cl.numAliensSpotted--;

	if (le->type == ET_ACTOR)
		le->i.c[csi.idFloor] = NULL;
	Com_DestroyInventory(&le->i);

	if (le->type == ET_ITEM) {
		le_t *actor;

		actor = LE_Find(ET_ACTOR, le->pos);
		if (actor)
			actor->i.c[csi.idFloor] = NULL;
	}

	if (le->type == ET_ACTOR || le->type == ET_UGV)
		CL_RemoveActorFromTeamList(le);

	le->inuse = qfalse;

/*	le->startTime = cl.time; */
/*	le->think = LET_Perish; */
}


/*
=====================
CL_ActorDoStartMove
=====================
*/
le_t *lastMoving;

void CL_ActorDoStartMove(sizebuf_t * sb)
{
	lastMoving = LE_Get(MSG_ReadShort(sb, NULL));
}


/*
=====================
CL_ActorAppear
=====================
*/
void CL_ActorAppear(sizebuf_t * sb)
{
	qboolean newActor;
	le_t *le;
	int16_t entnum, modelnum1, modelnum2;

	/* check if the actor is already visible */
	entnum = MSG_ReadShort(sb, NULL);
	le = LE_Get(entnum);

	if (!le) {
		le = LE_Add(entnum);
		newActor = qtrue;
	} else {
/*		Com_Printf( "Actor appearing already visible... overwriting the old one\n" ); */
		newActor = qfalse;
	}

	/* get the info */
	MSG_ReadFormat(sb, NULL, ev_format[EV_ACTOR_APPEAR],
				   &le->team, &le->pnum, &le->pos, &le->dir, &le->right, &le->left, &modelnum1, &modelnum2, &le->skinnum, &le->state, &le->fieldSize);

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
	VectorCopy(player_maxs, le->maxs);

	le->think = LET_StartIdle;

	/* count spotted aliens */
	if (!(le->state & STATE_DEAD) && newActor && le->team != cls.team && le->team != TEAM_CIVILIAN)
		cl.numAliensSpotted++;

	if (cls.state == ca_active && !(le->state & STATE_DEAD)) {
		/* center view (if wanted) */
		if ((int) cl_centerview->value > 1 || ((int) cl_centerview->value == 1 && cl.actTeam != cls.team)) {
			VectorCopy(le->origin, cl.cam.reforg);
			Cvar_SetValue("cl_worldlevel", le->pos[2]);
		}

		/* draw line of sight */
		if (le->team != cls.team) {
			if (cl.actTeam == cls.team && lastMoving) {
				ptl_t *ptl;

				ptl = CL_ParticleSpawn("fadeTracer", 0, lastMoving->origin, le->origin, NULL);
				if (le->team == TEAM_CIVILIAN)
					VectorSet(ptl->color, 0.2, 0.2, 1);
			}

			/* message */
			if (le->team != TEAM_CIVILIAN) {
				if (curCampaign)
					CL_DisplayHudMessage(_("Alien spotted!\n"), 2000);
				else
					CL_DisplayHudMessage(_("Enemy spotted!\n"), 2000);
			} else
				CL_DisplayHudMessage(_("Civilian spotted!\n"), 2000);
		}
	}

	/* add team members to the actor list */
	CL_AddActorToTeamList(le);

/*	Com_Printf( "Player at (%i %i %i) -> (%f %f %f)\n", */
/*		le->pos[0], le->pos[1], le->pos[2], le->origin[0], le->origin[1], le->origin[2] ); */
}


/*
=====================
CL_ActorStats
=====================
*/
void CL_ActorStats(sizebuf_t * sb)
{
	le_t *le;
	int16_t number;

	number = MSG_ReadShort(sb, NULL);
	le = LE_Get(number);

	if (!le) {
		Com_Printf("Stats message ignored... LE not found\n");
		return;
	}

	le->TU = MSG_ReadByte(sb, NULL);
	le->HP = MSG_ReadByte(sb, NULL);
	le->STUN = MSG_ReadByte(sb, NULL);
	le->morale = MSG_ReadByte(sb, NULL);
	if (le->TU > le->maxTU)
		le->maxTU = le->TU;
	if (le->HP > le->maxHP)
		le->maxHP = le->HP;
	if (le->morale > le->maxMorale)
		le->maxMorale = le->morale;
}


/*
=====================
CL_ActorStateChange
=====================
*/
void CL_ActorStateChange(sizebuf_t * sb)
{
	le_t *le;
	int16_t number;

	number = MSG_ReadShort(sb, NULL);
	le = LE_Get(number);

	if (!le) {
		Com_Printf("StateChange message ignored... LE not found\n");
		return;
	}

	le->state = MSG_ReadShort(sb, NULL);
	le->think = LET_StartIdle;
}


/*
=====================
CL_ActorShootHidden
=====================
*/
void CL_ActorShootHidden(sizebuf_t * sb)
{
	fireDef_t *fd;
	bool_t first;
	uint8_t type;

	first = MSG_ReadByte(sb, NULL);
	type = MSG_ReadByte(sb, NULL);

	/* get the fire def */
	fd = GET_FIREDEF(type);

	/* start the sound */
	if (fd->fireSound[0] && ((first && fd->soundOnce) || (!first && !fd->soundOnce)))
		S_StartLocalSound(fd->fireSound);
}


/*
=====================
CL_BiggestItem
=====================
*/
int CL_BiggestItem(invList_t * ic)
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
 */
void CL_PlaceItem(le_t * le)
{
	le_t *actor;
	int biggest;

/*		if ( !le->state ) */
	{
		/* search an owner */
		actor = LE_Find(ET_ACTOR, le->pos);
		if (actor) {

#if PARANOID
			Com_Printf("CL_PlaceItem: shared container: '%p'\n", le->i.c[csi.idFloor]);
#endif

			actor->i.c[csi.idFloor] = le->i.c[csi.idFloor];
		}
	}
	if (le->i.c[csi.idFloor]) {
		biggest = CL_BiggestItem(le->i.c[csi.idFloor]);
		le->model1 = cl.model_weapons[biggest];
		Grid_PosToVec(&clMap, le->pos, le->origin);
		VectorSubtract(le->origin, csi.ods[biggest].center, le->origin);
		le->origin[2] -= 28;
		le->angles[ROLL] = 90;
/*		le->angles[YAW] = 10*(int)(le->origin[0] + le->origin[1] + le->origin[2]) % 360; */
	}
}


/*
=====================
CL_InvAdd
=====================
*/
void CL_InvAdd(sizebuf_t * sb)
{
	item_t item;
	le_t *le;
	int16_t number, size;
	uint8_t container, x, y;

	size = MSG_ReadShort(sb, NULL);
	number = MSG_ReadShort(sb, NULL);

	le = LE_Get(number);
	if (!le) {
		Com_Printf("InvAdd message ignored... LE not found\n");
		return;
	}

	for (size -= 2; size > 0; size -= 6) {
		item.t = MSG_ReadByte(sb, NULL);
		item.a = MSG_ReadByte(sb, NULL);
		item.m = MSG_ReadByte(sb, NULL);
		container = MSG_ReadByte(sb, NULL);
		x = MSG_ReadByte(sb, NULL);
		y = MSG_ReadByte(sb, NULL);
		Com_AddToInventory(&le->i, item, container, x, y);
		if (container == csi.idRight)
			le->right = item.t;
		else if (container == csi.idLeft)
			le->left = item.t;
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


/*
=====================
CL_InvDel
=====================
*/
void CL_InvDel(sizebuf_t * sb)
{
	le_t *le;
	int16_t number;
	int8_t container, x, y;

	MSG_ReadFormat(sb, NULL, ev_format[EV_INV_DEL], &number, &container, &x, &y);

	le = LE_Get(number);
	if (!le) {
		Com_Printf("InvDel message ignored... LE not found\n");
		return;
	}

	Com_RemoveFromInventory(&le->i, container, x, y);
	if (container == csi.idRight)
		le->right = NONE;
	else if (container == csi.idLeft)
		le->left = NONE;

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


/*
=====================
CL_InvAmmo
=====================
*/
void CL_InvAmmo(sizebuf_t * sb)
{
	invList_t *ic;
	le_t *le;
	int16_t number;
	int8_t ammo, container, x, y;

	MSG_ReadFormat(sb, NULL, ev_format[EV_INV_AMMO], &number, &ammo, &container, &x, &y);

	le = LE_Get(number);
	if (!le) {
		Com_Printf("InvAmmo message ignored... LE not found\n");
		return;
	}

	if (le->team != cls.team)
		return;

	ic = Com_SearchInInventory(&le->i, container, x, y);
	if (!ic)
		return;

	/* if we're reloading and the displaced clip had any remaining */
	/* bullets, store them as loose */
	if (curCampaign && le->team == cls.team && ammo == csi.ods[ic->item.t].ammo && ic->item.a > 0) {
		ccs.eMission.num_loose[ic->item.m] += ic->item.a;
		/* Accumulate loose ammo into clips (only accessable post-mission) */
		if (ccs.eMission.num_loose[ic->item.m] >= csi.ods[ic->item.t].ammo) {
			ccs.eMission.num_loose[ic->item.m] -= csi.ods[ic->item.t].ammo;
			ccs.eMission.num[ic->item.m]++;
		}
	}

	/* set new ammo */
	ic->item.a = ammo;

	if (ic && csi.ods[ic->item.t].ammo == ammo && le->team != TEAM_ALIEN)
		S_StartLocalSound("weapons/verschluss.wav");
}


/*
=====================
CL_LogEvent
=====================
*/
void CL_LogEvent(int num)
{
	FILE *logfile;

	if (!cl_logevents->value)
		return;

	logfile = fopen(va("%s/events.log", FS_Gamedir()), "a");
	fprintf(logfile, "%10i %s\n", cl.eventTime, ev_names[num]);
	fclose(logfile);
}


/*
=====================
CL_ParseEvent
=====================
*/
void CL_ParseEvent(void)
{
	evTimes_t *et, *last, *cur;
	bool_t now;
	int16_t length;
	int time, oldCount;
	uint8_t eType;

	while ((eType = MSG_ReadByte(&net_message, NULL)) != 0) {
		if (net_message.readcount > net_message.cursize) {
			Com_Error(ERR_DROP, "CL_ParseEvent: Bad event message");
			break;
		}

		/* check instantly flag */
		if (eType & INSTANTLY) {
			now = qtrue;
			eType &= ~INSTANTLY;
		} else
			now = qfalse;

		/* check if eType is valid */
		if (eType >= EV_NUM_EVENTS)
			Com_Error(ERR_DROP, "CL_ParseEvent: invalid event %i\n", eType);

		if (!ev_func[eType])
			Com_Error(ERR_DROP, "CL_ParseEvent: no handling function for event %i\n", eType);

		oldCount = net_message.readcount;
		/* FIXME: MSG_LengthFormat will return an int, length is a short */
		length = (ev_format[eType][0] == 'n') ? MSG_ReadShort(&net_message, NULL) + 2 : MSG_LengthFormat(&net_message, NULL, ev_format[eType]);

		if (now) {
			/* log and call function */
			CL_LogEvent(eType);
			ev_func[eType] (&net_message);
		} else {
			if (evWp - evBuf + length + 2 >= EV_STORAGE_SIZE)
				evWp = evBuf;

			/* get event time */
			if (nextTime < cl.eventTime)
				nextTime = cl.eventTime;

			if (eType == EV_ACTOR_DIE || eType == EV_MODEL_EXPLODE)
				time = impactTime;
			else if (eType == EV_ACTOR_SHOOT)
				time = shootTime;
			else
				time = nextTime;

			/* calculate next and shoot time */
			switch (eType) {
			case EV_ACTOR_APPEAR:
				if (cls.state == ca_active && cl.actTeam != cls.team)
					nextTime += 600;
				break;
			case EV_ACTOR_START_SHOOT:
				nextTime += 300;
				shootTime = nextTime;
				break;
			case EV_ACTOR_SHOOT_HIDDEN:
				{
					uint8_t flags;

					flags = MSG_ReadByte(&net_message, NULL);
					if (!flags) {
						fireDef_t *fd;

						fd = GET_FIREDEF(MSG_ReadByte(&net_message, NULL));
						if (fd->rof)
							nextTime += 1000 / fd->rof;
					} else
						nextTime += 500;
					shootTime = nextTime;
					break;
				}
			case EV_ACTOR_SHOOT:
				{
					fireDef_t *fd;
					uint8_t type, flags;
					vec3_t muzzle, impact;

					/* read data */
					MSG_ReadShort(&net_message, NULL);
					type = MSG_ReadByte(&net_message, NULL);
					flags = MSG_ReadByte(&net_message, NULL);
					MSG_ReadPos(&net_message, NULL, muzzle);
					MSG_ReadPos(&net_message, NULL, impact);

					fd = GET_FIREDEF(type);
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
						time = impactTime;
						if (fd->speed)
							impactTime += 1000 * VectorDist(muzzle, impact) / fd->speed;
					}
				}
				break;
			case EV_ACTOR_THROW:
				nextTime += MSG_ReadShort(&net_message, NULL);
				shootTime = impactTime = nextTime;
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
				Com_Error(ERR_DROP, "CL_ParseEvent: timetable overflow\n");
			cur = etUnused;
			etUnused = cur->next;

			cur->start = time;
			cur->pos = evWp - evBuf;

			if (last)
				last->next = cur;
			else
				etCurrent = cur;
			cur->next = et;

			/* copy data */
			*evWp++ = eType;
			memcpy(evWp, net_message.data + oldCount, length);
			evWp += length;
		}

		net_message.readcount = oldCount + length;
	}
}


/*
=====================
CL_Events
=====================
*/
void CL_Events(void)
{
	evTimes_t *last;
	uint8_t eType;

	if (cls.state < ca_connected)
		return;

	while (!blockEvents && etCurrent && cl.eventTime >= etCurrent->start) {
		/* get event type */
		evStorage.readcount = etCurrent->pos;
		eType = MSG_ReadByte(&evStorage, NULL);

#if PARANOID
		/* check if eType is valid */
		if (eType >= EV_NUM_EVENTS)
			Com_Error(ERR_DROP, "CL_Events: invalid event %i\n", eType);
#endif

		/* free timetable entry */
		last = etCurrent;
		etCurrent = etCurrent->next;
		last->next = etUnused;
		etUnused = last;

		/* call function */
		CL_LogEvent(eType);
		ev_func[eType] (&evStorage);
	}
}


/*
=====================
CL_InitEvents
=====================
*/
void CL_InitEvents(void)
{
	SZ_Init(&evStorage, evBuf, EV_STORAGE_SIZE);
	evStorage.cursize = EV_STORAGE_SIZE;
	evWp = evBuf;
}


void SHOWNET(char *s)
{
	if (cl_shownet->value >= 2)
		Com_Printf("%3i:%s\n", net_message.readcount - 1, s);
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage(void)
{
	int cmd;					/* can be -1 */
	uint8_t i, end = 0;
	char *s;

	/* if recording demos, copy the message out */
	if (cl_shownet->value == 1)
		Com_Printf("%i ", net_message.cursize);
	else if (cl_shownet->value >= 2)
		Com_Printf("------------------\n");

	/* parse the message */
	while (1) {
		if (net_message.readcount > net_message.cursize) {
			Com_Error(ERR_DROP, "CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte(&net_message, &end);

		if (end == 1) {
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cl_shownet->value >= 2) {
			if (!svc_strings[cmd])
				Com_Printf("%3i:BAD CMD %i\n", net_message.readcount - 1, cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}

		/* other commands */
		switch (cmd) {
		case svc_nop:
/*			Com_Printf ("svc_nop\n"); */
			break;

		case svc_disconnect:
			Com_Error(ERR_DISCONNECT, "Server disconnected\n");
			break;

		case svc_reconnect:
			Com_Printf("Server disconnected, reconnecting\n");
			cls.state = ca_connecting;
			cls.connect_time = -99999;	/* CL_CheckForResend() will fire immediately */
			break;

		case svc_print:
			i = MSG_ReadByte(&net_message, NULL);
			if (i == PRINT_CHAT) {
				S_StartLocalSound("misc/talk.wav");
				con.ormask = 128;
			}
			Com_Printf("%s", MSG_ReadString(&net_message, NULL));
			con.ormask = 0;
			break;

		case svc_centerprint:

			SCR_CenterPrint(MSG_ReadString(&net_message, NULL));
			break;

		case svc_stufftext:
			s = MSG_ReadString(&net_message, NULL);
			Com_DPrintf("stufftext: %s\n", s);
			Cbuf_AddText(s);
			break;

		case svc_serverdata:
			Cbuf_Execute();		/* make sure any stuffed commands are done */
			CL_ParseServerData();
			break;

		case svc_configstring:
			CL_ParseConfigString();
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			break;

		case svc_layout:
			s = MSG_ReadString(&net_message, NULL);
			Q_strncpyz(cl.layout, s, sizeof(cl.layout));
			break;

		case svc_event:
			CL_ParseEvent();
			break;

		default:
			Com_Error(ERR_DROP, "CL_ParseServerMessage: Illegible server message %d\n", cmd);
			break;
		}
	}

	CL_AddNetgraph();
}
