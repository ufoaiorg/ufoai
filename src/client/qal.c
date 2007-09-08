/**
 * @file qal.c
 * @brief binding of AL to QAL function pointers
 */

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

#ifdef HAVE_OPENAL

#include "../client/client.h"
#include "../client/snd_loc.h"

qboolean	openal_active = qfalse;

ALCdevice *		(ALCAPIENTRY * qalcOpenDevice)(ALCubyte *deviceName);
ALCvoid			(ALCAPIENTRY * qalcCloseDevice)(ALCdevice *device);
ALCcontext *	(ALCAPIENTRY * qalcCreateContext)(ALCdevice *device, ALCint *attrList);
ALCvoid			(ALCAPIENTRY * qalcDestroyContext)(ALCcontext *context);
ALCboolean		(ALCAPIENTRY * qalcMakeContextCurrent)(ALCcontext *context);
ALCvoid			(ALCAPIENTRY * qalcProcessContext)(ALCcontext *context);
ALCvoid			(ALCAPIENTRY * qalcSuspendContext)(ALCcontext *context);
ALCcontext *	(ALCAPIENTRY * qalcGetCurrentContext)(ALCvoid);
ALCdevice *		(ALCAPIENTRY * qalcGetContextsDevice)(ALCcontext *context);
ALCubyte *		(ALCAPIENTRY * qalcGetString)(ALCdevice *device, ALCenum param);
ALCvoid			(ALCAPIENTRY * qalcGetIntegerv)(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *data);
ALCenum			(ALCAPIENTRY * qalcGetError)(ALCdevice *device);
ALCboolean		(ALCAPIENTRY * qalcIsExtensionPresent)(ALCdevice *device, ALCubyte *extName);
ALCvoid *		(ALCAPIENTRY * qalcGetProcAddress)(ALCdevice *device, ALCubyte *funcName);
ALCenum			(ALCAPIENTRY * qalcGetEnumValue)(ALCdevice *device, ALCubyte *enumName);

ALvoid			(ALAPIENTRY * qalBufferData)(ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq);
ALvoid			(ALAPIENTRY * qalDeleteBuffers)(ALsizei n, ALuint *buffers);
ALvoid			(ALAPIENTRY * qalDeleteSources)(ALsizei n, ALuint *sources);
ALvoid			(ALAPIENTRY * qalDisable)(ALenum capability);
ALvoid			(ALAPIENTRY * qalDistanceModel)(ALenum value);
ALvoid			(ALAPIENTRY * qalDopplerFactor)(ALfloat value);
ALvoid			(ALAPIENTRY * qalDopplerVelocity)(ALfloat value);
ALvoid			(ALAPIENTRY * qalEnable)(ALenum capability);
ALvoid			(ALAPIENTRY * qalGenBuffers)(ALsizei n, ALuint *buffers);
ALvoid			(ALAPIENTRY * qalGenSources)(ALsizei n, ALuint *sources);
ALboolean		(ALAPIENTRY * qalGetBoolean)(ALenum param);
ALvoid			(ALAPIENTRY * qalGetBooleanv)(ALenum param, ALboolean *data);
ALvoid			(ALAPIENTRY * qalGetBufferf)(ALuint buffer, ALenum param, ALfloat *value);
ALvoid			(ALAPIENTRY * qalGetBufferi)(ALuint buffer, ALenum param, ALint *value);
ALdouble		(ALAPIENTRY * qalGetDouble)(ALenum param);
ALvoid			(ALAPIENTRY * qalGetDoublev)(ALenum param, ALdouble *data);
ALenum			(ALAPIENTRY * qalGetEnumValue)(ALubyte *enumName);
ALenum			(ALAPIENTRY * qalGetError)(ALvoid);
ALfloat			(ALAPIENTRY * qalGetFloat)(ALenum param);
ALvoid			(ALAPIENTRY * qalGetFloatv)(ALenum param, ALfloat *data);
ALint			(ALAPIENTRY * qalGetInteger)(ALenum param);
ALvoid			(ALAPIENTRY * qalGetIntegerv)(ALenum param, ALint *data);
ALvoid			(ALAPIENTRY * qalGetListener3f)(ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3);
ALvoid			(ALAPIENTRY * qalGetListenerf)(ALenum param, ALfloat *value);
ALvoid			(ALAPIENTRY * qalGetListenerfv)(ALenum param, ALfloat *values);
ALvoid			(ALAPIENTRY * qalGetListeneri)(ALenum param, ALint *value);
ALvoid *		(ALAPIENTRY * qalGetProcAddress)(ALubyte *funcName);
ALvoid			(ALAPIENTRY * qalGetSource3f)(ALuint source, ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3);
ALvoid			(ALAPIENTRY * qalGetSourcef)(ALuint source, ALenum param, ALfloat *value);
ALvoid			(ALAPIENTRY * qalGetSourcefv)(ALuint source, ALenum param, ALfloat *values);
ALvoid			(ALAPIENTRY * qalGetSourcei)(ALuint source, ALenum param, ALint *value);
ALubyte *		(ALAPIENTRY * qalGetString)(ALenum param);
/*ALvoid			(ALAPIENTRY * qalHint)(ALenum target, ALenum * mode);*/
ALboolean		(ALAPIENTRY * qalIsBuffer)(ALuint buffer);
ALboolean		(ALAPIENTRY * qalIsEnabled)(ALenum capability);
ALboolean		(ALAPIENTRY * qalIsExtensionPresent)(ALubyte *extName);
ALboolean		(ALAPIENTRY * qalIsSource)(ALuint source);
ALvoid			(ALAPIENTRY * qalListener3f)(ALenum param, ALfloat v1, ALfloat v2, ALfloat v3);
ALvoid			(ALAPIENTRY * qalListenerf)(ALenum param, ALfloat value);
ALvoid			(ALAPIENTRY * qalListenerfv)(ALenum param, ALfloat *values);
ALvoid			(ALAPIENTRY * qalListeneri)(ALenum param, ALint value);
ALvoid			(ALAPIENTRY * qalSource3f)(ALuint source, ALenum param, ALfloat v1, ALfloat v2, ALfloat v3);
ALvoid			(ALAPIENTRY * qalSourcef)(ALuint source, ALenum param, ALfloat value);
ALvoid			(ALAPIENTRY * qalSourcefv)(ALuint source, ALenum param, ALfloat *values);
ALvoid			(ALAPIENTRY * qalSourcei)(ALuint source, ALenum param, ALint value);
ALvoid			(ALAPIENTRY * qalSourcePause)(ALuint source);
ALvoid			(ALAPIENTRY * qalSourcePausev)(ALsizei n, ALuint *sources);
ALvoid			(ALAPIENTRY * qalSourcePlay)(ALuint source);
ALvoid			(ALAPIENTRY * qalSourcePlayv)(ALsizei n, ALuint *sources);
ALvoid			(ALAPIENTRY * qalSourceQueueBuffers)(ALuint source, ALsizei n, ALuint *buffers);
ALvoid			(ALAPIENTRY * qalSourceRewind)(ALuint source);
ALvoid			(ALAPIENTRY * qalSourceRewindv)(ALsizei n, ALuint *sources);
ALvoid			(ALAPIENTRY * qalSourceStop)(ALuint source);
ALvoid			(ALAPIENTRY * qalSourceStopv)(ALsizei n, ALuint *sources);
ALvoid			(ALAPIENTRY * qalSourceUnqueueBuffers)(ALuint source, ALsizei n, ALuint *buffers);

static qboolean alinit_fail = qfalse;

/**
 * @brief Loads symbols from openal lib
 */
static void *QAL_GetProcAddress (char *str)
{
	void *rv;

	rv = Sys_GetProcAddress(oalState.lib, str);
	if (!rv) {
		Com_Printf("Can't load symbol %s\n", str);
		alinit_fail = qtrue;
		return NULL;
	}
	return rv;
}


/**
 * @brief Unloads the specified DLL then nulls out all the proc pointers
 * @sa QAL_Init
 */
static void QAL_Shutdown (void)
{
	if (oalState.lib)
		Sys_FreeLibrary(oalState.lib);

	oalState.lib = NULL;

	/* general pointers */
	QAL_Unlink();
}

/**
 * @brief Set all openAL bindings to NULL
 * @sa QAL_Init
 */
void QAL_Unlink (void)
{
	if (openal_active) {
		Com_Printf("call QAL_Shutdown first\n");
		return;
	}

	qalcOpenDevice				= NULL;
	qalcCloseDevice				= NULL;
	qalcCreateContext			= NULL;
	qalcDestroyContext			= NULL;
	qalcMakeContextCurrent		= NULL;
	qalcProcessContext			= NULL;
	qalcSuspendContext			= NULL;
	qalcGetCurrentContext		= NULL;
	qalcGetContextsDevice		= NULL;
	qalcGetString				= NULL;
	qalcGetIntegerv				= NULL;
	qalcGetError				= NULL;
	qalcIsExtensionPresent		= NULL;
	qalcGetProcAddress			= NULL;
	qalcGetEnumValue			= NULL;

	qalBufferData				= NULL;
	qalDeleteBuffers			= NULL;
	qalDeleteSources			= NULL;
	qalDisable					= NULL;
	qalDistanceModel			= NULL;
	qalDopplerFactor			= NULL;
	qalDopplerVelocity			= NULL;
	qalEnable					= NULL;
	qalGenBuffers				= NULL;
	qalGenSources				= NULL;
	qalGetBoolean				= NULL;
	qalGetBooleanv				= NULL;
	qalGetBufferf				= NULL;
	qalGetBufferi				= NULL;
	qalGetDouble				= NULL;
	qalGetDoublev				= NULL;
	qalGetEnumValue				= NULL;
	qalGetError					= NULL;
	qalGetFloat					= NULL;
	qalGetFloatv				= NULL;
	qalGetInteger				= NULL;
	qalGetIntegerv				= NULL;
	qalGetListener3f			= NULL;
	qalGetListenerf				= NULL;
	qalGetListenerfv			= NULL;
	qalGetListeneri				= NULL;
	qalGetProcAddress			= NULL;
	qalGetSource3f				= NULL;
	qalGetSourcef				= NULL;
	qalGetSourcefv				= NULL;
	qalGetSourcei				= NULL;
	qalGetString				= NULL;
	/*	qalHint						= NULL;*/
	qalIsBuffer					= NULL;
	qalIsEnabled				= NULL;
	qalIsExtensionPresent		= NULL;
	qalIsSource					= NULL;
	qalListener3f				= NULL;
	qalListenerf				= NULL;
	qalListenerfv				= NULL;
	qalListeneri				= NULL;
	qalSource3f					= NULL;
	qalSourcef					= NULL;
	qalSourcefv					= NULL;
	qalSourcei					= NULL;
	qalSourcePause				= NULL;
	qalSourcePausev				= NULL;
	qalSourcePlay				= NULL;
	qalSourcePlayv				= NULL;
	qalSourceQueueBuffers		= NULL;
	qalSourceRewind				= NULL;
	qalSourceRewindv			= NULL;
	qalSourceStop				= NULL;
	qalSourceStopv				= NULL;
	qalSourceUnqueueBuffers		= NULL;
}

/**
 * @brief Bind openAL function to function pointers
 * @sa QAL_Init
 */
qboolean QAL_Link(void)
{
	if (openal_active) {
		Com_Printf("OpenAL already active\n");
		return qtrue;
	}
	qalcOpenDevice				= QAL_GetProcAddress("alcOpenDevice");
	qalcCloseDevice				= QAL_GetProcAddress("alcCloseDevice");
	qalcCreateContext			= QAL_GetProcAddress("alcCreateContext");
	qalcDestroyContext			= QAL_GetProcAddress("alcDestroyContext");
	qalcMakeContextCurrent		= QAL_GetProcAddress("alcMakeContextCurrent");
	qalcProcessContext			= QAL_GetProcAddress("alcProcessContext");
	qalcSuspendContext			= QAL_GetProcAddress("alcSuspendContext");
	qalcGetCurrentContext		= QAL_GetProcAddress("alcGetCurrentContext");
	qalcGetContextsDevice		= QAL_GetProcAddress("alcGetContextsDevice");
	qalcGetString				= QAL_GetProcAddress("alcGetString");
	qalcGetIntegerv				= QAL_GetProcAddress("alcGetIntegerv");
	qalcGetError				= QAL_GetProcAddress("alcGetError");
	qalcIsExtensionPresent		= QAL_GetProcAddress("alcIsExtensionPresent");
	qalcGetProcAddress			= QAL_GetProcAddress("alcGetProcAddress");
	qalcGetEnumValue			= QAL_GetProcAddress("alcGetEnumValue");

	qalBufferData				= QAL_GetProcAddress("alBufferData");
	qalDeleteBuffers			= QAL_GetProcAddress("alDeleteBuffers");
	qalDeleteSources			= QAL_GetProcAddress("alDeleteSources");
	qalDisable					= QAL_GetProcAddress("alDisable");
	qalDistanceModel			= QAL_GetProcAddress("alDistanceModel");
	qalDopplerFactor			= QAL_GetProcAddress("alDopplerFactor");
	qalDopplerVelocity			= QAL_GetProcAddress("alDopplerVelocity");
	qalEnable					= QAL_GetProcAddress("alEnable");
	qalGenBuffers				= QAL_GetProcAddress("alGenBuffers");
	qalGenSources				= QAL_GetProcAddress("alGenSources");
	qalGetBoolean				= QAL_GetProcAddress("alGetBoolean");
	qalGetBooleanv				= QAL_GetProcAddress("alGetBooleanv");
	qalGetBufferf				= QAL_GetProcAddress("alGetBufferf");
	qalGetBufferi				= QAL_GetProcAddress("alGetBufferi");
	qalGetDouble				= QAL_GetProcAddress("alGetDouble");
	qalGetDoublev				= QAL_GetProcAddress("alGetDoublev");
	qalGetEnumValue				= QAL_GetProcAddress("alGetEnumValue");
	qalGetError					= QAL_GetProcAddress("alGetError");
	qalGetFloat					= QAL_GetProcAddress("alGetFloat");
	qalGetFloatv				= QAL_GetProcAddress("alGetFloatv");
	qalGetInteger				= QAL_GetProcAddress("alGetInteger");
	qalGetIntegerv				= QAL_GetProcAddress("alGetIntegerv");
	qalGetListener3f			= QAL_GetProcAddress("alGetListener3f");
	qalGetListenerf				= QAL_GetProcAddress("alGetListenerf");
	qalGetListenerfv			= QAL_GetProcAddress("alGetListenerfv");
	qalGetListeneri				= QAL_GetProcAddress("alGetListeneri");
	qalGetProcAddress			= QAL_GetProcAddress("alGetProcAddress");
	qalGetSource3f				= QAL_GetProcAddress("alGetSource3f");
	qalGetSourcef				= QAL_GetProcAddress("alGetSourcef");
	qalGetSourcefv				= QAL_GetProcAddress("alGetSourcefv");
	qalGetSourcei				= QAL_GetProcAddress("alGetSourcei");
	qalGetString				= QAL_GetProcAddress("alGetString");
	/*qalHint						= QAL_GetProcAddress("alHint");*/
	qalIsBuffer					= QAL_GetProcAddress("alIsBuffer");
	qalIsEnabled				= QAL_GetProcAddress("alIsEnabled");
	qalIsExtensionPresent		= QAL_GetProcAddress("alIsExtensionPresent");
	qalIsSource					= QAL_GetProcAddress("alIsSource");
	qalListener3f				= QAL_GetProcAddress("alListener3f");
	qalListenerf				= QAL_GetProcAddress("alListenerf");
	qalListenerfv				= QAL_GetProcAddress("alListenerfv");
	qalListeneri				= QAL_GetProcAddress("alListeneri");
	qalSource3f					= QAL_GetProcAddress("alSource3f");
	qalSourcef					= QAL_GetProcAddress("alSourcef");
	qalSourcefv					= QAL_GetProcAddress("alSourcefv");
	qalSourcei					= QAL_GetProcAddress("alSourcei");
	qalSourcePause				= QAL_GetProcAddress("alSourcePause");
	qalSourcePausev				= QAL_GetProcAddress("alSourcePausev");
	qalSourcePlay				= QAL_GetProcAddress("alSourcePlay");
	qalSourcePlayv				= QAL_GetProcAddress("alSourcePlayv");
	qalSourceQueueBuffers		= QAL_GetProcAddress("alSourceQueueBuffers");
	qalSourceRewind				= QAL_GetProcAddress("alSourceRewind");
	qalSourceRewindv			= QAL_GetProcAddress("alSourceRewindv");
	qalSourceStop				= QAL_GetProcAddress("alSourceStop");
	qalSourceStopv				= QAL_GetProcAddress("alSourceStopv");
	qalSourceUnqueueBuffers		= QAL_GetProcAddress("alSourceUnqueueBuffers");

	if (alinit_fail) {
		QAL_Shutdown();
		Com_Printf("QAL_Link: One or more symbols not found\n");
		return qfalse;
	}

	return qtrue;
}

#endif /* HAVE_OPENAL */
