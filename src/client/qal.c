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

/* alut */
ALboolean		(ALAPIENTRY * qalutInit)(int *argcp, char **argv);
ALboolean		(ALAPIENTRY * qalutInitWithoutContext)(int *argcp, char **argv);
ALboolean		(ALAPIENTRY * qalutExit)(void);
ALenum			(ALAPIENTRY * qalutGetError)(void);
const char *	(ALAPIENTRY * qalutGetErrorString)(ALenum error);
ALuint			(ALAPIENTRY * qalutCreateBufferFromFile)(const char *fileName);
ALuint			(ALAPIENTRY * qalutCreateBufferFromFileImage)(const ALvoid *data, ALsizei length);
ALvoid *		(ALAPIENTRY * qalutLoadMemoryFromFile)(const char *fileName, ALenum *format, ALsizei *size, ALfloat *frequency);
ALvoid *		(ALAPIENTRY * qalutLoadMemoryFromFileImage)(const ALvoid *data, ALsizei length, ALenum *format, ALsizei *size, ALfloat *frequency);
const char *	(ALAPIENTRY * qalutGetMIMETypes)(ALenum loader);
ALuint			(ALAPIENTRY * qalutCreateBufferHelloWorld)(void);
ALuint			(ALAPIENTRY * qalutCreateBufferWaveform)(ALenum waveshape, ALfloat frequency, ALfloat phase, ALfloat duration);
ALint			(ALAPIENTRY * qalutGetMajorVersion)(void);
ALint			(ALAPIENTRY * qalutGetMinorVersion)(void);
ALboolean		(ALAPIENTRY * qalutSleep)(ALfloat duration);

static qboolean alinit_fail = qfalse;

/**
 * @brief Loads symbols from openal lib
 */
static void *GPAT(char *str)
{
	void *rv;

	rv = GPA(str);
	if (!rv) {
		Com_Printf( " Can't load symbol %s\n", str);
		alinit_fail = qtrue;
		return NULL;
	}
	return rv;
}

/**
 * @brief Loads symbols from alut lib
 */
static void *GPAT2(char *str)
{
	void *rv;

	rv = GPA2(str);
	if (!rv) {
		Com_Printf( " Can't load symbol %s\n", str);
		alinit_fail = qtrue;
		return NULL;
	}
	return rv;
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

	/* alut */
	qalutInit						= NULL;
	qalutInitWithoutContext			= NULL;
	qalutExit						= NULL;
	qalutGetError					= NULL;
	qalutGetErrorString				= NULL;
	qalutCreateBufferFromFile		= NULL;
	qalutCreateBufferFromFileImage	= NULL;
	qalutLoadMemoryFromFile			= NULL;
	qalutLoadMemoryFromFileImage	= NULL;
	qalutGetMIMETypes				= NULL;
	qalutCreateBufferHelloWorld		= NULL;
	qalutCreateBufferWaveform		= NULL;
	qalutGetMajorVersion			= NULL;
	qalutGetMinorVersion			= NULL;
	qalutSleep						= NULL;
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
	qalcOpenDevice				= GPAT("alcOpenDevice");
	qalcCloseDevice				= GPAT("alcCloseDevice");
	qalcCreateContext			= GPAT("alcCreateContext");
	qalcDestroyContext			= GPAT("alcDestroyContext");
	qalcMakeContextCurrent		= GPAT("alcMakeContextCurrent");
	qalcProcessContext			= GPAT("alcProcessContext");
	qalcSuspendContext			= GPAT("alcSuspendContext");
	qalcGetCurrentContext		= GPAT("alcGetCurrentContext");
	qalcGetContextsDevice		= GPAT("alcGetContextsDevice");
	qalcGetString				= GPAT("alcGetString");
	qalcGetIntegerv				= GPAT("alcGetIntegerv");
	qalcGetError				= GPAT("alcGetError");
	qalcIsExtensionPresent		= GPAT("alcIsExtensionPresent");
	qalcGetProcAddress			= GPAT("alcGetProcAddress");
	qalcGetEnumValue			= GPAT("alcGetEnumValue");

	qalBufferData				= GPAT("alBufferData");
	qalDeleteBuffers			= GPAT("alDeleteBuffers");
	qalDeleteSources			= GPAT("alDeleteSources");
	qalDisable					= GPAT("alDisable");
	qalDistanceModel			= GPAT("alDistanceModel");
	qalDopplerFactor			= GPAT("alDopplerFactor");
	qalDopplerVelocity			= GPAT("alDopplerVelocity");
	qalEnable					= GPAT("alEnable");
	qalGenBuffers				= GPAT("alGenBuffers");
	qalGenSources				= GPAT("alGenSources");
	qalGetBoolean				= GPAT("alGetBoolean");
	qalGetBooleanv				= GPAT("alGetBooleanv");
	qalGetBufferf				= GPAT("alGetBufferf");
	qalGetBufferi				= GPAT("alGetBufferi");
	qalGetDouble				= GPAT("alGetDouble");
	qalGetDoublev				= GPAT("alGetDoublev");
	qalGetEnumValue				= GPAT("alGetEnumValue");
	qalGetError					= GPAT("alGetError");
	qalGetFloat					= GPAT("alGetFloat");
	qalGetFloatv				= GPAT("alGetFloatv");
	qalGetInteger				= GPAT("alGetInteger");
	qalGetIntegerv				= GPAT("alGetIntegerv");
	qalGetListener3f			= GPAT("alGetListener3f");
	qalGetListenerf				= GPAT("alGetListenerf");
	qalGetListenerfv			= GPAT("alGetListenerfv");
	qalGetListeneri				= GPAT("alGetListeneri");
	qalGetProcAddress			= GPAT("alGetProcAddress");
	qalGetSource3f				= GPAT("alGetSource3f");
	qalGetSourcef				= GPAT("alGetSourcef");
	qalGetSourcefv				= GPAT("alGetSourcefv");
	qalGetSourcei				= GPAT("alGetSourcei");
	qalGetString				= GPAT("alGetString");
	/*qalHint						= GPAT("alHint");*/
	qalIsBuffer					= GPAT("alIsBuffer");
	qalIsEnabled				= GPAT("alIsEnabled");
	qalIsExtensionPresent		= GPAT("alIsExtensionPresent");
	qalIsSource					= GPAT("alIsSource");
	qalListener3f				= GPAT("alListener3f");
	qalListenerf				= GPAT("alListenerf");
	qalListenerfv				= GPAT("alListenerfv");
	qalListeneri				= GPAT("alListeneri");
	qalSource3f					= GPAT("alSource3f");
	qalSourcef					= GPAT("alSourcef");
	qalSourcefv					= GPAT("alSourcefv");
	qalSourcei					= GPAT("alSourcei");
	qalSourcePause				= GPAT("alSourcePause");
	qalSourcePausev				= GPAT("alSourcePausev");
	qalSourcePlay				= GPAT("alSourcePlay");
	qalSourcePlayv				= GPAT("alSourcePlayv");
	qalSourceQueueBuffers		= GPAT("alSourceQueueBuffers");
	qalSourceRewind				= GPAT("alSourceRewind");
	qalSourceRewindv			= GPAT("alSourceRewindv");
	qalSourceStop				= GPAT("alSourceStop");
	qalSourceStopv				= GPAT("alSourceStopv");
	qalSourceUnqueueBuffers		= GPAT("alSourceUnqueueBuffers");

	/* alut */
	qalutInit						= GPAT2("alutInit");
	qalutInitWithoutContext			= GPAT2("alutInitWithoutContext");
	qalutExit						= GPAT2("alutExit");
	qalutGetError					= GPAT2("alutGetError");
	qalutGetErrorString				= GPAT2("alutGetErrorString");
	qalutCreateBufferFromFile		= GPAT2("alutCreateBufferFromFile");
	qalutCreateBufferFromFileImage	= GPAT2("alutCreateBufferFromFileImage");
	qalutLoadMemoryFromFile			= GPAT2("alutLoadMemoryFromFile");
	qalutLoadMemoryFromFileImage	= GPAT2("alutLoadMemoryFromFileImage");
	qalutGetMIMETypes				= GPAT2("alutGetMIMETypes");
	qalutCreateBufferHelloWorld		= GPAT2("alutCreateBufferHelloWorld");
	qalutCreateBufferWaveform		= GPAT2("alutCreateBufferWaveform");
	qalutGetMajorVersion			= GPAT2("alutGetMajorVersion");
	qalutGetMinorVersion			= GPAT2("alutGetMinorVersion");
	qalutSleep						= GPAT2("alutSleep");

	if (alinit_fail) {
		QAL_Shutdown();
		Com_Printf("QAL_Link: One or more symbols not found\n");
		return qfalse;
	}

	return qtrue;
}

