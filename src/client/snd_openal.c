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

static char* alBuffer;
static ALenum alFormatBuffer;
static ALsizei alFreqBuffer;
static long alBufferLen;
static ALboolean alLoop;
static unsigned int alSource;
static unsigned int alSampleSet;

/**
 * @brief Init function that should be called after system implementation QAL_Init was called
 * @sa QAL_Init
 */
qboolean SND_OAL_Init (char* device)
{
	if ((oalState.device = qalcOpenDevice((ALubyte*)device) == NULL) {
		Com_Printf("OpenAL device (%s) could not be opened\n", device);
		return qfalse;
	}
	if ((oalState.context = qalcCreateContext(oalState.device, NULL)) == NULL) {
		/* TODO: close device here, too */
		return qfalse;
	}
	qalcMakeContextCurrent(oalState.context);
	/* clear error code */
	qalGetError();
	return qtrue;
}

/**
 * @brief
 * @sa SND_OAL_LoadSound
 */
qboolean SND_OAL_LoadWAV (char* filename)
{
	qalutLoadWAVFile(fname,&alFormatBuffer, (void **) &alBuffer,(unsigned int *)&alBufferLen, &alFreqBuffer, &loop);

	qalGenSources(1, &alSource);
	qalGenBuffers(1, &alSampleSet);
	qalBufferData(alSampleSet, alFormatBuffer, alBuffer, alBufferLen, alFreqBuffer);
	qalSourcei(alSource, AL_BUFFER, alSampleSet);

	qalutUnloadWAV(alFormatBuffer, alBuffer, alBufferLen, alFreqBuffer);
	return qtrue;
}

/**
 * @brief Loads a sound via openAL
 * @sa SND_OAL_LoadWAV
 * @sa SND_OAL_LoadOGG
 */
qboolean SND_OAL_LoadSound (char* filename, qboolean looping)
{
	/* load our sound */
	ALboolean loop;
	loop = looping;

	SND_OAL_LoadWAV(filename);

	/* set the pitch */
	qalSourcef(alSource,AL_PITCH,1.0f);
	/* set the gain */
	qalSourcef(alSource,AL_GAIN,1.0f);
	/* set looping to true */
	qalSourcei(alSource,AL_LOOPING,AL_TRUE);
}

/**
 * @brief
 */
void PlaySound (void)
{
	qalSourcePlay(alSource);
}

/**
 * @brief
 */
void StopSound (void)
{
	qalSourceStop(alSource);
}

/**
 * @brief
 */
void DestroySound (void)
{
	qalDeleteSources(1,&alSource);
	qalDeleteBuffers(1,&alSampleSet);
}
