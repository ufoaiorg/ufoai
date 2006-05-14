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
#include <sys/ioctl.h>
#include "snd_oss.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int audio_fd = -1;
int snd_inited;

cvar_t *sndbits;
cvar_t *sndspeed;
cvar_t *sndchannels;
cvar_t *snddevice;

static int tryrates[] = { 11025, 22051, 44100, 48000, 8000 };

qboolean OSS_SNDDMA_Init(void)
{
	int rc, fmt, tmp, i, caps;
	struct audio_buf_info info;
	extern uid_t saved_euid;

	if (snd_inited)
		return qtrue;

	snd_inited = 0;

	Com_Printf("Soundsystem: OSS.\n");

	if (!snddevice)
	{
		sndbits = Cvar_Get("sndbits", "16", CVAR_ARCHIVE);
		sndspeed = Cvar_Get("sndspeed", "0", CVAR_ARCHIVE);
		sndchannels = Cvar_Get("sndchannels", "2", CVAR_ARCHIVE);
		snddevice = Cvar_Get("snddevice", "/dev/dsp", CVAR_ARCHIVE);
	}

	//alsa => oss
	if ( ! strcmp (snddevice->string, "default") )
	{
		snddevice->string = "/dev/dsp";
	}
	// open /dev/dsp, confirm capability to mmap, and get size of dma buffer

	if (audio_fd == -1)
	{
		seteuid ( saved_euid );

		// see https://www.redhat.com/archives/sound-list/1999-September/msg00012.html for reason
		audio_fd = open( snddevice->string, O_WRONLY );

		if (audio_fd == -1)
		{
			perror( snddevice->string );
			seteuid( getuid() );
			Com_Printf("SNDDMA_Init: Could not open %s.\n", snddevice->string);
			return qfalse;
		}
		seteuid( getuid() );
	}

	rc = ioctl( audio_fd, SNDCTL_DSP_RESET, 0 );
	if ( rc == -1 )
	{
		perror(snddevice->string);
		Com_Printf("SNDDMA_Init: Could not reset %s.\n", snddevice->string);
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	if ( ioctl ( audio_fd, SNDCTL_DSP_GETCAPS, &caps ) == -1 )
	{
		perror(snddevice->string);
		Com_Printf("SNDDMA_Init: Sound driver too old.\n");
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	if ( ! ( caps & DSP_CAP_TRIGGER ) || ! ( caps & DSP_CAP_MMAP ) )
	{
		Com_Printf("SNDDMA_Init: Sorry, but your soundcard doesn't support trigger or mmap. (%08x)\n", caps);
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	if ( ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) == -1 )
	{
		perror("GETOSPACE");
		Com_Printf("SNDDMA_Init: GETOSPACE ioctl failed.\n");
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	// set sample bits & speed

	dma.samplebits = (int)sndbits->value;
	if (dma.samplebits != 16 && dma.samplebits != 8)
	{
		ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &fmt);
		if (fmt & AFMT_S16_LE) dma.samplebits = 16;
		else if (fmt & AFMT_U8) dma.samplebits = 8;
	}

	if (dma.samplebits == 16)
	{
        	rc = AFMT_S16_LE;
		rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0)
		{
			perror(snddevice->string);
			Com_Printf("SNDDMA_Init: Could not support 16-bit data.  Try 8-bit.\n");
			close(audio_fd);
			audio_fd = -1;
			return qfalse;
		}
	}
	else if (dma.samplebits == 8)
    	{
		rc = AFMT_U8;
		rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0)
		{
			perror(snddevice->string);
			Com_Printf("SNDDMA_Init: Could not support 8-bit data.\n");
			close(audio_fd);
			audio_fd = -1;
			return qfalse;
		}
	}
	else
	{
		perror(snddevice->string);
		Com_Printf("SNDDMA_Init: %d-bit sound not supported.", dma.samplebits);
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	dma.speed = (int)sndspeed->value;
	if (!dma.speed)
	{
		for ( i = 0 ; i < sizeof ( tryrates ) / 4 ; i++ )
			if ( ! ioctl(audio_fd, SNDCTL_DSP_SPEED, &tryrates[i]) )
				break;
		dma.speed = tryrates[i];
	}

	dma.channels = (int)sndchannels->value;
	if (dma.channels < 1 || dma.channels > 2)
		dma.channels = 2;

	tmp = 0;
	if (dma.channels == 2)
		tmp = 1;
	rc = ioctl(audio_fd, SNDCTL_DSP_STEREO, &tmp); //FP: bugs here.
	if (rc < 0)
	{
		perror(snddevice->string);
		Com_Printf("SNDDMA_Init: Could not set %s to stereo=%d.", snddevice->string, dma.channels);
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	if (tmp)
		dma.channels = 2;
	else
		dma.channels = 1;


	rc = ioctl(audio_fd, SNDCTL_DSP_SPEED, &dma.speed);
	if (rc < 0)
	{
		perror(snddevice->string);
		Com_Printf("SNDDMA_Init: Could not set %s speed to %d.", snddevice->string, dma.speed);
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	dma.samples = info.fragstotal * info.fragsize / (dma.samplebits/8);
	dma.submission_chunk = 1;

	// memory map the dma buffer

	if (!dma.buffer)
		dma.buffer = (unsigned char *) mmap(NULL, info.fragstotal
			* info.fragsize, PROT_WRITE|PROT_READ, MAP_FILE|MAP_SHARED, audio_fd, 0);
	if (!dma.buffer || dma.buffer == MAP_FAILED)
	{
		perror(snddevice->string);
		Com_Printf("SNDDMA_Init: Could not mmap %s.\n", snddevice->string);
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	// toggle the trigger & start her up

	tmp = 0;
	rc  = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(snddevice->string);
		Com_Printf("SNDDMA_Init: Could not toggle. (1)\n");
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}
	tmp = PCM_ENABLE_OUTPUT;
	rc = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(snddevice->string);
		Com_Printf("SNDDMA_Init: Could not toggle. (2)\n");
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	dma.samplepos = 0;

	snd_inited = 1;
	return qtrue;
}

int OSS_SNDDMA_GetDMAPos(void)
{
	struct count_info count;

	if (!snd_inited)
		return 0;

	if (ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &count)==-1)
	{
// 		perror(snddevice->string);
		Com_Printf("SNDDMA_GetDMAPos: GETOPTR failed.\n");
		close(audio_fd);
		audio_fd = -1;
		snd_inited = 0;
		return 0;
	}
	dma.samplepos = count.ptr / (dma.samplebits / 8);

	return dma.samplepos;
}

void OSS_SNDDMA_Shutdown(void)
{
#if 0
	if (snd_inited)
	{
		close(audio_fd);
		audio_fd = -1;
		snd_inited = 0;
	}
#endif
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void OSS_SNDDMA_Submit(void)
{
}

void OSS_SNDDMA_BeginPainting (void)
{
}

