/**
 * @file snd_jack.c
 * @brief Sound code for jack sound server
 * @todo finish the implementation
 */

/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
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

#include "snd_jack.h"

static jack_client_t *client;

static struct sndinfo *si;

static int sample_bytes;
static int buffer_bytes;

/**
 * @brief The sample rates which will be attempted.
 */
static const unsigned int RATES[] = { 48000, 44100, 22050, 11025 };

/**
 * @brief
 */
static int process_callback (jack_nframes_t nframes, void *arg)
{
	if (!si)
		return 0;

	si->dma->samplepos += nframes / (si->dma->samplebits / 4);
	si->S_PaintChannels(si->dma->samplepos);
	return si->dma->samplepos;
}

/**
 * @brief Initialize JACK
 */
qboolean SND_Init (struct sndinfo *s)
{
	jack_options_t options = JackNullOption;
	jack_status_t status;
	const char *client_name = "UFOAI";
	const char *server_name = NULL;
	si = s;

	client = jack_client_open(client_name, options, &status, server_name);
	if (client == NULL) {
		si->Com_Printf("jack_client_open() failed, status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			si->Com_Printf("Unable to connect to JACK server\n");
    	}
		return qfalse;
	}
	if (status & JackServerStarted)
		si->Com_Printf("JACK server started\n");
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		si->Com_Printf("unique name `%s' assigned\n", client_name);
	}
	/**
	 * tell the JACK server to call `jack_shutdown()' if it ever shuts down,
	 * either entirely, or if it just decides to stop calling us.
	 */
	/*jack_on_shutdown(client, jack_shutdown, 0);*/

	sample_bytes = jack_get_sample_rate(client);
	buffer_bytes = jack_get_buffer_size(client);

	if (jack_set_process_callback(client, process_callback, NULL) != 0) {
		si->Com_Printf("JACK: cannot set callback\n");
		return qfalse;
	}

	if (jack_activate(client)) {
		si->Com_Printf("JACK: cannot activate client\n");
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Returns the current sample position, if sound is running.
 */
int SND_GetDMAPos (void)
{
	return 0;
}

/**
 * @brief Closes the JACK client
 */
void SND_Shutdown (void)
{
	if (jack_deactivate(client))
		si->Com_Printf("JACK: cannot deactivate client\n");
	jack_client_close(client);
}

/**
 * @brief Writes the buffer to the server
 */
void SND_Submit (void)
{
}

/**
 * @brief Callback provided by the engine in case we need it
 */
void SND_BeginPainting (void)
{
}

/**
 * @brief Not needed (only win32 drivers)
 */
void SND_Activate (qboolean active)
{
}
