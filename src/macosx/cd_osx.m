//______________________________________________________________________________________________________________nFO
// "cd_osx.m" - MacOS X audio CD driver.
//
// Written by:	awe				[mailto:awe@fruitz-of-dojo.de].
//		©2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quake IIª is copyrighted by id software	[http://www.idsoftware.com].
//
// Version History:
// v1.0.8: Rewritten. Uses now QuickTime for playback.
//	   Added support for MP3 and MP4 [AAC] playback.
// v1.0.3: Fixed an issue with requesting a track number greater than the max number.
// v1.0.1: Added "cdda" as extension for detection of audio-tracks [required by MacOS X v10.1 or later]
// v1.0.0: Initial release.
//_________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <CoreAudio/AudioHardware.h>
#import <QuickTime/QuickTime.h>

#include <sys/mount.h>

#define MAC_OSX_M_SPECIAL_BOOLEAN

#include "client.h"

#pragma mark -

//_________________________________________________________________________________________________________tYPEDEFS

#pragma mark =TypeDefs=

typedef enum
{
    CDERR_ALLOC_TRACK = 1,
    CDERR_MOVIE_DATA,
    CDERR_AUDIO_DATA,
    CDERR_QUICKTIME_ERROR,
    CDERR_NO_MEDIA_FOUND,
    CDERR_MEDIA_TRACK,
    CDERR_MEDIA_TRACK_CONTROLLER,
    CDERR_EJECT,
    CDERR_NO_FILES_FOUND
} cderror_t;

#pragma mark -

//________________________________________________________________________________________________________vARIABLES

#pragma mark =Variables=

extern NSString			*gSysMP3Folder;
extern BOOL			gSysAbortMediaScan;

cvar_t				*cd_volume;

static UInt16			gCDTrackCount, gCurCDTrack;
static NSMutableArray *		gCDTrackList;
static char 			gCDDevice[MAX_OSPATH];
static BOOL			gCDLoop, gCDNextTrack;

static Movie			gCDController = NULL;

#pragma mark -

//______________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

BOOL			CDAudio_GetTrackList (void);
void			CDAudio_Enable (BOOL theState);

static	void		CDAudio_Error (cderror_t theErrorNumber);
static	SInt32		CDAudio_StripVideoTracks (Movie theMovie);
static	void		CDAudio_SafePath (const char *thePath);
static	void		CDAudio_AddTracks2List (NSString *theMountPath, NSArray *theExtensions);
static 	void 		CD_f (void);

#pragma mark -

//__________________________________________________________________________________________________CDAudio_Error()

void	CDAudio_Error (cderror_t theErrorNumber)
{
    if (gSysMP3Folder == NULL)
    {
        Con_Print ("Audio-CD driver: ");
    }
    else
    {
        Con_Print ("MP3/MP4 driver: ");
    }
    
    switch (theErrorNumber)
    {
        case CDERR_ALLOC_TRACK:
            Con_Print ("Failed to allocate track!\n");
            break;
        case CDERR_MOVIE_DATA:
            Con_Print ("Failed to retrieve track data!\n");
            break;
        case CDERR_AUDIO_DATA:
            Con_Print ("File without audio data!\n");
            break;
        case CDERR_QUICKTIME_ERROR:
            Con_Print ("QuickTime error!\n");
            break;
        case CDERR_NO_MEDIA_FOUND:
            Con_Print ("No Audio-CD found.\n");
            break;
        case CDERR_MEDIA_TRACK:
            Con_Print ("Failed to retrieve media track!\n");
            break;
        case CDERR_MEDIA_TRACK_CONTROLLER:
            Con_Print ("Failed to retrieve track controller!\n");
            break;
        case CDERR_EJECT:
            Con_Print ("Can\'t eject Audio-CD!\n");
            break;
        case CDERR_NO_FILES_FOUND:
            if (gSysMP3Folder == NULL)
            {
                Con_Print ("No audio tracks found.\n");
            }
            else
            {
                Con_Print ("No files found with the extension \'.mp3\'!\n");
            }
            break;
    }
}

//_______________________________________________________________________________________CDAudio_StripVideoTracks()

SInt32	CDAudio_StripVideoTracks (Movie theMovie)
{
    SInt64 	myTrackCount, i;
    Track 	myCurTrack;
    OSType 	myMediaType;
	
    myTrackCount = GetMovieTrackCount (theMovie);

    for (i = myTrackCount; i >= 1; i--)
    {
        myCurTrack = GetMovieIndTrack (theMovie, i);
        GetMediaHandlerDescription (GetTrackMedia (myCurTrack), &myMediaType, NULL, NULL);
        if (myMediaType != SoundMediaType && myMediaType != MusicMediaType)
        {
            DisposeMovieTrack (myCurTrack);
        }
    }

    return (GetMovieTrackCount (theMovie));
}

//_________________________________________________________________________________________CDAudio_AddTracks2List()

void	CDAudio_AddTracks2List (NSString *theMountPath, NSArray *theExtensions)
{
    NSFileManager		*myFileManager = [NSFileManager defaultManager];
    
    if (myFileManager != NULL)
    {
        NSDirectoryEnumerator	*myDirEnum = [myFileManager enumeratorAtPath: theMountPath];

        if (myDirEnum != NULL)
        {
            NSString	*myFilePath;
            SInt32	myIndex, myExtensionCount;
            
            myExtensionCount = [theExtensions count];
            
            // get all audio tracks:
            while ((myFilePath = [myDirEnum nextObject]))
            {
                if (gSysAbortMediaScan == YES)
                {
                    break;
                }

                for (myIndex = 0; myIndex < myExtensionCount; myIndex++)
                {
                    if ([[myFilePath pathExtension] isEqualToString: [theExtensions objectAtIndex: myIndex]])
                    {
                        NSString	*myFullPath = [theMountPath stringByAppendingPathComponent: myFilePath];
                        NSURL		*myMoviePath = [NSURL fileURLWithPath: myFullPath];
                        NSMovie		*myMovie = NULL;
                        
                        myMovie = [[NSMovie alloc] initWithURL: myMoviePath byReference: YES];
                        if (myMovie != NULL)
                        {
                            Movie	myQTMovie = [myMovie QTMovie];
                            
                            if (myQTMovie != NULL)
                            {
                                // add only movies with audiotacks and use only the audio track:
                                if (CDAudio_StripVideoTracks (myQTMovie) > 0)
                                {
                                    [gCDTrackList addObject: myMovie];
                                }
                                else
                                {
                                    CDAudio_Error (CDERR_AUDIO_DATA);
                                }
                            }
                            else
                            {
                                CDAudio_Error (CDERR_MOVIE_DATA);
                            }
                        }
                        else
                        {
                            CDAudio_Error (CDERR_ALLOC_TRACK);
                        }
                    }
                }
            }
        }
    }
    gCDTrackCount = [gCDTrackList count];
}

//_______________________________________________________________________________________________CDAudio_SafePath()

void	CDAudio_SafePath (const char *thePath)
{
    SInt32	myStrLength = 0;

    if (thePath != NULL)
    {
        SInt32		i;
        
        myStrLength = strlen (thePath);
        if (myStrLength > MAX_OSPATH - 1)
        {
            myStrLength = MAX_OSPATH - 1;
        }
        for (i = 0; i < myStrLength; i++)
        {
            gCDDevice[i] = thePath[i];
        }
    }
    gCDDevice[myStrLength] = 0x00;
}

//___________________________________________________________________________________________CDAudio_GetTrackList()

BOOL	CDAudio_GetTrackList (void)
{
    NSAutoreleasePool 		*myPool;
    
    // release previously allocated memory:
    CDAudio_Shutdown ();
    
    // get memory for the new tracklisting:
    gCDTrackList = [[NSMutableArray alloc] init];
    myPool = [[NSAutoreleasePool alloc] init];
    gCDTrackCount = 0;
    
    // Get the current MP3 listing or retrieve the TOC of the AudioCD:
    if (gSysMP3Folder != NULL)
    {
        CDAudio_SafePath ([gSysMP3Folder fileSystemRepresentation]);
        Con_Print ("Scanning for audio tracks. Be patient!\n");
        CDAudio_AddTracks2List (gSysMP3Folder, [NSArray arrayWithObjects: @"mp3", @"mp4", NULL]);
    }
    else
    {
        NSString		*myMountPath;
        struct statfs  		*myMountList;
        UInt32			myMountCount;

        // get number of mounted devices:
        myMountCount = getmntinfo (&myMountList, MNT_NOWAIT);
        
        // zero devices? return.
        if (myMountCount <= 0)
        {
            [gCDTrackList release];
            gCDTrackList = NULL;
            gCDTrackCount = 0;
            CDAudio_Error (CDERR_NO_MEDIA_FOUND);
            return (0);
        }
        
        while (myMountCount--)
        {
            // is the device read only?
            if ((myMountList[myMountCount].f_flags & MNT_RDONLY) != MNT_RDONLY) continue;
            
            // is the device local?
            if ((myMountList[myMountCount].f_flags & MNT_LOCAL) != MNT_LOCAL) continue;
            
            // is the device "cdda"?
            if (strcmp (myMountList[myMountCount].f_fstypename, "cddafs")) continue;
            
            // is the device a directory?
            if (strrchr (myMountList[myMountCount].f_mntonname, '/') == NULL) continue;
            
            // we have found a Audio-CD!
            Com_Printf ("Found Audio-CD at mount entry: \"%s\".\n", myMountList[myMountCount].f_mntonname);
            
            // preserve the device name:
            CDAudio_SafePath (myMountList[myMountCount].f_mntonname);
            myMountPath = [NSString stringWithCString: myMountList[myMountCount].f_mntonname];
    
            Con_Print ("Scanning for audio tracks. Be patient!\n");
            CDAudio_AddTracks2List (gSysMP3Folder, [NSArray arrayWithObjects: @"aiff", @"cdda", NULL]);
            
            break;
        }
    }
    
    // release the pool:
    [myPool release];
    
    // just security:
    if (![gCDTrackList count])
    {
        [gCDTrackList release];
        gCDTrackList = NULL;
        gCDTrackCount = 0;
        CDAudio_Error (CDERR_NO_FILES_FOUND);
        return (0);
    }
    
    return (1);
}

//___________________________________________________________________________________________________CDAudio_Play()

void	CDAudio_Play (int theTrack, qboolean theLoop)
{
    gCDNextTrack = NO;
    
    if (gCDTrackList != NULL && gCDTrackCount != 0)
    {
        NSMovie		*myMovie;
        
        // check for mismatching CD track number:
        if (theTrack > gCDTrackCount || theTrack <= 0)
        {
            theTrack = 1;
        }
        gCurCDTrack = 0;
        
        if (gCDController != NULL && IsMovieDone (gCDController) == NO)
        {
            StopMovie(gCDController);
            gCDController = NULL;
        }
        
        myMovie = [gCDTrackList objectAtIndex: theTrack - 1];
        
        if (myMovie != NULL)
        {
            gCDController = [myMovie QTMovie];
            
            if (gCDController != NULL)
            {
                gCurCDTrack = theTrack;
                gCDLoop = theLoop;
                GoToBeginningOfMovie (gCDController);
                SetMovieActive (gCDController, YES);
                StartMovie (gCDController);
		if (GetMoviesError () != noErr)
		{
                    CDAudio_Error (CDERR_QUICKTIME_ERROR);
		}
            }
            else
            {
                CDAudio_Error (CDERR_MEDIA_TRACK);
            }
        }
        else
        {
            CDAudio_Error (CDERR_MEDIA_TRACK_CONTROLLER);
        }
    }
}

//___________________________________________________________________________________________________CDAudio_Stop()

void	CDAudio_Stop (void)
{
    // just stop the audio IO:
    if (gCDController != NULL && IsMovieDone (gCDController) == NO)
    {
        StopMovie (gCDController);
        GoToBeginningOfMovie (gCDController);
        SetMovieActive (gCDController, NO);
    }
}

//__________________________________________________________________________________________________CDAudio_Pause()

void	CDAudio_Pause (void)
{
    if (gCDController != NULL && GetMovieActive (gCDController) == YES && IsMovieDone (gCDController) == NO)
    {
        StopMovie (gCDController);
        SetMovieActive (gCDController, NO);
    }
}

//_________________________________________________________________________________________________CDAudio_Resume()

void	CDAudio_Resume (void)
{
    if (gCDController != NULL && GetMovieActive (gCDController) == NO && IsMovieDone (gCDController) == NO)
    {
        SetMovieActive (gCDController, YES);
        StartMovie (gCDController);
    }
}

//_________________________________________________________________________________________________CDAudio_Update()

void	CDAudio_Update (void)
{
    // update volume settings:
    if (gCDController != NULL)
    {
        SetMovieVolume (gCDController, kFullVolume * cd_volume->value);

        if (GetMovieActive (gCDController) == YES)
        {
            if (IsMovieDone (gCDController) == NO)
            {
                MoviesTask (gCDController, 0);
            }
            else
            {
                if (gCDLoop == YES)
                {
                    GoToBeginningOfMovie (gCDController);
                    StartMovie (gCDController);
                }
                else
                {
                    gCurCDTrack++;
                    CDAudio_Play (gCurCDTrack, NO);
                }
            }
        }
    }
}

//_________________________________________________________________________________________________CDAudio_Enable()

void	CDAudio_Enable (BOOL theState)
{
    static BOOL		myCDIsEnabled = YES;
    
    if (myCDIsEnabled != theState)
    {
        static BOOL	myCDWasPlaying = NO;
        
        if (theState == NO)
        {
            if (gCDController != NULL &&
                GetMovieActive (gCDController) == YES &&
                IsMovieDone (gCDController) == NO)
            {
                CDAudio_Pause ();
                myCDWasPlaying = YES;
            }
            else
            {
                myCDWasPlaying = NO;
            }
        }
        else
        {
            if (myCDWasPlaying == YES)
            {
                CDAudio_Resume ();
            }
        }
        myCDIsEnabled = theState;
    }
}

//___________________________________________________________________________________________________CDAudio_Init()

int	CDAudio_Init (void)
{
    // register the volume var:
    cd_volume = Cvar_Get ("cd_volume", "1", CVAR_ARCHIVE);

    // add "cd" and "mp3" console command:
    if (gSysMP3Folder != NULL)
    {
        Cmd_AddCommand ("mp3", CD_f);
        Cmd_AddCommand ("mp4", CD_f);
    }
    Cmd_AddCommand ("cd", CD_f);
    
    gCurCDTrack = 0;
    
    // required because of our GUI prescan:
    if (gCDTrackList == NULL && gSysAbortMediaScan == YES)
    {
        CDAudio_GetTrackList ();
    }
    
    // init SNDDMA, if "-nosound" was used:
    if (gCDTrackList == NULL)
    {
        if (gSysMP3Folder == NULL)
        {
            Con_Print ("QuickTime CD driver initialized...\n");
        }
        else
        {
            Con_Print ("QuickTime MP3/MP4 driver initialized...\n");
        }

        return (1);
    }
    
    // failure. return 0.
    if (gSysMP3Folder == NULL)
    {
        Con_Print ("QuickTime CD driver failed.\n");
    }
    else
    {
        Con_Print ("QuickTime MP3/MP4 driver failed.\n");
    }
    
    return (0);
}

//_______________________________________________________________________________________________CDAudio_Shutdown()

void	CDAudio_Shutdown (void)
{
    // shutdown the audio IO:
    CDAudio_Stop ();

    gCDController = NULL;
    gCDDevice[0] = 0x00;    
    gCurCDTrack = 0;

    if (gCDTrackList != NULL)
    {
       while ([gCDTrackList count])
        {
            NSMovie 	*myMovie = [gCDTrackList objectAtIndex: 0];
            
            [gCDTrackList removeObjectAtIndex: 0];
            [myMovie release];
        }
        [gCDTrackList release];
        gCDTrackList = NULL;
        gCDTrackCount = 0;
    }
}

//___________________________________________________________________________________________________________CD_f()

void	CD_f (void)
{
    char	*myCommandOption;

    // this command requires options!
    if (Cmd_Argc () < 2)
    {
        return;
    }

    // get the option:
    myCommandOption = Cmd_Argv (1);
    
    // turn CD playback on:
    if (Q_strcasecmp (myCommandOption, "on") == 0)
    {
        if (gCDTrackList == NULL)
        {
            CDAudio_GetTrackList();
        }
        CDAudio_Play(1, 0);
        
	return;
    }
    
    // turn CD playback off:
    if (Q_strcasecmp (myCommandOption, "off") == 0)
    {
        CDAudio_Shutdown ();
        
	return;
    }

    // just for compatibility:
    if (Q_strcasecmp (myCommandOption, "remap") == 0)
    {
        return;
    }

    // reset the current CD:
    if (Q_strcasecmp (myCommandOption, "reset") == 0)
    {
        CDAudio_Stop ();
        if (CDAudio_GetTrackList ())
        {
            if (gSysMP3Folder == NULL)
            {
                Con_Print ("CD");
            }
            else
            {
                Con_Print ("MP3/MP4 files");
            }
            Com_Printf (" found. %d tracks (\"%s\").\n", gCDTrackCount, gCDDevice);
	}
        else
        {
            CDAudio_Error (CDERR_NO_FILES_FOUND);
        }
        
	return;
    }
    
    // the following commands require a valid track array, so build it, if not present:
    if (gCDTrackCount == 0)
    {
        CDAudio_GetTrackList ();
        if (gCDTrackCount == 0)
        {
            CDAudio_Error (CDERR_NO_FILES_FOUND);
            return;
        }
    }
    
    // play the selected track:
    if (Q_strcasecmp (myCommandOption, "play") == 0)
    {
        CDAudio_Play (atoi (Cmd_Argv (2)), 0);
        
	return;
    }
    
    // loop the selected track:
    if (Q_strcasecmp (myCommandOption, "loop") == 0)
    {
        CDAudio_Play (atoi (Cmd_Argv (2)), 1);
        
	return;
    }
    
    // stop the current track:
    if (Q_strcasecmp (myCommandOption, "stop") == 0)
    {
        CDAudio_Stop ();
        
	return;
    }
    
    // pause the current track:
    if (Q_strcasecmp (myCommandOption, "pause") == 0)
    {
        CDAudio_Pause ();
        
	return;
    }
    
    // resume the current track:
    if (Q_strcasecmp (myCommandOption, "resume") == 0)
    {
        CDAudio_Resume ();
        
	return;
    }
    
    // eject the CD:
    if (gSysMP3Folder == NULL && Q_strcasecmp (myCommandOption, "eject") == 0)
    {
        // eject the CD:
        if (gCDDevice[0] != 0x00)
        {
            NSString	*myDevicePath = [NSString stringWithCString: gCDDevice];
            
            if (myDevicePath != NULL)
            {
                CDAudio_Shutdown ();
                
                if (![[NSWorkspace sharedWorkspace] unmountAndEjectDeviceAtPath: myDevicePath])
                {
                    CDAudio_Error (CDERR_EJECT);
                }
            }
            else
            {
                CDAudio_Error (CDERR_EJECT);
            }
        }
        else
        {
            CDAudio_Error (CDERR_NO_MEDIA_FOUND);
        }
        
	return;
    }
    
    // output CD info:
    if (Q_strcasecmp(myCommandOption, "info") == 0)
    {
        if (gCDTrackCount == 0)
        {
            CDAudio_Error (CDERR_NO_FILES_FOUND);
        }
        else
        {
            if (gCDController != NULL && GetMovieActive (gCDController) == YES)
            {
                Com_Printf ("Playing track %d of %d (\"%s\").\n", gCurCDTrack, gCDTrackCount, gCDDevice);
            }
            else
            {
                Com_Printf ("Not playing. Tracks: %d (\"%s\").\n", gCDTrackCount, gCDDevice);
            }
            Com_Printf ("Volume is: %.2f.\n", cd_volume->value);
        }
        
	return;
    }
}

//______________________________________________________________________________________________________________eOF
