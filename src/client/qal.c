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

qboolean	openal_active;

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
ALvoid			(ALAPIENTRY * qalHint)(ALenum target, ALenum mode);
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
	qalHint						= NULL;
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
 * @brief
 * @sa QAL_Init
 */
void QAL_Link(void)
{
	if (openal_active) {
		Com_Printf("OpenAL already active\n");
		return;
	}
	qalcOpenDevice				= GPA("alcOpenDevice");
	qalcCloseDevice				= GPA("alcCloseDevice");
	qalcCreateContext			= GPA("alcCreateContext");
	qalcDestroyContext			= GPA("alcDestroyContext");
	qalcMakeContextCurrent		= GPA("alcMakeContextCurrent");
	qalcProcessContext			= GPA("alcProcessContext");
	qalcSuspendContext			= GPA("alcSuspendContext");
	qalcGetCurrentContext		= GPA("alcGetCurrentContext");
	qalcGetContextsDevice		= GPA("alcGetContextsDevice");
	qalcGetString				= GPA("alcGetString");
	qalcGetIntegerv				= GPA("alcGetIntegerv");
	qalcGetError				= GPA("alcGetError");
	qalcIsExtensionPresent		= GPA("alcIsExtensionPresent");
	qalcGetProcAddress			= GPA("alcGetProcAddress");
	qalcGetEnumValue			= GPA("alcGetEnumValue");

	qalBufferData				= GPA("alBufferData");
	qalDeleteBuffers			= GPA("alDeleteBuffers");
	qalDeleteSources			= GPA("alDeleteSources");
	qalDisable					= GPA("alDisable");
	qalDistanceModel			= GPA("alDistanceModel");
	qalDopplerFactor			= GPA("alDopplerFactor");
	qalDopplerVelocity			= GPA("alDopplerVelocity");
	qalEnable					= GPA("alEnable");
	qalGenBuffers				= GPA("alGenBuffers");
	qalGenSources				= GPA("alGenSources");
	qalGetBoolean				= GPA("alGetBoolean");
	qalGetBooleanv				= GPA("alGetBooleanv");
	qalGetBufferf				= GPA("alGetBufferf");
	qalGetBufferi				= GPA("alGetBufferi");
	qalGetDouble				= GPA("alGetDouble");
	qalGetDoublev				= GPA("alGetDoublev");
	qalGetEnumValue				= GPA("alGetEnumValue");
	qalGetError					= GPA("alGetError");
	qalGetFloat					= GPA("alGetFloat");
	qalGetFloatv				= GPA("alGetFloatv");
	qalGetInteger				= GPA("alGetInteger");
	qalGetIntegerv				= GPA("alGetIntegerv");
	qalGetListener3f			= GPA("alGetListener3f");
	qalGetListenerf				= GPA("alGetListenerf");
	qalGetListenerfv			= GPA("alGetListenerfv");
	qalGetListeneri				= GPA("alGetListeneri");
	qalGetProcAddress			= GPA("alGetProcAddress");
	qalGetSource3f				= GPA("alGetSource3f");
	qalGetSourcef				= GPA("alGetSourcef");
	qalGetSourcefv				= GPA("alGetSourcefv");
	qalGetSourcei				= GPA("alGetSourcei");
	qalGetString				= GPA("alGetString");
	qalHint						= GPA("alHint");
	qalIsBuffer					= GPA("alIsBuffer");
	qalIsEnabled				= GPA("alIsEnabled");
	qalIsExtensionPresent		= GPA("alIsExtensionPresent");
	qalIsSource					= GPA("alIsSource");
	qalListener3f				= GPA("alListener3f");
	qalListenerf				= GPA("alListenerf");
	qalListenerfv				= GPA("alListenerfv");
	qalListeneri				= GPA("alListeneri");
	qalSource3f					= GPA("alSource3f");
	qalSourcef					= GPA("alSourcef");
	qalSourcefv					= GPA("alSourcefv");
	qalSourcei					= GPA("alSourcei");
	qalSourcePause				= GPA("alSourcePause");
	qalSourcePausev				= GPA("alSourcePausev");
	qalSourcePlay				= GPA("alSourcePlay");
	qalSourcePlayv				= GPA("alSourcePlayv");
	qalSourceQueueBuffers		= GPA("alSourceQueueBuffers");
	qalSourceRewind				= GPA("alSourceRewind");
	qalSourceRewindv			= GPA("alSourceRewindv");
	qalSourceStop				= GPA("alSourceStop");
	qalSourceStopv				= GPA("alSourceStopv");
	qalSourceUnqueueBuffers		= GPA("alSourceUnqueueBuffers");
}

