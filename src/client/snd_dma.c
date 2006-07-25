/**
 * @file snd_dma.c
 * @brief Main control for any streaming sound output device.
 */

/* TODO: Implement snd_ref */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

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
#define dlladdr (void*)GetProcAddress
#include <windows.h>
#else
#define dlladdr dlsym
#include <dlfcn.h>
#endif

void S_Play(void);
void S_SoundList_f(void);
void S_Update_();
void S_StopAllSounds(void);

void S_PlayOGG(void);
void S_StartOGG(void);

/* ======================================================================= */
/* Internal sound data & structures */
/* ======================================================================= */

/* only begin attenuating sound volumes when outside the FULLVOLUME range */
#define		SOUND_FULLVOLUME	80

#define		SOUND_LOOPATTENUATE	0.003

int s_registration_sequence;

channel_t channels[MAX_CHANNELS];

qboolean snd_initialized = qfalse;
int sound_started = 0;

dma_t dma;

vec3_t listener_origin;
vec3_t listener_forward;
vec3_t listener_right;
vec3_t listener_up;

qboolean s_registering;

int soundtime;					/* sample PAIRS */
int paintedtime;				/* sample PAIRS */

/* during registration it is possible to have more sounds */
/* than could actually be referenced during gameplay, */
/* because we don't want to free anything until we are */
/* sure we won't need it. */
#define		MAX_SFX		(MAX_SOUNDS*2)
sfx_t known_sfx[MAX_SFX];
int num_sfx;

#define		MAX_PLAYSOUNDS	128
playsound_t s_playsounds[MAX_PLAYSOUNDS];
playsound_t s_freeplays;
playsound_t s_pendingplays;

int s_beginofs;

cvar_t *s_volume;
cvar_t *s_testsound;
cvar_t *s_loadas8bit;
cvar_t *s_khz;
cvar_t *s_show;
cvar_t *s_mixahead;

#ifdef _WIN32
cvar_t *s_primary;
#endif

cvar_t *ov_volume;
cvar_t *ov_loop;

cvar_t *snd_ref;
qboolean snd_ref_active;

int s_rawend;
portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

struct sndinfo si;
static void* snd_ref_lib = NULL;

qboolean (*SND_Init)(struct sndinfo* si);
int (*SND_GetDMAPos)(void);
void (*SND_Shutdown)(void);
void (*SND_BeginPainting)(void);
void (*SND_Submit)(void);

/* ==================================================================== */
/* User-setable variables */
/* ==================================================================== */
void S_SoundInfo_f(void)
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
	Com_Printf("0x%x dma buffer\n", dma.buffer);

}


void S_ModifyKhz_f(void)
{
	if (Cmd_Argc() < 2)
		return;

	if (*Cmd_Argv(1) == '+') {
		if (s_khz->value == 11)
			Cvar_SetValue("s_khz", 22);
		else if (s_khz->value == 22)
			Cvar_SetValue("s_khz", 44);
		else if (s_khz->value != 44)
			Cvar_SetValue("s_khz", 11);
	} else if (*Cmd_Argv(1) == '-') {
		if (s_khz->value == 44)
			Cvar_SetValue("s_khz", 22);
		else if (s_khz->value == 22)
			Cvar_SetValue("s_khz", 11);
		else if (s_khz->value != 11)
			Cvar_SetValue("s_khz", 11);
	}

	if (s_khz->modified)
		Cbuf_AddText("snd_restart\n");
}

/**
 * @brief
 * @sa S_Shutdown
 */
void S_Init(void)
{
	cvar_t *cv;

	Com_Printf("\n------- sound initialization -------\n");

	cv = Cvar_Get("s_initsound", "1", 0);

	if (!cv->value)
		Com_Printf("not initializing.\n");
	else {
		s_volume = Cvar_Get("s_volume", "0.7", CVAR_ARCHIVE);
		s_khz = Cvar_Get("s_khz", "44", CVAR_ARCHIVE);
		s_loadas8bit = Cvar_Get("s_loadas8bit", "0", CVAR_ARCHIVE);
		s_mixahead = Cvar_Get("s_mixahead", "0.2", CVAR_ARCHIVE);
		s_show = Cvar_Get("s_show", "0", 0);
		s_testsound = Cvar_Get("s_testsound", "0", 0);
#ifdef _WIN32
		s_primary = Cvar_Get("s_primary", "0", CVAR_ARCHIVE);	/* win32 specific */
#endif
		ov_volume = Cvar_Get("ov_volume", "0.5", CVAR_ARCHIVE);
		ov_loop = Cvar_Get("ov_loop", "1", 0);

		{
			char fn[MAX_QPATH];
			snd_ref = Cvar_Get("snd_ref", "sdl", CVAR_ARCHIVE);
			/* don't restart right again */
			snd_ref->modified = qfalse;

			Com_Printf("Loading snd_%s sound driver\n", snd_ref->string);
#ifdef _WIN32
			Com_sprintf(fn, sizeof(fn), "snd_%s.dll", snd_ref->string);
			if ((snd_ref_lib = LoadLibrary()) == 0) {
				Com_Printf("Load library failed - no sound available\n");
				return;
#else
			Com_sprintf(fn, sizeof(fn), "./snd_%s.so", snd_ref->string);
			if ((snd_ref_lib = dlopen(fn, RTLD_LAZY)) == 0) {
				Com_Printf("Load library failed: %s\n", dlerror());
				return;
#endif
			}

			if ((SND_Init = dlladdr(snd_ref_lib, "SND_Init")) == 0)
				Com_Error(ERR_FATAL, "dladdr failed loading SND_Init\n");
			if ((SND_Shutdown = dlladdr(snd_ref_lib, "SND_Shutdown")) == 0)
				Com_Error(ERR_FATAL, "dladdr failed loading SND_Shutdown\n");
			if ((SND_GetDMAPos = dlladdr(snd_ref_lib, "SND_GetDMAPos")) == 0)
				Com_Error(ERR_FATAL, "dladdr failed loading SND_GetDMAPos\n");
			if ((SND_BeginPainting = dlladdr(snd_ref_lib, "SND_BeginPainting")) == 0)
				Com_Error(ERR_FATAL, "dladdr failed loading SND_BeginPainting\n");
			if ((SND_Submit = dlladdr(snd_ref_lib, "SND_Submit")) == 0)
				Com_Error(ERR_FATAL, "dladdr failed loading SND_Submit\n");

			snd_ref_active = qtrue;
		}

		si.dma = &dma;
		si.bits = Cvar_Get("sndbits", "16", CVAR_ARCHIVE);
		si.speed = Cvar_Get("sndspeed", "44100", CVAR_ARCHIVE);
		si.channels = Cvar_Get("sndchannels", "2", CVAR_ARCHIVE);
		si.device = Cvar_Get("snddevice", "default", CVAR_ARCHIVE);
		si.s_khz = Cvar_Get("s_khz", "0", CVAR_ARCHIVE);
		si.Com_Printf = Com_Printf;
		si.S_PaintChannels = S_PaintChannels;

		if (!SND_Init(&si)) {
			Com_Printf("SND_Init failed\n");
			return;
		}

		Cmd_AddCommand("play", S_Play);
		Cmd_AddCommand("stopsound", S_StopAllSounds);
		Cmd_AddCommand("soundlist", S_SoundList_f);
		Cmd_AddCommand("soundinfo", S_SoundInfo_f);

		Cmd_AddCommand("ov_play", S_PlayOGG);
		Cmd_AddCommand("ov_start", S_StartOGG);
		Cmd_AddCommand("ov_stop", OGG_Stop);

		Cmd_AddCommand("modifykhz", S_ModifyKhz_f);

		S_InitScaletable();

		sound_started = 1;
		num_sfx = 0;

		ovPlaying[0] = 0;
		soundtime = 0;
		paintedtime = 0;

		Com_Printf("sound sampling rate: %i\n", dma.speed);

		S_StopAllSounds();

		/* start music again */
		if (*Cvar_VariableString("music"))
			S_StartOGG();
	}

	Com_Printf("------------------------------------\n");
}


/* ======================================================================= */
/* Shutdown sound engine */
/* ======================================================================= */

/**
 * @brief
 * @sa S_Init
 */
void S_Shutdown(void)
{
	int i;
	sfx_t *sfx;

	if (!sound_started)
		return;

	SND_Shutdown();
	OGG_Stop();

	sound_started = 0;

	Cmd_RemoveCommand("play");
	Cmd_RemoveCommand("stopsound");
	Cmd_RemoveCommand("soundlist");
	Cmd_RemoveCommand("soundinfo");
	Cmd_RemoveCommand("ov_play");
	Cmd_RemoveCommand("ov_start");
	Cmd_RemoveCommand("ov_stop");
	Cmd_RemoveCommand("modifykhz");

	/* free all sounds */
	for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
		if (!sfx->name[0])
			continue;
		if (sfx->cache)
			Z_Free(sfx->cache);
		memset(sfx, 0, sizeof(*sfx));
	}

	if (snd_ref_lib) {
		SND_Init = NULL;
		SND_Shutdown = NULL;
		SND_Submit = NULL;
		SND_GetDMAPos = NULL;
		SND_BeginPainting = NULL;
#ifdef _WIN32
		FreeLibrary(snd_ref_lib);
#else
		dlclose(snd_ref_lib);
#endif
		memset(&si, 0, sizeof(struct sndinfo));
		snd_ref_lib = NULL;
		snd_ref_active = qfalse;
		memset(&dma, 0, sizeof(dma_t));
	}

	num_sfx = 0;
}


/* ======================================================================= */
/* Load a sound */
/* ======================================================================= */

/*
==================
S_FindName

==================
*/
sfx_t *S_FindName(char *name, qboolean create)
{
	int i;
	sfx_t *sfx;

	if (!name)
		Com_Error(ERR_FATAL, "S_FindName: NULL\n");
	if (!name[0])
		Com_Error(ERR_FATAL, "S_FindName: empty name\n");

	if (strlen(name) >= MAX_QPATH)
		Com_Error(ERR_FATAL, "Sound name too long: %s", name);

	/* see if already loaded */
	for (i = 0; i < num_sfx; i++)
		if (!strcmp(known_sfx[i].name, name)) {
			return &known_sfx[i];
		}

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
	Q_strncpyz(sfx->name, name, MAX_QPATH);
	sfx->registration_sequence = s_registration_sequence;

	return sfx;
}


/*
==================
S_AliasName

==================
*/
sfx_t *S_AliasName(char *aliasname, char *truename)
{
	sfx_t *sfx;
	char *s;

	int i;

	s = Z_Malloc(MAX_QPATH);
	Q_strncpyz(s, truename, MAX_QPATH);

	/* find a free sfx */
	for (i = 0; i < num_sfx; i++)
		if (!known_sfx[i].name[0])
			break;

	if (i == num_sfx) {
		if (num_sfx == MAX_SFX)
			Com_Error(ERR_FATAL, "S_FindName: out of sfx_t");
		num_sfx++;
	}

	sfx = &known_sfx[i];
	memset(sfx, 0, sizeof(*sfx));
	Q_strncpyz(sfx->name, aliasname, MAX_QPATH);
	sfx->registration_sequence = s_registration_sequence;
	sfx->truename = s;

	return sfx;
}


/*
=====================
S_BeginRegistration

=====================
*/
void S_BeginRegistration(void)
{
	s_registration_sequence++;
	s_registering = qtrue;
}

/*
==================
S_RegisterSound

==================
*/
sfx_t *S_RegisterSound(char *name)
{
	sfx_t *sfx;

	if (!sound_started)
		return NULL;

	sfx = S_FindName(name, qtrue);
	sfx->registration_sequence = s_registration_sequence;

	if (!s_registering)
		S_LoadSound(sfx);

	return sfx;
}


/*
=====================
S_EndRegistration

=====================
*/
void S_EndRegistration(void)
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
				Z_Free(sfx->cache);	/* from a server that didn't finish loading */
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


/*============================================================================= */

/*
=================
S_PickChannel
=================
*/
channel_t *S_PickChannel(int entnum, int entchannel)
{
	int ch_idx;
	int first_to_die;
	int life_left;
	channel_t *ch;

	if (entchannel < 0)
		Com_Error(ERR_DROP, "S_PickChannel: entchannel<0");

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
void S_SpatializeOrigin(vec3_t origin, float master_vol, float dist_mult, int *left_vol, int *right_vol)
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
	if (dist < 0)
		dist = 0;				/* close enough to be at full volume */
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
 * @brief
 */
void S_Spatialize(channel_t * ch)
{
	vec3_t origin;

	/* anything coming from the view entity will always be full volume */
	if (ch->entnum == cl.pnum) {
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	VectorCopy(ch->origin, origin);

	S_SpatializeOrigin(origin, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol);
}


/**
 * @brief
 */
playsound_t *S_AllocPlaysound(void)
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
void S_FreePlaysound(playsound_t * ps)
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
void S_IssuePlaysound(playsound_t * ps)
{
	channel_t *ch;
	sfxcache_t *sc;

	if (s_show->value)
		Com_Printf("Issue %i\n", ps->begin);
	/* pick a channel to play on */
	ch = S_PickChannel(ps->entnum, ps->entchannel);
	if (!ch) {
		S_FreePlaysound(ps);
		return;
	}

	/* spatialize */
	if (ps->attenuation == ATTN_STATIC)
		ch->dist_mult = ps->attenuation * 0.001;
	else
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
	ch->end = paintedtime + sc->length;

	/* free the playsound */
	S_FreePlaysound(ps);
}

/* ======================================================================= */
/* Start a sound effect */
/* ======================================================================= */

/**
 * @brief Validates the parms and ques the sound up
 * if pos is NULL, the sound will be dynamically sourced from the entity
 * Entchannel 0 will never override a playing sound
 */
void S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t * sfx, float fvol, float attenuation, float timeofs)
{
	sfxcache_t *sc;
	int vol;
	playsound_t *ps, *sort;
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

	/* sort into the pending sound list */
	for (sort = s_pendingplays.next; sort != &s_pendingplays && sort->begin < ps->begin; sort = sort->next);

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}


/**
 * @brief
 */
void S_StartLocalSound(char *sound)
{
	sfx_t *sfx;

	if (!sound_started)
		return;

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
void S_ClearBuffer(void)
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
void S_StopAllSounds(void)
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
	ovPlaying[0] = 0;

	S_ClearBuffer();

	/* restart music */
	S_StartOGG();
}

/**
 * @brief Entities with a ->sound field will generated looped sounds
 * that are automatically started, stopped, and merged together
 * as the entities are sent to the client
 */
#if 0
void S_AddLoopSounds(void)
{
	int i, j;
	int sounds[MAX_EDICTS];
	int left, right, left_total, right_total;
	channel_t *ch;
	sfx_t *sfx;
	sfxcache_t *sc;
	int num;
	entity_state_t *ent;

	if (cl_paused->value)
		return;

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

/*============================================================================= */

/**
 * @brief Cinematic streaming and voice over network
 */
void S_RawSamples(int samples, int rate, int width, int channels, byte * data, float volume)
{
	int i;
	int src, dst;
	float scale;

	if (!sound_started)
		return;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	scale = (float) rate / dma.speed;

/*	Com_Printf ("%i < %i < %i\n", soundtime, paintedtime, s_rawend); */
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

/*============================================================================= */

/**
 * @brief Called once each time through the main loop
 */
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int i;
	int total;
	channel_t *ch;
	channel_t *combine;

	if (!sound_started)
		return;

	/* if the laoding plaque is up, clear everything */
	/* out to make sure we aren't looping a dirty */
	/* dma buffer while loading */
/*	if (cls.disable_screen)
	{
		S_ClearBuffer ();
		return;
	}*/

	/* rebuild scale tables if volume is modified */
	if (s_volume->modified)
		S_InitScaletable();

	if (snd_ref->modified) {
		snd_ref->modified = qfalse;
		CL_Snd_Restart_f();
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
	if (s_show->value) {
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

	/* stream music */
	while (ovPlaying[0] && paintedtime + MAX_RAW_SAMPLES - 2048 > s_rawend)
		OGG_Read();
}

/**
 * @brief
 */
void GetSoundtime(void)
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
void S_Update_(void)
{
	unsigned endtime;
	int samps;

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
	endtime = soundtime + s_mixahead->value * dma.speed;
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
==========================================================

  OGG Vorbis decoding

==========================================================
*/

char ovPlaying[MAX_QPATH];
static OggVorbis_File ovFile;
static char ovBuf[4096];
static int ovSection;

/**
 * @brief
 */
qboolean OGG_Open(char *filename)
{
	FILE *f;
	int res, length;
	vorbis_info *vi;

	if (ov_volume->value <= 0)
		return qfalse;

	/* check running music */
	if (ovPlaying[0]) {
		if (!strcmp(ovPlaying, filename))
			return qtrue;
		else
			OGG_Stop();
	}

	/* find file */
	length = FS_FOpenFile(va("music/%s.ogg", filename), &f);
	if (!f) {
		Com_Printf("Couldn't open 'music/%s.ogg'\n", filename);
		return qfalse;
	}

	/* open ogg vorbis file */
	res = ov_open(f, &ovFile, NULL, 0);
	if (res < 0) {
		Com_Printf("'music/%s.ogg' isn't a valid ogg vorbis file (error %i)\n", filename, res);
		fclose(f);
		return qfalse;
	}

	vi = ov_info(&ovFile, -1);
	if ((vi->channels != 1) && (vi->channels != 2)) {
		Com_Printf("%s has an unsupported number of channels: %i\n", filename, vi->channels);
		fclose(f);
		return qfalse;
	}

/*	Com_Printf( "Playing '%s'\n", filename ); */
	Q_strncpyz(ovPlaying, filename, MAX_QPATH);
	ovSection = 0;
	return qtrue;
}

/**
 * @brief
 */
void OGG_Stop(void)
{
	ovPlaying[0] = 0;
	ov_clear(&ovFile);
}

/**
 * @brief
 */
int OGG_Read(void)
{
	int res;

	if (ov_volume->value <= 0) {
		OGG_Stop();
		return 0;
	}

	/* read and resample */
	res = ov_read(&ovFile, ovBuf, sizeof(ovBuf), 0, 2, 1, &ovSection);
	S_RawSamples(res >> 2, 44100, 2, 2, (byte *) ovBuf, ov_volume->value);

	/* end of file? */
	if (!res) {
		if ((int) ov_loop->value)
			ov_raw_seek(&ovFile, 0);
		else
			OGG_Stop();
	}
	return res;
}

/**
 * @brief
 */
void S_PlayOGG(void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: ov_play <filename>\n");
		return;
	}
	OGG_Open(Cmd_Argv(1));
}

/**
 * @brief
 */
void S_StartOGG(void)
{
	char *str;

	str = Cvar_VariableString("music");
	if (str[0])
		OGG_Open(str);
}

/*
===============================================================================

console functions

===============================================================================
*/

/**
 * @brief
 */
void S_Play(void)
{
	int i;
	char name[256];
	sfx_t *sfx;

	i = 1;
	while (i < Cmd_Argc()) {
		if (!strrchr(Cmd_Argv(i), '.')) {
			Q_strncpyz(name, Cmd_Argv(i), 256);
			Q_strcat(name, ".wav", sizeof(name));
		} else
			Q_strncpyz(name, Cmd_Argv(i), 256);
		sfx = S_RegisterSound(name);
		S_StartSound(NULL, cl.pnum, 0, sfx, 1.0, 1.0, 0);
		i++;
	}
}

/**
 * @brief
 */
void S_SoundList_f(void)
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
