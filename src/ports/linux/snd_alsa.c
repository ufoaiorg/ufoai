/**
 * @file snd_alsa.c
 * @brief Sound code for linux alsa sound architecture
 */

/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
*(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "snd_alsa.h"

#define BUFFER_SAMPLES 4096
#define SUBMISSION_CHUNK BUFFER_SAMPLES / 2

static snd_pcm_t *pcm_handle;
static snd_pcm_hw_params_t *hw_params;

/*static struct sndinfo * si; */

static int sample_bytes;
static int buffer_bytes;

/**
  * @brief The sample rates which will be attempted.
  */
static int RATES[] = { 48000, 44100, 22051, 11025, 8000 };

cvar_t *sndbits;
cvar_t *sndspeed;
cvar_t *sndchannels;
cvar_t *snddevice;

/**
  * @brief Initialize ALSA pcm device, and bind it to sndinfo.
  */
qboolean ALSA_SNDDMA_Init(struct sndinfo *si)
{
	int i, err, dir;
	unsigned int r;
	snd_pcm_uframes_t p;

	if (!snddevice) {
		sndbits = Cvar_Get("sndbits", "16", CVAR_ARCHIVE);
		sndspeed = Cvar_Get("sndspeed", "0", CVAR_ARCHIVE);
		sndchannels = Cvar_Get("sndchannels", "2", CVAR_ARCHIVE);
		snddevice = Cvar_Get("snddevice", "/dev/dsp", CVAR_ARCHIVE);
	}

	/*silly oss default */
	if(!Q_strcmp(snddevice->string, "/dev/dsp")) {
		snddevice->string = "default";
		Com_Printf( "Using sounddevice \"default\" - if you have problems with this try to set the cvar snddevice to a appropriate alsa-device" );
	}

	if((err = snd_pcm_open(&pcm_handle, snddevice->string, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
		Com_Printf("ALSA: cannot open device %s(%s)\n", snddevice->string, snd_strerror(err) );
		return qfalse;
	}

	if((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		Com_Printf("ALSA: cannot allocate hw params(%s)\n", snd_strerror(err));
		return qfalse;
	}

	if((err = snd_pcm_hw_params_any(pcm_handle, hw_params)) < 0){
		Com_Printf("ALSA: cannot init hw params(%s)\n", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return qfalse;
	}

	if((err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		Com_Printf("ALSA: cannot set access(%s)\n", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return qfalse;
	}

	dma.samplebits = (int)sndbits->value;
	/* try 16 by default */
	if(dma.samplebits != 8) {
		/* ensure this is set for other calculations */
		dma.samplebits = 16;

		if((err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16)) < 0) {
			Com_Printf("ALSA: 16 bit not supported, trying 8\n");
			dma.samplebits = 8;
		}
	}
	/* or 8 if specifically asked to */
	if(dma.samplebits == 8) {
		if((err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_U8)) < 0) {
			Com_Printf("ALSA: cannot set format(%s)\n", snd_strerror(err));
			snd_pcm_hw_params_free(hw_params);
			return qfalse;
		}
	}

	dma.speed = (int)sndspeed->value;
	/* try specified rate */
	if(dma.speed){
		r = dma.speed;

		if((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &r, &dir)) < 0)
			Com_Printf("ALSA: cannot set rate %d (%s)\n", r, snd_strerror(err));
		/* rate succeeded, but is perhaps slightly different */
		else {
			if(dir != 0)
				Com_Printf("ALSA: rate %d not supported, using %d\n", sndspeed->value, r);
			dma.speed = r;
		}
	}
	/* or all available ones */
	if(!dma.speed){
		for(i = 0; i < sizeof(RATES); i++) {
			r = RATES[i];
			dir = 0;

			if((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &r, &dir)) < 0)
				Com_Printf("ALSA: cannot set rate %d(%s)\n", r, snd_strerror(err));
			else {
				/* rate succeeded, but is perhaps slightly different */
				dma.speed = r;
				if(dir != 0)
					Com_Printf("ALSA: rate %d not supported, using %d\n", RATES[i], r);
				break;
			}
		}
	}
	/* failed */
	if(!dma.speed){
		Com_Printf("ALSA: cannot set rate\n");
		snd_pcm_hw_params_free(hw_params);
		return qfalse;
	}

	dma.channels = sndchannels->value;
	/* ensure either stereo or mono */
	if(dma.channels < 1 || dma.channels > 2)
		dma.channels = 2;

	if((err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, dma.channels)) < 0) {
		Com_Printf("ALSA: cannot set channels %d(%s)\n", sndchannels->value, snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return qfalse;
	}

	p = BUFFER_SAMPLES / dma.channels;
	if((err = snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &p, &dir)) < 0) {
		Com_Printf("ALSA: cannot set period size (%s)\n", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return qfalse;
	} else {
		/* rate succeeded, but is perhaps slightly different */
		if(dir != 0)
			Com_Printf("ALSA: period %d not supported, using %d\n", (BUFFER_SAMPLES/dma.channels), p);
	}

	/* set params */
	if((err = snd_pcm_hw_params(pcm_handle, hw_params)) < 0) {
		Com_Printf("ALSA: cannot set params(%s)\n", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return qfalse;
	}

	sample_bytes = dma.samplebits / 8;
	buffer_bytes = BUFFER_SAMPLES * sample_bytes;

	/* allocate pcm frame buffer */
	dma.buffer = malloc(buffer_bytes);
	memset(dma.buffer, 0, buffer_bytes);

	dma.samplepos = 0;

	dma.samples = BUFFER_SAMPLES;
	dma.submission_chunk = SUBMISSION_CHUNK;

	snd_pcm_prepare(pcm_handle);

	return qtrue;
}

/**
  * @brief Returns the current sample position, if sound is running.
  */
int ALSA_SNDDMA_GetDMAPos(void)
{
	if(dma.buffer)
		return dma.samplepos;

	Com_Printf("Sound not inizialized\n");
	return 0;
}

/**
  * @brief Closes the ALSA pcm device and frees the dma buffer.
  */
void ALSA_SNDDMA_Shutdown(void)
{
	if(dma.buffer) {
		snd_pcm_drop(pcm_handle);
		snd_pcm_close(pcm_handle);
	}

	free(dma.buffer);
	dma.buffer = 0;
}

/**
  * @brief Writes the dma buffer to the ALSA pcm device.
  */
void ALSA_SNDDMA_Submit(void)
{
	int s, w, frames;
	void *start;

	if(!dma.buffer)
		return;

	s = dma.samplepos * sample_bytes;
	start = (void *)&dma.buffer[s];

	frames = dma.submission_chunk / dma.channels;

	/* write to card */
	if((w = snd_pcm_writei(pcm_handle, start, frames)) < 0) {
		/* xrun occured */
		snd_pcm_prepare(pcm_handle);
		return;
	}

	/* mark progress */
	dma.samplepos += w * dma.channels;

	/* wrap buffer */
	if(dma.samplepos >= dma.samples)
		dma.samplepos = 0;
}

/**
  * @brief Callback provided by the engine in case we need it.  We don't.
  */
void ALSA_SNDDMA_BeginPainting(void)
{
}
