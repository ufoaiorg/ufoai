//______________________________________________________________________________________________________________nFO
// "swimp_osx.c" - MacOS X software renderer.
//
// Written by:	awe				[mailto:awe@fruitz-of-dojo.de].
//		©2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quake IIª is copyrighted by id software	[http://www.idsoftware.com].
//
// Version History:
// v1.0.3: Screenshots are now saved as TIFF instead of PCX files [see "r_misc.c"].
// v1.0.0: Initial release.
//_________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <AppKit/AppKit.h>

#include "r_local.h"

#pragma mark -

//___________________________________________________________________________________________________________mACROS

#pragma mark =Macros=

// We could iterate through active displays and capture them each, to avoid the CGReleaseAllDisplays() bug,
// but this would result in awfully switches between renderer selections, since we would have to clean-up the
// captured device list each time...

#ifdef CAPTURE_ALL_DISPLAYS

#define VID_CAPTURE_DISPLAYS()	CGCaptureAllDisplays ()
#define VID_RELEASE_DISPLAYS()	CGReleaseAllDisplays ()

#else

#define VID_CAPTURE_DISPLAYS()	CGDisplayCapture (kCGDirectMainDisplay)
#define VID_RELEASE_DISPLAYS()	CGDisplayRelease (kCGDirectMainDisplay)

#endif /* CAPTURE_ALL_DISPLAYS */

#pragma mark -

//_______________________________________________________________________________________________________iNTERFACES

#pragma mark =ObjC Interfaces=

@interface Quake2View : NSView
@end

#pragma mark -

//________________________________________________________________________________________________________vARIABLES

#pragma mark =Variables=

static Boolean			gVidFullscreen = NO,
                                gVidIconChanged = NO;
static UInt16			gVidWidth;
static CFDictionaryRef		gVidOriginalMode;
static Quake2View		*gVidView = NULL;
static CGDirectPaletteRef 	gVidPalette = NULL;
static NSWindow			*gVidWindow = NULL;
static NSImage			*gVidMiniWindow = NULL,
                                *gVidOldIcon = NULL;
static UInt32			*gVidBuffer = NULL,
                                gVidRGBAPalette[256];
static cvar_t			*gVidIsMinimized = NULL;

#pragma mark -

//______________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

inline	UInt16	 SWimp_GetRowBytes (void);
inline	UInt64 * SWimp_GetDisplayBaseAddress (void);
inline	void	 SWimp_RestoreOldIcon (void);

static	void	 SWimp_BlitWindow (void);
static	void	 SWimp_BlitFullscreen1x1 (void);
static	void	 SWimp_BlitFullscreen2x2 (void);
static	void	 SWimp_CloseWindow (void);

#pragma mark -

//____________________________________________________________________________________SWimp_GetDisplayBaseAddress()

UInt64 * SWimp_GetDisplayBaseAddress (void)
{
    return ((UInt64 *) CGDisplayBaseAddress(kCGDirectMainDisplay));
}

//______________________________________________________________________________________________SWimp_GetRowBytes()

UInt16	SWimp_GetRowBytes (void)
{
    return (CGDisplayBytesPerRow (kCGDirectMainDisplay));
}

//___________________________________________________________________________________________SWimp_RestoreOldIcon()

void	SWimp_RestoreOldIcon (void)
{
    if (gVidIconChanged == YES)
    {
        if (gVidOldIcon != NULL)
        {
            [NSApp setApplicationIconImage: gVidOldIcon];
            if (gVidIsMinimized != NULL)
            {
                gVidIsMinimized->value = 0.0f;
            }
        }
        gVidIconChanged = NO;
    }
}

//_______________________________________________________________________________________________SWimp_BlitWindow()

void	SWimp_BlitWindow (void)
{
    UInt32		i, myVidSize;
    UInt8		*myPlanes[5];

    // any view available?
    if (gVidView == NULL || gVidWindow == NULL)
    {
        return;
    }

    // translate 8 bit to 24 bit color:
    myVidSize = vid.width * vid.height;
    for (i=0 ; i < myVidSize; i++)
    {
        gVidBuffer[i] = gVidRGBAPalette[vid.buffer[i]];
    }

    myPlanes[0] = (UInt8 *) gVidBuffer;

    if ([gVidWindow isMiniaturized])
    {
        // security:
        if (gVidMiniWindow == NULL)
        {
            return;
        }
        
        // draw into the miniwindow image:
        [gVidMiniWindow lockFocus];
        NSDrawBitmap (NSMakeRect (0, 0, 128, 128), vid.width, vid.height, 8, 3, 32, vid.width * 4, NO, NO,
                      @"NSDeviceRGBColorSpace", (const UInt8 * const *) myPlanes);
        [gVidMiniWindow unlockFocus];
        
        // doesn't seem to work:
        //[gVidWindow setMiniwindowImage: gVidMiniWindow];
        // so use the app icon instead:
        [NSApp setApplicationIconImage: gVidMiniWindow];

        if (gVidIconChanged == NO && gVidIsMinimized != NULL)
        {
            gVidIsMinimized->value = 1.0f;
        }
        gVidIconChanged = YES;
    }
    else
    {
        // restore the old icon, if the window was miniaturized:
        SWimp_RestoreOldIcon ();
        
        // draw the bitmap:
        [gVidView lockFocus];
        NSDrawBitmap ([gVidView bounds], vid.width, vid.height, 8, 3, 32, vid.width * 4, NO, NO,
                      @"NSDeviceRGBColorSpace", (const UInt8 * const *) myPlanes);
        [gVidView unlockFocus];
        [gVidWindow flushWindow];
    }
}

//________________________________________________________________________________________SWimp_BlitFullscreen1x1()

void	SWimp_BlitFullscreen1x1 (void)
{
    UInt64	myRowBytes = (SWimp_GetRowBytes () - vid.width) / sizeof (UInt64);
    UInt64	*myScreen = SWimp_GetDisplayBaseAddress (),
                *myOffScreen = (UInt64 *) vid.buffer;

    // just security:
    if (myScreen == NULL || myOffScreen == NULL)
    {
        ri.Sys_Error( ERR_FATAL, "Bad video buffer!");
    }

    // blit it [1x1]:
    if (myRowBytes == 0)
    {
        UInt32		myWidth = vid.width * vid.height / sizeof (UInt64),
                        i;
        
        for (i = 0; i < myWidth; i++)
        {
            *(myScreen++) = *(myOffScreen++); 
        }
    }
    else
    {
        UInt32		myWidth = vid.width / sizeof (UInt64),
                        x , y;
        
        for (y = 0; y < vid.height; y++)
        {
            for (x = 0; x < myWidth; x++)
            {
                *(myScreen++) = *(myOffScreen++); 
            }
            myScreen += myRowBytes;
        }
    }
}

//________________________________________________________________________________________SWimp_BlitFullscreen2x2()

void	SWimp_BlitFullscreen2x2 (void)
{
    UInt8	*myOffScreen = vid.buffer;
    UInt32	myWidthLoop = vid.width >> 2,
                i, j;
    UInt64	myRowBytes = (SWimp_GetRowBytes () - gVidWidth) / sizeof(UInt64),
                *myScreenLo = SWimp_GetDisplayBaseAddress (),
                *myScreenHi = myScreenLo + gVidWidth / sizeof (UInt64) + myRowBytes,
                myPixels;

    // just security:
    if (myScreenLo == NULL || myScreenHi == NULL || myOffScreen == NULL)
    {
        ri.Sys_Error( ERR_FATAL, "Bad video buffer!");
    }

    myRowBytes = ((SWimp_GetRowBytes () << 1) - gVidWidth) / sizeof (UInt64);

    // blit it [2x2]:
    for (i = 0; i < vid.height; i++)
    {
        for (j = 0; j < myWidthLoop; j++)
        {
            myPixels  = ((UInt64) (*myOffScreen++) << 48);
            myPixels |= ((UInt64) (*(myOffScreen++)) << 32);
            myPixels |= ((UInt32) (*(myOffScreen++)) << 16);
            myPixels |= (*(myOffScreen++));
            myPixels |= (myPixels << 8);
            
            *myScreenLo++ = myPixels;
            *myScreenHi++ = myPixels;
        }
        myScreenLo += myRowBytes;
        myScreenHi += myRowBytes;
    }
}

//_________________________________________________________________________________________________SWimp_EndFrame()

void	SWimp_EndFrame (void)
{
    // are we in windowed mode?
    if (gVidFullscreen == NO)
    {
        SWimp_BlitWindow ();
        return;
    }

    // wait for the VBL:
    CGDisplayWaitForBeamPositionOutsideLines (kCGDirectMainDisplay, 0, 1);

    // change the palette:
    if (gVidPalette != NULL)
    {
        CGDisplaySetPalette (kCGDirectMainDisplay, gVidPalette);
        free (gVidPalette);
        gVidPalette = NULL;
    }

    // blit the video to the screen:
    if (gVidWidth == vid.width)
    {
        SWimp_BlitFullscreen1x1 ();
    }
    else
    {
        SWimp_BlitFullscreen2x2 ();
    }
}

//_______________________________________________________________________________________SWimp_SetPaletteWindowed()

void	SWimp_SetPaletteWindowed (const unsigned char *thePalette)
{
    UInt8	*myPalette = (UInt8 *) gVidRGBAPalette,
                i = 0;

    do
    {
        myPalette[0] = thePalette[0];
       	myPalette[1] = thePalette[1];
        myPalette[2] = thePalette[2];
        myPalette[3] = 0xff;
        
        myPalette += 4;
        thePalette += 4;
        i++;
    } while (i != 0);
}

//_______________________________________________________________________________________________SWimp_SetPalette()

void	SWimp_SetPalette (const unsigned char *thePalette)
{
    CGDeviceColor 	mySampleTable[256];
    UInt16		i;

    // was a palette submitted?
    if (thePalette == NULL)
    {
        //return;
        thePalette = (const unsigned char *) sw_state.currentpalette;
    }

    // are we in windowed mode?
    if (gVidFullscreen == NO)
    {
        SWimp_SetPaletteWindowed (thePalette);
        return;
    }
    
    // check if the display can set the palette:
    if (CGDisplayCanSetPalette(kCGDirectMainDisplay) == 0)
    {
        ri.Con_Printf (PRINT_ALL, "Can\'t set palette...\n");
        return;
    }
    
    // convert the palette to float:
    for (i = 0; i < 256; i++)
    {
        mySampleTable[i].red = (float) thePalette[i << 2] / 256.0f;
        mySampleTable[i].green = (float) thePalette[(i << 2) + 1] / 256.0f;
        mySampleTable[i].blue = (float) thePalette[(i << 2) + 2] / 256.0f;
    }
    
    // create a palette for core graphics:
    gVidPalette = CGPaletteCreateWithSamples (mySampleTable, 256);
    if (gVidPalette == NULL)
    {
        ri.Con_Printf (PRINT_ALL, "Can\'t create palette...\n");
        return;
    }
}

//_____________________________________________________________________________________________________SWimp_Init()

int	SWimp_Init (void *hInstance, void *wndProc)
{
    // for controlling mouse and minimized window:
    gVidIsMinimized = ri.Cvar_Get ("_miniwindow", "0", 0);
    
    // save the original display mode:
    gVidOriginalMode = CGDisplayCurrentMode (kCGDirectMainDisplay);

    // initialize the miniwindow [will not used, if alloc fails]:
    gVidMiniWindow = [[NSImage alloc] initWithSize: NSMakeSize (128, 128)];
    gVidOldIcon = [NSImage imageNamed: @"NSApplicationIcon"];
    
    return (0);
}

//_________________________________________________________________________________________________SWimp_ShutDown()

void	SWimp_Shutdown (void)
{
    // restore the old icon, if the window was miniaturized:
    SWimp_RestoreOldIcon ();
        
    // get the original display mode back:
    if (gVidOriginalMode)
    {
        CGDisplaySwitchToMode (kCGDirectMainDisplay, gVidOriginalMode);
    }

    // release the miniwindow:
    if (gVidMiniWindow != NULL)
    {
        [gVidMiniWindow release];
        gVidMiniWindow = NULL;
    }

    // release the old icon:
    if (gVidOldIcon != NULL)
    {
        [gVidOldIcon release];
        gVidOldIcon = NULL;
    }

    // close the window if available:
    if (gVidFullscreen == NO)
    {
        SWimp_CloseWindow ();
    }

    // free the offscreen buffer:
    if (vid.buffer)
    {
        free(vid.buffer);
        vid.buffer = NULL;
    }
}

//______________________________________________________________________________________________SWimp_CloseWindow()

void	SWimp_CloseWindow (void)
{
    SWimp_RestoreOldIcon ();

    // close the old window:
    if (gVidWindow != NULL)
    {
        [gVidWindow close];
        gVidWindow = NULL;
    }
    
    // remove the old view:
    if (gVidView != NULL)
    {
        [gVidView release];
        gVidView = NULL;
    }
    
    // free the RGBA buffer:
    if (gVidBuffer != NULL)
    {
        free (gVidBuffer);
        gVidBuffer = NULL;
    }
}

//__________________________________________________________________________________________________SWimp_SetMode()

rserr_t	SWimp_SetMode (int *theWidth, int *theHeight, int theMode, qboolean theFullscreen)
{
    ri.Con_Printf (PRINT_ALL, "setting mode %d:", theMode);

    if (!ri.Vid_GetModeInfo (theWidth, theHeight, theMode))
    {
	ri.Con_Printf (PRINT_ALL, " invalid mode\n");
	return (rserr_invalid_mode);
    }

    ri.Con_Printf (PRINT_ALL, " %d %d\n", *theWidth, *theHeight);

    vid.width = vid.rowbytes = *theWidth;
    vid.height = *theHeight;

    if (theFullscreen == true)
    {
        CFDictionaryRef		myDisplayMode;
        Boolean			myVideoIsZoomed;
        boolean_t		myExactMatch;
        UInt16			myVidHeight;
        float			myNewRefreshRate = 0;
        cvar_t			*myRefreshRate;
        
        // get the refresh rate set by Vid_GetModeInfo ():
        myRefreshRate = ri.Cvar_Get ("vid_refreshrate", "0", 0);
        if (myRefreshRate != NULL)
        {
            myNewRefreshRate = myRefreshRate->value;
        }
        
        // remove the old window, if available:
        if (CGDisplayIsCaptured (kCGDirectMainDisplay) == false)
        {
            VID_CAPTURE_DISPLAYS();
        }
        SWimp_CloseWindow ();

        ri.Vid_NewWindow (vid.width, vid.height);

        if (vid.width < 640)
        {
            gVidWidth = vid.width << 1;
            myVidHeight = vid.height << 1;
            myVideoIsZoomed = YES;
        }
        else
        {
            gVidWidth = vid.width;
            myVidHeight = vid.height;
            myVideoIsZoomed = NO;
        }
    
        // switch to the new display mode:
                // get the requested mode:
        if (myNewRefreshRate > 0)
        {
            myDisplayMode = CGDisplayBestModeForParametersAndRefreshRate (kCGDirectMainDisplay, 8, gVidWidth,
                                                                          myVidHeight, myNewRefreshRate,
                                                                          &myExactMatch);
        }
        else
        {
            myDisplayMode = CGDisplayBestModeForParameters (kCGDirectMainDisplay, 8, gVidWidth, myVidHeight,
                                                            &myExactMatch);
        }
        
        // got we an exact mode match? if not report the new resolution again:
        if (myExactMatch == NO)
        {
            gVidWidth = [[(NSDictionary *) myDisplayMode objectForKey: (NSString *) kCGDisplayWidth] intValue];
            myVidHeight = [[(NSDictionary *) myDisplayMode objectForKey:(NSString *) kCGDisplayHeight] intValue];

            if (myVideoIsZoomed == YES)
            {
                vid.width = gVidWidth >> 1;
                vid.height =  myVidHeight >> 1;
            }
            else
            {
                vid.width = gVidWidth;
                vid.height = myVidHeight;
            }

            *theWidth = vid.rowbytes = vid.width;
            *theHeight = vid.height;

            ri.Vid_NewWindow (vid.width, vid.height);
            ri.Con_Printf (PRINT_ALL, "can\'t switch to mode %d. using %d %d.\n", theMode, *theWidth, *theHeight);
        }

        if (CGDisplaySwitchToMode (kCGDirectMainDisplay, myDisplayMode) != kCGErrorSuccess)
        {
            ri.Sys_Error( ERR_FATAL, "Can\'t switch to the selected mode!\n");
        }
    
        gVidFullscreen = YES;
    }
    else
    {
        NSRect		myContentRect;
        cvar_t		*myVidPosX;
        cvar_t		*myVidPosY;
        
        gVidWidth = vid.width;

        // get the window position:
        myVidPosX = ri.Cvar_Get ("vid_xpos", "0", 0);
        myVidPosY = ri.Cvar_Get ("vid_ypos", "0", 0);
        
        SWimp_CloseWindow ();
        CGDisplaySwitchToMode (kCGDirectMainDisplay, gVidOriginalMode);

        ri.Vid_NewWindow (vid.width, vid.height);

        // open the window:
        myContentRect = NSMakeRect (myVidPosX->value, myVidPosY->value, vid.width, vid.height);
        gVidWindow = [[NSWindow alloc] initWithContentRect: myContentRect
                                                 styleMask: NSTitledWindowMask | NSMiniaturizableWindowMask
                                                   backing: NSBackingStoreBuffered
                                                     defer: NO];

        if (gVidWindow == NULL)
        {
            ri.Sys_Error (ERR_FATAL, "Unable to create window!\n");
        }

        [gVidWindow setTitle: @"Quake II"];

        // setup the content view:
        myContentRect.origin.x = myContentRect.origin.y = 0;
        myContentRect.size.width = vid.width;
        myContentRect.size.height = vid.height;
        gVidView = [[Quake2View alloc] initWithFrame: myContentRect];
        
        if (gVidView == NULL)
        {
            ri.Sys_Error (ERR_FATAL, "Unable to create content view!\n");
        }

        [gVidWindow setContentView: gVidView];
        [gVidWindow makeFirstResponder: gVidView];
        [gVidWindow setDelegate: gVidView];
        
        [NSApp activateIgnoringOtherApps: YES];
        [gVidWindow display];
        [gVidWindow setAcceptsMouseMovedEvents: YES];

        // obtain window buffer:
        gVidBuffer = malloc (vid.rowbytes * vid.height * 4);
        if (gVidBuffer == NULL)
        {
            ri.Sys_Error (ERR_FATAL, "Unabled to allocate the window buffer!\n");
        }

        // release displays:
        if (CGDisplayIsCaptured (kCGDirectMainDisplay) == true)
        {
            VID_RELEASE_DISPLAYS ();
        }

        [gVidWindow makeKeyAndOrderFront: nil];
        [gVidWindow makeMainWindow];
        
        gVidFullscreen = NO;
    }

    // get the backbuffer:
    vid.buffer = malloc(vid.rowbytes * vid.height);
    if (vid.buffer == NULL)
    {
        ri.Sys_Error (ERR_FATAL, "Unabled to allocate the video backbuffer!\n");
    }

    return (rserr_ok);
}

//______________________________________________________________________________________________SWimp_AppActivate()

void	SWimp_AppActivate (qboolean active)
{
    // do nothing!
}

//________________________________________________________________________________________iMPLEMENTATION_Quake2View

#pragma mark -

@implementation Quake2View

//____________________________________________________________________________________________acceptsFirstResponder

-(BOOL) acceptsFirstResponder
{
    return YES;
}

//___________________________________________________________________________________________________windowDidMove:

- (void)windowDidMove: (NSNotification *)note
{
    NSRect	myRect;

    myRect = [gVidWindow frame];
    ri.Cmd_ExecuteText (EXEC_NOW, va ("vid_xpos %i", (int) myRect.origin.x + 1));
    ri.Cmd_ExecuteText (EXEC_NOW, va ("vid_ypos %i", (int) myRect.origin.y + 1));
}

@end

//______________________________________________________________________________________________________________eOF
