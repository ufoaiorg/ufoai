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
#include <float.h>

#include "../client/client.h"
#include "../client/snd_loc.h"
#include "winquake.h"

#ifndef USE_SDL_SOUND
#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)

HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

typedef enum {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL} sndinitstat;

cvar_t	*s_wavonly;

static qboolean	dsound_init;
static qboolean	wav_init;
static qboolean	snd_firsttime = true, snd_isdirect, snd_iswave;
static qboolean	primary_format_set;

// starts at 0 for disabled
static int	snd_buffer_count = 0;
static int	sample16;
static int	snd_sent, snd_completed;

static struct sndinfo *si;

/*
 * Global variables. Must be visible to window-procedure function
 *  so it can unlock and free the data block after it has been played.
 */


HANDLE		hData;
HPSTR		lpData, lpData2;

HGLOBAL		hWaveHdr;
LPWAVEHDR	lpWaveHdr;

HWAVEOUT    hWaveOut;

WAVEOUTCAPS	wavecaps;

DWORD	gSndBufSize;

MMTIME		mmstarttime;

LPDIRECTSOUND pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

HINSTANCE hInstDS;

sndinitstat SNDDMA_InitDirect (void);
qboolean SNDDMA_InitWav (void);

void FreeSound( void );

static const char *DSoundError( int error )
{
	switch ( error )
	{
	case DSERR_BUFFERLOST:
		return "DSERR_BUFFERLOST";
	case DSERR_INVALIDCALL:
		return "DSERR_INVALIDCALLS";
	case DSERR_INVALIDPARAM:
		return "DSERR_INVALIDPARAM";
	case DSERR_PRIOLEVELNEEDED:
		return "DSERR_PRIOLEVELNEEDED";
	}

	return "unknown";
}

/*
** DS_CreateBuffers
*/
static qboolean DS_CreateBuffers( void )
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	WAVEFORMATEX	pformat, format;
	DWORD			dwWrite;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec*format.nBlockAlign;

	Com_Printf( "Creating DS buffers\n" );

	Com_DPrintf("...setting EXCLUSIVE coop level: " );
	if ( DS_OK != pDS->lpVtbl->SetCooperativeLevel( pDS, cl_hwnd, DSSCL_EXCLUSIVE ) )
	{
		Com_Printf ("failed\n");
		FreeSound ();
		return false;
	}
	Com_DPrintf("ok\n" );

	// get access to the primary buffer, if possible, so we can set the
	// sound hardware format
	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = false;

	Com_DPrintf( "...creating primary buffer: " );
	if (DS_OK == pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSPBuf, NULL))
	{
		pformat = format;

		Com_DPrintf( "ok\n" );
		if (DS_OK != pDSPBuf->lpVtbl->SetFormat (pDSPBuf, &pformat))
		{
			if (snd_firsttime)
				Com_DPrintf ("...setting primary sound format: failed\n");
		}
		else
		{
			if (snd_firsttime)
				Com_DPrintf ("...setting primary sound format: ok\n");

			primary_format_set = true;
		}
	}
	else
		Com_Printf( "failed\n" );

	if ( !primary_format_set || !s_primary->value)
	{
		// create the secondary buffer we'll actually work with
		memset (&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		Com_DPrintf( "...creating secondary buffer: " );
		if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL))
		{
			Com_Printf( "failed\n" );
			FreeSound ();
			return false;
		}
		Com_DPrintf( "ok\n" );

		dma.channels = format.nChannels;
		dma.samplebits = format.wBitsPerSample;
		dma.speed = format.nSamplesPerSec;

		if (DS_OK != pDSBuf->lpVtbl->GetCaps (pDSBuf, &dsbcaps))
		{
			Com_Printf ("*** GetCaps failed ***\n");
			FreeSound ();
			return false;
		}

		Com_Printf ("...using secondary sound buffer\n");
	}
	else
	{
		Com_Printf( "...using primary buffer\n" );

		Com_DPrintf( "...setting WRITEPRIMARY coop level: " );
		if (DS_OK != pDS->lpVtbl->SetCooperativeLevel (pDS, cl_hwnd, DSSCL_WRITEPRIMARY))
		{
			Com_Printf( "failed\n" );
			FreeSound ();
			return false;
		}
		Com_DPrintf( "ok\n" );

		if (DS_OK != pDSPBuf->lpVtbl->GetCaps (pDSPBuf, &dsbcaps))
		{
			Com_Printf ("*** GetCaps failed ***\n");
			return false;
		}

		pDSBuf = pDSPBuf;
	}

	// Make sure mixer is active
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
		Com_Printf("   %d channel(s)\n"
		               "   %d bits/sample\n"
					   "   %d bytes/sec\n",
					   dma.channels, dma.samplebits, dma.speed);

	gSndBufSize = dsbcaps.dwBufferBytes;

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = NULL;

	pDSBuf->lpVtbl->Stop(pDSBuf);
	pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmstarttime.u.sample, &dwWrite);
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	dma.samples = gSndBufSize/(dma.samplebits/8);
	dma.samplepos = 0;
	dma.submission_chunk = 1;
	dma.buffer = (unsigned char *) lpData;
	sample16 = (dma.samplebits/8) - 1;

	return true;
}

/*
** DS_DestroyBuffers
*/
static void DS_DestroyBuffers( void )
{
	Com_DPrintf( "Destroying DS buffers\n" );
	if ( pDS )
	{
		Com_DPrintf( "...setting NORMAL coop level\n" );
		pDS->lpVtbl->SetCooperativeLevel( pDS, cl_hwnd, DSSCL_NORMAL );
	}

	if ( pDSBuf )
	{
		Com_DPrintf( "...stopping and releasing sound buffer\n" );
		pDSBuf->lpVtbl->Stop( pDSBuf );
		pDSBuf->lpVtbl->Release( pDSBuf );
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if ( pDSPBuf && ( pDSBuf != pDSPBuf ) )
	{
		Com_DPrintf( "...releasing primary buffer\n" );
		pDSPBuf->lpVtbl->Release( pDSPBuf );
	}
	pDSBuf = NULL;
	pDSPBuf = NULL;

	dma.buffer = NULL;
}

/*
==================
FreeSound
==================
*/
void FreeSound (void)
{
	int		i;

	Com_DPrintf( "Shutting down sound system\n" );

	if ( pDS )
		DS_DestroyBuffers();

	if ( hWaveOut )
	{
		Com_DPrintf( "...resetting waveOut\n" );
		waveOutReset (hWaveOut);

		if (lpWaveHdr)
		{
			Com_DPrintf( "...unpreparing headers\n" );
			for (i=0 ; i< WAV_BUFFERS ; i++)
				waveOutUnprepareHeader (hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR));
		}

		Com_DPrintf( "...closing waveOut\n" );
		waveOutClose (hWaveOut);

		if (hWaveHdr)
		{
			Com_DPrintf( "...freeing WAV header\n" );
			GlobalUnlock(hWaveHdr);
			GlobalFree(hWaveHdr);
		}

		if (hData)
		{
			Com_DPrintf( "...freeing WAV buffer\n" );
			GlobalUnlock(hData);
			GlobalFree(hData);
		}

	}

	if ( pDS )
	{
		Com_DPrintf( "...releasing DS object\n" );
		pDS->lpVtbl->Release( pDS );
	}

	if ( hInstDS )
	{
		Com_DPrintf( "...freeing DSOUND.DLL\n" );
		FreeLibrary( hInstDS );
		hInstDS = NULL;
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;
}

/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
sndinitstat SNDDMA_InitDirect (void)
{
	DSCAPS			dscaps;
	HRESULT			hresult;

	dma.channels = 2;
	dma.samplebits = 16;

	if (s_khz->value == 44)
		dma.speed = 44100;
	else if (s_khz->value == 22)
		dma.speed = 22050;
	else
		dma.speed = 11025;

	Com_Printf( "Initializing DirectSound\n");

	if ( !hInstDS )
	{
		Com_DPrintf( "...loading dsound.dll: " );

		hInstDS = LoadLibrary("dsound.dll");

		if (hInstDS == NULL)
		{
			Com_Printf ("failed\n");
			return SIS_FAILURE;
		}

		Com_DPrintf ("ok\n");
		pDirectSoundCreate = (void *)GetProcAddress(hInstDS,"DirectSoundCreate");

		if (!pDirectSoundCreate)
		{
			Com_Printf ("*** couldn't get DS proc addr ***\n");
			return SIS_FAILURE;
		}
	}

	Com_DPrintf( "...creating DS object: " );
	while ( ( hresult = iDirectSoundCreate( NULL, &pDS, NULL ) ) != DS_OK )
	{
		if (hresult != DSERR_ALLOCATED)
		{
			Com_Printf( "failed\n" );
			return SIS_FAILURE;
		}

		if (MessageBox (NULL,
						"The sound hardware is in use by another app.\n\n"
					    "Select Retry to try to start sound again or Cancel to run Quake with no sound.",
						"Sound not available",
						MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Com_Printf ("failed, hardware already in use\n" );
			return SIS_NOTAVAIL;
		}
	}
	Com_DPrintf( "ok\n" );

	dscaps.dwSize = sizeof(dscaps);

	if ( DS_OK != pDS->lpVtbl->GetCaps( pDS, &dscaps ) )
	{
		Com_Printf ("*** couldn't get DS caps ***\n");
	}

	if ( dscaps.dwFlags & DSCAPS_EMULDRIVER )
	{
		Com_DPrintf ("...no DSound driver found\n" );
		FreeSound();
		return SIS_FAILURE;
	}

	if ( !DS_CreateBuffers() )
		return SIS_FAILURE;

	dsound_init = true;

	Com_DPrintf("...completed successfully\n" );

	return SIS_SUCCESS;
}


/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
qboolean SNDDMA_InitWav (void)
{
	WAVEFORMATEX  format;
	int				i;
	HRESULT			hr;

	Com_Printf( "Initializing wave sound\n" );

	snd_sent = 0;
	snd_completed = 0;

	dma.channels = 2;
	dma.samplebits = 16;

	if (s_khz->value == 44)
		dma.speed = 44100;
	else if (s_khz->value == 22)
		dma.speed = 22050;
	else
		dma.speed = 11025;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels
		*format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec
		*format.nBlockAlign;

	/* Open a waveform device for output using window callback. */
	Com_DPrintf ("...opening waveform device: ");
	while ((hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER,
					&format,
					0, 0L, CALLBACK_NULL)) != MMSYSERR_NOERROR)
	{
		if (hr != MMSYSERR_ALLOCATED)
		{
			Com_Printf ("failed\n");
			return false;
		}

		if (MessageBox (NULL,
						"The sound hardware is in use by another app.\n\n"
					    "Select Retry to try to start sound again or Cancel to run Quake 2 with no sound.",
						"Sound not available",
						MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Com_Printf ("hw in use\n" );
			return false;
		}
	}
	Com_DPrintf( "ok\n" );

	/*
	 * Allocate and lock memory for the waveform data. The memory
	 * for waveform data must be globally allocated with
	 * GMEM_MOVEABLE and GMEM_SHARE flags.

	*/
	Com_DPrintf ("...allocating waveform buffer: ");
	gSndBufSize = WAV_BUFFERS*WAV_BUFFER_SIZE;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData)
	{
		Com_Printf( " failed\n" );
		FreeSound ();
		return false;
	}
	Com_DPrintf( "ok\n" );

	Com_DPrintf ("...locking waveform buffer: ");
	lpData = GlobalLock(hData);
	if (!lpData)
	{
		Com_Printf( " failed\n" );
		FreeSound ();
		return false;
	}
	memset (lpData, 0, gSndBufSize);
	Com_DPrintf( "ok\n" );

	/*
	 * Allocate and lock memory for the header. This memory must
	 * also be globally allocated with GMEM_MOVEABLE and
	 * GMEM_SHARE flags.
	 */
	Com_DPrintf ("...allocating waveform header: ");
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE,
		(DWORD) sizeof(WAVEHDR) * WAV_BUFFERS);

	if (hWaveHdr == NULL)
	{
		Com_Printf( "failed\n" );
		FreeSound ();
		return false;
	}
	Com_DPrintf( "ok\n" );

	Com_DPrintf ("...locking waveform header: ");
	lpWaveHdr = (LPWAVEHDR) GlobalLock(hWaveHdr);

	if (lpWaveHdr == NULL)
	{
		Com_Printf( "failed\n" );
		FreeSound ();
		return false;
	}
	memset (lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);
	Com_DPrintf( "ok\n" );

	/* After allocation, set up and prepare headers. */
	Com_DPrintf ("...preparing headers: ");
	for (i=0 ; i<WAV_BUFFERS ; i++)
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i*WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR)) !=
				MMSYSERR_NOERROR)
		{
			Com_Printf ("failed\n");
			FreeSound ();
			return false;
		}
	}
	Com_DPrintf ("ok\n");

	dma.samples = gSndBufSize/(dma.samplebits/8);
	dma.samplepos = 0;
	dma.submission_chunk = 512;
	dma.buffer = (unsigned char *) lpData;
	sample16 = (dma.samplebits/8) - 1;

	wav_init = true;

	return true;
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/
qboolean SNDDMA_Init(struct sndinfo *s)
{
	sndinitstat	stat;

	memset ((void *)&dma, 0, sizeof (dma));

	s_wavonly = Cvar_Get ("s_wavonly", "0", 0);

	dsound_init = wav_init = 0;

	stat = SIS_FAILURE;	// assume DirectSound won't initialize

	/* Init DirectSound */
	if (!s_wavonly->value)
	{
		if (snd_firsttime || snd_isdirect)
		{
			stat = SNDDMA_InitDirect ();

			if (stat == SIS_SUCCESS)
			{
				snd_isdirect = true;

				if (snd_firsttime)
					Com_Printf ("dsound init succeeded\n" );
			}
			else
			{
				snd_isdirect = false;
				Com_Printf ("*** dsound init failed ***\n");
			}
		}
	}

// if DirectSound didn't succeed in initializing, try to initialize
// waveOut sound, unless DirectSound failed because the hardware is
// already allocated (in which case the user has already chosen not
// to have sound)
	if (!dsound_init && (stat != SIS_NOTAVAIL))
	{
		if (snd_firsttime || snd_iswave)
		{

			snd_iswave = SNDDMA_InitWav ();

			if (snd_iswave)
			{
				if (snd_firsttime)
					Com_Printf ("Wave sound init succeeded\n");
			}
			else
			{
				Com_Printf ("Wave sound init failed\n");
			}
		}
	}

	snd_firsttime = false;

	snd_buffer_count = 1;

	if (!dsound_init && !wav_init)
	{
		if (snd_firsttime)
			Com_Printf ("*** No sound device initialized ***\n");

		return 0;
	}

	return 1;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	MMTIME	mmtime;
	int		s;
	DWORD	dwWrite;

	if (dsound_init)
	{
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmtime.u.sample, &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	}
	else if (wav_init)
	{
		s = snd_sent * WAV_BUFFER_SIZE;
	}


	s >>= sample16;

	s &= (dma.samples-1);

	return s;
}

/*
==============
SNDDMA_BeginPainting

Makes sure dma.buffer is valid
===============
*/
DWORD	locksize;
void SNDDMA_BeginPainting (void)
{
	int		reps;
	DWORD	dwSize2;
	DWORD	*pbuf, *pbuf2;
	HRESULT	hresult;
	DWORD	dwStatus;

	if (!pDSBuf)
		return;

	// if the buffer was lost or stopped, restore it and/or restart it
	if (pDSBuf->lpVtbl->GetStatus (pDSBuf, &dwStatus) != DS_OK)
		Com_Printf ("Couldn't get sound buffer status\n");

	if (dwStatus & DSBSTATUS_BUFFERLOST)
		pDSBuf->lpVtbl->Restore (pDSBuf);

	if (!(dwStatus & DSBSTATUS_PLAYING))
		pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	// lock the dsound buffer

	reps = 0;
	dma.buffer = NULL;

	while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pbuf, &locksize,
								   &pbuf2, &dwSize2, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Com_Printf( "S_TransferStereo16: Lock failed with error '%s'\n", DSoundError( hresult ) );
			S_Shutdown ();
			return;
		}
		else
		{
			pDSBuf->lpVtbl->Restore( pDSBuf );
		}

		if (++reps > 2)
			return;
	}
	dma.buffer = (unsigned char *)pbuf;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
Also unlocks the dsound buffer
===============
*/
void SNDDMA_Submit(void)
{
	LPWAVEHDR	h;
	int			wResult;

	if (!dma.buffer)
		return;

	// unlock the dsound buffer
	if (pDSBuf)
		pDSBuf->lpVtbl->Unlock(pDSBuf, dma.buffer, locksize, NULL, 0);

	if (!wav_init)
		return;

	//
	// find which sound blocks have completed
	//
	while (1)
	{
		if ( snd_completed == snd_sent )
		{
			Com_DPrintf ("Sound overrun\n");
			break;
		}

		if ( ! (lpWaveHdr[ snd_completed & WAV_MASK].dwFlags & WHDR_DONE) )
		{
			break;
		}

		snd_completed++;	// this buffer has been played
	}

//Com_Printf ("completed %i\n", snd_completed);
	//
	// submit a few new sound blocks
	//
	while (((snd_sent - snd_completed) >> sample16) < 8)
	{
		h = lpWaveHdr + ( snd_sent&WAV_MASK );
	if (paintedtime/256 <= snd_sent)
		break;	//	Com_Printf ("submit overrun\n");
//Com_Printf ("send %i\n", snd_sent);
		snd_sent++;
		/*
		 * Now the data block can be sent to the output device. The
		 * waveOutWrite function returns immediately and waveform
		 * data is sent to the output device in the background.
		 */
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));

		if (wResult != MMSYSERR_NOERROR)
		{
			Com_Printf ("Failed to write block to device\n");
			FreeSound ();
			return;
		}
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	FreeSound ();
}


/*
===========
S_Activate

Called when the main window gains or loses focus.
The window have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void S_Activate (qboolean active)
{
	if ( active )
	{
		if ( pDS && cl_hwnd && snd_isdirect )
		{
			DS_CreateBuffers();
		}
	}
	else
	{
		if ( pDS && cl_hwnd && snd_isdirect )
		{
			DS_DestroyBuffers();
		}
	}
}

#else // ifndef USE_SDL

static int  snd_inited;
static dma_t *shm;
cvar_t* sdlMixSamples;

static void paint_audio (void *unused, Uint8 * stream, int len)
{
	if (shm) {
		int pos = (shm->dmapos * (shm->samplebits/8));
		if (pos >= shm->dmasize)
			shm->dmapos = pos = 0;

#if 0
		shm->buffer = stream;
		shm->samplepos += len / (shm->samplebits / 4);

		// Check for samplepos overflow?
		S_PaintChannels (shm->samplepos);
#else
		int tobufend = shm->dmasize - pos;  /* bytes to buffer's end. */
		int len1 = len;
		int len2 = 0;

		if (len1 > tobufend)
		{
			len1 = tobufend;
			len2 = len - len1;
		}
		memcpy(stream, shm->buffer + pos, len1);
		if (len2 <= 0)
			shm->dmapos += (len1 / (shm->samplebits/8));
		else  /* wraparound? */
		{
			memcpy(stream+len1, shm->buffer, len2);
			shm->dmapos = (len2 / (shm->samplebits/8));
		}
#endif
		if (shm->dmapos >= shm->dmasize)
			shm->dmapos = 0;
	}
}

qboolean SNDDMA_Init (struct sndinfo *s)
{
	SDL_AudioSpec desired, obtained;
	int desired_bits, freq, tmp;
	char drivername[128];

	if (snd_inited)
		return true;

	snd_inited = 0;

	Com_Printf("Soundsystem: SDL.\n");

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		if (SDL_Init(SDL_INIT_AUDIO) == -1)
		{
			Com_Printf("Couldn't init SDL audio: %s\n", SDL_GetError () );
			return false;
		}
	}

	if (SDL_AudioDriverName(drivername, sizeof (drivername)) == NULL)
		strcpy(drivername, "(UNKNOWN)");
	Com_Printf("SDL audio driver is \"%s\".\n", drivername);

	memset(&desired, '\0', sizeof (desired));
	memset(&obtained, '\0', sizeof (obtained));

	sdlMixSamples = Cvar_Get("s_sdlMixSamps", "0", CVAR_ARCHIVE);

	desired_bits = (Cvar_Get("sndbits", "16", CVAR_ARCHIVE))->value;

	/* Set up the desired format */
	freq = (Cvar_Get("s_khz", "0", CVAR_ARCHIVE))->value;
	if (freq == 44)
	{
		desired.freq = 44100;
		desired.samples = 1024;
	}
	else if (freq == 22)
	{
		desired.freq = 22050;
		desired.samples = 512;
	}
	else
	{
		desired.freq = 11025;
		desired.samples = 256;
	}

	switch (desired_bits)
	{
		case 8:
			desired.format = AUDIO_U8;
			break;
		case 16:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				desired.format = AUDIO_S16MSB;
			else
				desired.format = AUDIO_S16LSB;
			break;
		default:
			Com_Printf ("Unknown number of audio bits: %d\n", desired_bits);
			return false;
	}
	desired.channels = (Cvar_Get("sndchannels", "2", CVAR_ARCHIVE))->value;
	desired.callback = paint_audio;

	Com_Printf ("Bits: %i\n", desired_bits );
	Com_Printf ("Frequency: %i\n", desired.freq );
	Com_Printf ("Samples: %i\n", desired.samples );
	Com_Printf ("Channels: %i\n", desired.channels );

	/* Open the audio device */
	if (SDL_OpenAudio (&desired, &obtained) < 0)
	{
		Com_Printf ("Couldn't open SDL audio - quitting soundsystem: %s\n", SDL_GetError ());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return false;
	}

	/* Make sure we can support the audio format */
	switch (obtained.format)
	{
		case AUDIO_U8:
			/* Supported */
			break;
		case AUDIO_S16LSB:
		case AUDIO_S16MSB:
			if (((obtained.format == AUDIO_S16LSB) &&
				 (SDL_BYTEORDER == SDL_LIL_ENDIAN)) ||
				((obtained.format == AUDIO_S16MSB) &&
				 (SDL_BYTEORDER == SDL_BIG_ENDIAN)))
			{
				/* Supported */
				break;
			}
			/* Unsupported, fall through */ ;
		default:
			/* Not supported -- force SDL to do our bidding */
			SDL_CloseAudio ();
			if (SDL_OpenAudio (&desired, NULL) < 0)
			{
				Com_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
				return false;
			}
			memcpy (&obtained, &desired, sizeof (desired));
			break;
	}

	// dma.samples needs to be big, or id's mixer will just refuse to
	//  work at all; we need to keep it significantly bigger than the
	//  amount of SDL callback samples, and just copy a little each time
	//  the callback runs.
	// 32768 is what the OSS driver filled in here on my system. I don't
	//  know if it's a good value overall, but at least we know it's
	//  reasonable...this is why I let the user override.
	tmp = sdlMixSamples->value;
	if (!tmp)
		tmp = (obtained.samples * obtained.channels) * 10;

	if (tmp & (tmp - 1))  // not a power of two? Seems to confuse something.
	{
		int val = 1;
		while (val < tmp)
			val <<= 1;

		tmp = val;
	}

	/* Fill the audio DMA information block */
	shm = &dma;
	shm->samplebits = (obtained.format & 0xFF);
	shm->speed = obtained.freq;
	shm->channels = obtained.channels;
	shm->samples = tmp;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->dmasize = (shm->samples * (shm->samplebits/8));
	shm->buffer = calloc(1, shm->dmasize);;

	SDL_PauseAudio (0);
	snd_inited = 1;
	return true;
}

int SNDDMA_GetDMAPos (void)
{
	return shm->samplepos;
}

void SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		SDL_CloseAudio ();
		snd_inited = 0;
	}

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/*
SNDDMA_Submit
Send sound to device if buffer isn't really the dma buffer
*/
void SNDDMA_Submit (void)
{
}


void SNDDMA_BeginPainting(void)
{
}

void S_Activate (qboolean active){}

#endif // ifndef USE_SDL
