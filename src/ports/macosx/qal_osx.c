#include "../../client/qal.h"
#include "qal_osx.h"


#ifdef HAVE_OPENAL

qboolean QAL_Init (void)
{
	char libPath[MAX_OSPATH];
	cvar_t* s_libdir = Cvar_Get("s_libdir", "", CVAR_ARCHIVE, "lib dir for graphic and sound renderer - no game libs");

	Com_Printf("QAL_Init: Loading \"%s\"...\n", AL_DRIVER_OPENAL);

	if ((oalState.hInstOpenAL = dlopen(AL_DRIVER_OPENAL, RTLD_LAZY|RTLD_GLOBAL)) == 0) {
		/* try path given via cvar */
		if (strlen(s_libdir->string))
			Q_strncpyz(libPath, s_libdir->string, sizeof(libPath));
		else
			strcpy(libPath, ".");

		Q_strcat(libPath, "/", sizeof(libPath));
		Q_strcat(libPath, AL_DRIVER_OPENAL, sizeof(libPath));

		if ((oalState.hInstOpenAL = dlopen(libPath, RTLD_LAZY)) == 0) {
			Com_Printf("%s\n", dlerror());
			return qfalse;
		}
	}

	if ((oalState.hInstALUT = dlopen(AL_DRIVER_ALUT, RTLD_LAZY|RTLD_GLOBAL)) == 0) {
		/* try path given via cvar */
		if (strlen(s_libdir->string))
			Q_strncpyz(libPath, s_libdir->string, sizeof(libPath));
		else
			strcpy(libPath, ".");

		Q_strcat(libPath, "/", sizeof(libPath));
		Q_strcat(libPath, AL_DRIVER_ALUT, sizeof(libPath));

		if ((oalState.hInstOpenAL = dlopen(libPath, RTLD_LAZY)) == 0) {
			Com_Printf("%s\n", dlerror());
			return qfalse;
		}
	}

	if (QAL_Link()) {
		openal_active = qtrue;
		return qtrue;
	} else
		return qfalse;
}

void QAL_Shutdown (void)
{
	if (oalState.hInstOpenAL)
		dlclose(oalState.hInstOpenAL);

	oalState.hInstOpenAL = NULL;

	/* general pointers */
	QAL_Unlink();
}

#endif