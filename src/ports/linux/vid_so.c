/**
 * @file vid_so.c
 * @brief Main windowed and fullscreen graphics interface module.
 * @note This module is used for the OpenGL rendering versions of the UFO refresh engine.
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

#include <assert.h>
#include <dlfcn.h> /* ELF dl loader */
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
/* #include <uuid/uuid.h> */

#include "../../client/client.h"

#include "rw_linux.h"

/* Structure containing functions exported from refresh DLL */
refexport_t	re;

/* Console variables that we need to access from this module */
cvar_t		*vid_gamma;
cvar_t		*vid_ref;			/* Name of Refresh DLL loaded */
cvar_t		*vid_xpos;			/* X coordinate of window position */
cvar_t		*vid_ypos;			/* Y coordinate of window position */
cvar_t		*vid_fullscreen;
cvar_t		*vid_grabmouse;

/* Global variables used internally by this module */
viddef_t	viddef;				/* global video state; used by other modules */
void		*reflib_library;		/* Handle to refresh DLL */
qboolean	reflib_active = qfalse;

const char so_file[] = "/etc/ufo.conf";

/** KEYBOARD **************************************************************/

void Do_Key_Event(int key, qboolean down);

void (*KBD_Update_fp)(void);
void (*KBD_Init_fp)(Key_Event_fp_t fp);
void (*KBD_Close_fp)(void);

/** MOUSE *****************************************************************/

in_state_t in_state;

void (*RW_IN_Init_fp)(in_state_t *in_state_p);
void (*RW_IN_Shutdown_fp)(void);
void (*RW_IN_Activate_fp)(qboolean active);
void (*RW_IN_Commands_fp)(void);
void (*RW_IN_GetMousePos_fp)(int *mx, int *my);
void (*RW_IN_Frame_fp)(void);

void Real_IN_Init (void);

/*
==========================================================================
DLL GLUE
==========================================================================
*/

#define	MAXPRINTMSG	4096
/**
 * @brief
 */
void VID_Printf (int print_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	if (print_level == PRINT_ALL)
		Com_Printf ("%s", msg);
	else
		Com_DPrintf ("%s", msg);
}

/**
 * @brief
 */
void VID_Error (int err_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
/*	static qboolean	inupdate; */

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	Com_Error (err_level,"%s", msg);
}

/**
 * @brief Console command to re-start the video mode and refresh DLL. We do this
 * simply by setting the modified flag for the vid_ref variable, which will
 * cause the entire video mode and refresh DLL to be reset on the next frame.
 */
void VID_Restart_f (void)
{
	vid_ref->modified = qtrue;
}

int maxVidModes;
/**
 * @brief
 */
const vidmode_t vid_modes[] =
{
	{ 320, 240,   0 },
	{ 400, 300,   1 },
	{ 512, 384,   2 },
	{ 640, 480,   3 },
	{ 800, 600,   4 },
	{ 960, 720,   5 },
	{ 1024, 768,  6 },
	{ 1152, 864,  7 },
	{ 1280, 1024, 8 },
	{ 1600, 1200, 9 },
	{ 2048, 1536, 10 },
	{ 1024,  480, 11 }, /* Sony VAIO Pocketbook */
	{ 1152,  768, 12 }, /* Apple TiBook */
	{ 1280,  854, 13 }, /* Apple TiBook */
	{ 640,  400, 14 }, /* generic 16:10 widescreen*/
	{ 800,  500, 15 }, /* as found modern */
	{ 1024,  640, 16 }, /* notebooks    */
	{ 1280,  800, 17 },
	{ 1680, 1050, 18 },
	{ 1920, 1200, 19 },
	{ 1400, 1050, 20 }, /* samsung x20 */
};

/**
 * @brief
 */
qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
	if ( mode < 0 || mode >= VID_NUM_MODES )
		return qfalse;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return qtrue;
}

/**
 * @brief
 */
void VID_NewWindow ( int width, int height)
{
	viddef.width  = width;
	viddef.height = height;

	viddef.rx = (float)width  / VID_NORM_WIDTH;
	viddef.ry = (float)height / VID_NORM_HEIGHT;
}

/**
 * @brief
 */
void VID_FreeReflib (void)
{
	if (reflib_library) {
		if (KBD_Close_fp)

			KBD_Close_fp();
		if (RW_IN_Shutdown_fp)
			RW_IN_Shutdown_fp();
		dlclose(reflib_library);
	}

	KBD_Init_fp = NULL;
	KBD_Update_fp = NULL;
	KBD_Close_fp = NULL;
	RW_IN_Init_fp = NULL;
	RW_IN_Shutdown_fp = NULL;
	RW_IN_Activate_fp = NULL;
	RW_IN_Commands_fp = NULL;
	RW_IN_GetMousePos_fp = NULL;
	RW_IN_Frame_fp = NULL;

	memset (&re, 0, sizeof(re));
	reflib_library = NULL;
	reflib_active  = qfalse;
}

/**
 * @brief
 */
qboolean VID_LoadRefresh( char *name )
{
	refimport_t	ri;
	GetRefAPI_t	GetRefAPI;
	qboolean	restart = qfalse;
	char	fn[MAX_OSPATH];
	struct stat st;
	extern uid_t saved_euid;
	FILE *fp;

	if ( reflib_active ) {
		if (KBD_Close_fp)
			KBD_Close_fp();
		if (RW_IN_Shutdown_fp)
			RW_IN_Shutdown_fp();
		KBD_Close_fp = NULL;
		RW_IN_Shutdown_fp = NULL;
		re.Shutdown();
		VID_FreeReflib();
		restart = qtrue;
	}

	Com_Printf( "------- Loading %s -------\n", name );

	/*regain root */
	seteuid(saved_euid);

	if ((fp = fopen(so_file, "r")) == NULL) {
		Com_Printf( "LoadLibrary(\"%s\"): can't open %s - setting search path to .\n", name, so_file);
		strcpy(fn, ".");
	} else {
		fgets(fn, sizeof(fn), fp);
		fclose(fp);
		while (*fn && isspace(fn[strlen(fn) - 1]))
			fn[strlen(fn) - 1] = 0;
	}

	Q_strcat(fn, "/", sizeof(fn));
	Q_strcat(fn, name, sizeof(fn));

	if (stat(fn, &st) == -1) {
		Com_Printf( "LoadLibrary(\"%s\") failed: %s\n", name, strerror(errno));
		return qfalse;
	}

	if ( ( reflib_library = dlopen( fn, RTLD_LAZY | RTLD_GLOBAL ) ) == 0 ) {
		Com_Printf( "LoadLibrary(\"%s\") failed: %s\n", name , dlerror());
		return qfalse;
	}

	Com_Printf( "LoadLibrary(\"%s\")\n", fn );

	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_WriteFile = FS_WriteFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_CheckFile = FS_CheckFile;
	ri.FS_ListFiles = FS_ListFiles;
	ri.FS_Gamedir = FS_Gamedir;
	ri.FS_NextPath = FS_NextPath;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_NewWindow = VID_NewWindow;
	ri.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;
	ri.CL_GetFontData = CL_GetFontData;

	if ( ( GetRefAPI = (void *) dlsym( reflib_library, "GetRefAPI" ) ) == 0 )
		Com_Error( ERR_FATAL, "dlsym failed on %s", name );

	re = GetRefAPI( ri );

	if (re.api_version != API_VERSION) {
		VID_FreeReflib ();
		Com_Error (ERR_FATAL, "%s has incompatible api_version", name);
	}

	/* Init IN (Mouse) */
	in_state.Key_Event_fp = Do_Key_Event;

	if ((RW_IN_Init_fp = dlsym(reflib_library, "RW_IN_Init")) == NULL ||
		(RW_IN_Shutdown_fp = dlsym(reflib_library, "RW_IN_Shutdown")) == NULL ||
		(RW_IN_Activate_fp = dlsym(reflib_library, "RW_IN_Activate")) == NULL ||
		(RW_IN_Commands_fp = dlsym(reflib_library, "RW_IN_Commands")) == NULL ||
		(RW_IN_GetMousePos_fp = dlsym(reflib_library, "RW_IN_GetMousePos")) == NULL ||

		(RW_IN_Frame_fp = dlsym(reflib_library, "RW_IN_Frame")) == NULL)
		Sys_Error("No RW_IN functions in REF.\n");

	Real_IN_Init();

	if ( re.Init( 0, 0 ) == -1 ) {
		re.Shutdown();
		VID_FreeReflib ();
		return qfalse;
	}

	/* Init KBD */
#if 1
	if ((KBD_Init_fp = dlsym(reflib_library, "KBD_Init")) == NULL ||
		(KBD_Update_fp = dlsym(reflib_library, "KBD_Update")) == NULL ||
		(KBD_Close_fp = dlsym(reflib_library, "KBD_Close")) == NULL)
		Sys_Error("No KBD functions in REF.\n");
#else
	{

		void KBD_Init(void);
		void KBD_Update(void);
		void KBD_Close(void);

		KBD_Init_fp = KBD_Init;
		KBD_Update_fp = KBD_Update;
		KBD_Close_fp = KBD_Close;
	}
#endif
	KBD_Init_fp(Do_Key_Event);

	/* give up root now */
	setreuid(getuid(), getuid());
	setegid(getgid());

	/* vid_restart */
	if ( restart )
		CL_InitFonts();

	Com_Printf( "------------------------------------\n");

	reflib_active = qtrue;

	return qtrue;
}

/**
 * @brief This function gets called once just before drawing each frame, and it's sole purpose in life
 * is to check to see if any of the video mode parameters have changed, and if they have to
 * update the rendering DLL and/or video mode to match.
 */
void VID_CheckChanges (void)
{
	char name[100];

	if ( vid_ref->modified )
		S_StopAllSounds();

	while (vid_ref->modified) {
		/* refresh has changed */
		vid_ref->modified = qfalse;
		vid_fullscreen->modified = qtrue;
		cl.refresh_prepped = qfalse;
		cls.disable_screen = qtrue;

		Com_sprintf(name, sizeof(name), "ref_%s.so", vid_ref->string);
		if ( !VID_LoadRefresh( name ) ) {
			Cmd_ExecuteString( "condump gl_debug" );

			Com_Error (ERR_FATAL, "Couldn't initialize OpenGL renderer!\nConsult gl_debug.txt for further information.");
		}
		cls.disable_screen = qfalse;
	}

}

/**
 * @brief
 */
void VID_Init (void)
{
	/* Create the video variables so we know how to start the graphics drivers */
	vid_ref = Cvar_Get ("vid_ref", "glx", CVAR_ARCHIVE);
	vid_xpos = Cvar_Get ("vid_xpos", "3", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get ("vid_ypos", "22", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "1", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);

	/* Start the graphics mode and load refresh DLL */
	VID_CheckChanges();
	maxVidModes = VID_NUM_MODES;
}

/**
 * @brief
 */
void VID_Shutdown (void)
{
	if ( reflib_active ) {
		if (KBD_Close_fp)
			KBD_Close_fp();
		if (RW_IN_Shutdown_fp)
			RW_IN_Shutdown_fp();
		KBD_Close_fp = NULL;
		RW_IN_Shutdown_fp = NULL;
		re.Shutdown ();
		VID_FreeReflib ();
	}
}


/*****************************************************************************/
/* INPUT                                                                     */
/*****************************************************************************/

/**
 * @brief
 */
void IN_Init ( void )
{
}

/**
 * @brief
 */
void Real_IN_Init (void)
{
	if (RW_IN_Init_fp)
		RW_IN_Init_fp(&in_state);
}

/**
 * @brief
 */
void IN_Shutdown (void)
{
	if (RW_IN_Shutdown_fp)
		RW_IN_Shutdown_fp();
}

/**
 * @brief
 */
void IN_Commands (void)
{
	if (RW_IN_Commands_fp)
		RW_IN_Commands_fp();
}

/**
 * @brief
 */
void IN_GetMousePos (int *mx, int *my)
{
	if (RW_IN_GetMousePos_fp)
		RW_IN_GetMousePos_fp(mx, my);
}

/**
 * @brief
 */
void IN_Frame (void)
{
	if (RW_IN_Activate_fp) {
		if ( cls.key_dest == key_console)
			RW_IN_Activate_fp(qfalse);
		else
			RW_IN_Activate_fp(qtrue);
	}

	if (RW_IN_Frame_fp)
		RW_IN_Frame_fp();
}

/**
 * @brief
 */
void IN_Activate (qboolean active)
{
}

/**
 * @brief
 */
void Do_Key_Event(int key, qboolean down)
{
	Key_Event(key, down, Sys_Milliseconds());
}

