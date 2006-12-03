/**
 * @file snd_dx.c
 * @brief DirectX (DirectSound) sound driver
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
#include <float.h>

#include "../../client/client.h"
#define _IDirectSoundFullDuplex_
#include <dsound.h>
#include "winquake.h"

HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *, LPDIRECTSOUND FAR *, IUnknown FAR *);

/* 64K is > 1 second at 16-bit, 22050 Hz */
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define SECONDARY_BUFFER_SIZE	0x10000

typedef enum {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL} sndinitstat;

cvar_t *s_primary;

static qboolean	dsound_init;
static qboolean	snd_firsttime = qtrue;
static qboolean	primary_format_set;

/* starts at 0 for disabled */
static int snd_buffer_count = 0;
static int sample16;

/*
 * Global variables. Must be visible to window-procedure function
 * so it can unlock and free the data block after it has been played.
 */

static HANDLE hData;
static HPSTR lpData;

static HGLOBAL hWaveHdr;
static LPWAVEHDR lpWaveHdr;

static HWAVEOUT hWaveOut;

static DWORD gSndBufSize;

static MMTIME mmstarttime;

static LPDIRECTSOUND pDS;
static LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

static HINSTANCE hInstDS;

static sndinitstat SND_InitDirect (void);

static void FreeSound( void );

static struct sndinfo *si;

/**
 * @brief
 */
static const char *DSoundError( int error )
{
	switch ( error ) {
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

/**
 * @brief
 */
static qboolean DS_CreateBuffers( void )
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	WAVEFORMATEX	pformat, format;
	DWORD			dwWrite;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = si->dma->channels;
	format.wBitsPerSample = si->dma->samplebits;
	format.nSamplesPerSec = si->dma->speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec*format.nBlockAlign;

	si->Com_DPrintf("Creating DS buffers\n");

	si->Com_DPrintf("...setting EXCLUSIVE coop level: " );
	if ( DS_OK != pDS->lpVtbl->SetCooperativeLevel( pDS, si->cl_hwnd, DSSCL_EXCLUSIVE ) ) {
		si->Com_DPrintf("failed\n");
		FreeSound();
		return qfalse;
	}

	/* get access to the primary buffer, if possible, so we can set the */
	/* sound hardware format */
	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = qfalse;

	si->Com_DPrintf("...creating primary buffer: ");
	if (DS_OK == pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSPBuf, NULL)) {
		pformat = format;

		if (DS_OK != pDSPBuf->lpVtbl->SetFormat (pDSPBuf, &pformat)) {
			if (snd_firsttime)
				si->Com_DPrintf("...setting primary sound format: failed\n");
		} else {
			if (snd_firsttime)
				si->Com_DPrintf("...setting primary sound format: ok\n");

			primary_format_set = qtrue;
		}
	} else
		si->Com_DPrintf("failed\n");

	if ( !primary_format_set || !s_primary->value) {
		/* create the secondary buffer we'll actually work with */
		memset (&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		si->Com_DPrintf("...creating secondary buffer: ");
		if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL)) {
			si->Com_DPrintf("failed\n");
			FreeSound();
			return qfalse;
		}

		si->dma->channels = format.nChannels;
		si->dma->samplebits = format.wBitsPerSample;
		si->dma->speed = format.nSamplesPerSec;

		if (DS_OK != pDSBuf->lpVtbl->GetCaps (pDSBuf, &dsbcaps)) {
			si->Com_DPrintf("*** GetCaps failed ***\n");
			FreeSound();
			return qfalse;
		}

		si->Com_DPrintf("...using secondary sound buffer\n");
	} else {
		si->Com_DPrintf("...using primary buffer\n");

		si->Com_DPrintf("...setting WRITEPRIMARY coop level: ");
		if (DS_OK != pDS->lpVtbl->SetCooperativeLevel (pDS, si->cl_hwnd, DSSCL_WRITEPRIMARY)) {
			si->Com_DPrintf("failed\n");
			FreeSound();
			return qfalse;
		}

		if (DS_OK != pDSPBuf->lpVtbl->GetCaps (pDSPBuf, &dsbcaps)) {
			si->Com_DPrintf("*** GetCaps failed ***\n");
			return qfalse;
		}

		pDSBuf = pDSPBuf;
	}

	/* Make sure mixer is active */
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
		si->Com_DPrintf("   %d channel(s)\n   %d bits/sample\n   %d bytes/sec\n", si->dma->channels, si->dma->samplebits, si->dma->speed);

	gSndBufSize = dsbcaps.dwBufferBytes;

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = NULL;

	pDSBuf->lpVtbl->Stop(pDSBuf);
	pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmstarttime.u.sample, &dwWrite);
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	si->dma->samples = gSndBufSize/(si->dma->samplebits/8);
	si->dma->samplepos = 0;
	si->dma->submission_chunk = 1;
	si->dma->buffer = (unsigned char *) lpData;
	sample16 = (si->dma->samplebits/8) - 1;

	return qtrue;
}

/**
 * @brief
 */
static void DS_DestroyBuffers( void )
{
	si->Com_DPrintf("Destroying DS buffers\n");
	if ( pDS ) {
		si->Com_DPrintf("...setting NORMAL coop level\n");
		pDS->lpVtbl->SetCooperativeLevel( pDS, si->cl_hwnd, DSSCL_NORMAL );
	}

	if ( pDSBuf ) {
		si->Com_DPrintf("...stopping and releasing sound buffer\n");
		pDSBuf->lpVtbl->Stop( pDSBuf );
		pDSBuf->lpVtbl->Release( pDSBuf );
	}

	/* only release primary buffer if it's not also the mixing buffer we just released */
	if ( pDSPBuf && ( pDSBuf != pDSPBuf ) ) {
		si->Com_DPrintf( "...releasing primary buffer\n" );
		pDSPBuf->lpVtbl->Release( pDSPBuf );
	}
	pDSBuf = NULL;
	pDSPBuf = NULL;

	si->dma->buffer = NULL;
}

/**
 * @brief
 */
static void FreeSound (void)
{
	int i;

	si->Com_Printf("Shutting down sound system\n");

	if (pDS)
		DS_DestroyBuffers();

	if (hWaveOut) {
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

	if (pDS)
		pDS->lpVtbl->Release(pDS);

	if (hInstDS) {
		FreeLibrary(hInstDS);
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
	dsound_init = qfalse;
}

/**
 * @brief Direct-Sound support
 * @sa SND_Init
 */
static sndinitstat SND_InitDirect (void)
{
	DSCAPS dscaps;
	HRESULT hresult;

	si->dma->channels = 2;
	si->dma->samplebits = 16;

	if (si->khz->value == 48)
		si->dma->speed = 48000;
	else if (si->khz->value == 44)
		si->dma->speed = 44100;
	else if (si->khz->value == 22)
		si->dma->speed = 22050;
	else
		si->dma->speed = 11025;

	si->Com_DPrintf("Initializing DirectSound\n");

	if (!hInstDS) {
		hInstDS = LoadLibrary("dsound.dll");

		if (hInstDS == NULL) {
			si->Com_Printf("failed\n");
			return SIS_FAILURE;
		}

		pDirectSoundCreate = ( HRESULT (WINAPI *)(GUID FAR *, LPDIRECTSOUND FAR *, IUnknown FAR *) )GetProcAddress(hInstDS,"DirectSoundCreate");

		if (!pDirectSoundCreate) {
			si->Com_Printf ("*** couldn't get DS proc addr ***\n");
			return SIS_FAILURE;
		}
	}

	si->Com_DPrintf("...creating DS object: ");
	while ((hresult = pDirectSoundCreate(NULL, &pDS, NULL))!= DS_OK) {
		if (hresult != DSERR_ALLOCATED) {
			si->Com_DPrintf( "failed\n" );
			return SIS_FAILURE;
		}

		if (MessageBox (NULL,
						"The sound hardware is in use by another app.\n\n"
						"Select Retry to try to start sound again or Cancel to run Quake with no sound.",
						"Sound not available",
						MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY) {
			si->Com_DPrintf ("failed, hardware already in use\n" );
			return SIS_NOTAVAIL;
		}
	}
	si->Com_DPrintf("ok\n");

	dscaps.dwSize = sizeof(dscaps);

	if ( DS_OK != pDS->lpVtbl->GetCaps( pDS, &dscaps ) ) {
		si->Com_DPrintf ("*** couldn't get DS caps ***\n");
	}

	if ( dscaps.dwFlags & DSCAPS_EMULDRIVER ) {
		si->Com_Printf("...no DSound driver found\n");
		FreeSound();
		return SIS_FAILURE;
	}

	if ( !DS_CreateBuffers() )
		return SIS_FAILURE;

	dsound_init = qtrue;

	si->Com_DPrintf("...completed successfully\n" );

	return SIS_SUCCESS;
}

/**
 * @brief Try to find a sound device to mix for.
 * @return false if nothing is found.
 */
qboolean SND_Init(struct sndinfo *s)
{
	sndinitstat	stat;

	dsound_init = qfalse;
	si = s;
	s_primary = si->Cvar_Get("s_primary", "0", CVAR_ARCHIVE, NULL);

	stat = SIS_FAILURE;	/* assume DirectSound won't initialize */

	/* Init DirectSound */
	if (snd_firsttime) {
		stat = SND_InitDirect();

		if (stat == SIS_SUCCESS) {
			if (snd_firsttime)
				si->Com_DPrintf("dsound init succeeded\n");
		} else {
			si->Com_Printf ("*** dsound init failed ***\n");
		}
	}

	snd_firsttime = qfalse;

	snd_buffer_count = 1;

	if (!dsound_init) {
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
	MMTIME	mmtime;
	int		s = 0;
	DWORD	dwWrite;

	if (dsound_init) {
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmtime.u.sample, &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	}

	s >>= sample16;
	s &= (si->dma->samples-1);

	return s;
}

/**
 * @brief Reset the sound device for exiting
 */
void SND_Shutdown(void)
{
	FreeSound();
}

/**
 * @brief Makes sure si->dma->buffer is valid
 */
static DWORD locksize;
void SND_BeginPainting (void)
{
	int		reps;
	DWORD	dwSize2;
	LPVOID	pbuf, pbuf2;
	HRESULT	hresult;
	DWORD	dwStatus;

	if (!pDSBuf)
		return;

	/* if the buffer was lost or stopped, restore it and/or restart it */
	if (pDSBuf->lpVtbl->GetStatus (pDSBuf, &dwStatus) != DS_OK)
		si->Com_Printf ("Couldn't get sound buffer status\n");

	if (dwStatus & DSBSTATUS_BUFFERLOST)
		pDSBuf->lpVtbl->Restore (pDSBuf);

	if (!(dwStatus & DSBSTATUS_PLAYING))
		pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	/* lock the dsound buffer */
	reps = 0;
	si->dma->buffer = NULL;

	while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pbuf, &locksize,
								&pbuf2, &dwSize2, 0)) != DS_OK) {
		if (hresult != DSERR_BUFFERLOST) {
			si->Com_Printf( "S_TransferStereo16: Lock failed with error '%s'\n", DSoundError( hresult ) );
			/* FIXME: maybe a Sys_Error? */
			SND_Shutdown();
			SND_Init(si);
			return;
		} else
			pDSBuf->lpVtbl->Restore( pDSBuf );

		if (++reps > 2)
			return;
	}
	si->dma->buffer = (unsigned char *)pbuf;
}

/**
 * @brief Unlocks the dsound buffer
 */
void SND_Submit(void)
{
	if (!si->dma->buffer)
		return;

	/* unlock the dsound buffer */
	if (pDSBuf)
		pDSBuf->lpVtbl->Unlock(pDSBuf, si->dma->buffer, locksize, NULL, 0);
}

/**
 * @brief Called when the main window gains or loses focus.
 * The window have been destroyed and recreated
 * between a deactivate and an activate.
 */
void SND_Activate (qboolean active)
{
	if (active) {
		if (pDS && si->cl_hwnd)
			DS_CreateBuffers();
	} else {
		if (pDS && si->cl_hwnd)
			DS_DestroyBuffers();
	}
}
