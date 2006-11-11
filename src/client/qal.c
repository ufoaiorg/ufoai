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

ALCOPENDEVICE				qalcOpenDevice;
ALCCLOSEDEVICE				qalcCloseDevice;
ALCCREATECONTEXT			qalcCreateContext;
ALCDESTROYCONTEXT			qalcDestroyContext;
ALCMAKECONTEXTCURRENT		qalcMakeContextCurrent;
ALCPROCESSCONTEXT			qalcProcessContext;
ALCSUSPENDCONTEXT			qalcSuspendContext;
ALCGETCURRENTCONTEXT		qalcGetCurrentContext;
ALCGETCONTEXTSDEVICE		qalcGetContextsDevice;
ALCGETSTRING				qalcGetString;
ALCGETINTEGERV				qalcGetIntegerv;
ALCGETERROR					qalcGetError;
ALCISEXTENSIONPRESENT		qalcIsExtensionPresent;
ALCGETPROCADDRESS			qalcGetProcAddress;
ALCGETENUMVALUE				qalcGetEnumValue;

ALBUFFERDATA				qalBufferData;
ALDELETEBUFFERS				qalDeleteBuffers;
ALDELETESOURCES				qalDeleteSources;
ALDISABLE					qalDisable;
ALDISTANCEMODEL				qalDistanceModel;
ALDOPPLERFACTOR				qalDopplerFactor;
ALDOPPLERVELOCITY			qalDopplerVelocity;
ALENABLE					qalEnable;
ALGENBUFFERS				qalGenBuffers;
ALGENSOURCES				qalGenSources;
ALGETBOOLEAN				qalGetBoolean;
ALGETBOOLEANV				qalGetBooleanv;
ALGETBUFFERF				qalGetBufferf;
ALGETBUFFERI				qalGetBufferi;
ALGETDOUBLE					qalGetDouble;
ALGETDOUBLEV				qalGetDoublev;
ALGETENUMVALUE				qalGetEnumValue;
ALGETERROR					qalGetError;
ALGETFLOAT					qalGetFloat;
ALGETFLOATV					qalGetFloatv;
ALGETINTEGER				qalGetInteger;
ALGETINTEGERV				qalGetIntegerv;
ALGETLISTENER3F				qalGetListener3f;
ALGETLISTENERF				qalGetListenerf;
ALGETLISTENERFV				qalGetListenerfv;
ALGETLISTENERI				qalGetListeneri;
ALGETPROCADDRESS			qalGetProcAddress;
ALGETSOURCE3F				qalGetSource3f;
ALGETSOURCEF				qalGetSourcef;
ALGETSOURCEFV				qalGetSourcefv;
ALGETSOURCEI				qalGetSourcei;
ALGETSTRING					qalGetString;
ALHINT						qalHint;
ALISBUFFER					qalIsBuffer;
ALISENABLED					qalIsEnabled;
ALISEXTENSIONPRESENT		qalIsExtensionPresent;
ALISSOURCE					qalIsSource;
ALLISTENER3F				qalListener3f;
ALLISTENERF					qalListenerf;
ALLISTENERFV				qalListenerfv;
ALLISTENERI					qalListeneri;
ALSOURCE3F					qalSource3f;
ALSOURCEF					qalSourcef;
ALSOURCEFV					qalSourcefv;
ALSOURCEI					qalSourcei;
ALSOURCEPAUSE				qalSourcePause;
ALSOURCEPAUSEV				qalSourcePausev;
ALSOURCEPLAY				qalSourcePlay;
ALSOURCEPLAYV				qalSourcePlayv;
ALSOURCEQUEUEBUFFERS		qalSourceQueueBuffers;
ALSOURCEREWIND				qalSourceRewind;
ALSOURCEREWINDV				qalSourceRewindv;
ALSOURCESTOP				qalSourceStop;
ALSOURCESTOPV				qalSourceStopv;
ALSOURCEUNQUEUEBUFFERS		qalSourceUnqueueBuffers;

ALEAXSET					qalEAXSet;
ALEAXGET					qalEAXGet;

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

	qalEAXSet					= NULL;
	qalEAXGet					= NULL;
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
	qalcOpenDevice				= (ALCOPENDEVICE)GPA("alcOpenDevice");
	qalcCloseDevice				= (ALCCLOSEDEVICE)GPA("alcCloseDevice");
	qalcCreateContext			= (ALCCREATECONTEXT)GPA("alcCreateContext");
	qalcDestroyContext			= (ALCDESTROYCONTEXT)GPA("alcDestroyContext");
	qalcMakeContextCurrent		= (ALCMAKECONTEXTCURRENT)GPA("alcMakeContextCurrent");
	qalcProcessContext			= (ALCPROCESSCONTEXT)GPA("alcProcessContext");
	qalcSuspendContext			= (ALCSUSPENDCONTEXT)GPA("alcSuspendContext");
	qalcGetCurrentContext		= (ALCGETCURRENTCONTEXT)GPA("alcGetCurrentContext");
	qalcGetContextsDevice		= (ALCGETCONTEXTSDEVICE)GPA("alcGetContextsDevice");
	qalcGetString				= (ALCGETSTRING)GPA("alcGetString");
	qalcGetIntegerv				= (ALCGETINTEGERV)GPA("alcGetIntegerv");
	qalcGetError				= (ALCGETERROR)GPA("alcGetError");
	qalcIsExtensionPresent		= (ALCISEXTENSIONPRESENT)GPA("alcIsExtensionPresent");
	qalcGetProcAddress			= (ALCGETPROCADDRESS)GPA("alcGetProcAddress");
	qalcGetEnumValue			= (ALCGETENUMVALUE)GPA("alcGetEnumValue");

	qalBufferData				= (ALBUFFERDATA)GPA("alBufferData");
	qalDeleteBuffers			= (ALDELETEBUFFERS)GPA("alDeleteBuffers");
	qalDeleteSources			= (ALDELETESOURCES)GPA("alDeleteSources");
	qalDisable					= (ALDISABLE)GPA("alDisable");
	qalDistanceModel			= (ALDISTANCEMODEL)GPA("alDistanceModel");
	qalDopplerFactor			= (ALDOPPLERFACTOR)GPA("alDopplerFactor");
	qalDopplerVelocity			= (ALDOPPLERVELOCITY)GPA("alDopplerVelocity");
	qalEnable					= (ALENABLE)GPA("alEnable");
	qalGenBuffers				= (ALGENBUFFERS)GPA("alGenBuffers");
	qalGenSources				= (ALGENSOURCES)GPA("alGenSources");
	qalGetBoolean				= (ALGETBOOLEAN)GPA("alGetBoolean");
	qalGetBooleanv				= (ALGETBOOLEANV)GPA("alGetBooleanv");
	qalGetBufferf				= (ALGETBUFFERF)GPA("alGetBufferf");
	qalGetBufferi				= (ALGETBUFFERI)GPA("alGetBufferi");
	qalGetDouble				= (ALGETDOUBLE)GPA("alGetDouble");
	qalGetDoublev				= (ALGETDOUBLEV)GPA("alGetDoublev");
	qalGetEnumValue				= (ALGETENUMVALUE)GPA("alGetEnumValue");
	qalGetError					= (ALGETERROR)GPA("alGetError");
	qalGetFloat					= (ALGETFLOAT)GPA("alGetFloat");
	qalGetFloatv				= (ALGETFLOATV)GPA("alGetFloatv");
	qalGetInteger				= (ALGETINTEGER)GPA("alGetInteger");
	qalGetIntegerv				= (ALGETINTEGERV)GPA("alGetIntegerv");
	qalGetListener3f			= (ALGETLISTENER3F)GPA("alGetListener3f");
	qalGetListenerf				= (ALGETLISTENERF)GPA("alGetListenerf");
	qalGetListenerfv			= (ALGETLISTENERFV)GPA("alGetListenerfv");
	qalGetListeneri				= (ALGETLISTENERI)GPA("alGetListeneri");
	qalGetProcAddress			= (ALGETPROCADDRESS)GPA("alGetProcAddress");
	qalGetSource3f				= (ALGETSOURCE3F)GPA("alGetSource3f");
	qalGetSourcef				= (ALGETSOURCEF)GPA("alGetSourcef");
	qalGetSourcefv				= (ALGETSOURCEFV)GPA("alGetSourcefv");
	qalGetSourcei				= (ALGETSOURCEI)GPA("alGetSourcei");
	qalGetString				= (ALGETSTRING)GPA("alGetString");
	qalHint						= (ALHINT)GPA("alHint");
	qalIsBuffer					= (ALISBUFFER)GPA("alIsBuffer");
	qalIsEnabled				= (ALISENABLED)GPA("alIsEnabled");
	qalIsExtensionPresent		= (ALISEXTENSIONPRESENT)GPA("alIsExtensionPresent");
	qalIsSource					= (ALISSOURCE)GPA("alIsSource");
	qalListener3f				= (ALLISTENER3F)GPA("alListener3f");
	qalListenerf				= (ALLISTENERF)GPA("alListenerf");
	qalListenerfv				= (ALLISTENERFV)GPA("alListenerfv");
	qalListeneri				= (ALLISTENERI)GPA("alListeneri");
	qalSource3f					= (ALSOURCE3F)GPA("alSource3f");
	qalSourcef					= (ALSOURCEF)GPA("alSourcef");
	qalSourcefv					= (ALSOURCEFV)GPA("alSourcefv");
	qalSourcei					= (ALSOURCEI)GPA("alSourcei");
	qalSourcePause				= (ALSOURCEPAUSE)GPA("alSourcePause");
	qalSourcePausev				= (ALSOURCEPAUSEV)GPA("alSourcePausev");
	qalSourcePlay				= (ALSOURCEPLAY)GPA("alSourcePlay");
	qalSourcePlayv				= (ALSOURCEPLAYV)GPA("alSourcePlayv");
	qalSourceQueueBuffers		= (ALSOURCEQUEUEBUFFERS)GPA("alSourceQueueBuffers");
	qalSourceRewind				= (ALSOURCEREWIND)GPA("alSourceRewind");
	qalSourceRewindv			= (ALSOURCEREWINDV)GPA("alSourceRewindv");
	qalSourceStop				= (ALSOURCESTOP)GPA("alSourceStop");
	qalSourceStopv				= (ALSOURCESTOPV)GPA("alSourceStopv");
	qalSourceUnqueueBuffers		= (ALSOURCEUNQUEUEBUFFERS)GPA("alSourceUnqueueBuffers");

	qalEAXGet					= NULL;
	qalEAXSet					= NULL;
}

