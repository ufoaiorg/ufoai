/**
 * @file snd_ref.c
 * @brief Main control for any streaming sound output device.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/snd_dma.c
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

#ifdef _WIN32
#define dlladdr GetProcAddress
#include <windows.h>
#include "../ports/win32/win_local.h"
#else
#define dlladdr dlsym
#include <dlfcn.h>
#endif

static void S_Play_f(void);
static void S_SoundList_f(void);
void S_Update_(void);
void S_StopAllSounds(void);

channel_t* s_streamingChannel;

/*
=======================================================================
Internal sound data & structures
=======================================================================
*/

/* only begin attenuating sound volumes when outside the FULLVOLUME range */
#define		SOUND_FULLVOLUME	80

#define		SOUND_LOOPATTENUATE	0.003

static int s_registration_sequence;

channel_t channels[MAX_CHANNELS];

static int sound_started = 0;

dma_t dma;

static vec3_t listener_origin;
static vec3_t listener_forward;
static vec3_t listener_right;
static vec3_t listener_up;

static qboolean s_registering;

static int soundtime;					/* sample PAIRS */
int paintedtime;				/* sample PAIRS */

/* during registration it is possible to have more sounds */
/* than could actually be referenced during gameplay, */
/* because we don't want to free anything until we are */
/* sure we won't need it. */
#define		MAX_SFX		(MAX_SOUNDS*2)
static sfx_t known_sfx[MAX_SFX];
static int num_sfx;

#define		MAX_PLAYSOUNDS	128
static playsound_t s_playsounds[MAX_PLAYSOUNDS];
static playsound_t s_freeplays;
playsound_t s_pendingplays;

static int s_beginofs;

cvar_t *snd_init;
cvar_t *snd_volume;
cvar_t *snd_testsound;
cvar_t *snd_loadas8bit;
cvar_t *snd_khz;
cvar_t *snd_show;
cvar_t *snd_mixahead;

cvar_t *snd_ref;
cvar_t *snd_openal;

static qboolean snd_ref_active;

int s_rawend;
portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

struct sndinfo si;
static void* snd_ref_lib = NULL;

/*
==========================================================
OGG Vorbis stuff
==========================================================
*/

music_t music;

/*
==========================================================
Sound renderer function pointers
==========================================================
*/

typedef qboolean (*SND_Init_t)(struct sndinfo* si);
typedef int (*SND_GetDMAPos_t)(void);
typedef void (*SND_Shutdown_t)(void);
typedef void (*SND_BeginPainting_t)(void);
typedef void (*SND_Submit_t)(void);
typedef void (*SND_Activate_t)(qboolean active);

SND_Init_t SND_Init;
SND_GetDMAPos_t SND_GetDMAPos;
SND_Shutdown_t SND_Shutdown;
SND_BeginPainting_t SND_BeginPainting;
SND_Submit_t SND_Submit;
SND_Activate_t SND_Activate;

/**
 * @brief Prints sound variables
 */
static void S_SoundInfo_f (void)
{
	if (!sound_started) {
		Com_Printf("sound system not started\n");
		return;
	}

	Com_Printf("%5d stereo\n", dma.channels - 1);
	Com_Printf("%5d samples\n", dma.samples);
	Com_Printf("%5d samplepos\n", dma.samplepos);
	Com_Printf("%5d samplebits\n", dma.samplebits);
	Com_Printf("%5d submission_chunk\n", dma.submission_chunk);
	Com_Printf("%5d speed\n", dma.speed);
	Com_Printf("0x%p dma buffer\n", dma.buffer);
	S_OGG_Info();
}

/**
 * @brief Lists all sound renderers for options menu - and cycles between them
 * @note Sound renderer differers from os to os
 */
void S_ModifySndRef_f (void)
{
	if (!Q_strcmp(snd_ref->string, "sdl")) {
#ifdef _WIN32
		Cvar_Set("snd_ref", "wapi");
	} else if (!Q_strcmp(snd_ref->string, "wapi")) {
		Cvar_Set("snd_ref", "dx");
	} else if (!Q_strcmp(snd_ref->string, "dx")) {
#else
		Cvar_Set("snd_ref", "alsa");
	} else if (!Q_strcmp(snd_ref->string, "alsa")) {
		Cvar_Set("snd_ref", "oss");
	} else if (!Q_strcmp(snd_ref->string, "oss")) {
		Cvar_Set("snd_ref", "arts");
	} else if (!Q_strcmp(snd_ref->string, "arts")) {
#endif
		Cvar_Set("snd_ref", "sdl");
	}
}

/**
 * @brief Cycles between khz values for sound renderer
 * @note initiate a sound renderer restart at change
 * @sa CL_Snd_Restart_f
 */
static void S_ModifyKhz_f (void)
{
	if (Cmd_Argc() < 2)
		return;

	if (*Cmd_Argv(1) == '+') {
		if (snd_khz->integer == 11)
			Cvar_SetValue("snd_khz", 22);
		else if (snd_khz->integer == 22)
			Cvar_SetValue("snd_khz", 44);
		else if (snd_khz->integer == 44)
			Cvar_SetValue("snd_khz", 48);
		else if (snd_khz->integer == 48)
			Cvar_SetValue("snd_khz", 11);
		else
			Cvar_SetValue("snd_khz", 48);
	} else if (*Cmd_Argv(1) == '-') {
		if (snd_khz->integer == 48)
			Cvar_SetValue("snd_khz", 44);
		else if (snd_khz->integer == 44)
			Cvar_SetValue("snd_khz", 22);
		else if (snd_khz->integer == 22)
			Cvar_SetValue("snd_khz", 11);
		else if (snd_khz->integer == 11)
			Cvar_SetValue("snd_khz", 48);
		else
			Cvar_SetValue("snd_khz", 48);
	}

	if (snd_khz->modified)
		Cbuf_AddText("snd_restart\n");
}

/**
 * @brief Make sure that all pointers are set to null
 * @note Call this in case of failed initialization - otherwise S_Activate
 * might produce a segfault
 */
static void S_FreeLibs (void)
{
	SND_Init = NULL;
	SND_Shutdown = NULL;
	SND_Submit = NULL;
	SND_GetDMAPos = NULL;
	SND_BeginPainting = NULL;
	SND_Activate = NULL;
	Sys_FreeLibrary(snd_ref_lib);

	memset(&si, 0, sizeof(struct sndinfo));
	snd_ref_lib = NULL;
	snd_ref_active = qfalse;
	memset(&dma, 0, sizeof(dma_t));
}

/** @brief sound renderer commands */
static const cmdList_t r_commands[] = {
	{"musiclist", S_OGG_List_f, "List all available music files"},
	{"snd_play", S_Play_f, NULL},
	{"snd_stop", S_StopAllSounds, "Stop all sounds"},
	{"snd_list", S_SoundList_f, "List of loaded sounds"},
	{"snd_info", S_SoundInfo_f, NULL},
	{"music_randomtrack", S_OGG_RandomTrack_f, "Play random music track"},
	{"music_play", S_OGG_Play, "Play an ogg sound track"},
	{"music_start", S_OGG_Start, "Start the ogg music track from cvar music"},
	{"music_stop", S_OGG_Stop, "Stop currently playing music tracks"},
	{"snd_modifykhz", S_ModifyKhz_f, "Modify khz for sound renderer - use + or - as paramaters"},

	{NULL, NULL, NULL}
};

/**
 * @brief
 * @sa S_Shutdown
 * @sa CL_Snd_Restart_f
 * @todo: openal should be an replacement for the 'normal' snd_ref
 */
void S_Init (void)
{
	const cmdList_t *commands;

	Com_Printf("\n------- sound initialization -------\n");

	snd_init = Cvar_Get("snd_init", "1", CVAR_ARCHIVE, "Should the sound renderer get initialized");

	if (!snd_init->value) {
		Com_Printf("not initializing.\n");
		return;
	}

	snd_volume = Cvar_Get("snd_volume", "0.7", CVAR_ARCHIVE, "Sound volume - default is 0.7");
	snd_khz = Cvar_Get("snd_khz", "48", CVAR_ARCHIVE, "Khz value for sound renderer - default is 48");
	snd_loadas8bit = Cvar_Get("snd_loadas8bit", "0", CVAR_ARCHIVE, NULL);
	snd_mixahead = Cvar_Get("snd_mixahead", "0.2", CVAR_ARCHIVE, NULL);
	snd_show = Cvar_Get("snd_show", "0", 0, NULL);
	snd_testsound = Cvar_Get("snd_testsound", "0", 0, NULL);

	S_OGG_Init();

	{
		char lib[MAX_OSPATH];

		/* @todo: make openal the default when it is working (i.e. change 0 below to a 1) */
		snd_openal = Cvar_Get("snd_openal", "0", CVAR_ARCHIVE, "use OpenAL");
		snd_ref = Cvar_Get("snd_ref", "sdl", CVAR_ARCHIVE, "Sound renderer libary name - default is sdl");
		/* don't restart right again */
		snd_ref->modified = qfalse;

		Com_Printf("Loading snd_%s sound driver\n", snd_ref->string);
		Com_sprintf(lib, sizeof(lib), "snd_%s", snd_ref->string);
		snd_ref_lib = Sys_LoadLibrary(lib, 0);
		if (snd_ref_lib == NULL) {
			Com_Printf("Load library (%s) failed - no sound available\n", lib);
			return;
		}

		if ((SND_Init = (SND_Init_t) Sys_GetProcAddress(snd_ref_lib, "SND_Init")) == 0)
			Com_Error(ERR_FATAL, "Sys_GetProcAddress failed loading SND_Init\n");
		if ((SND_Shutdown = (SND_Shutdown_t) Sys_GetProcAddress(snd_ref_lib, "SND_Shutdown")) == 0)
			Com_Error(ERR_FATAL, "Sys_GetProcAddress failed loading SND_Shutdown\n");
		if ((SND_GetDMAPos = (SND_GetDMAPos_t) Sys_GetProcAddress(snd_ref_lib, "SND_GetDMAPos")) == 0)
			Com_Error(ERR_FATAL, "Sys_GetProcAddress failed loading SND_GetDMAPos\n");
		if ((SND_BeginPainting = (SND_BeginPainting_t) Sys_GetProcAddress(snd_ref_lib, "SND_BeginPainting")) == 0)
			Com_Error(ERR_FATAL, "Sys_GetProcAddress failed loading SND_BeginPainting\n");
		if ((SND_Submit = (SND_Submit_t) Sys_GetProcAddress(snd_ref_lib, "SND_Submit")) == 0)
			Com_Error(ERR_FATAL, "Sys_GetProcAddress failed loading SND_Submit\n");
		if ((SND_Activate = (SND_Activate_t) Sys_GetProcAddress(snd_ref_lib, "SND_Activate")) == 0)
			Com_Error(ERR_FATAL, "Sys_GetProcAddress failed loading SND_Activate\n");

		snd_ref_active = qtrue;
	}

	si.dma = &dma;
	si.bits = Cvar_Get("snd_bits", "16", CVAR_ARCHIVE, "Sound bits");
	si.channels = Cvar_Get("snd_channels", "2", CVAR_ARCHIVE, "Sound channels");
	si.device = Cvar_Get("snd_device", "default", CVAR_ARCHIVE, "Default sound device");
	si.khz = snd_khz;
	si.Cvar_Get = Cvar_Get;
	si.Cvar_Set = Cvar_Set;
	si.Com_Printf = Com_Printf;
	si.Com_DPrintf = Com_DPrintf;
	si.S_PaintChannels = S_PaintChannels;
	si.paintedtime = &paintedtime;
#ifdef _WIN32
	si.cl_hwnd = cl_hwnd;
#endif

	if (!SND_Init(&si)) {
		Com_Printf("SND_Init failed\n");
		S_FreeLibs();
		sound_started = 0;
		return;
	}

	for (commands = r_commands; commands->name; commands++)
		Cmd_AddCommand(commands->name, commands->function, commands->description);

	S_InitScaletable();

	sound_started = 1;
	num_sfx = 0;

	music.ovPlaying[0] = 0;
	soundtime = 0;
	paintedtime = 0;

	Com_Printf("sound sampling rate: %i\n", dma.speed);

	/* FIXME: Error checking/wrong place here */
	if (snd_openal->value) {
#ifdef HAVE_OPENAL
		/* bindings */
		QAL_Init();
		/* client side init */
		SND_OAL_Init(NULL);
#else
		Com_Printf("No OpenAL support compiled into this binary\n");
#endif
	}

	S_StopAllSounds();

	/* Check memory integrity */
	Mem_CheckPoolIntegrity(cl_soundSysPool);

	/* start music again */
	if (*Cvar_VariableString("music"))
		S_OGG_Start();
}


/*
=======================================================================
Shutdown sound engine
======================================================================
*/

/**
 * @brief
 * @sa S_Init
 * @sa CL_Snd_Restart_f
 */
void S_Shutdown (void)
{
	int i;
	uint32_t size;
	sfx_t *sfx;
	const cmdList_t *commands;

	if (!sound_started)
		return;

	SND_Shutdown();
	S_OGG_Stop();
	S_OGG_Shutdown();

	sound_started = 0;

	for (commands = r_commands; commands->name; commands++)
		Cmd_RemoveCommand(commands->name);

	/* free all sounds */
	for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
		if (!sfx->name[0])
			continue;
		if (sfx->cache)
			Mem_Free(sfx->cache);
		memset(sfx, 0, sizeof(*sfx));
	}

	/* Free all memory */
	size = Mem_PoolSize(cl_soundSysPool);
	Com_Printf("...releasing %u bytes\n", size);
	Mem_FreePool(cl_soundSysPool);

	if (snd_ref_lib)
		S_FreeLibs();

	num_sfx = 0;
}


/*
=======================================================================
Load a sound
=======================================================================
*/

/**
 * @brief
 */
static sfx_t *S_FindName (const char *name, qboolean create)
{
	int i;
	sfx_t *sfx;
	char lname[MAX_QPATH];
	char *ename;
	size_t len;

	if (!name)
		Com_Error(ERR_FATAL, "S_FindName: NULL\n");
	if (!name[0])
		Com_Error(ERR_FATAL, "S_FindName: empty name\n");

	len = strlen(name);
	if (len >= MAX_QPATH)
		Com_Error(ERR_FATAL, "Sound name too long: %s", name);

	/* drop extension */
	Q_strncpyz(lname, name, sizeof(lname));
	if (lname[len - 4] == '.')
		len -= 4;
	ename = &(lname[len]);
	*ename = '\0';

	strcpy(ename, ".wav");
	/* see if already loaded */
	for (i = 0; i < num_sfx; i++)
		if (!Q_strncmp(known_sfx[i].name, lname, sizeof(known_sfx[i].name))) {
			return &known_sfx[i];
		}
	strcpy(ename, ".ogg");
	/* see if already loaded */
	for (i = 0; i < num_sfx; i++)
		if (!Q_strncmp(known_sfx[i].name, lname, sizeof(known_sfx[i].name))) {
			return &known_sfx[i];
		}
	*ename = '\0';

	if (!create)
		return NULL;

	/* find a free sfx */
	for (i = 0; i < num_sfx; i++)
		if (!known_sfx[i].name[0])
/*			registration_sequence < s_registration_sequence) */
			break;

	if (i == num_sfx) {
		if (num_sfx == MAX_SFX)
			Com_Error(ERR_FATAL, "S_FindName: out of sfx_t");
		num_sfx++;
	}

	sfx = &known_sfx[i];
	memset(sfx, 0, sizeof(*sfx));
	Q_strncpyz(sfx->name, lname, sizeof(sfx->name));
	sfx->registration_sequence = s_registration_sequence;

	return sfx;
}

/**
 * @brief
 */
void S_BeginRegistration (void)
{
	s_registration_sequence++;
	s_registering = qtrue;
}

/**
 * @brief
 */
sfx_t *S_RegisterSound (const char *name)
{
	sfx_t *sfx;

	if (!sound_started)
		return NULL;

	sfx = S_FindName(name, qtrue);
	if (!sfx)
		return NULL;
	sfx->registration_sequence = s_registration_sequence;

	if (!s_registering)
		S_LoadSound(sfx);

	return sfx;
}


/**
 * @brief
 */
void S_EndRegistration (void)
{
	int i;
	sfx_t *sfx;
	int size;

	/* free any sounds not from this registration sequence */
	for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
		if (!sfx->name[0])
			continue;
		if (sfx->registration_sequence != s_registration_sequence) {	/* don't need this sound */
			if (sfx->cache)		/* it is possible to have a leftover */
				Mem_Free(sfx->cache);	/* from a server that didn't finish loading */
			memset(sfx, 0, sizeof(*sfx));
		} else {				/* make sure it is paged in */
			if (sfx->cache) {
				size = sfx->cache->length * sfx->cache->width;
				Com_PageInMemory((byte *) sfx->cache, size);
			}
		}

	}

	/* load everything in */
	for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
		if (!sfx->name[0])
			continue;
		S_LoadSound(sfx);
	}

	s_registering = qfalse;
}


/**
 * @brief picks a channel based on priorities, empty slots, number of channels
 */
static channel_t *S_PickChannel (int entnum, int entchannel)
{
	int ch_idx;
	int first_to_die;
	int life_left;
	channel_t *ch;

	if (entchannel < 0)
		Com_Error(ERR_DROP, "S_PickChannel: entchannel < 0");

	/* Check for replacement sound, or find the best one to replace */
	first_to_die = -1;
	life_left = 0x7fffffff;
	for (ch_idx = 0; ch_idx < MAX_CHANNELS; ch_idx++) {
		if (entchannel != 0		/* channel 0 never overrides */
			&& channels[ch_idx].entnum == entnum && channels[ch_idx].entchannel == entchannel) {	/* always override sound from same entity */
			first_to_die = ch_idx;
			break;
		}

		/* don't let monster sounds override player sounds */
		if (channels[ch_idx].entnum == cl.pnum && entnum != cl.pnum && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left) {
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	ch = &channels[first_to_die];
	memset(ch, 0, sizeof(*ch));

	return ch;
}

/**
 * @brief Used for spatializing channels and autosounds
 */
static void S_SpatializeOrigin (vec3_t origin, float master_vol, float dist_mult, int *left_vol, int *right_vol)
{
	vec_t dot;
	vec_t dist;
	vec_t lscale, rscale, scale;
	vec3_t source_vec;

	if (cls.state != ca_active) {
		*left_vol = *right_vol = 255;
		return;
	}

	/* calculate stereo seperation and distance attenuation */
	VectorSubtract(origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec);
	dist -= SOUND_FULLVOLUME;
	if (dist < 0.)
		dist = 0.;				/* close enough to be at full volume */
	dist *= dist_mult;			/* different attenuation levels */

	dot = DotProduct(listener_right, source_vec);

	if (dma.channels == 1 || !dist_mult) {	/* no attenuation = no spatialization */
		rscale = 1.0;
		lscale = 1.0;
	} else {
		rscale = 0.5 * (1.0 + dot);
		lscale = 0.5 * (1.0 - dot);
	}

	/* add in distance effect */
	scale = (1.0 - dist) * rscale;
	*right_vol = (int) (master_vol * scale);
	if (*right_vol < 0)
		*right_vol = 0;

	scale = (1.0 - dist) * lscale;
	*left_vol = (int) (master_vol * scale);
	if (*left_vol < 0)
		*left_vol = 0;
}

/**
 * @brief spatializes a channel
 */
static void S_Spatialize (channel_t * ch)
{
	vec3_t origin;

	/* anything coming from the view entity will always be full volume */
	if (ch->entnum == cl.pnum) {
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	FastVectorCopy(ch->origin, origin);

	S_SpatializeOrigin(origin, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol);
}


/**
 * @brief
 */
static playsound_t *S_AllocPlaysound (void)
{
	playsound_t *ps;

	ps = s_freeplays.next;
	if (ps == &s_freeplays)
		return NULL;			/* no free playsounds */

	/* unlink from freelist */
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	return ps;
}


/**
 * @brief
 */
static void S_FreePlaysound (playsound_t * ps)
{
	/* unlink from channel */
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	/* add to free list */
	ps->next = s_freeplays.next;
	s_freeplays.next->prev = ps;
	ps->prev = &s_freeplays;
	s_freeplays.next = ps;
}



/**
 * @brief Take the next playsound and begin it on the channel
 * This is never called directly by S_Play*, but only
 * by the update loop.
 */
void S_IssuePlaysound (playsound_t * ps)
{
	channel_t *ch;
	sfxcache_t *sc = NULL;

	if (snd_show->value)
		Com_Printf("Issue %i\n", ps->begin);
	/* pick a channel to play on */
	ch = S_PickChannel(ps->entnum, ps->entchannel);
	if (!ch) {
		S_FreePlaysound(ps);
		return;
	}

	ch->dist_mult = ps->attenuation * 0.0005;
	ch->master_vol = ps->volume;
	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sfx = ps->sfx;
	VectorCopy(ps->origin, ch->origin);
	ch->fixed_origin = ps->fixed_origin;

	S_Spatialize(ch);

	ch->pos = 0;
	sc = S_LoadSound(ch->sfx);
	if (!sc)
		return;
	ch->end = paintedtime + sc->length;

	/* free the playsound */
	S_FreePlaysound(ps);
}

/*
=======================================================================
Start a sound effect
=======================================================================
*/

/**
 * @brief Validates the parms and ques the sound up
 * if pos is NULL, the sound will be dynamically sourced from the entity
 * Entchannel 0 will never override a playing sound
 */
void S_StartSound (vec3_t origin, int entnum, int entchannel, sfx_t * sfx, float fvol, float attenuation, float timeofs)
{
	sfxcache_t *sc;
	int vol;
	playsound_t *ps;
	int start;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	/* make sure the sound is loaded */
	sc = S_LoadSound(sfx);
	if (!sc)
		return;					/* couldn't load the sound's data */

	vol = fvol * 255;

	/* make the playsound_t */
	ps = S_AllocPlaysound();
	if (!ps)
		return;

	if (origin) {
		VectorCopy(origin, ps->origin);
		ps->fixed_origin = qtrue;
	} else
		ps->fixed_origin = qfalse;

	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->volume = vol;
	ps->sfx = sfx;

	/* drift s_beginofs */
	start = cl.time * 0.001 * dma.speed + s_beginofs;
	if (start < paintedtime) {
		start = paintedtime;
		s_beginofs = start - (cl.time * 0.001 * dma.speed);
	} else if (start > paintedtime + 0.3 * dma.speed) {
		start = paintedtime + 0.1 * dma.speed;
		s_beginofs = start - (cl.time * 0.001 * dma.speed);
	} else {
		s_beginofs -= 10;
	}

	if (!timeofs)
		ps->begin = paintedtime;
	else
		ps->begin = start + timeofs * dma.speed;

	ps->next = &s_pendingplays;
	ps->prev = &s_pendingplays;

	ps->next->prev = ps;
	ps->prev->next = ps;
}


/**
 * @brief
 */
void S_StartLocalSound (const char *sound)
{
	sfx_t *sfx;

	if (!sound_started) {
		Com_Printf("Sound is not yet started\n");
		return;
	}

	sfx = S_RegisterSound(sound);
	if (!sfx) {
		Com_Printf("S_StartLocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound(NULL, cl.pnum, 0, sfx, 1, 1, 0);
}


/**
 * @brief
 */
void S_ClearBuffer (void)
{
	int clear;

	if (!sound_started)
		return;

	s_rawend = 0;

	if (dma.samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	SND_BeginPainting();
	if (dma.buffer)
		memset(dma.buffer, clear, dma.samples * dma.samplebits / 8);
	SND_Submit();
}

/**
 * @brief
 */
void S_StopAllSounds (void)
{
	int i;

	if (!sound_started)
		return;

	/* clear all the playsounds */
	memset(s_playsounds, 0, sizeof(s_playsounds));
	s_freeplays.next = s_freeplays.prev = &s_freeplays;
	s_pendingplays.next = s_pendingplays.prev = &s_pendingplays;

	for (i = 0; i < MAX_PLAYSOUNDS; i++) {
		s_playsounds[i].prev = &s_freeplays;
		s_playsounds[i].next = s_freeplays.next;
		s_playsounds[i].prev->next = &s_playsounds[i];
		s_playsounds[i].next->prev = &s_playsounds[i];
	}

	/* clear all the channels */
	memset(channels, 0, sizeof(channels));

	/* stop music, if playing */
	S_OGG_Stop();

	S_ClearBuffer();

	/* restart music */
	S_OGG_Start();
}

/**
 * @brief Entities with a ->sound field will generated looped sounds
 * that are automatically started, stopped, and merged together
 * as the entities are sent to the client
 */
#if 0
void S_AddLoopSounds (void)
{
	int i, j;
	int sounds[MAX_EDICTS];
	int left, right, left_total, right_total;
	channel_t *ch;
	sfx_t *sfx;
	sfxcache_t *sc;
	int num;
	entity_state_t *ent;

	if (cls.state != ca_active)
		return;

	if (!cl.sound_prepped)
		return;

	for (i = 0; i < cl.frame.num_entities; i++) {
		num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		ent = &cl_parse_entities[num];
		sounds[i] = ent->sound;
	}

	for (i = 0; i < cl.frame.num_entities; i++) {
		if (!sounds[i])
			continue;

		sfx = cl.sound_precache[sounds[i]];
		if (!sfx)
			continue;			/* bad sound effect */
		sc = sfx->cache;
		if (!sc)
			continue;

		num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		ent = &cl_parse_entities[num];

		/* find the total contribution of all sounds of this type */
		S_SpatializeOrigin(ent->origin, 255.0, SOUND_LOOPATTENUATE, &left_total, &right_total);
		for (j = i + 1; j < cl.frame.num_entities; j++) {
			if (sounds[j] != sounds[i])
				continue;
			sounds[j] = 0;		/* don't check this again later */

			num = (cl.frame.parse_entities + j) & (MAX_PARSE_ENTITIES - 1);
			ent = &cl_parse_entities[num];

			S_SpatializeOrigin(ent->origin, 255.0, SOUND_LOOPATTENUATE, &left, &right);
			left_total += left;
			right_total += right;
		}

		if (left_total == 0 && right_total == 0)
			continue;			/* not audible */

		/* allocate a channel */
		ch = S_PickChannel(0, 0);
		if (!ch)
			return;

		if (left_total > 255)
			left_total = 255;
		if (right_total > 255)
			right_total = 255;
		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->autosound = qtrue;	/* remove next frame */
		ch->sfx = sfx;
		/* sometimes, the sc->length argument can become 0, and in that
		 * case we get a SIGFPE in the next modulo.  The workaround checks
		 * for this situation and sets the pos and end to zero if true */
		if (sc->length == 0) {
			ch->pos = ch->end = 0;
		} else {
			ch->pos = paintedtime % sc->length;
			ch->end = paintedtime + sc->length - ch->pos;
		}
	}
}
#endif /* 0 */

/**
 * @brief Cinematic streaming and voice over network
 */
void S_RawSamples (int samples, int rate, int width, int channels, byte * data, float volume)
{
	int i;
	int src, dst;
	float scale;

	if (!sound_started)
		return;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	scale = (float) rate / dma.speed;

/*	Com_Printf("%i < %i < %i\n", soundtime, paintedtime, s_rawend); */
	if (channels == 2 && width == 2) {
		if (scale == 1.0) {		/* optimized case */
			for (i = 0; i < samples; i++) {
				dst = s_rawend & (MAX_RAW_SAMPLES - 1);
				s_rawend++;
				s_rawsamples[dst].left = (int) (volume * LittleShort(((short *) data)[i * 2])) << 8;
				s_rawsamples[dst].right = (int) (volume * LittleShort(((short *) data)[i * 2 + 1])) << 8;
			}
		} else {
			for (i = 0;; i++) {
				src = i * scale;
				if (src >= samples)
					break;
				dst = s_rawend & (MAX_RAW_SAMPLES - 1);
				s_rawend++;
				s_rawsamples[dst].left = (int) (volume * LittleShort(((short *) data)[src * 2])) << 8;
				s_rawsamples[dst].right = (int) (volume * LittleShort(((short *) data)[src * 2 + 1])) << 8;
			}
		}
	} else if (channels == 1 && width == 2) {
		for (i = 0;; i++) {
			src = i * scale;
			if (src >= samples)
				break;
			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left = (int) (volume * LittleShort(((short *) data)[src])) << 8;
			s_rawsamples[dst].right = (int) (volume * LittleShort(((short *) data)[src])) << 8;
		}
	} else if (channels == 2 && width == 1) {
		for (i = 0;; i++) {
			src = i * scale;
			if (src >= samples)
				break;
			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left = (int) (volume * ((char *) data)[src * 2]) << 16;
			s_rawsamples[dst].right = (int) (volume * ((char *) data)[src * 2 + 1]) << 16;
		}
	} else if (channels == 1 && width == 1) {
		for (i = 0;; i++) {
			src = i * scale;
			if (src >= samples)
				break;
			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left = (int) (volume * (((byte *) data)[src] - 128)) << 16;
			s_rawsamples[dst].right = (int) (volume * (((byte *) data)[src] - 128)) << 16;
		}
	}
}

/**
 * @brief Called once each time through the main loop
 */
void S_Update (vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int i;
	int total;
	channel_t *ch;
	channel_t *combine;

	/* maybe the sound system is not meant to be initalized */
	if (!snd_init->value)
		return;

	/* did we switch the sound renderer */
	if (snd_ref->modified) {
		snd_ref->modified = qfalse;
		CL_Snd_Restart_f();
	}

	/* maybe no sound initialized */
	if (!sound_started)
		return;

	/* if the loading plaque is up, clear everything */
	/* out to make sure we aren't looping a dirty */
	/* dma buffer while loading */
/*	if (cls.disable_screen) {
		S_ClearBuffer ();
		return;
	}*/

	/* rebuild scale tables if volume is modified */
	if (snd_volume->modified)
		S_InitScaletable();

	/* re-sync music.fading if music volume modified */
	if (snd_music_volume->modified) {
		snd_music_volume->modified = qfalse;
		music.fading = snd_music_volume->value;
	}

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	combine = NULL;

	/* update spatialization for dynamic sounds */
	ch = channels;
	for (i = 0; i < MAX_CHANNELS; i++, ch++) {
		if (!ch->sfx)
			continue;
		if (ch->autosound) {	/* autosounds are regenerated fresh each frame */
			memset(ch, 0, sizeof(*ch));
			continue;
		}
		S_Spatialize(ch);		/* respatialize channel */
		if (!ch->leftvol && !ch->rightvol) {
			memset(ch, 0, sizeof(*ch));
			continue;
		}
	}

	/* add loopsounds */
	/*S_AddLoopSounds(); */

	/* debugging output */
	if (snd_show->value) {
		total = 0;
		ch = channels;
		for (i = 0; i < MAX_CHANNELS; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol)) {
				Com_Printf("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}

		Com_Printf("----(%i)---- painted: %i\n", total, paintedtime);
	}

	/* mix some sound */
	S_Update_();

	while (music.ovPlaying[0] && paintedtime + MAX_RAW_SAMPLES - 2048 > s_rawend)
		S_OGG_Read();
}

/**
 * @brief
 */
static void GetSoundtime (void)
{
	int samplepos;
	static int buffers;
	static int oldsamplepos;
	int fullsamples;

	fullsamples = dma.samples / dma.channels;

	/* it is possible to miscount buffers if it has wrapped twice between */
	/* calls to S_Update.  Oh well. */
	samplepos = SND_GetDMAPos();

	if (samplepos < oldsamplepos) {
		buffers++;				/* buffer wrapped */

		if (paintedtime > 0x40000000) {	/* time to chop things off to avoid 32 bit limits */
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds();
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers * fullsamples + samplepos / dma.channels;
}


/**
 * @brief
 */
void S_Update_ (void)
{
	unsigned int samps, endtime;

	if (!sound_started)
		return;

	SND_BeginPainting();

	if (!dma.buffer)
		return;

	/* Updates DMA time */
	GetSoundtime();

	/* check to make sure that we haven't overshot */
	if (paintedtime < soundtime) {
		Com_DPrintf("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

	/* mix ahead of current position */
	endtime = soundtime + snd_mixahead->value * dma.speed;
	/*endtime = (soundtime + 4096) & ~4095; */

	/* mix to an even submission block size */
	endtime = (endtime + dma.submission_chunk - 1)
		& ~(dma.submission_chunk - 1);
	samps = dma.samples >> (dma.channels - 1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	S_PaintChannels(endtime);

	SND_Submit();
}

/*
===============================================================================
console functions
===============================================================================
*/

/**
 * @brief
 */
static void S_Play_f (void)
{
	int i;
	char name[256];
	sfx_t *sfx;

	i = 1;
	while (i < Cmd_Argc()) {
		if (!strrchr(Cmd_Argv(i), '.')) {
			Q_strncpyz(name, Cmd_Argv(i), sizeof(name));
			Q_strcat(name, ".wav", sizeof(name));
		} else
			Q_strncpyz(name, Cmd_Argv(i), sizeof(name));
		sfx = S_RegisterSound(name);
		S_StartSound(NULL, cl.pnum, 0, sfx, 1.0, 1.0, 0);
		i++;
	}
}

/**
 * @brief
 */
static void S_SoundList_f (void)
{
	int i;
	sfx_t *sfx;
	sfxcache_t *sc;
	int size, total;

	total = 0;
	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++) {
		if (!sfx->registration_sequence)
			continue;
		sc = sfx->cache;
		if (sc) {
			size = sc->length * sc->width * (sc->stereo + 1);
			total += size;
			if (sc->loopstart >= 0)
				Com_Printf("L");
			else
				Com_Printf(" ");
			Com_Printf("(%2db) %6i : %s\n", sc->width * 8, size, sfx->name);
		} else {
			if (sfx->name[0] == '*')
				Com_Printf("  placeholder : %s\n", sfx->name);
			else
				Com_Printf("  not loaded  : %s\n", sfx->name);
		}
	}
	Com_Printf("Total resident: %i\n", total);
}

/**
 * @brief
 */
void S_Activate (qboolean active)
{
	if (SND_Activate)
		SND_Activate(active);
}

/**
 * @brief
 */
void S_ResampleSfx (sfx_t * sfx, int inrate, int inwidth, byte * data)
{
	int outcount;
	int srcsample;
	float stepscale;
	int i;
	int sample, samplefrac, fracstep;
	sfxcache_t *sc;

	sc = sfx->cache;
	if (!sc)
		return;

	stepscale = (float) inrate / dma.speed;	/* this is usually 0.5, 1, or 2 */

	outcount = (int)(sc->length / stepscale);
	if (outcount == 0) {
		Com_Printf("ResampleSfx: Invalid sound file '%s' (zero length)\n", sfx->name);
		/* free at next opportunity */
		Mem_Free(sfx->cache);
		sfx->cache = NULL;
		return;
	}

	sc->length = outcount;

	if (sc->loopstart != -1)
		sc->loopstart = (int)(sc->loopstart / stepscale);

	sc->speed = dma.speed;
	if (snd_loadas8bit->value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

	/* resample / decimate to the current source rate */
	if (stepscale == 1 && inwidth == 1 && sc->width == 1) {
		/* fast special case */
		for (i = 0; i < outcount; i++)
			((signed char *) sc->data)[i]
				= (int) ((unsigned char) (data[i]) - 128);
	} else {
		/* general case */
		samplefrac = 0;
		fracstep = stepscale * 256;
		for (i = 0; i < outcount; i++) {
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort(((short *) data)[srcsample]);
			else
				sample = (int) ((unsigned char) (data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *) sc->data)[i] = sample;
			else
				((signed char *) sc->data)[i] = sample >> 8;
		}
	}
}

/**
 * @brief
 * @sa S_L
 */
sfxcache_t *S_LoadSound (sfx_t *s)
{
	char name[MAX_QPATH];
	size_t len;
	int status;
	sfxcache_t *sc;
	char *extension;

	if (!s->name[0])
		return NULL;

	if (s->name[0] == '*')
		return NULL;

	/* see if still in memory */
	sc = s->cache;
	if (sc)
		return sc;

	Q_strncpyz(name, s->name, sizeof(name));
	len = strlen(name);
	if (len + 4 >= MAX_QPATH) {
		Com_Printf("S_LoadSound: MAX_QPATH exceeded: %i\n", len + 4);
		return NULL;
	}
	extension = &name[len];

	/* load it in */
	strcpy(extension, ".ogg");
	status = FS_CheckFile(va("sound/%s", name));
	if (status >= 0)
		return S_OGG_LoadSFX(s);
	strcpy(extension, ".wav");
	status = FS_CheckFile(va("sound/%s", name));
	if (status >= 0)
		return S_Wave_LoadSFX(s);
	Com_Printf("Could not load sound 'sound/%s'\n", s->name);

	return NULL;
}
