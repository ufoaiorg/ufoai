//______________________________________________________________________________________________________________nFO
// "glimp_osx.c" - MacOS X OpenGL renderer.
//
// Written by:	awe				[mailto:awe@fruitz-of-dojo.de].
//		©2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quake IIª is copyrighted by id software	[http://www.idsoftware.com].
//
// Version History:
// v1.0.8: ¥ Added support for non-overbright gamma [variable "gl_overbright_gamma"].
// v1.0.6: ¥ Added support for FSAA [variable "gl_ext_multisample"].
// v1.0.5: ¥ÊAdded support for anisotropic texture filtering [variable "gl_anisotropic"].
//	   ¥ Added support for Truform [variable "gl_truform"].
//	   ¥ Improved renderer performance thru smaller lightmaps [define USE_SMALL_LIGHTMAPS at compile time].
//	   ¥ "gl_mode" is now set to "0" instead of "3" by default. Fixes a problem with monitors which provide
//	     only a single resolution.
// v1.0.3: ¥ Screenshots are now saved as TIFF instead of TGA files [see "gl_rmisc.c"].
//	   ¥ Fixed an issue with wrong pixels at the right and left border of cinematics [see "gl_draw.c"].
// v1.0.1: ¥ added "gl_force16bit" command.
//	   ¥ added "gl_swapinterval". 0 = no VBL wait, 1 = wait for VBL. Available via "Video Options" dialog, too.
//	   ¥ added rendering inside the Dock [if window is minimized].
//	   changes in "gl_rmain.c", line 1043 and later:
//	   ¥ "gl_ext_palettedtexture" is now by default turned off.
//	   ¥ "gl_ext_multitexture" is now possible, however default value is "0", due to bad performance.
// v1.0.0: Initial release.
//_________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <AppKit/AppKit.h>
#import <IOKit/graphics/IOGraphicsTypes.h>
#import <OpenGL/OpenGL.h>

// Matthijs FIXED
// Taken from /System/Library/Frameworks/OpenGL.framework/Versions/A/Headers/glext.h
#define GL_PN_TRIANGLES_ATIX                            0x6090
#define GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATIX      0x6091
#define GL_PN_TRIANGLES_POINT_MODE_ATIX                 0x6092
#define GL_PN_TRIANGLES_NORMAL_MODE_ATIX                0x6093
#define GL_PN_TRIANGLES_TESSELATION_LEVEL_ATIX          0x6094
#define GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATIX          0x6095
#define GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATIX           0x6096
#define GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATIX         0x6097
#define GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATIX      0x6098


#define MAC_OSX_M_SPECIAL_BOOLEAN 1
#include "../ref_gl/gl_local.h"

#pragma mark -

//___________________________________________________________________________________________________________mACROS

#pragma mark =Macros=

// We could iterate through active displays and capture them each, to avoid the CGReleaseAllDisplays() bug,
// but this would result in awfully switches between renderer selections, since we would have to clean-up the
// captured device list each time...

#ifdef CAPTURE_ALL_DISPLAYS

#define GL_CAPTURE_DISPLAYS()	CGCaptureAllDisplays ()
#define GL_RELEASE_DISPLAYS()	CGReleaseAllDisplays ()

#else

#define GL_CAPTURE_DISPLAYS()	CGDisplayCapture (kCGDirectMainDisplay)
#define GL_RELEASE_DISPLAYS()	CGDisplayRelease (kCGDirectMainDisplay)

#endif /* CAPTURE_ALL_DISPLAYS */

#pragma mark -

//__________________________________________________________________________________________________________dEFINES

#define CG_MAX_GAMMA_TABLE_SIZE	256		// Required for getting and setting non-overbright gamma tables.

//_________________________________________________________________________________________________________tYPEDEFS

typedef struct		{
                                CGTableCount		count;
                                CGGammaValue		red[CG_MAX_GAMMA_TABLE_SIZE];
                                CGGammaValue		green[CG_MAX_GAMMA_TABLE_SIZE];
                                CGGammaValue		blue[CG_MAX_GAMMA_TABLE_SIZE];
                        }	gl_gammatable_t;
                        
//_______________________________________________________________________________________________________iNTERFACES

#pragma mark =ObjC Interfaces=

@interface Quake2GLView : NSView
@end

@interface NSOpenGLContext (CGLContextAccess)
- (CGLContextObj) cglContext;
@end

#pragma mark -

//________________________________________________________________________________________________________vARIABLES

#pragma mark =Variables=

qboolean			gGLTruformAvailable = NO;
long				gGLMaxARBMultiSampleBuffers,
                                gGLCurARBMultiSamples;

static CFDictionaryRef		gGLOriginalMode;
static CGGammaValue		gGLOriginalGamma[9];
static gl_gammatable_t		gGLOriginalGammaTable;
static NSOpenGLContext		*gGLContext = NULL;
static NSWindow			*gGLWindow = NULL;
static NSImage			*gGLMiniWindow = NULL,
                                *gGLOldIcon = NULL;
static Quake2GLView		*gGLView = NULL;
static float			gGLOldGamma = 0.0f,
                                gGLOldOverbrightGamma;
static Boolean			gGLFullscreen = NO,
                                gGLCanSetGamma = NO,
                                gGLIconChanged = NO;
cvar_t				*gGLSwapInterval = NULL,
                                *gGLIsMinimized = NULL,
                                *gGLGamma = NULL,
                                *gGLTextureAnisotropyLevel = NULL,
                                *gGLARBMultiSampleLevel = NULL,
                                *gGLTrufomTesselationLevel = NULL,
                                *gGLOverbrightGamma = NULL;
static UInt32			*gGLMiniBuffer = NULL;
static long	                gGLMaxARBMultiSamples;
static const float 		gGLTruformAmbient[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

#pragma mark -

//______________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

static void			GLimp_RestoreOldIcon (void);
static void			GLimp_DestroyContext (void);
static NSOpenGLPixelFormat *	GLimp_CreateGLPixelFormat (int theDepth, Boolean theFullscreen);
static Boolean			GLimp_InitGraphics (int *theWidth, int *theHeight, float theRefreshRate,
                                                    Boolean theFullscreen);
static void			GLimp_SetSwapInterval (void);
static void			GLimp_SetAnisotropyTextureLevel (void);
static void			GLimp_SetTruform (void);
static void			GLimp_SetARBMultiSample (void);
static void			GLimp_CheckForARBMultiSample (void);

#pragma mark -

//___________________________________________________________________________________________GLimp_RestoreOldIcon()

void	GLimp_RestoreOldIcon (void)
{
    if (gGLIconChanged == YES)
    {
        if (gGLOldIcon != NULL)
        {
            [NSApp setApplicationIconImage: gGLOldIcon];
            if (gGLIsMinimized != NULL)
            {
                gGLIsMinimized->value = 0.0f;
            }
        }
        gGLIconChanged = NO;
    }
}

//_______________________________________________________________________________________________GLimp_BeginFrame()

void	GLimp_BeginFrame (float camera_separation)
{
    if (gGLFullscreen == YES)
    {
        if ((gGLGamma != NULL && gGLGamma->value != gGLOldGamma) ||
            (gGLOverbrightGamma != NULL && gGLOverbrightGamma->value != gGLOldOverbrightGamma))
        {
            if (gGLCanSetGamma == YES)
            {
                // clamp "gl_overbright_gamma" to "0" or "1":
                if (gGLOverbrightGamma != NULL)
                {
                    if (gGLOverbrightGamma->value < 0.0f)
                    {
                        ri.Cvar_SetValue ("gl_overbright_gamma", 0);
                    }
                    else
                    {
                        if (gGLOverbrightGamma->value > 0.0f && gGLOverbrightGamma->value != 1.0f)
                        {
                            ri.Cvar_SetValue ("gl_overbright_gamma", 1);
                        }
                    }
                }
                
                // finally set the new gamma:
                if (gGLOverbrightGamma != NULL && gGLOverbrightGamma->value == 0.0f)
                {
                    static gl_gammatable_t	myNewTable;
                    UInt16 			i;
                    float			myNewGamma;
                    
                    myNewGamma = (1.4f - gGLGamma->value) * 2.5f;
                    if (myNewGamma < 1.0f)
                    {
                        myNewGamma = 1.0f;
                    }
                    else
                    {
                        if (myNewGamma > 2.25f)
                        {
                            myNewGamma = 2.25f;
                        }
                    }
                    
                    for (i = 0; i < gGLOriginalGammaTable.count; i++)
                    {
                        myNewTable.red[i]   = myNewGamma * gGLOriginalGammaTable.red[i];
                        myNewTable.green[i] = myNewGamma * gGLOriginalGammaTable.green[i];
                        myNewTable.blue[i]  = myNewGamma * gGLOriginalGammaTable.blue[i];
                    }
                    
                    CGSetDisplayTransferByTable (kCGDirectMainDisplay, gGLOriginalGammaTable.count,
                                                 myNewTable.red, myNewTable.green, myNewTable.blue);
                }
                else
                {
                    gGLOldGamma = 1.0f - gGLGamma->value;
                    if (gGLOldGamma < 0.0f)
                    {
                        gGLOldGamma = 0.0f;
                    }
                    else
                    {
                        if (gGLOldGamma >= 1.0f)
                        {
                            gGLOldGamma = 0.999f;
                        }
                    }
                    CGSetDisplayTransferByFormula (kCGDirectMainDisplay,
                                                   gGLOldGamma,
                                                   1.0f,
                                                   gGLOriginalGamma[2],
                                                   gGLOldGamma,
                                                   1.0f,
                                                   gGLOriginalGamma[5],
                                                   gGLOldGamma,
                                                   1.0f,
                                                   gGLOriginalGamma[8]);
                }
                gGLOldGamma = gGLGamma->value;
                gGLOldOverbrightGamma = gGLOverbrightGamma->value;
            }
        }
    }
}

//_________________________________________________________________________________________________GLimp_EndFrame()

void	GLimp_EndFrame (void)
{
    if (gGLContext != NULL)
    {
        [gGLContext makeCurrentContext];
        [gGLContext flushBuffer];
    }
    
    if (gGLFullscreen == NO)
    {
        if ([gGLWindow isMiniaturized])
        {
            UInt8		*myPlanes[5];
            UInt32		myStore,
                                *myTop,
                                *myBottom,
                                myRow = vid.width,
                                mySize = myRow * vid.height,
                                j;
            
            if (gGLMiniBuffer == NULL || gGLMiniWindow == NULL)
            {
                return;
            }
            
            // get the current OpenGL rendering:
            qglReadPixels (0, 0, vid.width, vid.height, GL_RGBA, GL_UNSIGNED_BYTE, gGLMiniBuffer);

            // mirror the buffer [only vertical]:
            myTop = gGLMiniBuffer;
            myBottom = myTop + mySize - myRow;
            while (myTop < myBottom)
            {
                for (j = 0; j < myRow; j++)
                {
                    myStore = myTop[j];
                    myTop[j] = myBottom[j];
                    myBottom[j] = myStore;
                }
                myTop += myRow;
                myBottom -= myRow;
            }
                
            myPlanes[0] = (UInt8 *) gGLMiniBuffer;
            
            [gGLMiniWindow lockFocus];
            NSDrawBitmap (NSMakeRect (0, 0, 128, 128), vid.width, vid.height, 8, 3, 32, vid.width * 4, NO, NO,
                          @"NSDeviceRGBColorSpace", (const UInt8 * const *) myPlanes);
            [gGLMiniWindow unlockFocus];
        
            // doesn't seem to work:
            //[gGLWindow setMiniwindowImage: gGLMiniWindow];
            
            // so use the app icon instead:
            [NSApp setApplicationIconImage: gGLMiniWindow];
            
            if (gGLIconChanged == NO && gGLIsMinimized != NULL)
            {
               gGLIsMinimized->value = 1.0f;
            }
            
            gGLIconChanged = YES;
        }
        else
        {
            GLimp_RestoreOldIcon ();
        }
    }

    GLimp_SetSwapInterval ();
    GLimp_SetAnisotropyTextureLevel ();
    GLimp_SetARBMultiSample ();
    GLimp_SetTruform ();
}

//_____________________________________________________________________________________________________GLimp_Init()

qboolean 	GLimp_Init (void *hinstance, void *hWnd)
{
    CGDisplayErr	myError;

    // for controlling mouse and minimized window:
    gGLIsMinimized = ri.Cvar_Get ("_miniwindow", "0", 0);

    // initialize the miniwindow [will not used, if alloc fails]:
    gGLMiniWindow = [[NSImage alloc] initWithSize: NSMakeSize (128, 128)];
    gGLOldIcon = [NSImage imageNamed: @"NSApplicationIcon"];

    // get the swap interval variable:
    gGLSwapInterval = ri.Cvar_Get ("gl_swapinterval", "1", 0);
    
    // get the video gamma variable:
    gGLGamma = ri.Cvar_Get ("vid_gamma", "0", 0);
    
    // get the variable for the aniostropic texture level:
    gGLTextureAnisotropyLevel = ri.Cvar_Get ("gl_anisotropic", "0", CVAR_ARCHIVE);

    // get the variable for the multisample level:
    gGLARBMultiSampleLevel = ri.Cvar_Get ("gl_arb_multisample", "0", CVAR_ARCHIVE);

    // get the variable for the truform tesselation level:
    gGLTrufomTesselationLevel = ri.Cvar_Get ("gl_truform", "-1", CVAR_ARCHIVE);
    
    // get the variable for overbright gamma:
    gGLOverbrightGamma = ri.Cvar_Get ("gl_overbright_gamma", "1", CVAR_ARCHIVE);
    
    // save the original display mode:
    gGLOriginalMode = CGDisplayCurrentMode (kCGDirectMainDisplay);

    // get the gamma:
    myError = CGGetDisplayTransferByFormula (kCGDirectMainDisplay,
                                             &gGLOriginalGamma[0],
                                             &gGLOriginalGamma[1],
                                             &gGLOriginalGamma[2],
                                             &gGLOriginalGamma[3],
                                             &gGLOriginalGamma[4],
                                             &gGLOriginalGamma[5],
                                             &gGLOriginalGamma[6],
                                             &gGLOriginalGamma[7],
                                             &gGLOriginalGamma[8]);
    
    if (myError == CGDisplayNoErr)
    {
        gGLCanSetGamma = YES;
    }
    else
    {
        gGLCanSetGamma = NO;
    }

    // get the gamma for non-overbright gamma:
    myError = CGGetDisplayTransferByTable (kCGDirectMainDisplay,
                                           CG_MAX_GAMMA_TABLE_SIZE,
                                           gGLOriginalGammaTable.red,
                                           gGLOriginalGammaTable.green,
                                           gGLOriginalGammaTable.blue,
                                           &gGLOriginalGammaTable.count);

    if (myError != CGDisplayNoErr)
    {
        gGLCanSetGamma = NO;
    }

    gGLOldGamma = 0.0f;
    gGLOldOverbrightGamma = 1.0f;
    
    return (true);
}

//_________________________________________________________________________________________________GLimp_Shutdown()

void	GLimp_Shutdown (void)
{
    // restore the old icon:
    GLimp_RestoreOldIcon ();
    
    // release the buffer of the mini window:
    if (gGLMiniBuffer != NULL)
    {
        free (gGLMiniBuffer);
        gGLMiniBuffer = NULL;
    }
            
    // release the miniwindow:
    if (gGLMiniWindow != NULL)
    {
        [gGLMiniWindow release];
        gGLMiniWindow = NULL;
    }

    // release the old icon:
    if (gGLOldIcon != NULL)
    {
        [gGLOldIcon release];
        gGLOldIcon = NULL;
    }
    
    // destroy the OpenGL context:
    GLimp_DestroyContext ();

    // get the original display mode back:
    if (gGLOriginalMode)
    {
        CGDisplaySwitchToMode (kCGDirectMainDisplay, gGLOriginalMode);
    }
}

//___________________________________________________________________________________________GLimp_DestroyContext()

void	GLimp_DestroyContext (void)
{
    // set variable states to modfied:
    gGLSwapInterval->modified = YES;
    gGLTextureAnisotropyLevel->modified = YES;
    gGLARBMultiSampleLevel->modified = YES;
    gGLTrufomTesselationLevel->modified = YES;

    // restore old gamma settings:
    if (gGLCanSetGamma == YES)
    {
            CGSetDisplayTransferByFormula (kCGDirectMainDisplay,
                                            gGLOriginalGamma[0],
                                            gGLOriginalGamma[1],
                                            gGLOriginalGamma[2],
                                            gGLOriginalGamma[3],
                                            gGLOriginalGamma[4],
                                            gGLOriginalGamma[5],
                                            gGLOriginalGamma[6],
                                            gGLOriginalGamma[7],
                                            gGLOriginalGamma[8]);
            gGLOldGamma = 0.0f;
    }

    // clean up the OpenGL context:
    if (gGLContext != NULL)
    {
        [gGLContext makeCurrentContext];
        [NSOpenGLContext clearCurrentContext];
        [gGLContext clearDrawable];
        [gGLContext release];
        gGLContext = NULL;
    }

    // close the old window:
    if (gGLWindow != NULL)
    {
        [gGLWindow close];
        gGLWindow = NULL;
    }
    
    // close the content view:
    if (gGLView != NULL)
    {
        [gGLView release];
        gGLView = NULL;
    }
}

//______________________________________________________________________________________________GLimp_AppActivate()

rserr_t     GLimp_SetMode (unsigned int *theWidth, unsigned int *theHeight, int theMode, qboolean theFullscreen)
{
    int		myWidth, myHeight;
    float	myNewRefreshRate = 0;
    cvar_t	*myRefreshRate;
    
    ri.Con_Printf (PRINT_ALL, "Initializing OpenGL display\n");

    ri.Con_Printf (PRINT_ALL, "...setting mode %d:", theMode );

    if (!ri.Vid_GetModeInfo (&myWidth, &myHeight, theMode))
    {
        ri.Con_Printf (PRINT_ALL, " invalid mode\n");
        return (rserr_invalid_mode);
    }

    myRefreshRate = ri.Cvar_Get ("vid_refreshrate", "0", 0);
    if (myRefreshRate != NULL)
    {
        myNewRefreshRate = myRefreshRate->value;
    }

    ri.Con_Printf (PRINT_ALL, " %d x %d\n", myWidth, myHeight);
    
    GLimp_DestroyContext ();
    
    *theWidth = myWidth;
    *theHeight = myHeight;

    GLimp_InitGraphics (&myWidth, &myHeight, myNewRefreshRate, theFullscreen);

    ri.Vid_NewWindow (myWidth, myHeight);

    return (rserr_ok);
}

//__________________________________________________________________________________________GLimp_SetSwapInterval()

void	GLimp_SetSwapInterval (void)
{
    // change the swap interval if the value changed:
    if (gGLSwapInterval == NULL)
    {
        return;
    }

    if (gGLSwapInterval->modified == YES)
    {
        UInt32		myCurSwapInterval = (UInt32) gGLSwapInterval->value;
        
        if (myCurSwapInterval > 1)
        {
            myCurSwapInterval = 1;
            ri.Cvar_SetValue ("gl_swapinterval", 1.0f);
        }
        else
        {
            if (myCurSwapInterval < 0)
            {
                myCurSwapInterval = 0;
                ri.Cvar_SetValue ("gl_swapinterval", 0.0f);
            }
        }
        [gGLContext makeCurrentContext];
        CGLSetParameter (CGLGetCurrentContext (), kCGLCPSwapInterval, &myCurSwapInterval);
        gGLSwapInterval->modified = NO;
    }
}

//_______________________________________________________________________________GLimp_SetAnisotropyTextureLevel()

void	GLimp_SetAnisotropyTextureLevel (void)
{
    extern GLfloat	qglMaxAnisotropyTextureLevel,
                        qglCurAnisotropyTextureLevel;
                        
    if (gGLTextureAnisotropyLevel == NULL || gGLTextureAnisotropyLevel->modified == NO)
    {
        return;
    }
    
    if (gGLTextureAnisotropyLevel->value == 0.0f)
    {
        qglCurAnisotropyTextureLevel = 1.0f;
    }
    else
    {
        qglCurAnisotropyTextureLevel = qglMaxAnisotropyTextureLevel;
    }

    gGLTextureAnisotropyLevel->modified = NO;
}

//_______________________________________________________________________________________________GLimp_SetTruform()

void	GLimp_SetTruform (void)
{
    if (gGLTruformAvailable == NO ||
        gGLTrufomTesselationLevel == NULL ||
        gGLTrufomTesselationLevel->modified == NO)
    {
        return;
    }
    else
    {
        SInt32	myPNTriangleLevel = gGLTrufomTesselationLevel->value;

        if (myPNTriangleLevel >= 0)
        {
            if (myPNTriangleLevel > 7)
            {
                myPNTriangleLevel = 7;
                ri.Cvar_SetValue ("gl_truform", myPNTriangleLevel);
                ri.Con_Printf (PRINT_ALL, "Clamping to max. pntriangle level 7!\n");
                ri.Con_Printf (PRINT_ALL, "value < 0  : Disable Truform\n");
                ri.Con_Printf (PRINT_ALL, "value 0 - 7: Enable Truform with the specified tesselation level\n");
            }
            
            // enable pn_triangles. lightning required due to a bug of OpenGL!
            qglEnable (GL_PN_TRIANGLES_ATIX);
            qglEnable (GL_LIGHTING);
            qglLightModelfv (GL_LIGHT_MODEL_AMBIENT, gGLTruformAmbient);
            qglEnable (GL_COLOR_MATERIAL);
        
            // point mode:
            //qglPNTrianglesiATIX (GL_PN_TRIANGLES_POINT_MODE_ATIX, GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATIX);
            qglPNTrianglesiATIX (GL_PN_TRIANGLES_POINT_MODE_ATIX, GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATIX);
            
            // normal mode (no normals used at all by Quake):
            //qglPNTrianglesiATIX (GL_PN_TRIANGLES_NORMAL_MODE_ATIX, GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATIX);
            qglPNTrianglesiATIX (GL_PN_TRIANGLES_NORMAL_MODE_ATIX, GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATIX);
        
            // tesselation level:
            qglPNTrianglesiATIX (GL_PN_TRIANGLES_TESSELATION_LEVEL_ATIX, myPNTriangleLevel);
        }
        else
        {
            if (myPNTriangleLevel != -1)
            {
                myPNTriangleLevel = -1;
                ri.Cvar_SetValue ("gl_truform", myPNTriangleLevel);
            }
            qglDisable (GL_PN_TRIANGLES_ATIX);
            qglDisable (GL_LIGHTING);
        }
        gGLTrufomTesselationLevel->modified = NO;
    }
}

//________________________________________________________________________________________GLimp_SetARBMultiSample()

void	GLimp_SetARBMultiSample (void)
{
    if (gGLARBMultiSampleLevel->modified == NO)
    {
        return;
    }
    
    if (gGLMaxARBMultiSampleBuffers <= 0)
    {
        ri.Con_Printf (PRINT_ALL, "No ARB_multisample extension available!\n");
        ri.Cvar_SetValue ("gl_arb_multisample", 0);
        gGLARBMultiSampleLevel->modified = NO;
    }
    else
    {
        float		mySampleLevel = gGLARBMultiSampleLevel->value;
        BOOL		myRestart = NO;
        
        if (gGLARBMultiSampleLevel->value != gGLCurARBMultiSamples)
        {
            if ((mySampleLevel == 0.0f ||
                 mySampleLevel == 4.0f ||
                 mySampleLevel == 8.0f ||
                 mySampleLevel == gGLMaxARBMultiSamples) &&
                mySampleLevel <= gGLMaxARBMultiSamples)
            {
                myRestart = YES;
            }
            else
            {
//                float	myOldValue;
//                
//                qglGetFloatv (GL_SAMPLES_ARB, &myOldValue);
//                ri.Cvar_SetValue ("gl_arb_multisample", myOldValue);
//                ri.Con_Printf (PRINT_ALL, "Invalid multisample level. Reverting to: %f.\n", myOldValue);
                gGLARBMultiSampleLevel->value = gGLCurARBMultiSamples;
                ri.Con_Printf (PRINT_ALL, "Invalid multisample level. Reverting to: %d.\n", gGLCurARBMultiSamples);
            }
        }
        
        gGLARBMultiSampleLevel->modified = NO;
        
        if (myRestart == YES)
        {
            ri.Cmd_ExecuteText (EXEC_NOW, "vid_restart\n");
        }
    }
}

//___________________________________________________________________________________GLimp_CheckForMultiSampleARB()

void	GLimp_CheckForARBMultiSample (void)
{
    CGLRendererInfoObj			myRendererInfo;
    CGLError				myError;
    UInt64				myDisplayMask;
    long				myCount,
                                        myIndex,
                                        mySampleBuffers,
                                        mySamples;

    // reset out global values:
    gGLMaxARBMultiSampleBuffers = 0;
    gGLMaxARBMultiSamples = 0;
    
    // retrieve the renderer info for the main display:
    myDisplayMask = CGDisplayIDToOpenGLDisplayMask (kCGDirectMainDisplay);
    myError = CGLQueryRendererInfo (myDisplayMask, &myRendererInfo, &myCount);
    
    if (myError == kCGErrorSuccess)
    {
        // loop through all renderers:
        for (myIndex = 0; myIndex < myCount; myIndex++)
        {
            // check if the current renderer supports sample buffers:
            myError = CGLDescribeRenderer (myRendererInfo, myIndex, kCGLRPMaxSampleBuffers, &mySampleBuffers);
            if (myError == kCGErrorSuccess && mySampleBuffers > 0)
            {
                // retrieve the number of samples supported by the current renderer:
                myError = CGLDescribeRenderer (myRendererInfo, myIndex, kCGLRPMaxSamples, &mySamples);
                if (myError == kCGErrorSuccess && mySamples > gGLMaxARBMultiSamples)
                {
                    gGLMaxARBMultiSampleBuffers = mySampleBuffers;
                    
                    // The ATI Radeon/PCI drivers report a value of "4", but "8" is maximum...
                    gGLMaxARBMultiSamples = mySamples << 1;
                }
            }
        }
        
        // get rid of the renderer info:
        CGLDestroyRendererInfo (myRendererInfo);
    }
    
    // shouldn't happen, but who knows...
    if (gGLMaxARBMultiSamples <= 1)
    {
        gGLMaxARBMultiSampleBuffers = 0;
        gGLMaxARBMultiSamples = 0;
    }
    
    // because of the Radeon issue above...
    if (gGLMaxARBMultiSamples > 8)
    {
        gGLMaxARBMultiSamples = 8;
    }
}

//______________________________________________________________________________________GLimp_CreateGLPixelFormat()

NSOpenGLPixelFormat *	GLimp_CreateGLPixelFormat (int theDepth, Boolean theFullscreen)
{
    NSOpenGLPixelFormat			*myPixelFormat;
    NSOpenGLPixelFormatAttribute	myAttributeList[16];
    UInt8				i = 0;

    if (gGLMaxARBMultiSampleBuffers > 0 &&
        gGLARBMultiSampleLevel->value != 0 &&
        (gGLARBMultiSampleLevel->value == 4.0f ||
         gGLARBMultiSampleLevel->value == 8.0f ||
         gGLARBMultiSampleLevel->value == gGLMaxARBMultiSamples))
    {
        gGLCurARBMultiSamples = gGLARBMultiSampleLevel->value;
        myAttributeList[i++] = NSOpenGLPFASampleBuffers;
        myAttributeList[i++] = gGLMaxARBMultiSampleBuffers;
        myAttributeList[i++] = NSOpenGLPFASamples;
        myAttributeList[i++] = gGLCurARBMultiSamples;
    }
    else
    {
        gGLARBMultiSampleLevel->value = 0.0f;
        gGLCurARBMultiSamples = 0;
    }
    
    // are we running fullscreen or windowed?
    if (theFullscreen)
    {
        myAttributeList[i++] = NSOpenGLPFAFullScreen;
    }
    else
    {
        myAttributeList[i++] = NSOpenGLPFAWindow;
    }

    // choose the main display automatically:
    myAttributeList[i++] = NSOpenGLPFAScreenMask;
    myAttributeList[i++] = CGDisplayIDToOpenGLDisplayMask (kCGDirectMainDisplay);

    // we need a double buffered context:
    myAttributeList[i++] = NSOpenGLPFADoubleBuffer;

    // set the "accelerated" attribute only if we don't allow the software renderer!
    if ((ri.Cvar_Get ("gl_allow_software", "0", 0))->value == 0.0f)
    {
        myAttributeList[i++] = NSOpenGLPFAAccelerated;
    }
    
    myAttributeList[i++] = NSOpenGLPFAColorSize;
    myAttributeList[i++] = theDepth;

    myAttributeList[i++] = NSOpenGLPFADepthSize;
    myAttributeList[i++] =  1;

    myAttributeList[i++] = NSOpenGLPFANoRecovery;

    myAttributeList[i++] = 0;

    myPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes: myAttributeList];

    return (myPixelFormat);
}

//_____________________________________________________________________________________________GLimp_InitGraphics()

Boolean	GLimp_InitGraphics (int *theWidth, int *theHeight, float theRefreshRate, Boolean theFullscreen)
{
    NSOpenGLPixelFormat		*myPixelFormat = NULL;
    int				myDisplayDepth;

    GLimp_RestoreOldIcon ();
    
    if (theFullscreen)
    {
        CFDictionaryRef		myDisplayMode;
        boolean_t		myExactMatch;

        if (CGDisplayIsCaptured (kCGDirectMainDisplay) != true)
        {
            GL_CAPTURE_DISPLAYS();
        }
        
        // force 16bit OpenGL display?
        if ((ri.Cvar_Get ("gl_force16bit", "0", 0))->value != 0.0f)
        {
            myDisplayDepth = 16;
        }
        else
        {
            myDisplayDepth = 32;
        }
        
        // get the requested mode:
        if (theRefreshRate > 0)
        {
            myDisplayMode = CGDisplayBestModeForParametersAndRefreshRate (kCGDirectMainDisplay, myDisplayDepth,
                                                                          *theWidth, *theHeight, theRefreshRate,
                                                                          &myExactMatch);
        }
        else
        {
            myDisplayMode = CGDisplayBestModeForParameters (kCGDirectMainDisplay, myDisplayDepth, *theWidth,
                                                            *theHeight, &myExactMatch);
        }

        // got we an exact mode match? if not report the new resolution again:
        if (myExactMatch == NO)
        {
            *theWidth = [[(NSDictionary *) myDisplayMode objectForKey: (NSString *) kCGDisplayWidth] intValue];
            *theHeight = [[(NSDictionary *) myDisplayMode objectForKey: (NSString *) kCGDisplayHeight] intValue];
            ri.Con_Printf (PRINT_ALL, "can\'t switch to requested mode. using %d x %d.\n", *theWidth, *theHeight);
        }

        // switch to the new display mode:
        if (CGDisplaySwitchToMode (kCGDirectMainDisplay, myDisplayMode) != kCGErrorSuccess)
        {
            ri.Sys_Error (ERR_FATAL, "Can\'t switch to the selected mode!\n");
        }

        myDisplayDepth = [[(NSDictionary *) myDisplayMode objectForKey: (id) kCGDisplayBitsPerPixel] intValue];
    }
    else
    {
        if (gGLOriginalMode)
        {
            CGDisplaySwitchToMode (kCGDirectMainDisplay, gGLOriginalMode);
        }
    
        myDisplayDepth = [[(NSDictionary *)  gGLOriginalMode objectForKey: (id) kCGDisplayBitsPerPixel] intValue];
    }
    
    // check if we have access to sample buffers:
    GLimp_CheckForARBMultiSample ();
    
    // get the pixel format [the loop is just for sample buffer failures]:
    while (myPixelFormat == NULL)
    {
        if (gGLARBMultiSampleLevel->value < 0.0f)
            gGLARBMultiSampleLevel->value = 0.0f;

        if ((myPixelFormat = GLimp_CreateGLPixelFormat (myDisplayDepth, theFullscreen)) == NULL)
        {
            if (gGLARBMultiSampleLevel->value == 0.0f)
            {
                ri.Sys_Error (ERR_FATAL,"Unable to find a matching pixelformat. Please try other displaymode(s).");
            }
            gGLARBMultiSampleLevel->value -= 4.0;
        }
    }

    // initialize the OpenGL context:
    if (!(gGLContext = [[NSOpenGLContext alloc] initWithFormat: myPixelFormat shareContext: nil]))
    {
        ri.Sys_Error (ERR_FATAL, "Unable to create an OpenGL context. Please try other displaymode(s).");
    }

    // get rid of the pixel format:
    [myPixelFormat release];

    if (theFullscreen)
    {
        // attach the OpenGL context to fullscreen:
        if (CGLSetFullScreen ([gGLContext cglContext]) != CGDisplayNoErr)
        {
            ri.Sys_Error (ERR_FATAL, "Unable to use the selected displaymode for fullscreen OpenGL.");
        }
    }
    else
    {
        NSRect 		myContentRect;
        cvar_t		*myVidPosX;
        cvar_t		*myVidPosY;
        
        // get the window position:
        myVidPosX = ri.Cvar_Get ("vid_xpos", "0", 0);
        myVidPosY = ri.Cvar_Get ("vid_ypos", "0", 0);

        // setup the window according to our settings:
        myContentRect = NSMakeRect (myVidPosX->value, myVidPosY->value, *theWidth, *theHeight);
        gGLWindow = [[NSWindow alloc] initWithContentRect: myContentRect
                                                styleMask: NSTitledWindowMask | NSMiniaturizableWindowMask
                                                  backing: NSBackingStoreBuffered
                                                    defer: NO];

        if (gGLWindow == NULL)
        {
            ri.Sys_Error (ERR_FATAL, "Unable to create window!\n");
        }

        [gGLWindow setTitle: @"Quake II"];

        // setup the content view:
        myContentRect.origin.x = myContentRect.origin.y = 0;
        myContentRect.size.width = vid.width;
        myContentRect.size.height = vid.height;
        gGLView = [[Quake2GLView alloc] initWithFrame: myContentRect];
        if (gGLView == NULL)
        {
            ri.Sys_Error (ERR_FATAL, "Unable to create content view!\n");
        }

        // setup the view for tracking the window location:
        [gGLWindow setContentView: gGLView];
        [gGLWindow makeFirstResponder: gGLView];
        [gGLWindow setDelegate: gGLView];

        // attach the OpenGL context to the window:
        [gGLContext setView: [gGLWindow contentView]];
        
        // finally show the window:
        [NSApp activateIgnoringOtherApps: YES];
        [gGLWindow display];        
        [gGLWindow flushWindow];
        [gGLWindow setAcceptsMouseMovedEvents: YES];
        
        if (CGDisplayIsCaptured (kCGDirectMainDisplay) == true)
        {
            GL_RELEASE_DISPLAYS ();
        }

        [gGLWindow makeKeyAndOrderFront: nil];
        [gGLWindow makeMainWindow];
    }
    
    // Lock the OpenGL context to the refresh rate of the display [for clean rendering], if desired:
    [gGLContext makeCurrentContext];

    // set the buffers for the mini window [if buffer is available, will be checked later]:
    if (gGLMiniBuffer != NULL)
    {
        free (gGLMiniBuffer);
        gGLMiniBuffer = NULL;
    }
    
    gGLMiniBuffer = malloc (*theWidth * *theHeight * 4);

    // last clean up:
    vid.width = *theWidth;
    vid.height = *theHeight;
    
    gGLFullscreen = theFullscreen;

    return (true);
}

//______________________________________________________________________________________________GLimp_AppActivate()

void	GLimp_AppActivate (qboolean active)
{
    // not required!
}

#pragma mark -

//___________________________________________________________________________________iMPLEMENTATION_NSOpenGLContext

@implementation NSOpenGLContext (CGLContextAccess)

//_______________________________________________________________________________________________________cglContext

- (CGLContextObj) cglContext;
{
    return (_contextAuxiliary);
}

@end

//______________________________________________________________________________________iMPLEMENTATION_Quake2GLView

@implementation Quake2GLView

//____________________________________________________________________________________________acceptsFirstResponder

-(BOOL) acceptsFirstResponder
{
    return (YES);
}

//___________________________________________________________________________________________________windowDidMove:

- (void)windowDidMove: (NSNotification *)note
{
    NSRect	myRect;

    myRect = [gGLWindow frame];
    ri.Cmd_ExecuteText (EXEC_NOW, va ("vid_xpos %i", (int) myRect.origin.x + 1));
    ri.Cmd_ExecuteText (EXEC_NOW, va ("vid_ypos %i", (int) myRect.origin.y + 1));
}

@end

//______________________________________________________________________________________________________________eOF
