/**
 * @file snddma_osx
 * @brief MacOS X Sound driver
 * Written by:	awe				[mailto:awe@fruitz-of-dojo.de]
 * 2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de]
 */

/*

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

#pragma mark =Includes=

#include <CoreAudio/AudioHardware.h>

#include "../../client/client.h"
#include "../../client/snd_loc.h"

#pragma mark -


#pragma mark =Defines=

#define OUTPUT_BUFFER_SIZE	(4 * 1024)
#define TOTAL_BUFFER_SIZE	(64 * 1024)
#define NO 			0
#define	YES			1

#pragma mark -


#pragma mark =TypeDefs=

typedef int				SInt;
typedef unsigned int			UInt;

#pragma mark -


#pragma mark =Variables=

cvar_t *snd_ref;

AudioDeviceID gSNDDMASoundDeviceID;

static Boolean gSNDDMAIOProcIsInstalled = NO;
static unsigned char gSNDDMABuffer[TOTAL_BUFFER_SIZE];
static UInt32 gSNDDMABufferPosition, gSNDDMABufferByteCount;
static AudioStreamBasicDescription	gSNDDMABasicDescription;
static OSStatus (*SNDDMA_AudioIOProc)(AudioDeviceID, const AudioTimeStamp *,
	const AudioBufferList *, const AudioTimeStamp *, AudioBufferList *, const AudioTimeStamp *, void *);

#pragma mark -


#pragma mark =Function Prototypes=

static OSStatus SNDDMA_Audio8BitIOProc (AudioDeviceID, const AudioTimeStamp *, const AudioBufferList *,
		const AudioTimeStamp *, AudioBufferList *, const AudioTimeStamp *, void *);
static OSStatus SNDDMA_Audio16BitIOProc (AudioDeviceID, const AudioTimeStamp *, const AudioBufferList *,
		const AudioTimeStamp *, AudioBufferList *, const AudioTimeStamp *, void *);

#pragma mark -

/**
 * @brief
 */
static OSStatus SNDDMA_Audio8BitIOProc (AudioDeviceID inDevice,	const AudioTimeStamp *inNow,
	const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime, AudioBufferList *outOutputData,
	const AudioTimeStamp *inOutputTime, void *inClientData)
{
	UInt16	i;
	UInt8	*myDMA = ((UInt8 *) gSNDDMABuffer) + gSNDDMABufferPosition / (dma.samplebits >> 3);
	float	*myOutBuffer = (float *) outOutputData->mBuffers[0].mData;

	/* convert the buffer from unsigned PCM to signed PCM and last not least to float, required by CoreAudio: */
	for (i = 0; i < gSNDDMABufferByteCount; i++) {
		*myOutBuffer++ = ((*myDMA++) - 0x80) * (1.0f / 128.0f);
	}

	/* increase the bufferposition: */
	gSNDDMABufferPosition += gSNDDMABufferByteCount * (dma.samplebits >> 3);
	if (gSNDDMABufferPosition >= sizeof (gSNDDMABuffer)) {
		gSNDDMABufferPosition = 0;
	}

	/* return 0 = no error: */
	return (0);
}

/**
 * @brief
 */
static OSStatus SNDDMA_Audio16BitIOProc (AudioDeviceID inDevice,
	const AudioTimeStamp *inNow, const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime,
	AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime, void *inClientData)
{
	UInt16	i;
	short	*myDMA = ((short *) gSNDDMABuffer) + gSNDDMABufferPosition / (dma.samplebits >> 3);
	float	*myOutBuffer = (float *) outOutputData->mBuffers[0].mData;

	/* convert the buffer to float, required by CoreAudio: */
	for (i = 0; i < gSNDDMABufferByteCount; i++) {
		*myOutBuffer++ = (*myDMA++) * (1.0f / 32768.0f);
	}

	/* increase the bufferposition: */
	gSNDDMABufferPosition += gSNDDMABufferByteCount * (dma.samplebits >> 3);
	if (gSNDDMABufferPosition >= sizeof (gSNDDMABuffer)) {
		gSNDDMABufferPosition = 0;
	}

	/* return 0 = no error: */
	return (0);
}

/**
 * @brief
 */
qboolean SNDDMA_ReserveBufferSize (void)
{
	OSStatus		myError;
	AudioDeviceID	myAudioDevice;
	UInt32		myPropertySize;

	/* this function has to be called before any QuickTime movie data is loaded, so that the QuickTime handler */
	/* knows about our custom buffersize! */
	myPropertySize = sizeof (AudioDeviceID);
	myError = AudioHardwareGetProperty (kAudioHardwarePropertyDefaultOutputDevice, &myPropertySize,&myAudioDevice);

	if (!myError && myAudioDevice != kAudioDeviceUnknown) {
		UInt32		myBufferByteCount = OUTPUT_BUFFER_SIZE * sizeof (float);

		myPropertySize = sizeof (myBufferByteCount);

		/* set the buffersize for the audio device: */
		myError = AudioDeviceSetProperty (myAudioDevice, NULL, 0, NO, kAudioDevicePropertyBufferSize,
										myPropertySize, &myBufferByteCount);

		if (!myError) {
			return (YES);
		}
	}

	return (NO);
}

/**
 * @brief
 * FIXME: Change to dynamic sound lib
 */
qboolean SNDDMA_Init (struct sndinfo *si__) /* argument ignored */
{
	UInt32	myPropertySize;

	/* check sample bits: */
	cvar_t* s_loadas8bit = Cvar_Get("s_loadas8bit", "16", CVAR_ARCHIVE);
	if ((int) s_loadas8bit->value) {
		dma.samplebits = 8;
		SNDDMA_AudioIOProc = SNDDMA_Audio8BitIOProc;
	} else {
		dma.samplebits = 16;
		SNDDMA_AudioIOProc = SNDDMA_Audio16BitIOProc;
	}

	myPropertySize = sizeof (gSNDDMASoundDeviceID);

	/* find a suitable audio device: */
	if (AudioHardwareGetProperty (kAudioHardwarePropertyDefaultOutputDevice, &myPropertySize, &gSNDDMASoundDeviceID)) {
		Com_Printf ("Audio init fails: Can\'t get audio device.\n");
		return (0);
	}

	/* is the device valid? */
	if (gSNDDMASoundDeviceID == kAudioDeviceUnknown) {
		Com_Printf ("Audio init fails: Unsupported audio device.\n");
		return (0);
	}

	/* get the buffersize of the audio device [must previously be set via "SNDDMA_ReserveBufferSize ()"]: */
	myPropertySize = sizeof (gSNDDMABufferByteCount);
	if (AudioDeviceGetProperty (gSNDDMASoundDeviceID, 0, NO, kAudioDevicePropertyBufferSize,
								&myPropertySize, &gSNDDMABufferByteCount) || gSNDDMABufferByteCount == 0) {
		Com_Printf ("Audio init fails: Can't get audiobuffer.\n");
		return (0);
	}

	/*check the buffersize: */
	gSNDDMABufferByteCount /= sizeof (float);
	if (gSNDDMABufferByteCount != OUTPUT_BUFFER_SIZE) {
		Com_Printf ("Audio init: Audiobuffer size is not sufficient for clean movie playback!\n");
	}
	if (sizeof (gSNDDMABuffer) % gSNDDMABufferByteCount != 0 ||
		sizeof (gSNDDMABuffer) / gSNDDMABufferByteCount < 2) {
		Com_Printf ("Audio init: Bad audiobuffer size!\n");
		return (0);
	}

	/* get the audiostream format: */
	myPropertySize = sizeof (gSNDDMABasicDescription);
	if (AudioDeviceGetProperty (gSNDDMASoundDeviceID, 0, NO, kAudioDevicePropertyStreamFormat,
								&myPropertySize, &gSNDDMABasicDescription)) {
		Com_Printf ("Audio init fails.\n");
		return (0);
	}

	/* is the format LinearPCM? */
	if (gSNDDMABasicDescription.mFormatID != kAudioFormatLinearPCM) {
		Com_Printf ("Default Audio Device doesn't support Linear PCM!\n");
		return(0);
	}

	/* is sound ouput suppressed? */
	if (!COM_CheckParm ("-nosound")) {
		/* add the sound FX IO: */
		if (AudioDeviceAddIOProc (gSNDDMASoundDeviceID, SNDDMA_AudioIOProc, NULL)) {
			Com_Printf ("Audio init fails: Can\'t install IOProc.\n");
			return (0);
		}

		/* start the sound FX: */
		if (AudioDeviceStart (gSNDDMASoundDeviceID, SNDDMA_AudioIOProc)) {
			Com_Printf ("Audio init fails: Can\'t start audio.\n");
			return (0);
		}
		gSNDDMAIOProcIsInstalled = YES;
	} else {
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

/**
 * @brief
 */
void SNDDMA_Shutdown (void)
{
	/* shut everything down: */
	if (gSNDDMAIOProcIsInstalled == YES) {
		AudioDeviceStop (gSNDDMASoundDeviceID, SNDDMA_AudioIOProc);
		AudioDeviceRemoveIOProc (gSNDDMASoundDeviceID, SNDDMA_AudioIOProc);
	}
}

/**
 * @brief
 */
int	SNDDMA_GetDMAPos (void)
{
	if (gSNDDMAIOProcIsInstalled == NO) {
		return (0);
	}
	return (gSNDDMABufferPosition / (dma.samplebits >> 3));
}

/**
 * @brief
 */
void SNDDMA_Submit (void)
{
    /* not required! */
}

/**
 * @brief
 */
void SNDDMA_BeginPainting (void)
{
    /* not required! */
}
