/**
 * @file snd_arts.c
 * @brief Sound code for linux arts sound daemon
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

#include <artsc.h>

#include "../../client/client.h"
#include "../../client/snd_loc.h"

#define snd_buf (si->dma->samples * 2)

static arts_stream_t stream;
static int snd_inited;
static struct sndinfo *si;

/**
 * @brief
 */
qboolean SND_Init(struct sndinfo *s)
{
	int errorcode;
	int frag_spec;

	if (snd_inited)
		return qtrue;

	si = s;

	errorcode = arts_init();

	if (errorcode)
		Com_Printf ("aRts: %s\n", arts_error_text(errorcode));

	si->dma->samplebits=(si->Cvar_Get("snd_bits", "16", CVAR_ARCHIVE, NULL))->value;

	si->Com_Printf("Initializing aRts\n");

	si->dma->speed=(si->Cvar_Get("snd_khz", "44", CVAR_ARCHIVE, NULL))->value;

	if (si->dma->speed >= 48)
		si->dma->speed = 48000;
	else if (si->dma->speed >= 44)
		si->dma->speed = 44100;
	else if (si->dma->speed >= 22)
		si->dma->speed = 22050;
	else
		si->dma->speed = 11025;

	si->dma->channels=(int)si->channels->value;

	if (si->dma->speed == 44100)
		si->dma->samples = (2048 * si->dma->channels);
	else if (si->dma->speed == 22050)
		si->dma->samples = (1024 * si->dma->channels);
	else
		si->dma->samples = (512 * si->dma->channels);

	for ( frag_spec = 0; (0x01<<frag_spec) < snd_buf; ++frag_spec );

	si->dma->buffer=malloc(snd_buf);
	memset(si->dma->buffer, 0, snd_buf);

	frag_spec |= 0x00020000;
	stream = arts_play_stream(si->dma->speed, si->dma->samplebits, si->dma->channels, "UFOStream");
	arts_stream_set(stream, ARTS_P_PACKET_SETTINGS, frag_spec);
	arts_stream_set(stream, ARTS_P_BLOCKING, 0);

	si->dma->samplepos = 0;
	si->dma->submission_chunk = 1;

	snd_inited=1;

	return qtrue;
}

/**
 * @brief
 */
int SND_GetDMAPos(void)
{
	if (snd_inited)
		return si->dma->samplepos;
	else
		Com_Printf ("Sound not inizialized\n");
	return 0;
}

/**
 * @brief
 */
void SND_Shutdown(void)
{
	if (!snd_inited) {
		Com_Printf ("Sound not inizialized\n");
		return;
	}
	arts_close_stream(stream);
	arts_free();
	snd_inited = 0;
	free(si->dma->buffer);
	si->dma->buffer = NULL;
	si->dma->samplepos = 0;
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
void SND_Submit(void)
{
	int written;

	if (!snd_inited)
		return;

	written = arts_write(stream, si->dma->buffer, snd_buf);
	si->dma->samplepos+=(written / (si->dma->samplebits / 8));
}

/**
 * @brief
 */
void SND_Activate (qboolean active)
{
}
