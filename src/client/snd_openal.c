/**
 * @file snd_openal.c
 * @brief Sound functions for openAL
 * @note To activate openAL in UFO:AI you have to set the snd_openal cvar to 1
 */

/*
Copyright (C) 1997-2001 UFO:AI team

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

#include "snd_openal.h"

static char* alBuffer;
static ALenum alFormatBuffer;
static ALsizei alFreqBuffer;
static long alBufferLen;
static unsigned int alSource;
static unsigned int alSampleSet;

static int alNumChannels;

/* extern in system implementation */
oalState_t	oalState;

cvar_t *snd_openal_volume;
cvar_t *snd_openal_device;

/**
 * @brief Init function that should be called after system implementation QAL_Init was called
 * @sa QAL_Init
 */
qboolean SND_OAL_Init (char* device)
{
	if (!openal_active) {
		Com_Printf("Error: OpenAL bindings not initialized yet\n");
		return qfalse;
	}

	if ((oalState.device = qalcOpenDevice((ALubyte*)device) == NULL)) {
		Com_Printf("Error: OpenAL device (%s) could not be opened\n", device);
		return qfalse;
	}

	Com_Printf("OpenAL: initialized:\n");
	Com_Printf("..version: \"%s\"\n", qalGetString(AL_VERSION));
	Com_Printf("..vendor: \"%s\"\n", qalGetString(AL_VENDOR));
	Com_Printf("..renderer: \"%s\"\n", qalGetString(AL_RENDERER));
	Com_Printf("..device: \"%s\"\n", qalcGetString (oalState.device, ALC_DEFAULT_DEVICE_SPECIFIER));

	if ((oalState.context = qalcCreateContext(oalState.device, NULL)) == NULL) {
		Com_Printf("Error: OpenAL context creation error\n");
		/* TODO: close device here, too */
		return qfalse;
	}

	qalcMakeContextCurrent(oalState.context);
	/* clear error code */
	qalGetError();

	snd_openal_device = Cvar_Get ("snd_openal_device", "", CVAR_ARCHIVE, "Device for openAL");
	snd_openal_volume = Cvar_Get ("snd_openal_volume", "", CVAR_ARCHIVE, "Volume for openAL");

	return qtrue;
}

/**
 * @brief
 * @sa SND_OAL_LoadSound
 */
static qboolean SND_OAL_LoadOGG (char* filename, ALboolean loop)
{
	return qfalse;
}

/**
 * @brief
 * @sa SND_OAL_LoadSound
 */
static qboolean SND_OAL_LoadWAV (char* filename, ALboolean loop)
{
/*	qalutLoadWAVFile(filename, &alFormatBuffer, (void **) &alBuffer,(unsigned int *)&alBufferLen, &alFreqBuffer, &loop);

	qalGenSources(1, &alSource);
	qalGenBuffers(1, &alSampleSet);
	qalBufferData(alSampleSet, alFormatBuffer, alBuffer, alBufferLen, alFreqBuffer);
	qalSourcei(alSource, AL_BUFFER, alSampleSet);

	qalutUnloadWAV(alFormatBuffer, alBuffer, alBufferLen, alFreqBuffer);
	return qtrue;
*/
	return qfalse;
}

/**
 * @brief Loads a sound via openAL
 * @sa SND_OAL_LoadWAV
 * @sa SND_OAL_LoadOGG
 */
qboolean SND_OAL_LoadSound (char* filename, qboolean looping)
{
	/* load our sound */
	if (strstr(filename, ".wav")) {
		if (!SND_OAL_LoadWAV(filename, looping))
			return qfalse;
	} else if (strstr(filename, ".ogg")) {
		if (!SND_OAL_LoadOGG(filename, looping))
			return qfalse;
	} else {
		Com_Printf("Unknown file format for %s\n", filename);
		return qfalse;
	}

	/* set the pitch */
	qalSourcef(alSource, AL_PITCH, 1.0f);
	/* set the gain */
	qalSourcef(alSource, AL_GAIN, 1.0f);
	/* set looping to true */
	qalSourcei(alSource, AL_LOOPING, AL_TRUE);
	return qtrue;
}

#if 0
static void S_OpenAL_AllocChannels (void)
{
	openal_channel_t *ch;
	int i;

	for (i = 0, ch = s_openal_channels; i < MAX_CHANNELS; i++, ch++) {
		qalGenSources(1, &ch->sourceNum);

		if (qalGetError() != AL_NO_ERROR)
			break;

		alNumChannels++;
	}
}

static void S_OpenAL_FreeChannels (void)
{
	openal_channel_t	*ch;
	int					i;

	for (i = 0, ch = s_openal_channels; i < s_openal_numChannels; i++, ch++) {
		qalDeleteSources(1, &ch->sourceNum);
		memset(ch, 0, sizeof(*ch));
	}

	alNumChannels = 0;
}
#endif

/**
 * @brief
 */
void SND_OAL_PlaySound (void)
{
	qalSourcePlay(alSource);
}

/**
 * @brief
 */
void SND_OAL_StopSound (void)
{
	qalSourceStop(alSource);
}

/**
 * @brief
 */
void SND_OAL_DestroySound (void)
{
	qalDeleteSources(1,&alSource);
	qalDeleteBuffers(1,&alSampleSet);
}
