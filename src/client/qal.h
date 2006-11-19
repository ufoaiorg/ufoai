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

#include "client.h"

#ifdef _WIN32
	#include "../ports/win32/winquake.h"
	#include "../ports/win32/qal_win.h"
#endif

#ifdef __linux__
	#include "../ports/linux/qal_linux.h"
#endif

#ifndef ALCAPIENTRY
#  define ALCAPIENTRY
#endif

#ifndef ALUT_ERROR_NO_ERROR
#  define ALUT_ERROR_NO_ERROR 0
#endif

qboolean	QAL_Init (void);
void		QAL_Shutdown (void);
qboolean	QAL_Link(void);
void		QAL_Unlink(void);

extern qboolean					openal_active;

extern ALCdevice *		(ALCAPIENTRY * qalcOpenDevice)(ALCubyte *deviceName);
extern ALCvoid			(ALCAPIENTRY * qalcCloseDevice)(ALCdevice *device);
extern ALCcontext *		(ALCAPIENTRY * qalcCreateContext)(ALCdevice *device, ALCint *attrList);
extern ALCvoid			(ALCAPIENTRY * qalcDestroyContext)(ALCcontext *context);
extern ALCboolean		(ALCAPIENTRY * qalcMakeContextCurrent)(ALCcontext *context);
extern ALCvoid			(ALCAPIENTRY * qalcProcessContext)(ALCcontext *context);
extern ALCvoid			(ALCAPIENTRY * qalcSuspendContext)(ALCcontext *context);
extern ALCcontext *		(ALCAPIENTRY * qalcGetCurrentContext)(ALCvoid);
extern ALCdevice *		(ALCAPIENTRY * qalcGetContextsDevice)(ALCcontext *context);
extern ALCubyte *		(ALCAPIENTRY * qalcGetString)(ALCdevice *device, ALCenum param);
extern ALCvoid			(ALCAPIENTRY * qalcGetIntegerv)(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *data);
extern ALCenum			(ALCAPIENTRY * qalcGetError)(ALCdevice *device);
extern ALCboolean		(ALCAPIENTRY * qalcIsExtensionPresent)(ALCdevice *device, ALCubyte *extName);
extern ALCvoid *		(ALCAPIENTRY * qalcGetProcAddress)(ALCdevice *device, ALCubyte *funcName);
extern ALCenum			(ALCAPIENTRY * qalcGetEnumValue)(ALCdevice *device, ALCubyte *enumName);

extern ALvoid			(ALAPIENTRY * qalBufferData)(ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq);
extern ALvoid			(ALAPIENTRY * qalDeleteBuffers)(ALsizei n, ALuint *buffers);
extern ALvoid			(ALAPIENTRY * qalDeleteSources)(ALsizei n, ALuint *sources);
extern ALvoid			(ALAPIENTRY * qalDisable)(ALenum capability);
extern ALvoid			(ALAPIENTRY * qalDistanceModel)(ALenum value);
extern ALvoid			(ALAPIENTRY * qalDopplerFactor)(ALfloat value);
extern ALvoid			(ALAPIENTRY * qalDopplerVelocity)(ALfloat value);
extern ALvoid			(ALAPIENTRY * qalEnable)(ALenum capability);
extern ALvoid			(ALAPIENTRY * qalGenBuffers)(ALsizei n, ALuint *buffers);
extern ALvoid			(ALAPIENTRY * qalGenSources)(ALsizei n, ALuint *sources);
extern ALboolean		(ALAPIENTRY * qalGetBoolean)(ALenum param);
extern ALvoid			(ALAPIENTRY * qalGetBooleanv)(ALenum param, ALboolean *data);
extern ALvoid			(ALAPIENTRY * qalGetBufferf)(ALuint buffer, ALenum param, ALfloat *value);
extern ALvoid			(ALAPIENTRY * qalGetBufferi)(ALuint buffer, ALenum param, ALint *value);
extern ALdouble			(ALAPIENTRY * qalGetDouble)(ALenum param);
extern ALvoid			(ALAPIENTRY * qalGetDoublev)(ALenum param, ALdouble *data);
extern ALenum			(ALAPIENTRY * qalGetEnumValue)(ALubyte *enumName);
extern ALenum			(ALAPIENTRY * qalGetError)(ALvoid);
extern ALfloat			(ALAPIENTRY * qalGetFloat)(ALenum param);
extern ALvoid			(ALAPIENTRY * qalGetFloatv)(ALenum param, ALfloat *data);
extern ALint			(ALAPIENTRY * qalGetInteger)(ALenum param);
extern ALvoid			(ALAPIENTRY * qalGetIntegerv)(ALenum param, ALint *data);
extern ALvoid			(ALAPIENTRY * qalGetListener3f)(ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3);
extern ALvoid			(ALAPIENTRY * qalGetListenerf)(ALenum param, ALfloat *value);
extern ALvoid			(ALAPIENTRY * qalGetListenerfv)(ALenum param, ALfloat *values);
extern ALvoid			(ALAPIENTRY * qalGetListeneri)(ALenum param, ALint *value);
extern ALvoid *			(ALAPIENTRY * qalGetProcAddress)(ALubyte *funcName);
extern ALvoid			(ALAPIENTRY * qalGetSource3f)(ALuint source, ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3);
extern ALvoid			(ALAPIENTRY * qalGetSourcef)(ALuint source, ALenum param, ALfloat *value);
extern ALvoid			(ALAPIENTRY * qalGetSourcefv)(ALuint source, ALenum param, ALfloat *values);
extern ALvoid			(ALAPIENTRY * qalGetSourcei)(ALuint source, ALenum param, ALint *value);
extern ALubyte *		(ALAPIENTRY * qalGetString)(ALenum param);
extern ALvoid			(ALAPIENTRY * qalHint)(ALenum target, ALenum mode);
extern ALboolean		(ALAPIENTRY * qalIsBuffer)(ALuint buffer);
extern ALboolean		(ALAPIENTRY * qalIsEnabled)(ALenum capability);
extern ALboolean		(ALAPIENTRY * qalIsExtensionPresent)(ALubyte *extName);
extern ALboolean		(ALAPIENTRY * qalIsSource)(ALuint source);
extern ALvoid			(ALAPIENTRY * qalListener3f)(ALenum param, ALfloat v1, ALfloat v2, ALfloat v3);
extern ALvoid			(ALAPIENTRY * qalListenerf)(ALenum param, ALfloat value);
extern ALvoid			(ALAPIENTRY * qalListenerfv)(ALenum param, ALfloat *values);
extern ALvoid			(ALAPIENTRY * qalListeneri)(ALenum param, ALint value);
extern ALvoid			(ALAPIENTRY * qalSource3f)(ALuint source, ALenum param, ALfloat v1, ALfloat v2, ALfloat v3);
extern ALvoid			(ALAPIENTRY * qalSourcef)(ALuint source, ALenum param, ALfloat value);
extern ALvoid			(ALAPIENTRY * qalSourcefv)(ALuint source, ALenum param, ALfloat *values);
extern ALvoid			(ALAPIENTRY * qalSourcei)(ALuint source, ALenum param, ALint value);
extern ALvoid			(ALAPIENTRY * qalSourcePause)(ALuint source);
extern ALvoid			(ALAPIENTRY * qalSourcePausev)(ALsizei n, ALuint *sources);
extern ALvoid			(ALAPIENTRY * qalSourcePlay)(ALuint source);
extern ALvoid			(ALAPIENTRY * qalSourcePlayv)(ALsizei n, ALuint *sources);
extern ALvoid			(ALAPIENTRY * qalSourceQueueBuffers)(ALuint source, ALsizei n, ALuint *buffers);
extern ALvoid			(ALAPIENTRY * qalSourceRewind)(ALuint source);
extern ALvoid			(ALAPIENTRY * qalSourceRewindv)(ALsizei n, ALuint *sources);
extern ALvoid			(ALAPIENTRY * qalSourceStop)(ALuint source);
extern ALvoid			(ALAPIENTRY * qalSourceStopv)(ALsizei n, ALuint *sources);
extern ALvoid			(ALAPIENTRY * qalSourceUnqueueBuffers)(ALuint source, ALsizei n, ALuint *buffers);

/* alut */
extern ALboolean		(ALAPIENTRY * qalutInit)(int *argcp, char **argv);
extern ALboolean		(ALAPIENTRY * qalutInitWithoutContext)(int *argcp, char **argv);
extern ALboolean		(ALAPIENTRY * qalutExit)(void);
extern ALenum			(ALAPIENTRY * qalutGetError)(void);
extern const char *		(ALAPIENTRY * qalutGetErrorString)(ALenum error);
extern ALuint			(ALAPIENTRY * qalutCreateBufferFromFile)(const char *fileName);
extern ALuint			(ALAPIENTRY * qalutCreateBufferFromFileImage)(const ALvoid *data, ALsizei length);
extern ALvoid *			(ALAPIENTRY * qalutLoadMemoryFromFile)(const char *fileName, ALenum *format, ALsizei *size, ALfloat *frequency);
extern ALvoid *			(ALAPIENTRY * qalutLoadMemoryFromFileImage)(const ALvoid *data, ALsizei length, ALenum *format, ALsizei *size, ALfloat *frequency);
extern const char *		(ALAPIENTRY * qalutGetMIMETypes)(ALenum loader);
extern ALuint			(ALAPIENTRY * qalutCreateBufferHelloWorld)(void);
extern ALuint			(ALAPIENTRY * qalutCreateBufferWaveform)(ALenum waveshape, ALfloat frequency, ALfloat phase, ALfloat duration);
extern ALint			(ALAPIENTRY * qalutGetMajorVersion)(void);
extern ALint			(ALAPIENTRY * qalutGetMinorVersion)(void);
extern ALboolean		(ALAPIENTRY * qalutSleep)(ALfloat duration);


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


#endif	/* __QAL_H__ */
