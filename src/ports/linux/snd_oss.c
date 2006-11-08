/**
 * @file snd_oss.c
 * @brief OSS sound driver
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

#include "snd_oss.h"

int audio_fd = -1;
int snd_inited;
static struct sndinfo *si;

static int tryrates[] = { 22050, 11025, 44100, 48000, 8000 };
static unsigned long mmaplen;

qboolean SND_Init(struct sndinfo *s)
{
	int rc, fmt, tmp, i, caps;
	struct audio_buf_info info;
	unsigned long sz;
	uid_t saved_euid = getuid();


	if (snd_inited)
		return qtrue;

	si =s;

	snd_inited = 0;

	si->Com_Printf("Soundsystem: OSS.\n");

	/*alsa => oss */
	if ( !strcmp (si->device->string, "default") )
		si->Cvar_Set("snd_device", "/dev/dsp");

	/* open /dev/dsp, confirm capability to mmap, and get size of dma buffer */
	if (audio_fd == -1) {
		seteuid (saved_euid);

		audio_fd = open(si->device->string, O_RDWR);

		if (audio_fd == -1) {
			tmp = 3;
			while ( (audio_fd < 0) && tmp-- &&
				((errno == EAGAIN) || (errno == EBUSY)) ) {
				sleep(1);
				/* maybe O_RDWR failed - try the original way */
				/* see https://www.redhat.com/archives/sound-list/1999-September/msg00012.html for reason */
				audio_fd = open(si->device->string, O_WRONLY|O_NONBLOCK);
			}
			if (audio_fd < 0) {
				perror(si->device->string);
				seteuid(getuid());
				si->Com_Printf("SND_Init: Could not open %s. %s\n", si->device->string, strerror(errno));
				return qfalse;
			}
		}
		seteuid( getuid() );
	}

	rc = ioctl( audio_fd, SNDCTL_DSP_RESET, 0 );
	if ( rc == -1 ) {
		perror(si->device->string);
		si->Com_Printf("SND_Init: Could not reset %s. %s\n", si->device->string, strerror(errno));
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	if ( ioctl ( audio_fd, SNDCTL_DSP_GETCAPS, &caps ) == -1 ) {
		perror(si->device->string);
		si->Com_Printf("SND_Init: Sound driver too old. %s\n", strerror(errno));
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	if ( ! ( caps & DSP_CAP_TRIGGER ) || ! ( caps & DSP_CAP_MMAP ) ) {
		si->Com_Printf("SND_Init: Sorry, but your soundcard doesn't support trigger or mmap. (%08x). %s\n", caps, strerror(errno));
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	/* set sample bits & speed */
	si->dma->samplebits = (int)si->bits->value;
	if (si->dma->samplebits != 16 && si->dma->samplebits != 8) {
		ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &fmt);
		if (fmt & AFMT_S16_LE)
			si->dma->samplebits = 16;
		else if (fmt & AFMT_U8)
			si->dma->samplebits = 8;
	}

	if (si->khz->value == 48)
		si->dma->speed = 48000;
	else if (si->khz->value == 44)
		si->dma->speed = 44100;
	else if (si->khz->value == 22)
		si->dma->speed = 22050;
	else
		si->dma->speed = 0;

	if (!si->dma->speed) {
		for ( i = 0 ; i < sizeof ( tryrates ) / 4 ; i++ )
			if ( ! ioctl(audio_fd, SNDCTL_DSP_SPEED, &tryrates[i]) )
				break;
		si->dma->speed = tryrates[i];
	}

	si->dma->channels = (int)si->channels->value;
	if (si->dma->channels < 1 || si->dma->channels > 2)
		si->dma->channels = 2;

	tmp = 0;
	if (si->dma->channels == 2)
		tmp = 1;
	rc = ioctl(audio_fd, SNDCTL_DSP_STEREO, &tmp); /*FP: bugs here. */
	if (rc < 0) {
		perror(si->device->string);
		si->Com_Printf("SND_Init: Could not set %s to stereo=%d. %s", si->device->string, si->dma->channels, strerror(errno));
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	if (tmp)
		si->dma->channels = 2;
	else
		si->dma->channels = 1;

	rc = ioctl(audio_fd, SNDCTL_DSP_SPEED, &si->dma->speed);
	if (rc < 0) {
		perror(si->device->string);
		si->Com_Printf("SND_Init: Could not set %s speed to %d. %s", si->device->string, si->dma->speed, strerror(errno));
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	if (si->dma->samplebits == 16) {
		rc = AFMT_S16_LE;
		rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0) {
			perror(si->device->string);
			si->Com_Printf("SND_Init: Could not support 16-bit data.  Try 8-bit. %s\n", strerror(errno));
			close(audio_fd);
			audio_fd = -1;
			return qfalse;
		}
	} else if (si->dma->samplebits == 8) {
		rc = AFMT_U8;
		rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0) {
			perror(si->device->string);
			si->Com_Printf("SND_Init: Could not support 8-bit data.\n");
			close(audio_fd);
			audio_fd = -1;
			return qfalse;
		}
	} else {
		perror(si->device->string);
		si->Com_Printf("SND_Init: %d-bit sound not supported.", si->dma->samplebits);
		close(audio_fd);
		audio_fd = -1;
		return qfalse;
	}

	si->Com_Printf("OSS: speed %i\n", si->dma->speed);
	si->Com_Printf("OSS: channels %i\n", si->dma->channels);
	si->Com_Printf("OSS: bits %i\n", si->dma->samplebits);

	if ( ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) == -1 ) {
/* 		perror("GETOSPACE"); */
		si->Com_Printf("SND_Init: GETOSPACE ioctl failed. %s\n", strerror(errno));
/* 		close(audio_fd); */
/* 		audio_fd = -1; */
/*		return qfalse;*/
	}

	si->dma->samples = info.fragstotal * info.fragsize / (si->dma->samplebits/8);
	si->dma->submission_chunk = 1;

	/* memory map the dma buffer */
	sz = sysconf (_SC_PAGESIZE);
	mmaplen = info.fragstotal * info.fragsize;
	mmaplen = (mmaplen + sz - 1) & ~(sz - 1);
	if (!si->dma->buffer)
		si->dma->buffer = (unsigned char *) mmap(NULL, mmaplen, 
			PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, audio_fd, 0);
	
	if (si->dma->buffer == MAP_FAILED) {
		si->Com_Printf("Could not mmap dma buffer PROT_WRITE|PROT_READ\n");
		si->Com_Printf("trying mmap PROT_WRITE (with associated better compatibility / less performance code)\n");
		si->dma->buffer = (unsigned char *) mmap(NULL, mmaplen
			* info.fragsize, PROT_WRITE, MAP_FILE|MAP_SHARED, audio_fd, 0);
	}

	if (!si->dma->buffer || si->dma->buffer == MAP_FAILED) {
		si->Com_Printf("SND_Init: Could not mmap %s. %s\n", si->device->string, strerror(errno));
		close(audio_fd);
		audio_fd = -1;
		return qfalse;;
	}
	si->Com_Printf ("...mmaped %u bytes buffer\n", mmaplen);

	/* toggle the trigger & start her up */
	tmp = 0;
	rc  = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0) {
		perror(si->device->string);
		si->Com_Printf("SND_Init: Could not toggle. (1)\n");
		close(audio_fd);
		audio_fd = -1;
		munmap (si->dma->buffer, mmaplen);
		return qfalse;
	}
	tmp = PCM_ENABLE_OUTPUT;
	rc = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0) {
		perror(si->device->string);
		si->Com_Printf("SND_Init: Could not toggle. (2)\n");
		close(audio_fd);
		audio_fd = -1;
		munmap (si->dma->buffer, mmaplen);
		return qfalse;
	}

	si->dma->samplepos = 0;

	snd_inited = 1;
	return qtrue;
}

int SND_GetDMAPos(void)
{
	struct count_info count;

	if (!snd_inited)
		return 0;

	if (ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &count)==-1) {
/* 		perror(si->device->string); */
		si->Com_Printf("SND_GetDMAPos: GETOPTR failed.\n");
		close(audio_fd);
		audio_fd = -1;
		snd_inited = 0;
		return 0;
	}
	si->dma->samplepos = count.ptr / (si->dma->samplebits / 8);

	return si->dma->samplepos;
}

void SND_Shutdown(void)
{
#if 0
	if (snd_inited) {
		close(audio_fd);
		audio_fd = -1;
		snd_inited = 0;
	}
#endif
}

/**
 * @brief Send sound to device if buffer isn't really the dma buffer
 */
void SND_Submit(void)
{
}

/**
 * @brief
 */
void SND_BeginPainting (void)
{
}

/**
 * @brief
 */
void SND_Activate (qboolean active)
{
}
