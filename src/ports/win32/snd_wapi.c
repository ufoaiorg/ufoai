/**
 * @file snd_wapi.c
 * @brief WindowsAPI sound driver
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2005 Ben Lane

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

#include <float.h>

#include "../../client/client.h"
#include "../../client/snd_loc.h"
#include "winquake.h"

/* 64K is > 1 second at 16-bit, 22050 Hz */
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400

static qboolean	wav_init;
static qboolean	snd_firsttime = qtrue, snd_iswave;

static struct sndinfo *si;

/* starts at 0 for disabled */
static int	snd_buffer_count = 0;
static int	sample16;
static int	snd_sent, snd_completed;

/*
 * Global variables. Must be visible to window-procedure function
 *  so it can unlock and free the data block after it has been played.
 */


HANDLE hData;
HPSTR lpData, lpData2;

HGLOBAL hWaveHdr;
LPWAVEHDR lpWaveHdr;

HWAVEOUT hWaveOut;

WAVEOUTCAPS wavecaps;

DWORD gSndBufSize;

MMTIME mmstarttime;

/**
 * @brief
 */
void FreeSound (void)
{
	int i;

	si->Com_Printf( "Shutting down win api sound system\n" );

	if ( hWaveOut ) {
		waveOutReset (hWaveOut);

		if (lpWaveHdr) {
			for (i=0 ; i< WAV_BUFFERS ; i++)
				waveOutUnprepareHeader (hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR));
		}

		waveOutClose (hWaveOut);

		if (hWaveHdr) {
			GlobalUnlock(hWaveHdr);
			GlobalFree(hWaveHdr);
		}

		if (hData) {
			GlobalUnlock(hData);
			GlobalFree(hData);
		}

	}

	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	wav_init = qfalse;
}

/**
 * @brief Crappy windows multimedia base
 */
qboolean SND_InitWav (void)
{
	WAVEFORMATEX format;
	int i;
	HRESULT hr;

	si->Com_Printf( "Initializing wave sound\n" );

	snd_sent = 0;
	snd_completed = 0;

	si->dma->channels = 2;
	si->dma->samplebits = 16;

	if (si->s_khz->value == 44)
		si->dma->speed = 44100;
	else if (si->s_khz->value == 22)
		si->dma->speed = 22050;
	else
		si->dma->speed = 11025;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = si->dma->channels;
	format.wBitsPerSample = si->dma->samplebits;
	format.nSamplesPerSec = si->dma->speed;
	format.nBlockAlign = format.nChannels
	*format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec
	*format.nBlockAlign;

	/* Open a waveform device for output using window callback. */
	si->Com_Printf ("...opening waveform device: ");
	while ((hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER, &format,
			0, 0L, CALLBACK_NULL)) != MMSYSERR_NOERROR) {
		if (hr != MMSYSERR_ALLOCATED) {
			si->Com_Printf ("failed\n");
			return qfalse;
		}

		if (MessageBox (NULL,
				"The sound hardware is in use by another app.\n\n"
				"Select Retry to try to start sound again or Cancel to run UFO with no sound.",
				"Sound not available",
				MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY) {
			si->Com_Printf ("hw in use\n" );
			return qfalse;
		}
	}
	si->Com_Printf("ok\n");

	/*
	 * Allocate and lock memory for the waveform data. The memory
	 * for waveform data must be globally allocated with
	 * GMEM_MOVEABLE and GMEM_SHARE flags.
	*/
	si->Com_Printf ("...allocating waveform buffer: ");
	gSndBufSize = WAV_BUFFERS*WAV_BUFFER_SIZE;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData) {
		si->Com_Printf(" failed\n");
		FreeSound ();
		return qfalse;
	}
	si->Com_Printf("ok\n");

	si->Com_Printf ("...locking waveform buffer: ");
	lpData = GlobalLock(hData);
	if (!lpData) {
		si->Com_Printf( " failed\n" );
		FreeSound ();
		return qfalse;
	}
	memset (lpData, 0, gSndBufSize);
	si->Com_Printf("ok\n");

	/*
	 * Allocate and lock memory for the header. This memory must
	 * also be globally allocated with GMEM_MOVEABLE and
	 * GMEM_SHARE flags.
	 */
	si->Com_Printf ("...allocating waveform header: ");
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE,
		(DWORD) sizeof(WAVEHDR) * WAV_BUFFERS);

	if (hWaveHdr == NULL) {
		si->Com_Printf("failed\n");
		FreeSound ();
		return qfalse;
	}
	si->Com_Printf("ok\n");

	si->Com_Printf ("...locking waveform header: ");
	lpWaveHdr = (LPWAVEHDR) GlobalLock(hWaveHdr);

	if (lpWaveHdr == NULL) {
		si->Com_Printf("failed\n");
		FreeSound ();
		return qfalse;
	}
	memset (lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);
	si->Com_Printf("ok\n");

	/* After allocation, set up and prepare headers. */
	si->Com_Printf("...preparing headers: ");
	for (i=0 ; i<WAV_BUFFERS ; i++) {
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i*WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR)) !=
				MMSYSERR_NOERROR) {
			si->Com_Printf("failed\n");
			FreeSound ();
			return qfalse;
		}
	}
	si->Com_Printf("ok\n");

	si->dma->samples = gSndBufSize/(si->dma->samplebits/8);
	si->dma->samplepos = 0;
	si->dma->submission_chunk = 512;
	si->dma->buffer = (unsigned char *) lpData;
	sample16 = (si->dma->samplebits/8) - 1;

	wav_init = qtrue;

	return qtrue;
}


/**
 * @brief Try to find a sound device to mix for.
 * @return false if nothing is found.
 */
qboolean SND_Init(struct sndinfo *s)
{
	wav_init = qfalse;

	si = s;

	if (snd_firsttime || snd_iswave) {
		snd_iswave = SND_InitWav();

		if (snd_iswave) {
			if (snd_firsttime)
				si->Com_Printf ("Wave sound init succeeded\n");
		} else {
			si->Com_Printf ("Wave sound init failed\n");
		}
	}

	snd_firsttime = qfalse;

	snd_buffer_count = 1;

	if (!wav_init) {
		if (snd_firsttime)
			si->Com_Printf ("*** No sound device initialized ***\n");

		return qfalse;
	}

	return qtrue;
}

/**
 * @brief return the current sample position (in mono samples read)
 * inside the recirculating dma buffer, so the mixing code will know
 * how many sample are required to fill it up.
 */
int SND_GetDMAPos(void)
{
	int		s;

	if (wav_init)
		s = snd_sent * WAV_BUFFER_SIZE;

	s >>= sample16;

	s &= (si->dma->samples-1);

	return s;
}

/**
 * @brief
 */
void SND_BeginPainting (void)
{
}

/**
 @brief Send sound to device if buffer isn't really the dma buffer
 */
void SND_Submit(void)
{
	LPWAVEHDR	h;
	int			wResult;

	if (!si->dma->buffer)
		return;

	if (!wav_init)
		return;

	/* find which sound blocks have completed */
	while (1) {
		if ( snd_completed == snd_sent ) {
			si->Com_Printf ("Sound overrun\n");
			break;
		}

		if ( ! (lpWaveHdr[ snd_completed & WAV_MASK].dwFlags & WHDR_DONE) )
			break;

		snd_completed++;	/* this buffer has been played */
	}

	/* submit a few new sound blocks */
	while (((snd_sent - snd_completed) >> sample16) < 8) {
		h = lpWaveHdr + ( snd_sent&WAV_MASK );
		if (*(si->paintedtime)/256 <= snd_sent)
			break;
		snd_sent++;
		/*
		 * Now the data block can be sent to the output device. The
		 * waveOutWrite function returns immediately and waveform
		 * data is sent to the output device in the background.
		 */
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));

		if (wResult != MMSYSERR_NOERROR) {
			si->Com_Printf ("Failed to write block to device\n");
			FreeSound ();
			return;
		}
	}
}

/**
 * @brief Reset the sound device for exiting
 * @sa SND_Init
 */
void SND_Shutdown(void)
{
	FreeSound ();
}

/**
 * @brief Called when the main window gains or loses focus.
 * The window have been destroyed and recreated
 * between a deactivate and an activate.
 */
void SND_Activate (qboolean active)
{
}
