/*______________________________________________________________________________________________________________nFO */
/* "snddma_osx.c" - MacOS X Sound driver. */

/* Written by:	awe				[mailto:awe@fruitz-of-dojo.de]. */
/*		©2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de]. */

/* Quake IIª is copyrighted by id software		[http://www.idsoftware.com]. */

/* Version History: */
/* v1.0.4: Improved sound playback if sound quality is set to "low" [propper unsigned to signed PCM conversion]. */
/* v1.0.0: Initial release. */
/*_________________________________________________________________________________________________________iNCLUDES */

#pragma mark =Includes=

#include <CoreAudio/AudioHardware.h>

#include "../../client/client.h"
#include "../../client/snd_loc.h"

#pragma mark -

/*__________________________________________________________________________________________________________dEFINES */

#pragma mark =Defines=

#define OUTPUT_BUFFER_SIZE	(4 * 1024)
#define TOTAL_BUFFER_SIZE	(64 * 1024)
#define NO 			0
#define	YES			1

#pragma mark -

/*_________________________________________________________________________________________________________tYPEDEFS */

#pragma mark =TypeDefs=

typedef int				SInt;
typedef unsigned int			UInt;

#pragma mark -

/*________________________________________________________________________________________________________vARIABLES */

#pragma mark =Variables=

cvar_t *snd_ref;

AudioDeviceID 				gSNDDMASoundDeviceID;

static Boolean                          gSNDDMAIOProcIsInstalled = NO;
static unsigned char 			gSNDDMABuffer[TOTAL_BUFFER_SIZE];
static UInt32				gSNDDMABufferPosition,
                                        gSNDDMABufferByteCount;
static AudioStreamBasicDescription	gSNDDMABasicDescription;
static OSStatus 			(*SNDDMA_AudioIOProc)(AudioDeviceID, const AudioTimeStamp *,
                                                              const AudioBufferList *, const AudioTimeStamp *,
                                                              AudioBufferList *, const AudioTimeStamp *, void *);

#pragma mark -
                                                             
/*______________________________________________________________________________________________fUNCTION_pROTOTYPES */

#pragma mark =Function Prototypes=

static OSStatus SNDDMA_Audio8BitIOProc (AudioDeviceID, const AudioTimeStamp *, const AudioBufferList *,
                                        const AudioTimeStamp *, AudioBufferList *, const AudioTimeStamp *,
                                        void *);
static OSStatus SNDDMA_Audio16BitIOProc (AudioDeviceID, const AudioTimeStamp *, const AudioBufferList *,
                                         const AudioTimeStamp *, AudioBufferList *, const AudioTimeStamp *,
                                         void *);

#pragma mark -

/*_________________________________________________________________________________________SNDDMA_Audio8BitIOProc() */

static OSStatus SNDDMA_Audio8BitIOProc (AudioDeviceID inDevice,
                                         const AudioTimeStamp *inNow,
                                         const AudioBufferList *inInputData,
                                         const AudioTimeStamp *inInputTime,
                                         AudioBufferList *outOutputData, 
                                         const AudioTimeStamp *inOutputTime,
                                         void *inClientData)
{
    UInt16	i;
    UInt8	*myDMA = ((UInt8 *) gSNDDMABuffer) + gSNDDMABufferPosition / (dma.samplebits >> 3);
    float	*myOutBuffer = (float *) outOutputData->mBuffers[0].mData;

    /* convert the buffer from unsigned PCM to signed PCM and last not least to float, required by CoreAudio: */
    for (i = 0; i < gSNDDMABufferByteCount; i++)
    {
        *myOutBuffer++ = ((*myDMA++) - 0x80) * (1.0f / 128.0f);
    }
    
    /* increase the bufferposition: */
    gSNDDMABufferPosition += gSNDDMABufferByteCount * (dma.samplebits >> 3);
    if (gSNDDMABufferPosition >= sizeof (gSNDDMABuffer))
    {
        gSNDDMABufferPosition = 0;
    }
    
    /* return 0 = no error: */
    return (0);
}

/*________________________________________________________________________________________SNDDMA_Audio16BitIOProc() */

static OSStatus SNDDMA_Audio16BitIOProc (AudioDeviceID inDevice,
                                         const AudioTimeStamp *inNow,
                                         const AudioBufferList *inInputData,
                                         const AudioTimeStamp *inInputTime,
                                         AudioBufferList *outOutputData, 
                                         const AudioTimeStamp *inOutputTime,
                                         void *inClientData)
{
    UInt16	i;
    short	*myDMA = ((short *) gSNDDMABuffer) + gSNDDMABufferPosition / (dma.samplebits >> 3);
    float	*myOutBuffer = (float *) outOutputData->mBuffers[0].mData;

    /* convert the buffer to float, required by CoreAudio: */
    for (i = 0; i < gSNDDMABufferByteCount; i++)
    {
        *myOutBuffer++ = (*myDMA++) * (1.0f / 32768.0f);
    }
    
    /* increase the bufferposition: */
    gSNDDMABufferPosition += gSNDDMABufferByteCount * (dma.samplebits >> 3);
    if (gSNDDMABufferPosition >= sizeof (gSNDDMABuffer))
    {
        gSNDDMABufferPosition = 0;
    }
    
    /* return 0 = no error: */
    return (0);
}

/*_______________________________________________________________________________________SNDDMA_ReserveBufferSize() */

qboolean	SNDDMA_ReserveBufferSize (void)
{
    OSStatus		myError;
    AudioDeviceID	myAudioDevice;
    UInt32		myPropertySize;
    
    /* this function has to be called before any QuickTime movie data is loaded, so that the QuickTime handler */
    /* knows about our custom buffersize! */
    myPropertySize = sizeof (AudioDeviceID);
    myError = AudioHardwareGetProperty (kAudioHardwarePropertyDefaultOutputDevice, &myPropertySize,&myAudioDevice);
    
    if (!myError && myAudioDevice != kAudioDeviceUnknown)
    {
        UInt32		myBufferByteCount = OUTPUT_BUFFER_SIZE * sizeof (float);

        myPropertySize = sizeof (myBufferByteCount);

        /* set the buffersize for the audio device: */
        myError = AudioDeviceSetProperty (myAudioDevice, NULL, 0, NO, kAudioDevicePropertyBufferSize,
                                          myPropertySize, &myBufferByteCount);
        
        if (!myError)
        {
            return (YES);
        }
    }
    
    return (NO);
}

/*____________________________________________________________________________________________________SNDDMA_Init() */

/* qboolean SNDDMA_Init (void) */
qboolean SNDDMA_Init (struct sndinfo *si__) /* argument ignored */
{
    UInt32	myPropertySize;

    /* check sample bits: */
    s_loadas8bit = Cvar_Get("s_loadas8bit", "16", CVAR_ARCHIVE);
    if ((int) s_loadas8bit->value)
    {
	dma.samplebits = 8;
        SNDDMA_AudioIOProc = SNDDMA_Audio8BitIOProc;
    }
    else
    {
	dma.samplebits = 16;
        SNDDMA_AudioIOProc = SNDDMA_Audio16BitIOProc;
    }

    myPropertySize = sizeof (gSNDDMASoundDeviceID);
            
    /* find a suitable audio device: */
    if (AudioHardwareGetProperty (kAudioHardwarePropertyDefaultOutputDevice, &myPropertySize,
                                  &gSNDDMASoundDeviceID))
    {
        Com_Printf ("Audio init fails: Can\'t get audio device.\n");
        return (0);
    }
    
    /* is the device valid? */
    if (gSNDDMASoundDeviceID == kAudioDeviceUnknown)
    {
        Com_Printf ("Audio init fails: Unsupported audio device.\n");
        return (0);
    }
    
    /* get the buffersize of the audio device [must previously be set via "SNDDMA_ReserveBufferSize ()"]: */
    myPropertySize = sizeof (gSNDDMABufferByteCount);
    if (AudioDeviceGetProperty (gSNDDMASoundDeviceID, 0, NO, kAudioDevicePropertyBufferSize,
                                &myPropertySize, &gSNDDMABufferByteCount) || gSNDDMABufferByteCount == 0)
    {
        Com_Printf ("Audio init fails: Can't get audiobuffer.\n");
        return (0);
    }
    
    /*check the buffersize: */
    gSNDDMABufferByteCount /= sizeof (float);
    if (gSNDDMABufferByteCount != OUTPUT_BUFFER_SIZE)
    {
        Com_Printf ("Audio init: Audiobuffer size is not sufficient for clean movie playback!\n");
    }
    if (sizeof (gSNDDMABuffer) % gSNDDMABufferByteCount != 0 ||
        sizeof (gSNDDMABuffer) / gSNDDMABufferByteCount < 2)
    {
        Com_Printf ("Audio init: Bad audiobuffer size!\n");
        return (0);
    }
    
    /* get the audiostream format: */
    myPropertySize = sizeof (gSNDDMABasicDescription);
    if (AudioDeviceGetProperty (gSNDDMASoundDeviceID, 0, NO, kAudioDevicePropertyStreamFormat,
                                &myPropertySize, &gSNDDMABasicDescription))
    {
        Com_Printf ("Audio init fails.\n");
        return (0);
    }
    
    /* is the format LinearPCM? */
    if (gSNDDMABasicDescription.mFormatID != kAudioFormatLinearPCM)
    {
        Com_Printf ("Default Audio Device doesn't support Linear PCM!\n");
        return(0);
    }
    
    /* is sound ouput suppressed? */
    if (!COM_CheckParm ("-nosound"))
    {
        /* add the sound FX IO: */
        if (AudioDeviceAddIOProc (gSNDDMASoundDeviceID, SNDDMA_AudioIOProc, NULL))
        {
            Com_Printf ("Audio init fails: Can\'t install IOProc.\n");
            return (0);
        }
        
        /* start the sound FX: */
        if (AudioDeviceStart (gSNDDMASoundDeviceID, SNDDMA_AudioIOProc))
        {
            Com_Printf ("Audio init fails: Can\'t start audio.\n");
            return (0);
        }
        gSNDDMAIOProcIsInstalled = YES;
    }
    else
    {
        gSNDDMAIOProcIsInstalled = NO;
    }
    
    /* setup Quake sound variables: */
    dma.speed = gSNDDMABasicDescription.mSampleRate;
    dma.channels = gSNDDMABasicDescription.mChannelsPerFrame;
    dma.samples = sizeof (gSNDDMABuffer) / (dma.samplebits >> 3);
    dma.samplepos = 0;
    dma.submission_chunk = gSNDDMABufferByteCount;
    dma.buffer = gSNDDMABuffer;
    gSNDDMABufferPosition = 0;

    return (1);
}

/*________________________________________________________________________________________________SNDDMA_Shutdown() */

void	SNDDMA_Shutdown (void)
{
    /* shut everything down: */
    if (gSNDDMAIOProcIsInstalled == YES)
    {
        AudioDeviceStop (gSNDDMASoundDeviceID, SNDDMA_AudioIOProc);
        AudioDeviceRemoveIOProc (gSNDDMASoundDeviceID, SNDDMA_AudioIOProc);
    }
}

/*_______________________________________________________________________________________________SNDDMA_GetDMAPos() */

int	SNDDMA_GetDMAPos (void)
{
    if (gSNDDMAIOProcIsInstalled == NO)
    {
        return (0);
    }
    return (gSNDDMABufferPosition / (dma.samplebits >> 3));
}

/*__________________________________________________________________________________________________SNDDMA_Submit() */

void	SNDDMA_Submit (void)
{
    /* not required! */
}

/*___________________________________________________________________________________________SNDDMA_BeginPainting() */

void	SNDDMA_BeginPainting (void)
{
    /* not required! */
}

/*______________________________________________________________________________________________________________eOF */
