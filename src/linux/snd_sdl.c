/*
	snd_sdl.c

	Sound code taken from SDLQuake and modified to work with Quake2
	Robert B�ml 2001-12-25

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#include "snd_sdl.h"

#include "../client/client.h"
#include "../client/snd_loc.h"

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

qboolean SDL_SNDDMA_Init (void)
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
			Com_Printf(_("Unknown number of audio bits: %d\n"), desired_bits);
			return false;
	}
	desired.channels = (Cvar_Get("sndchannels", "2", CVAR_ARCHIVE))->value;
	desired.callback = paint_audio;

	Com_Printf (_("Bits: %i\n"), desired_bits );
	Com_Printf (_("Frequency: %i\n"), desired.freq );
	Com_Printf (_("Samples: %i\n"), desired.samples );
	Com_Printf (_("Channels: %i\n"), desired.channels );

	/* Open the audio device */
	if (SDL_OpenAudio (&desired, &obtained) == -1)
	{
		Com_Printf (_("Couldn't open SDL audio: %s\n"), SDL_GetError ());
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
			if (SDL_OpenAudio (&desired, NULL) == -1)
			{
				Com_Printf (_("Couldn't open SDL audio (format): %s\n"), SDL_GetError ());
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

int SDL_SNDDMA_GetDMAPos (void)
{
	return shm->dmapos;
}

void SDL_SNDDMA_Shutdown (void)
{
	if (snd_inited)
	{
		SDL_PauseAudio(1);
		SDL_CloseAudio ();
		snd_inited = 0;
		free(shm->buffer);
		shm->buffer = NULL;
		shm->samplepos = 0;
	}

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	Com_Printf(_("SDL audio device shut down.\n"));
}

/*
===============
SNDDMA_Submit
Send sound to device if buffer isn't really the dma buffer
===============
*/
void SDL_SNDDMA_Submit (void)
{
	SDL_UnlockAudio();
}

/*
===============
SNDDMA_BeginPainting
===============
*/
void SDL_SNDDMA_BeginPainting(void)
{
	SDL_LockAudio();
}
