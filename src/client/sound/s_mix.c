/**
 * @file s_mix.c
 * @brief Main control for any streaming sound output device.
 */

/*
All original material Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#include "s_local.h"
#include "s_mix.h"
#include "s_main.h"

static int S_AllocChannel (void)
{
	int i;

	for (i = 0; i < MAX_CHANNELS; i++) {
		if (!s_env.channels[i].sample)
			return i;
	}

	return -1;
}

void S_FreeChannel (int c)
{
	memset(&s_env.channels[c], 0, sizeof(s_env.channels[0]));
}

#define DISTANCE_SCALE 0.8

/**
 * @brief Set distance and stereo panning for the specified channel.
 */
void S_SpatializeChannel (const s_channel_t *ch)
{
	vec3_t origin, delta;
	float dist, angle;
	const int c = (int)((ptrdiff_t)(ch - s_env.channels));
	const le_t *actor;

	VectorCopy(ch->org, origin);

	actor = LE_GetClosestActor(origin);
	if (actor)
		VectorSubtract(origin, actor->origin, delta);
	else
		VectorSubtract(origin, cl.cam.camorg, delta);

	dist = VectorNormalize(delta) * DISTANCE_SCALE * ch->atten * ch->atten;

	if (dist > 255.0)  /* clamp to max */
		dist = 255.0;

	if (dist > 50.0) {  /* resolve stereo panning */
		const float dot = DotProduct(s_env.right, delta);
		angle = acos(dot) * 180.0 / M_PI - 90.0;
		angle = (int)(360.0 - angle) % 360;
	} else
		angle = 0;

	Mix_SetPosition(c, (int)angle, (int)dist);
}

/**
 * @brief Validates the parms and queues the sound up
 * @param[in] origin if is NULL, the sound will be dynamically sourced from the entity
 * @param[in] sample The soundfile to play
 * @param[in] relVolume Max mixer volume factor (0.0 - 1.0)
 * @sa S_StartLocalSample
 * @sa S_SetVolume
 */
void S_PlaySample (const vec3_t origin, s_sample_t* sample, float atten)
{
	s_channel_t *ch;
	int i;

	if (!s_env.initialized)
		return;

	if (!sample)
		return;

	if ((i = S_AllocChannel()) == -1)
		return;

	ch = &s_env.channels[i];

	ch->atten = atten;
	ch->sample = sample;

	if (origin != NULL) {
		VectorCopy(origin, ch->org);
		S_SpatializeChannel(ch);
	}

	Mix_PlayChannel(i, ch->sample->chunk, 0);
}

/**
 * @brief Adds a loop sample for e.g. ambient sounds
 */
void S_LoopSample (const vec3_t org, s_sample_t *sample)
{
	s_channel_t *ch;
	int i;

	if (!sample || !sample->chunk)
		return;

	ch = NULL;

	for (i = 0; i < MAX_CHANNELS; i++){  /* find existing loop sound */
		if (s_env.channels[i].sample == sample) {
			ch = &s_env.channels[i];
			break;
		}
	}

	if (ch) {  /* update existing loop sample */
		ch->count++;

		VectorMix(ch->org, org, 1.0 / ch->count, ch->org);
	} else {  /* or allocate a new one */
		if ((i = S_AllocChannel()) == -1)
			return;

		ch = &s_env.channels[i];

		VectorCopy(org, ch->org);
		ch->count = 1;
		ch->atten = DEFAULT_SOUND_ATTENUATION;
		ch->sample = sample;

		Mix_PlayChannel(i, ch->sample->chunk, 0);
	}

	S_SpatializeChannel(ch);
}

/**
 * @sa S_PlaySample
 */
void S_StartLocalSample (const char *name)
{
	s_sample_t *sample;

	if (!s_env.initialized)
		return;

	sample = S_LoadSample(name);
	if (!sample) {
		Com_Printf("S_StartLocalSample: Failed to load %s\n", name);
		return;
	}
	S_PlaySample(NULL, sample, DEFAULT_SOUND_ATTENUATION);
}

/**
 * @brief Loads a buffer from memory into the mixer
 * @param[in] mem 16 byte (short) buffer with data
 * @returns the channel the sound is played on or -1 in case of an error
 */
int S_StartRawSample (const short* mem, size_t size, int rate, int channel, int ms)
{
	SDL_AudioCVT wavecvt;
	Mix_Chunk chunk;
	const int chunkSize = 2 * channel;

	if (!s_env.initialized)
		return -1;

	/* Build the audio converter and create conversion buffers */
	if (SDL_BuildAudioCVT(&wavecvt, AUDIO_S16, channel, rate,
			audioFormat, audioChannels, audioRate) < 0) {
		return -1;
	}

	wavecvt.len = size & ~(chunkSize - 1);
	wavecvt.buf = (byte *)Mem_PoolAlloc(wavecvt.len * wavecvt.len_mult, cl_soundSysPool, 0);
	if (wavecvt.buf == NULL)
		return -1;
	memcpy(wavecvt.buf, mem, size);

	/* Run the audio converter */
	if (SDL_ConvertAudio(&wavecvt) < 0) {
		Mem_Free(wavecvt.buf);
		return -1;
	}

	chunk.allocated = 0;
	chunk.abuf = wavecvt.buf;
	chunk.alen = wavecvt.len_cvt;
	chunk.volume = MIX_MAX_VOLUME * snd_volume->value;

	/** @todo Free the channel data after the channel ended - memleak */
	return Mix_PlayChannelTimed(-1, &chunk, 0, ms);
}
