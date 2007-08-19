/**
 * @file snd_openal.c
 * @brief Sound functions for openAL
 * @note To activate openAL in UFO:AI you have to set the snd_openal cvar to 1
 * @todo fix music not restarting, fix sound effects
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

#ifdef HAVE_OPENAL

#include "snd_openal.h"

/* extern in system implementation */
oalState_t	oalState;

static unsigned int snd_openal_numChannels;

ALuint sourceNames[MAX_CHANNELS + 1];

static cvar_t *snd_openal_volume;
static cvar_t *snd_openal_device;
static cvar_t *snd_openal_channels;

#define SND_OAL_Error() SND_OAL_ErrorPrint(__FILE__, __LINE__)

/**
 * @brief Checks for OpenAL errors
 */
static void SND_OAL_ErrorPrint (const char* file, const int line)
{
	int err = qalGetError();
	if (err == AL_NO_ERROR)
		return;

	Com_Printf("OpenAL error was raised at %s:%i) ", file, line);

	switch (err) {
	case AL_INVALID_NAME:
		Com_Printf("AL_INVALID_NAME");
		break;
	case AL_INVALID_ENUM:
		Com_Printf("AL_INVALID_ENUM");
		break;
	case AL_INVALID_VALUE:
		Com_Printf("AL_INVALID_VALUE");
		break;
	case AL_INVALID_OPERATION:
		Com_Printf("AL_INVALID_OPERATION");
		break;
	case AL_OUT_OF_MEMORY:
		Com_Printf("AL_OUT_OF_MEMORY");
		break;
	default:
		Com_Printf("%i", err);
	};
	Com_Printf("\n");
}

/**
 * @brief Shutdown function
 */
void SND_OAL_Shutdown (void)
{
	qalDeleteSources(snd_openal_numChannels + 1, sourceNames);
	snd_openal_numChannels = 0;
}

/**
 * @brief Picks a free openAL channel
 */
static ALint SND_OAL_PickChannel (void)
{
	ALint i;
	ALint sourceState;

	for (i = 0; i < snd_openal_numChannels; i++) {
		qalGetSourcei(sourceNames[i], AL_SOURCE_STATE, &sourceState);

		/* The source already out of processing.*/
		if (sourceState == AL_STOPPED || sourceState == AL_INITIAL) {
			return i;
		}
	}
	return -1;
}

/**
 * @brief
 */
void SND_OAL_StartSound (ALuint bufferNum)
{
	if (snd_openal_numChannels && bufferNum) {
		/* pick a channel and start the sound effect */
		ALint sourceNum = SND_OAL_PickChannel();
		if (sourceNum != -1) {
			qalSourcef(sourceNum, AL_PITCH, 1);
			qalSource3f(sourceNum, AL_DIRECTION, 0, 0, 0);
			qalSourcef(sourceNum, AL_REFERENCE_DISTANCE, 0);
			qalSourcef(sourceNum, AL_ROLLOFF_FACTOR, 0);
			qalSource3f(sourceNum, AL_POSITION, 0, 0, 0);
			qalSource3f(sourceNum, AL_VELOCITY, 0, 0, 0);
			qalSourcei(sourceNum, AL_BUFFER, bufferNum);
			qalSourcei(sourceNum, AL_SOURCE_RELATIVE, AL_TRUE);
			qalSourcei(sourceNum, AL_LOOPING, AL_FALSE);
			qalSourcef(sourceNum, AL_GAIN, 0.47);
			qalSourcePlay(sourceNum);
		}
	}
}

/**
 * @brief
 */
static void SND_OAL_AllocateChannels (void)
{
	if (qalGenSources) {
		/* +1 more channel for streaming */
		snd_openal_numChannels = MAX_CHANNELS + 1;
		while (snd_openal_numChannels > MIN_CHANNELS) {
			qalGenSources(snd_openal_numChannels, sourceNames);
			snd_openal_numChannels--;
			if (qalGetError() == AL_NO_ERROR) {
				Com_Printf("..%i mix channels allocated.\n", snd_openal_numChannels);
				Com_Printf("..streaming channel allocated.\n");
				return;
			} else
				SND_OAL_Error();
		}
		Com_Printf("..not enough mix channels!\n");
	}
	snd_openal_numChannels = 0;
}

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

	if (device)
		oalState.device = qalcOpenDevice((ALubyte*)device);
	else
		oalState.device = qalcOpenDevice(NULL);
	SND_OAL_Error();

	if (!oalState.device) {
		Com_Printf("Error: OpenAL device (%s) could not be opened\n", device);
		return qfalse;
	}

	Com_Printf("OpenAL: initialized:\n");
	Com_Printf("..version: \"%s\"\n", qalGetString(AL_VERSION));
	Com_Printf("..vendor: \"%s\"\n", qalGetString(AL_VENDOR));
	Com_Printf("..renderer: \"%s\"\n", qalGetString(AL_RENDERER));
	Com_Printf("..device: \"%s\"\n", qalcGetString(oalState.device, ALC_DEFAULT_DEVICE_SPECIFIER));
	Com_Printf("..extensions: %s\n", qalcGetString(oalState.device, ALC_EXTENSIONS));

	if ((oalState.context = qalcCreateContext(oalState.device, NULL)) == NULL) {
		Com_Printf("Error: OpenAL context creation error\n");
		/* @todo: close device here, too */
		return qfalse;
	}

	qalcMakeContextCurrent(oalState.context);
	/* clear error code */
	SND_OAL_Error();

	SND_OAL_AllocateChannels();

	snd_openal_device = Cvar_Get("snd_openal_device", "", CVAR_ARCHIVE, "Device for openAL");
	snd_openal_volume = Cvar_Get("snd_openal_volume", "", CVAR_ARCHIVE, "Volume for openAL");
	snd_openal_channels = Cvar_Get("snd_openal_channels", "4", CVAR_ARCHIVE, "openAL channels");

	return qtrue;
}

/*=====================================================
 * OGG streaming
 *=====================================================*/

/**
 * @brief
 * @sa SND_OAL_Stream
 */
static qboolean SND_OAL_ReadOGG (music_t* music, ALuint buffer)
{
	char pcm[OAL_OGG_BUFFER_SIZE];
	int  size = 0, result;

	while (size < OAL_OGG_BUFFER_SIZE) {
		result = ov_read(&music->ovFile, pcm + size, OAL_OGG_BUFFER_SIZE - size, 0, 2, 1, &music->ovSection);

		if (result > 0)
			size += result;
		else {
			if (result < 0)
				return qfalse;
			else {
				if (snd_music_loop->integer)
					ov_raw_seek(&music->ovFile, 0);
				else
					S_OGG_Stop();
				break;
			}
		}
	}

	if (size == 0)
		return qfalse;

	qalBufferData(buffer, music->format, pcm, size, music->ovInfo->rate);
	SND_OAL_Error();

	return qtrue;
}

/**
 * @brief
 */
static qboolean SND_OAL_StreamPlaying (music_t *music)
{
	ALenum state;
	qalGetSourcei(music->source, AL_SOURCE_STATE, &state);
	return (state == AL_PLAYING);
}

/**
 * @brief Stream the background music via openAL
 * @param[in] music
 * @sa S_OGG_Stop
 */
static qboolean SND_OAL_StreamPlayback (music_t* music)
{
	if (SND_OAL_StreamPlaying(music))
		return qtrue;

	if (!SND_OAL_ReadOGG(music, music->buffers[0]))
		return qfalse;

	if (!SND_OAL_ReadOGG(music, music->buffers[1]))
		return qfalse;

	qalSourceQueueBuffers(music->source, 2, music->buffers);
	qalSourcePlay(music->source);

	return qtrue;
}

/**
 * @brief Stream the background music via openAL
 * @param[in] music
 * @sa S_OGG_Stop
 */
qboolean SND_OAL_Stream (music_t* music)
{
	int processed;
	qboolean active = qtrue;

	qalGetSourcei(music->source, AL_BUFFERS_PROCESSED, &processed);

	while (processed--) {
		ALuint buffer;

		qalSourceUnqueueBuffers(music->source, 1, &buffer);
		SND_OAL_Error();

		active = SND_OAL_ReadOGG(music, buffer);

		qalSourceQueueBuffers(music->source, 1, &buffer);
		SND_OAL_Error();
	}

	if (active && SND_OAL_StreamPlaying(music))
		SND_OAL_StreamPlayback(music);

	return active;
}

/**
 * @brief
 * @sa SND_OAL_Stream
 * @sa SND_OAL_StopStream
 */
void SND_OAL_StartStream (music_t* music)
{
	if (music->ovInfo->channels == 1)
		music->format = AL_FORMAT_MONO16;
	else
		music->format = AL_FORMAT_STEREO16;

	qalGenBuffers(2, music->buffers);
	SND_OAL_Error();
	qalGenSources(1, &music->source);
	SND_OAL_Error();

	qalSourcefv(music->source, AL_POSITION, vec3_origin);
	qalSourcefv(music->source, AL_VELOCITY, vec3_origin);
	qalSourcefv(music->source, AL_DIRECTION, vec3_origin);
	qalSourcef(music->source, AL_ROLLOFF_FACTOR, 0.0);
	qalSourcei(music->source, AL_SOURCE_RELATIVE, AL_TRUE);
	SND_OAL_Error();

	SND_OAL_StreamPlayback(music);
}

/**
 * @brief
 * @sa SND_OAL_StartStream
 */
void SND_OAL_StopStream (music_t* music)
{
	int queued;

	qalSourceStop(music->source);
	qalGetSourcei(music->source, AL_BUFFERS_QUEUED, &queued);

	while (queued--) {
		ALuint buffer;
		qalSourceUnqueueBuffers(music->source, 1, &buffer);
		SND_OAL_Error();
	}

	qalDeleteSources(1, &music->source);
	SND_OAL_Error();
	qalDeleteBuffers(2, music->buffers);
	SND_OAL_Error();
}

/*=====================================================
 * Sound effects
 *=====================================================*/

/**
 * @brief Updates the listeners position
 */
void SND_OAL_UpdateListeners (vec3_t listener_origin)
{
	qalListenerfv(AL_POSITION, listener_origin);
/*	qalListenerfv(AL_VELOCITY, listenerVel);
	qalListenerfv(AL_ORIENTATION, listenerOri);*/
}

/**
 * @brief
 * @sa SND_OAL_LoadSound
 */
static qboolean SND_OAL_LoadFile (sfx_t* sfx, ALboolean loop)
{
	sfxcache_t *sc;

	sc = S_LoadSound(sfx);
	if (sc) {
		qalGenBuffers(1, &sfx->bufferNum);
		qalBufferData(sfx->bufferNum, sc->stereo, sc->data, sc->length, sc->speed);
		qalSourceQueueBuffers(sfx->source, 1, &sfx->bufferNum);
	} else
		return qfalse;

	/* Generate a single source, attach the buffer to it and start playing. */
	qalGenSources(1, &sfx->source);
	qalSourcei(sfx->source, AL_BUFFER, sfx->bufferNum);

	/* Normally nothing should go wrong above, but one never knows... */
	SND_OAL_Error();
	return qtrue;
}

/**
 * @brief Loads a sound via openAL
 * @sa SND_OAL_LoadFile
 */
qboolean SND_OAL_LoadSound (sfx_t* sfx, qboolean looping)
{
	if (!sfx)
		return qfalse;

	/* load our sound */
	if (!SND_OAL_LoadFile(sfx, looping)) {
		Com_Printf("SND_OAL_LoadSound: %s\n", sfx->name);
		return qfalse;
	}

	/* set the pitch */
	qalSourcef(sfx->source, AL_PITCH, 1.0f);
	/* set the gain */
	qalSourcef(sfx->source, AL_GAIN, 1.0f);
	if (looping) {
		/* set looping to true */
		qalSourcei(sfx->source, AL_LOOPING, AL_TRUE);
	} else
		qalSourcei(sfx->source, AL_LOOPING, AL_FALSE);

	return qtrue;
}

/**
 * @brief
 */
void SND_OAL_PlaySound (sfx_t* sfx, vec3_t origin)
{
	if (!openal_active)
		return;

	qalSourcefv(sfx->source, AL_POSITION, origin);
	/*qalSourcefv(sfx->source, AL_VELOCITY, sourceVel);
	qalSourcefv(sfx->source, AL_DIRECTION, sourceOri);*/
	qalSourcePlay(sfx->source);
}

/**
 * @brief
 */
void SND_OAL_StopSound (void)
{
	int i;

	/* stop all the channels */
	qalSourceStopv(snd_openal_numChannels, sourceNames);
	SND_OAL_Error();

	/* mark all channels as free */
	for (i = 0; i < snd_openal_numChannels; i++) {
		qalSourcei(sourceNames[i], AL_BUFFER, AL_NONE);
		SND_OAL_Error();
	}
}

/**
 * @brief
 */
void SND_OAL_DestroySound (void)
{
	if (!openal_active)
		return;
}

#endif /* HAVE_OPENAL */
