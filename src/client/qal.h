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


#ifndef __QAL_H__
#define __QAL_H__

#ifdef _WIN32
#include "../win32/winquake.h"
#endif


#ifdef _WIN32
#include "../ports/win32/qal_win.h"
#endif
#ifdef __linux__
#include "../ports/linux/qal_linux.h"
#endif

qboolean	QAL_Init (const char *driver);
void		QAL_Shutdown (void);
void		QAL_Link(void);
void		QAL_Unlink(void);

extern qboolean					openal_active;

typedef ALCAPI ALCdevice *		(ALCAPIENTRY * ALCOPENDEVICE)(ALCubyte *deviceName);
typedef ALCAPI ALCvoid			(ALCAPIENTRY * ALCCLOSEDEVICE)(ALCdevice *device);
typedef ALCAPI ALCcontext *		(ALCAPIENTRY * ALCCREATECONTEXT)(ALCdevice *device, ALCint *attrList);
typedef ALCAPI ALCvoid			(ALCAPIENTRY * ALCDESTROYCONTEXT)(ALCcontext *context);
typedef ALCAPI ALCboolean		(ALCAPIENTRY * ALCMAKECONTEXTCURRENT)(ALCcontext *context);
typedef ALCAPI ALCvoid			(ALCAPIENTRY * ALCPROCESSCONTEXT)(ALCcontext *context);
typedef ALCAPI ALCvoid			(ALCAPIENTRY * ALCSUSPENDCONTEXT)(ALCcontext *context);
typedef ALCAPI ALCcontext *		(ALCAPIENTRY * ALCGETCURRENTCONTEXT)(ALCvoid);
typedef ALCAPI ALCdevice *		(ALCAPIENTRY * ALCGETCONTEXTSDEVICE)(ALCcontext *context);
typedef ALCAPI ALCubyte *		(ALCAPIENTRY * ALCGETSTRING)(ALCdevice *device, ALCenum param);
typedef ALCAPI ALCvoid			(ALCAPIENTRY * ALCGETINTEGERV)(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *data);
typedef ALCAPI ALCenum			(ALCAPIENTRY * ALCGETERROR)(ALCdevice *device);
typedef ALCAPI ALCboolean		(ALCAPIENTRY * ALCISEXTENSIONPRESENT)(ALCdevice *device, ALCubyte *extName);
typedef ALCAPI ALCvoid *		(ALCAPIENTRY * ALCGETPROCADDRESS)(ALCdevice *device, ALCubyte *funcName);
typedef ALCAPI ALCenum			(ALCAPIENTRY * ALCGETENUMVALUE)(ALCdevice *device, ALCubyte *enumName);

typedef ALAPI ALvoid			(ALAPIENTRY * ALBUFFERDATA)(ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq);
typedef ALAPI ALvoid			(ALAPIENTRY * ALDELETEBUFFERS)(ALsizei n, ALuint *buffers);
typedef ALAPI ALvoid			(ALAPIENTRY * ALDELETESOURCES)(ALsizei n, ALuint *sources);
typedef ALAPI ALvoid			(ALAPIENTRY * ALDISABLE)(ALenum capability);
typedef ALAPI ALvoid			(ALAPIENTRY * ALDISTANCEMODEL)(ALenum value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALDOPPLERFACTOR)(ALfloat value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALDOPPLERVELOCITY)(ALfloat value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALENABLE)(ALenum capability);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGENBUFFERS)(ALsizei n, ALuint *buffers);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGENSOURCES)(ALsizei n, ALuint *sources);
typedef ALAPI ALboolean			(ALAPIENTRY * ALGETBOOLEAN)(ALenum param);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETBOOLEANV)(ALenum param, ALboolean *data);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETBUFFERF)(ALuint buffer, ALenum param, ALfloat *value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETBUFFERI)(ALuint buffer, ALenum param, ALint *value);
typedef ALAPI ALdouble			(ALAPIENTRY * ALGETDOUBLE)(ALenum param);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETDOUBLEV)(ALenum param, ALdouble *data);
typedef ALAPI ALenum			(ALAPIENTRY * ALGETENUMVALUE)(ALubyte *enumName);
typedef ALAPI ALenum			(ALAPIENTRY * ALGETERROR)(ALvoid);
typedef ALAPI ALfloat			(ALAPIENTRY * ALGETFLOAT)(ALenum param);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETFLOATV)(ALenum param, ALfloat *data);
typedef ALAPI ALint				(ALAPIENTRY * ALGETINTEGER)(ALenum param);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETINTEGERV)(ALenum param, ALint *data);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETLISTENER3F)(ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETLISTENERF)(ALenum param, ALfloat *value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETLISTENERFV)(ALenum param, ALfloat *values);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETLISTENERI)(ALenum param, ALint *value);
typedef ALAPI ALvoid *			(ALAPIENTRY * ALGETPROCADDRESS)(ALubyte *funcName);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETSOURCE3F)(ALuint source, ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETSOURCEF)(ALuint source, ALenum param, ALfloat *value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETSOURCEFV)(ALuint source, ALenum param, ALfloat *values);
typedef ALAPI ALvoid			(ALAPIENTRY * ALGETSOURCEI)(ALuint source, ALenum param, ALint *value);
typedef ALAPI ALubyte *			(ALAPIENTRY * ALGETSTRING)(ALenum param);
typedef ALAPI ALvoid			(ALAPIENTRY * ALHINT)(ALenum target, ALenum mode);
typedef ALAPI ALboolean			(ALAPIENTRY * ALISBUFFER)(ALuint buffer);
typedef ALAPI ALboolean			(ALAPIENTRY * ALISENABLED)(ALenum capability);
typedef ALAPI ALboolean			(ALAPIENTRY * ALISEXTENSIONPRESENT)(ALubyte *extName);
typedef ALAPI ALboolean			(ALAPIENTRY * ALISSOURCE)(ALuint source);
typedef ALAPI ALvoid			(ALAPIENTRY * ALLISTENER3F)(ALenum param, ALfloat v1, ALfloat v2, ALfloat v3);
typedef ALAPI ALvoid			(ALAPIENTRY * ALLISTENERF)(ALenum param, ALfloat value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALLISTENERFV)(ALenum param, ALfloat *values);
typedef ALAPI ALvoid			(ALAPIENTRY * ALLISTENERI)(ALenum param, ALint value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCE3F)(ALuint source, ALenum param, ALfloat v1, ALfloat v2, ALfloat v3);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEF)(ALuint source, ALenum param, ALfloat value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEFV)(ALuint source, ALenum param, ALfloat *values);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEI)(ALuint source, ALenum param, ALint value);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEPAUSE)(ALuint source);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEPAUSEV)(ALsizei n, ALuint *sources);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEPLAY)(ALuint source);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEPLAYV)(ALsizei n, ALuint *sources);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEQUEUEBUFFERS)(ALuint source, ALsizei n, ALuint *buffers);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEREWIND)(ALuint source);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEREWINDV)(ALsizei n, ALuint *sources);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCESTOP)(ALuint source);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCESTOPV)(ALsizei n, ALuint *sources);
typedef ALAPI ALvoid			(ALAPIENTRY * ALSOURCEUNQUEUEBUFFERS)(ALuint source, ALsizei n, ALuint *buffers);

typedef ALAPI ALenum			(ALAPIENTRY * ALEAXSET)(const GUID*, ALuint, ALuint, ALvoid *, ALuint);
typedef ALAPI ALenum			(ALAPIENTRY * ALEAXGET)(const GUID*, ALuint, ALuint, ALvoid *, ALuint);

// =====================================================================

extern ALCOPENDEVICE			qalcOpenDevice;
extern ALCCLOSEDEVICE			qalcCloseDevice;
extern ALCCREATECONTEXT			qalcCreateContext;
extern ALCDESTROYCONTEXT		qalcDestroyContext;
extern ALCMAKECONTEXTCURRENT	qalcMakeContextCurrent;
extern ALCPROCESSCONTEXT		qalcProcessContext;
extern ALCSUSPENDCONTEXT		qalcSuspendContext;
extern ALCGETCURRENTCONTEXT		qalcGetCurrentContext;
extern ALCGETCONTEXTSDEVICE		qalcGetContextsDevice;
extern ALCGETSTRING				qalcGetString;
extern ALCGETINTEGERV			qalcGetIntegerv;
extern ALCGETERROR				qalcGetError;
extern ALCISEXTENSIONPRESENT	qalcIsExtensionPresent;
extern ALCGETPROCADDRESS		qalcGetProcAddress;
extern ALCGETENUMVALUE			qalcGetEnumValue;

extern ALBUFFERDATA				qalBufferData;
extern ALDELETEBUFFERS			qalDeleteBuffers;
extern ALDELETESOURCES			qalDeleteSources;
extern ALDISABLE				qalDisable;
extern ALDISTANCEMODEL			qalDistanceModel;
extern ALDOPPLERFACTOR			qalDopplerFactor;
extern ALDOPPLERVELOCITY		qalDopplerVelocity;
extern ALENABLE					qalEnable;
extern ALGENBUFFERS				qalGenBuffers;
extern ALGENSOURCES				qalGenSources;
extern ALGETBOOLEAN				qalGetBoolean;
extern ALGETBOOLEANV			qalGetBooleanv;
extern ALGETBUFFERF				qalGetBufferf;
extern ALGETBUFFERI				qalGetBufferi;
extern ALGETDOUBLE				qalGetDouble;
extern ALGETDOUBLEV				qalGetDoublev;
extern ALGETENUMVALUE			qalGetEnumValue;
extern ALGETERROR				qalGetError;
extern ALGETFLOAT				qalGetFloat;
extern ALGETFLOATV				qalGetFloatv;
extern ALGETINTEGER				qalGetInteger;
extern ALGETINTEGERV			qalGetIntegerv;
extern ALGETLISTENER3F			qalGetListener3f;
extern ALGETLISTENERF			qalGetListenerf;
extern ALGETLISTENERFV			qalGetListenerfv;
extern ALGETLISTENERI			qalGetListeneri;
extern ALGETPROCADDRESS			qalGetProcAddress;
extern ALGETSOURCE3F			qalGetSource3f;
extern ALGETSOURCEF				qalGetSourcef;
extern ALGETSOURCEFV			qalGetSourcefv;
extern ALGETSOURCEI				qalGetSourcei;
extern ALGETSTRING				qalGetString;
extern ALHINT 					qalHint;
extern ALISBUFFER				qalIsBuffer;
extern ALISENABLED				qalIsEnabled;
extern ALISEXTENSIONPRESENT		qalIsExtensionPresent;
extern ALISSOURCE				qalIsSource;
extern ALLISTENER3F				qalListener3f;
extern ALLISTENERF				qalListenerf;
extern ALLISTENERFV				qalListenerfv;
extern ALLISTENERI				qalListeneri;
extern ALSOURCE3F				qalSource3f;
extern ALSOURCEF				qalSourcef;
extern ALSOURCEFV				qalSourcefv;
extern ALSOURCEI				qalSourcei;
extern ALSOURCEPAUSE			qalSourcePause;
extern ALSOURCEPAUSEV			qalSourcePausev;
extern ALSOURCEPLAY				qalSourcePlay;
extern ALSOURCEPLAYV			qalSourcePlayv;
extern ALSOURCEQUEUEBUFFERS		qalSourceQueueBuffers;
extern ALSOURCEREWIND			qalSourceRewind;
extern ALSOURCEREWINDV			qalSourceRewindv;
extern ALSOURCESTOP				qalSourceStop;
extern ALSOURCESTOPV			qalSourceStopv;
extern ALSOURCEUNQUEUEBUFFERS	qalSourceUnqueueBuffers;

extern ALEAXSET					qalEAXSet;
extern ALEAXGET					qalEAXGet;

/*
=======================================================================
IMPLEMENTATION SPECIFIC FUNCTIONS
=======================================================================
*/

typedef struct
{
	const char			*vendorString;
	const char			*rendererString;
	const char			*versionString;
	const char			*extensionsString;

	const char			*deviceList;
	const char			*deviceName;

	qboolean			eax;
	unsigned			eaxState;
} alConfig_t;

extern alConfig_t		alConfig;

#ifdef _WIN32

#define AL_DRIVER_OPENAL	"OpenAL32.dll"
#define ALimp_Init			ALW_Init
#define ALimp_Shutdown		ALW_Shutdown

#else
#ifdef __linux__

#define AL_DRIVER_OPENAL	"libopenal.so"
#define ALimp_Init			AL_Init
#define ALimp_Shutdown		AL_Shutdown

#else

#error "ALimp_* not available for this platform"

#endif
#endif



#endif	// __QAL_H__
