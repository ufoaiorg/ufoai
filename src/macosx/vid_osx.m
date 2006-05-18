//______________________________________________________________________________________________________________nFO
// "vid_osx.c" - Main windowed and fullscreen graphics interface module. This module is used for
//               the OpenGL rendering versions of the Quake refresh engine.
//
// Written by:	awe				[mailto:awe@fruitz-of-dojo.de].
//		2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//

#pragma mark =Includes=

#include <AppKit/AppKit.h>
#include <sys/param.h>
#include <unistd.h>
#include <dlfcn.h>

#define MAC_OSX_M_SPECIAL_BOOLEAN 1

#include "client.h"

#pragma mark -

//___________________________________________________________________________________________________________mACROS

#pragma mark =Macros=

// We could iterate through active displays and capture them each, to avoid the CGReleaseAllDisplays() bug,
// but this would result in awfully switches between renderer selections, since we would have to clean-up the
// captured device list each time...

#ifdef CAPTURE_ALL_DISPLAYS

#define VID_FADE_ALL_SCREENS	YES
#define VID_CAPTURE_DISPLAYS()	CGCaptureAllDisplays ()
#define VID_RELEASE_DISPLAYS()	CGReleaseAllDisplays ()

#else

#define VID_FADE_ALL_SCREENS	NO
#define VID_CAPTURE_DISPLAYS()	CGDisplayCapture (kCGDirectMainDisplay)
#define VID_RELEASE_DISPLAYS()	CGDisplayRelease (kCGDirectMainDisplay)

#endif /* CAPTURE_ALL_DISPLAYS */

#pragma mark -

//__________________________________________________________________________________________________________dEFINES

#pragma mark =Defines=

#define	VID_MAX_PRINT_MSG	4096
#define VID_MAX_REF_NAME	256
#define VID_MAX_DISPLAYS	100
#define	VID_FADE_DURATION	1.0f


#pragma mark -

//_________________________________________________________________________________________________________tYPEDEFS

#pragma mark =Typedefs=

typedef struct vidgamma_s	{
                                    CGDirectDisplayID	displayID;
                                    CGGammaValue	component[9];
                                } vidgamma_t;

#pragma mark -

//________________________________________________________________________________________________________vARIABLES

#pragma mark =Variables=

extern cvar_t		*vid_grabmouse;

viddef_t		viddef;				// global video state

refexport_t		re;				// function wrapper to the current refresh bundle.
void			*reflib_library = NULL;		// Handle to refresh bundle.
qboolean		reflib_active = qfalse;

cvar_t			*vid_gamma = NULL,		// Video gamma value.
                        *vid_ref = NULL,		// Name of refresh bundle loaded.
                        *vid_xpos = NULL,		// X coordinate of window position.
                        *vid_ypos = NULL,		// Y coordinate of window position.
                        *vid_fullscreen = NULL,		// Video fullscreen.
                        *vid_minrefresh = NULL,		// Video min. refresh rate.
                        *vid_maxrefresh = NULL;		// Video max. refresh rate [-1 = infinite].

static vidgamma_t	*gVIDOriginalGamma = NULL;
static UInt16		gVIDModeCount = 0;
static CGDisplayCount	gVIDGammaCount = 0;

#pragma mark -

//______________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

extern void	IN_ShowCursor (qboolean theState);
extern void	Sys_Sleep (void);

void 		VID_Printf (int thePrintLevel, char *theFormat, ...);
void 		VID_Error (int theErrorLevel, char *theFormat, ...);
void 		VID_NewWindow (int theWidth, int theHeight);
qboolean 	VID_GetModeInfo (int *theWidth, int *theHeight, int theMode);
void		VID_Init (void);
qboolean 	VID_LoadRefresh (char *theName);
void		VID_FreeReflib (void);
void		VID_CheckChanges (void);
void		VID_Restart_f (void);

static qboolean	VID_FadeGammaInit (qboolean theFadeOnAllDisplays);
static void	VID_FadeGammaOut (qboolean theFadeOnAllDisplays, float theDuration);
static void	VID_FadeGammaIn (qboolean theFadeOnAllDisplays, float theDuration);

#pragma mark -

/*=======================
VID_Printf
=======================*/
void VID_Printf (int thePrintLevel, char *theFormat, ...)
{
	va_list		myArgPtr;
	char		myMessage[VID_MAX_PRINT_MSG];

	// formatted output conversion:
	va_start (myArgPtr, theFormat);
	vsnprintf (myMessage, VID_MAX_PRINT_MSG, theFormat, myArgPtr);
	va_end (myArgPtr);

	// print according to the print level:
	if (thePrintLevel == PRINT_ALL)
		Com_Printf ("%s", myMessage);
	else
		Com_DPrintf ("%s", myMessage);
}

/*=======================
VID_Error
=======================*/
void VID_Error (int theErrorLevel, char *theFormat, ...)
{
	va_list		myArgPtr;
	char		myMessage[VID_MAX_PRINT_MSG];

	// formatted output conversion:
	va_start (myArgPtr, theFormat);
	vsnprintf (myMessage, VID_MAX_PRINT_MSG, theFormat, myArgPtr);
	va_end (myArgPtr);

	// submitt the error string:
	Com_Error (theErrorLevel, "%s", myMessage);
}

/*=======================
VID_NewWindow
=======================*/
void VID_NewWindow (int theWidth, int theHeight)
{
	viddef.width = theWidth;
	viddef.height = theHeight;
}

/*=======================
VID_GetModeInfo
=======================*/
vidmode_t vid_modes[] =
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
	{ 1400, 1050, 20 }
};

qboolean VID_GetModeInfo (int *theWidth, int *theHeight, int theMode)
{
	// just return the current video size, false if the mode is not available:
	if (theMode < 0 || theMode >= gVIDModeCount)
	{
		*theWidth  = vid_modes[0].width;
		*theHeight = vid_modes[0].height;
		return (qfalse);
	}

	*theWidth  = vid_modes[theMode].width;
	*theHeight = vid_modes[theMode].height;

	return (qtrue);
}

/*=======================
VID_Init
=======================*/
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

	/* Hide the cursor */
	if ( vid_fullscreen->value )
		IN_ShowCursor (qfalse);

	/* Capture the screen(s): */
	if (vid_fullscreen->value)
	{
		VID_FadeGammaOut (VID_FADE_ALL_SCREENS, VID_FADE_DURATION);
		VID_CAPTURE_DISPLAYS ();
		VID_FadeGammaIn (VID_FADE_ALL_SCREENS, 0.0f);
	}

	/* Start the graphics mode and load refresh DLL */
	VID_CheckChanges();
}

/*=======================
VID_Shutdown
=======================*/
void VID_Shutdown (void)
{
	// shutdown the ref library:
	if (reflib_active)
	{
		re.Shutdown ();
		VID_FreeReflib ();
	}

	// release the screen(s):
	if (vid_fullscreen->value != 0.0f)
	{
		VID_FadeGammaOut (VID_FADE_ALL_SCREENS, 0.0f);
		if (CGDisplayIsCaptured (kCGDirectMainDisplay) == true)
		{
		VID_RELEASE_DISPLAYS ();
		}
		VID_FadeGammaIn (VID_FADE_ALL_SCREENS, VID_FADE_DURATION);
	}

	// show the cursor:
	IN_ShowCursor (YES);
}

/*=======================
VID_LoadRefresh
=======================*/
qboolean VID_LoadRefresh (char *theName)
{
	refimport_t	ri;
	GetRefAPI_t	myGetRefAPIProc;
	NSBundle	*myAppBundle = NULL;
	char		*myBundlePath = NULL,
			myCurrentPath[MAXPATHLEN],
			myFileName[MAXPATHLEN];

	// get current game directory:
	getcwd (myCurrentPath, sizeof (myCurrentPath));

	// get the plugin dir of the application:
	myAppBundle = [NSBundle mainBundle];
	if (myAppBundle == NULL)
	{
		Sys_Error ("Error while loading the renderer plug-in (invalid application bundle)!\n");
	}

	myBundlePath = (char *) [[myAppBundle builtInPlugInsPath] fileSystemRepresentation];
	if (myBundlePath == NULL)
	{
		Sys_Error ("Error while loading the renderer plug-in (invalid plug-in path)!\n");
	}

	chdir (myBundlePath);
	[myAppBundle release];

	// prepare the bundle name:
	Com_snprintf (myFileName, MAXPATHLEN, "%s.q2plug/Contents/MacOS/%s", theName, theName);

	if (reflib_active == true)
	{
		re.Shutdown ();
		VID_FreeReflib ();
		reflib_active = qfalse;
	}

	Com_Printf("------- Loading %s -------\n", theName);

	reflib_library = dlopen (myFileName, RTLD_LAZY | RTLD_GLOBAL);

	// return to the game directory:
	chdir (myCurrentPath);

	if (reflib_library == NULL)
	{
		Com_Printf ("LoadLibrary(\"%s\") failed: %s\n", theName , dlerror());
		return (qfalse);
	}

	Com_Printf ("LoadLibrary(\"%s\")\n", myFileName);

	if ((myGetRefAPIProc = (void *) dlsym (reflib_library, "GetRefAPI")) == NULL)
	{
		Com_Error (ERR_FATAL, "dlsym failed on %s", theName);
	}

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

	re = myGetRefAPIProc (ri);

	if (re.api_version != API_VERSION)
	{
		VID_FreeReflib ();
		Com_Error (ERR_FATAL, "%s has incompatible api_version!", theName);
	}

	if (re.Init (0, 0) == -1)
	{
		re.Shutdown ();
		VID_FreeReflib ();
		return qfalse;
	}

	Com_Printf ("------------------------------------\n");
	reflib_active = qtrue;
	return qtrue;
}

/*=======================
VID_FreeReflib
=======================*/
void	VID_FreeReflib (void)
{
	memset (&re, 0, sizeof (re));
	reflib_library = NULL;
	reflib_active  = qfalse;
}

/*=======================
VID_CheckChanges
=======================*/
void VID_CheckChanges (void)
{
	char 	myName[VID_MAX_REF_NAME];

	if (vid_ref->modified)
		S_StopAllSounds ();

	while (vid_ref->modified)
	{
		vid_ref->modified = qfalse;
		vid_fullscreen->modified = qtrue;
		cl.refresh_prepped = qfalse;
		cls.disable_screen = qtrue;

		Com_snprintf (myName, VID_MAX_REF_NAME, "ref_%s", vid_ref->string);
		if (VID_LoadRefresh (myName) == qfalse)
		{
			if (cls.key_dest != key_console)
				Con_ToggleConsole_f ();
		}
		cls.disable_screen = qfalse;
	}
}

/*=======================
VID_Restart_f
=======================*/
void VID_Restart_f (void)
{
	vid_ref->modified = qtrue;
}

/*=======================
VID_FadeGammaInit
=======================*/
qboolean VID_FadeGammaInit (qboolean theFadeOnAllDisplays)
{
	static qboolean		myFadeOnAllDisplays = qfalse;
	CGDirectDisplayID  	myDisplayList[VID_MAX_DISPLAYS];
	CGDisplayErr	myError;
	UInt32		i;

	// if init fails, no gamma fading will be used!
	if (gVIDOriginalGamma != NULL)
	{
		// initialized, but did we change the number of displays?
		if (theFadeOnAllDisplays == myFadeOnAllDisplays)
			return qboolean;
		free (gVIDOriginalGamma);
		gVIDOriginalGamma = NULL;
	}

	// get the list of displays:
	if (CGGetActiveDisplayList (VID_MAX_DISPLAYS, myDisplayList, &gVIDGammaCount) != CGDisplayNoErr)
		return qfalse;

	if (gVIDGammaCount == 0)
		return qfalse;

	if (theFadeOnAllDisplays == qfalse)
		gVIDGammaCount = 1;

	// get memory for our original gamma table(s):
	gVIDOriginalGamma = malloc (sizeof (vidgamma_t) * gVIDGammaCount);
	if (gVIDOriginalGamma == NULL)
		return qfalse;

	// store the original gamma values within this table(s):
	for (i = 0; i < gVIDGammaCount; i++)
	{
		if (gVIDGammaCount == 1)
			gVIDOriginalGamma[i].displayID = kCGDirectMainDisplay;
		else
			gVIDOriginalGamma[i].displayID = myDisplayList[i];

		myError = CGGetDisplayTransferByFormula (gVIDOriginalGamma[i].displayID,
							&gVIDOriginalGamma[i].component[0],
							&gVIDOriginalGamma[i].component[1],
							&gVIDOriginalGamma[i].component[2],
							&gVIDOriginalGamma[i].component[3],
							&gVIDOriginalGamma[i].component[4],
							&gVIDOriginalGamma[i].component[5],
							&gVIDOriginalGamma[i].component[6],
							&gVIDOriginalGamma[i].component[7],
							&gVIDOriginalGamma[i].component[8]);
		if (myError != CGDisplayNoErr)
		{
			free (gVIDOriginalGamma);
			gVIDOriginalGamma = NULL;
			return qfalse;
		}
	}
	myFadeOnAllDisplays = theFadeOnAllDisplays;

	return qtrue;
}

/*=======================
VID_FadeGammaOut
=======================*/
void VID_FadeGammaOut (qboolean theFadeOnAllDisplays, float theDuration)
{
	vidgamma_t		myCurGamma;
	float		myStartTime = 0.0f, myCurScale = 0.0f;
	UInt32		i, j;

	// check if initialized:
	if (VID_FadeGammaInit (theFadeOnAllDisplays) == qfalse)
		return;

	// get the time of the fade start:
	myStartTime = Sys_Milliseconds ();
	theDuration *= 1000.0f;

	// fade for the choosen duration:
	while (1)
	{
		// calculate the current scale and clamp it:
		if (theDuration > 0.0f)
		{
			myCurScale = 1.0f - (Sys_Milliseconds () - myStartTime) / theDuration;
			if (myCurScale < 0.0f)
				myCurScale = 0.0f;
		}

		// fade the gamma for each display:
		for (i = 0; i < gVIDGammaCount; i++)
		{
			// calculate the current intensity for each color component:
			for (j = 1; j < 9; j += 3)
				myCurGamma.component[j] = myCurScale * gVIDOriginalGamma[i].component[j];

			// set the current gamma:
			CGSetDisplayTransferByFormula (gVIDOriginalGamma[i].displayID,
						gVIDOriginalGamma[i].component[0],
						myCurGamma.component[1],
						gVIDOriginalGamma[i].component[2],
						gVIDOriginalGamma[i].component[3],
						myCurGamma.component[4],
						gVIDOriginalGamma[i].component[5],
						gVIDOriginalGamma[i].component[6],
						myCurGamma.component[7],
						gVIDOriginalGamma[i].component[8]);
		}

		// are we finished?
		if(myCurScale <= 0.0f)
			break;
	}
}

/*=======================
VID_FadeGammaInit
=======================*/
void VID_FadeGammaIn (qboolean theFadeOnAllDisplays, float theDuration)
{
	vidgamma_t	myCurGamma;
	float		myStartTime = 0.0f, myCurScale = 1.0f;
	UInt32		i, j;

	// check if initialized:
	if (gVIDOriginalGamma == NULL)
		return;

	// get the time of the fade start:
	myStartTime = Sys_Milliseconds ();
	theDuration *= 1000.0f;

	// fade for the choosen duration:
	while (1)
	{
		// calculate the current scale and clamp it:
		if (theDuration > 0.0f)
		{
			myCurScale = (Sys_Milliseconds () - myStartTime) / theDuration;
			if (myCurScale > 1.0f)
				myCurScale = 1.0f;
		}

		// fade the gamma for each display:
		for (i = 0; i < gVIDGammaCount; i++)
		{
			// calculate the current intensity for each color component:
			for (j = 1; j < 9; j += 3)
				myCurGamma.component[j] = myCurScale * gVIDOriginalGamma[i].component[j];

			// set the current gamma:
			CGSetDisplayTransferByFormula (gVIDOriginalGamma[i].displayID,
						gVIDOriginalGamma[i].component[0],
						myCurGamma.component[1],
						gVIDOriginalGamma[i].component[2],
						gVIDOriginalGamma[i].component[3],
						myCurGamma.component[4],
						gVIDOriginalGamma[i].component[5],
						gVIDOriginalGamma[i].component[6],
						myCurGamma.component[7],
						gVIDOriginalGamma[i].component[8]);
		}

		// are we finished?
		if(myCurScale >= 1.0f)
			break;
	}
}
