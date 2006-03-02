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

static snd_pcm_t *pcm_handle;
static snd_pcm_hw_params_t *hw_params;

static struct sndinfo *si;

/*
* These are reasonable default values for good latency.  If ALSA
* playback stutters or plain does not work, try adjusting these.
* Period must always be a multiple of 2.  Buffer must always be
* a multiple of period.  See http://alsa-project.org.
*/
static snd_pcm_uframes_t period_size = 1024;
static snd_pcm_uframes_t buffer_size = 4096;

static int sample_bytes;
static int buffer_bytes;

/*
*  The sample rates which will be attempted.
*/
static int RATES[] = { 44100, 22050, 11025 };

/*
*  Initialize ALSA pcm device, and bind it to sndinfo.
*/
qboolean ALSA_SNDDMA_Init(struct sndinfo *s){
	int i, r, err, dir;

	si = s;

	if(!strcmp(si->device->string, "/dev/dsp"))  //silly oss default
		si->device->string = "default";

	if((err = snd_pcm_open(&pcm_handle, si->device->string,
				SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0){
		si->Com_Printf("ALSA: cannot open device %s(%s)\n",
					   si->device->string, snd_strerror(err));
		return false;
	}

	if((err = snd_pcm_hw_params_malloc(&hw_params)) < 0){
		si->Com_Printf("ALSA: cannot allocate hw params(%s)\n",
					   snd_strerror(err));
		return false;
	}

	if((err = snd_pcm_hw_params_any(pcm_handle, hw_params)) < 0){
		si->Com_Printf("ALSA: cannot init hw params(%s)\n", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return false;
	}

	if((err = snd_pcm_hw_params_set_access(pcm_handle, hw_params,
				SND_PCM_ACCESS_RW_INTERLEAVED)) < 0){
		si->Com_Printf("ALSA: cannot set access(%s)\n", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return false;
	}

	si->dma->samplebits = si->bits->value;
	if(si->dma->samplebits != 8){  //try 16 by default

		si->dma->samplebits = 16;  //ensure this is set for other calculations

		if((err = snd_pcm_hw_params_set_format(pcm_handle, hw_params,
				SND_PCM_FORMAT_S16)) < 0){
			si->Com_Printf("ALSA: 16 bit not supported, trying 8\n");
			si->dma->samplebits = 8;
		}
	}
	if(si->dma->samplebits == 8){  //or 8 if specifically asked to
		if((err = snd_pcm_hw_params_set_format(pcm_handle, hw_params,
				SND_PCM_FORMAT_U8)) < 0){
			si->Com_Printf("ALSA: cannot set format(%s)\n", snd_strerror(err));
			snd_pcm_hw_params_free(hw_params);
			return false;
		}
	}

	si->dma->speed =(int)si->speed->value;
	if(si->dma->speed){  //try specified rate

		r = si->dma->speed;

		if((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &r, &dir)) < 0)
			si->Com_Printf("ALSA: cannot set rate %d(%s)\n", r, snd_strerror(err));
		else {  //rate succeeded, but is perhaps slightly different
			if(dir != 0)
				si->Com_Printf("ALSA: rate %d not supported, using %d\n", si->dma->speed, r);
			si->dma->speed = r;
		}
	}
	if(!si->dma->speed){  //or all available ones

		for(i = 0; i < sizeof(RATES); i++){

			r = RATES[i];
			dir = 0;

			if((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &r, &dir)) < 0)
				si->Com_Printf("ALSA: cannot set rate %d(%s)\n", r, snd_strerror(err));
			else {  //rate succeeded, but is perhaps slightly different
				si->dma->speed = r;
				if(dir != 0)
					si->Com_Printf("ALSA: rate %d not supported, using %d\n", RATES[i], r);
				break;
			}
		}
	}
	if(!si->dma->speed){  //failed
		si->Com_Printf("ALSA: cannot set rate\n");
		snd_pcm_hw_params_free(hw_params);
		return false;
	}

	si->dma->channels = (int)si->channels->value;

	if(si->dma->channels < 1 || si->dma->channels > 2)
		si->dma->channels = 2;  //ensure either stereo or mono

	if((err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params,
			   si->dma->channels)) < 0){
		si->Com_Printf("ALSA: cannot set channels %d(%s)\n",
					   si->dma->channels, snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return false;
	}

	if((err = snd_pcm_hw_params_set_period_size_near(pcm_handle,
			hw_params, &period_size, 0)) < 0){
		si->Com_Printf("ALSA: cannot set period size near(%s)\n", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return false;
	}

	if((err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle,
			hw_params, &buffer_size)) < 0){
		si->Com_Printf("ALSA: cannot set buffer size near(%s)\n", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return false;
	}

	if((err = snd_pcm_hw_params(pcm_handle, hw_params)) < 0){  //set params
		si->Com_Printf("ALSA: cannot set params(%s)\n", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		return false;
	}

	sample_bytes = si->dma->samplebits / 8;
	buffer_bytes = buffer_size * si->dma->channels * sample_bytes;

	si->dma->buffer = malloc(buffer_bytes);  //allocate pcm frame buffer
	memset(si->dma->buffer, 0, buffer_bytes);

	si->dma->samples = buffer_size * si->dma->channels;
	si->dma->submission_chunk = period_size * si->dma->channels;

	si->dma->samplepos = 0;

	snd_pcm_prepare(pcm_handle);
	return true;
}

/*
*  Returns the current sample position, if sound is running.
*/
int ALSA_SNDDMA_GetDMAPos(void){

	if(si->dma->buffer)
		return si->dma->samplepos;

	si->Com_Printf("Sound not inizialized\n");
	return 0;
}

/*
*  Closes the ALSA pcm device and frees the dma buffer.
*/
void ALSA_SNDDMA_Shutdown(void){

	if(!si->dma->buffer)
		return;

	snd_pcm_drop(pcm_handle);
	snd_pcm_close(pcm_handle);

	free(si->dma->buffer);
	si->dma->buffer = NULL;
}

/*
*  Writes the dma buffer to the ALSA pcm device.
*/
void ALSA_SNDDMA_Submit(void){
	int s, w, frames;
	void *start;

	if(!si->dma->buffer)
		return;

	s = si->dma->samplepos * sample_bytes;
	start = (void *) &si->dma->buffer[s];

	frames = si->dma->submission_chunk / si->dma->channels;

	if((w = snd_pcm_writei(pcm_handle, start, frames)) < 0){  //write to card
		snd_pcm_prepare(pcm_handle);  //xrun occured
		return;
	}

	si->dma->samplepos += w * si->dma->channels;  //mark progress

	if(si->dma->samplepos >= si->dma->samples)
		si->dma->samplepos = 0;  //wrap buffer
}

/*
*  Callback provided by the engine in case we need it.  We don't.
*/
void ALSA_SNDDMA_BeginPainting(void){}
