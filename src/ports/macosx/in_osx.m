/*
 * "in_osx.c" - MacOS X mouse input functions.
 *
 * Written by:	awe				[mailto:awe@fruitz-of-dojo.de].
 *		2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
 *
 * Quake II is copyrighted by id software	[http://www.idsoftware.com].
 *
 * Version History:
 * v1.0.8: F12 eject is now disabled while Quake II is running and if a key is bound to F12.
 * v1.0.2: MouseScaling is now disabled and enabled via IOKit.
 *	   Fixed an issue with mousepointer visibilty.
 * v1.0.0: Initial release.
 */

#pragma mark =Includes=

#import <AppKit/AppKit.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hidsystem/IOHIDLib.h>
#import <IOKit/hidsystem/IOHIDParameter.h>
#import <IOKit/hidsystem/event_status_driver.h>

#include "../../client/client.h"

#pragma mark -

#pragma mark =Variables=

extern BOOL gSysIsDeactivated;
extern cvar_t *gSysIsMinimized;
extern cvar_t *vid_fullscreen;

cvar_t *in_joystick, *in_mouse, *_windowed_mouse;

static BOOL gInMLooking = NO;
static cvar_t
			*m_filter,
			*gInSensitivity;
extern cvar_t
			*gInSensitivity;
static CGMouseDelta	gInMouseX,
			gInMouseY,
			gInMouseNewX,
			gInMouseNewY,
			gInMouseOldX,
			gInMouseOldY;

#pragma mark -

#pragma mark =Function Prototypes=

void		IN_SetKeyboardRepeatEnabled (BOOL theState);
void		IN_SetF12EjectEnabled (qboolean theState);
void 		IN_ShowCursor (BOOL theState);
void		IN_CenterCursor (void);
void		IN_ReceiveMouseMove (CGMouseDelta theDeltaX, CGMouseDelta theDeltaY);

static void	IN_SetMouseScalingEnabled (BOOL theState);
static void 	IN_MLookDown_f (void);
static void 	IN_MLookUp_f (void);

extern cvar_t *vid_xpos;			/* X coordinate of window position */
extern cvar_t *vid_ypos;			/* Y coordinate of window position */

#pragma mark -

io_connect_t IN_GetIOHandle (void)
{
	io_connect_t 	myHandle = MACH_PORT_NULL;
	kern_return_t	myStatus;
	io_service_t	myService = MACH_PORT_NULL;
	mach_port_t		myMasterPort;

	myStatus = IOMasterPort (MACH_PORT_NULL, &myMasterPort );
	if (myStatus != KERN_SUCCESS)
		return (NULL);

	myService = IORegistryEntryFromPath (myMasterPort, kIOServicePlane ":/IOResources/IOHIDSystem");
	if (myService == NULL) {
		return (NULL);
	}

	myStatus = IOServiceOpen (myService, mach_task_self (), kIOHIDParamConnectType, &myHandle);
	IOObjectRelease (myService);

	return (myHandle);
}

void IN_SetKeyboardRepeatEnabled (BOOL theState)
{
	static BOOL		myKeyboardRepeatEnabled = YES;
	static double	myOriginalKeyboardRepeatInterval;
	static double	myOriginalKeyboardRepeatThreshold;
	NXEventHandle	myEventStatus;

	if (theState == myKeyboardRepeatEnabled)
		return;
	if (!(myEventStatus = NXOpenEventStatus ()))
		return;

	if (theState == YES) {
		NXSetKeyRepeatInterval (myEventStatus, myOriginalKeyboardRepeatInterval);
		NXSetKeyRepeatThreshold (myEventStatus, myOriginalKeyboardRepeatThreshold);
		NXResetKeyboard (myEventStatus);
	} else {
		myOriginalKeyboardRepeatInterval = NXKeyRepeatInterval (myEventStatus);
		myOriginalKeyboardRepeatThreshold = NXKeyRepeatThreshold (myEventStatus);
		NXSetKeyRepeatInterval (myEventStatus, 3456000.0f);
		NXSetKeyRepeatThreshold (myEventStatus, 3456000.0f);
	}

	NXCloseEventStatus (myEventStatus);
	myKeyboardRepeatEnabled = theState;
}

void IN_SetF12EjectEnabled (qboolean theState)
{
	static BOOL		myF12KeyIsEnabled = YES;
	static UInt32	myOldValue;
	io_connect_t	myIOHandle = NULL;

	/* Do we have a state change? */
	if (theState == myF12KeyIsEnabled) {
		return;
	}

	/* Get the IOKit handle: */
	myIOHandle = IN_GetIOHandle ();
	if (myIOHandle == NULL) {
		return;
	}

	/*  Set the F12 key according to the current state: */
	if (theState == NO) // && keybindings[K_F12] != NULL && keybindings[K_F12][0] != 0x00) {
		UInt32		myValue = 0x00;
		IOByteCount	myCount;
		kern_return_t	myStatus;

		myStatus = IOHIDGetParameter (myIOHandle,
									CFSTR (kIOHIDF12EjectDelayKey),
									sizeof (UInt32),
									&myOldValue,
									&myCount);

		/* change only the settings, if we were successfull! */
		if (myStatus != kIOReturnSuccess) {
			theState = YES;
		} else  {
			IOHIDSetParameter(myIOHandle, CFSTR (kIOHIDF12EjectDelayKey), &myValue, sizeof (UInt32));
		}
	} else {
		if (myF12KeyIsEnabled == NO) {
			IOHIDSetParameter (myIOHandle, CFSTR (kIOHIDF12EjectDelayKey),  &myOldValue, sizeof (UInt32));
		}
		theState = YES;
	}

	myF12KeyIsEnabled = theState;
	IOServiceClose (myIOHandle);
}

void IN_SetMouseScalingEnabled (BOOL theState)
{
	static BOOL		myMouseScalingEnabled = YES;
	static double	myOldAcceleration = 0.0;
	io_connect_t	myIOHandle = NULL;

	/* Do we have a state change? */
	if (theState == myMouseScalingEnabled) {
		return;
	}

	/* Get the IOKit handle: */
	myIOHandle = IN_GetIOHandle ();
	if (myIOHandle == NULL) {
		return;
	}

	/* Set the mouse acceleration according to the current state: */
	if (theState == YES) {
		IOHIDSetAccelerationWithKey (myIOHandle,  CFSTR (kIOHIDMouseAccelerationType), myOldAcceleration);
	} else {
		kern_return_t	myStatus;

		myStatus = IOHIDGetAccelerationWithKey (myIOHandle, CFSTR (kIOHIDMouseAccelerationType),
			&myOldAcceleration);

		/* change only the settings, if we were successfull! */
		if (myStatus != kIOReturnSuccess || myOldAcceleration == 0.0) {
			theState = YES;
		}

		/* change only the settings, if we were successfull! */
		if (myStatus != kIOReturnSuccess) {
			theState = YES;
		}

		/* finally disable the acceleration: */
		if (theState == NO) {
			IOHIDSetAccelerationWithKey (myIOHandle,  CFSTR (kIOHIDMouseAccelerationType), -1.0);
		}
	}

	myMouseScalingEnabled = theState;
	IOServiceClose (myIOHandle);
}

void IN_ShowCursor (BOOL theState)
{
	static BOOL myCursorIsVisible = YES;

	/* change only if we got a state change: */
	if (theState != myCursorIsVisible) {
		if (theState == YES) {
			CGAssociateMouseAndMouseCursorPosition (YES);
			IN_CenterCursor ();
			IN_SetMouseScalingEnabled (YES);
			CGDisplayShowCursor (kCGDirectMainDisplay);
		} else {
			[NSApp activateIgnoringOtherApps: YES];
			CGDisplayHideCursor(kCGDirectMainDisplay);
			CGAssociateMouseAndMouseCursorPosition(NO);
			IN_CenterCursor();
			IN_SetMouseScalingEnabled(NO);
		}
		myCursorIsVisible = theState;
	}
}

/**
 * @brief
 */
void IN_CenterCursor (void)
{
	CGPoint		myCenter;

	if (vid_fullscreen != NULL && vid_fullscreen->value == 0.0f) {
		float		myCenterX, myCenterY;

		/* get the window position: */
		if (vid_xpos != NULL) {
			myCenterX = vid_xpos->value;
		} else {
			myCenterX = 0.0f;
		}

		if (vid_ypos != NULL) {
			myCenterY = -vid_ypos->value;
		} else {
			myCenterY = 0.0f;
		}

		/* calculate the window center: */
		myCenterX += (float) (viddef.width >> 1);
		myCenterY += (float) CGDisplayPixelsHigh (kCGDirectMainDisplay) - (float) (viddef.height >> 1);

		myCenter = CGPointMake (myCenterX, myCenterY);
	} else {
		/* just center at the middle of the screen: */
		myCenter = CGPointMake ((float) (viddef.width >> 1), (float) (viddef.height >> 1));
	}

	/* and go: */
	CGDisplayMoveCursorToPoint (kCGDirectMainDisplay, myCenter);
}

void IN_MLookDown_f (void)
{
	gInMLooking = true;
}

void IN_ReceiveMouseMove (CGMouseDelta theDeltaX, CGMouseDelta theDeltaY)
{
	gInMouseNewX = theDeltaX;
	gInMouseNewY = theDeltaY;
}

void IN_ActivateMouse (void)
{
}

void IN_DeactivateMouse (void)
{
}
