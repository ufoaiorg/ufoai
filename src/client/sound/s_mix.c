/**
 * @file s_mix.c
 * @brief Main control for any streaming sound output device.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../cl_shared.h"
#include "../cl_renderer.h"
#include "../cl_video.h"
#include "../battlescape/cl_camera.h"
#include "../battlescape/cl_localentity.h"
#include "../battlescape/cl_battlescape.h"

#include "s_mix.h"
#include "s_sample.h"
#include "s_main.h"

/**
 * @brief Searches a channel with no sample applied yet.
 * @return @c -1 if no free channel was found, otherwise the index of the channel
 */
static int S_AllocChannel (void)
{
	int i;

	for (i = 0; i < MAX_CHANNELS; i++) {
		if (!s_env.channels[i].sample)
			return i;
	}

	return -1;
}

/**
 * @brief Callback that is called when a channel finished playing
 * @param c The channel id
 */
void S_FreeChannel (int c)
{
	OBJZERO(s_env.channels[c]);
}

/**
 * @brief Set distance and stereo panning for the specified channel.
 * @param[in] ch The channel to perform the spatialization for.
 */
void S_SpatializeChannel (const s_channel_t *ch)
{
	vec3_t delta;
	float dist, angle;
	const int c = (int)((ptrdiff_t)(ch - s_env.channels));

	VectorSubtract(ch->org, cl.cam.camorg, delta);

	dist = VectorNormalize(delta) * snd_distance_scale->value * ch->atten;

	if (dist > 255.0)  /* clamp to max */
		dist = 255.0;

	if (dist > 50.0) {  /* resolve stereo panning */
		const float dot = DotProduct(s_env.right, delta);
		angle = (int)(450.0 - acos(dot) * todeg) % 360;
	} else
		angle = 0;

	Mix_SetPosition(c, (int)angle, (int)dist);
}

/**
 * @brief Validates the parms and queues the sound up
 * @param[in] origin if this is @c NULL, the sound will be dynamically sourced from the entity
 * @param[in] sample The soundfile to play
 * @param[in] atten Attenuation of sound to be played (for example, @c fireAttenuation
 * or @c impactAttenuation from @c fireDef_s).
 * @param[in] relVolume Max mixer volume factor (0.0 - 1.0)
 * @sa S_StartLocalSample
 * @sa S_SetVolume
 */
void S_PlaySample (const vec3_t origin, s_sample_t* sample, float atten, float relVolume)
{
	s_channel_t *ch;
	int i;
	float volume;

	if (!s_env.initialized)
		return;

	if (!sample)
		return;

	/* if the last mix of this particular sample is less than half a second ago, skip it */
	if (sample->lastPlayed > CL_Milliseconds() - s_env.sampleRepeatRate)
		return;

	if ((i = S_AllocChannel()) == -1)
		return;

	sample->lastPlayed = CL_Milliseconds();
	ch = &s_env.channels[i];

	ch->atten = atten;
	ch->sample = sample;

	if (origin != NULL) {
		VectorCopy(origin, ch->org);
		S_SpatializeChannel(ch);
	}

	volume = snd_volume->value * relVolume * MIX_MAX_VOLUME;
	Com_DPrintf(DEBUG_SOUND, "%i: Playing sample '%s' at volume %f at channel %i\n",
			CL_Milliseconds(), sample->name, volume, i);
	Mix_VolumeChunk(ch->sample->chunk, volume);
	Mix_PlayChannel(i, ch->sample->chunk, 0);
}

/**
 * @brief Adds a loop sample for e.g. ambient sounds
 */
void S_LoopSample (const vec3_t org, s_sample_t *sample, float relVolume, float attenuation)
{
	s_channel_t *ch;
	int i;

	if (!sample || !sample->chunk)
		return;

	ch = NULL;

	for (i = 0; i < MAX_CHANNELS; i++){  /* find existing loop sound */
		if (s_env.channels[i].sample == sample) {
			vec3_t delta;
			VectorSubtract(s_env.channels[i].org, org, delta);
			if (VectorLength(delta) < 255.0) {
				ch = &s_env.channels[i];
				break;
			}
		}
	}

	if (ch) {  /* update existing loop sample */
		ch->count++;

		VectorMix(ch->org, org, 1.0 / ch->count, ch->org);
	} else {  /* or allocate a new one */
		float volume;

		if ((i = S_AllocChannel()) == -1)
			return;

		ch = &s_env.channels[i];

		sample->lastPlayed = CL_Milliseconds();
		VectorCopy(org, ch->org);
		ch->count = 1;
		ch->atten = attenuation;
		ch->sample = sample;

		volume = snd_volume->value * relVolume * MIX_MAX_VOLUME;
		Mix_VolumeChunk(ch->sample->chunk, volume);
		Mix_PlayChannel(i, ch->sample->chunk, 0);
	}

	S_SpatializeChannel(ch);
}

/**
 * @brief Plays a sample without spatialization
 * @param[in] name The sample name
 * @param[in] relVolume Max mixer volume factor (0.0 - 1.0)
 * @sa S_PlaySample
 * @sa S_LoadSample
 */
void S_StartLocalSample (const char *name, float relVolume)
{
	s_sample_t *sample;

	if (!s_env.initialized)
		return;

	sample = S_LoadSample(name);
	if (!sample) {
		Com_Printf("S_StartLocalSample: Failed to load %s\n", name);
		return;
	}
	S_PlaySample(NULL, sample, SOUND_ATTN_NORM, relVolume);
}
