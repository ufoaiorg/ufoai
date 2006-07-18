/*

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
#include "../client/client.h"

#ifdef USE_SDL
#include "../linux/snd_sdl.h"
#endif

#ifdef USE_ALSA
#include "../linux/snd_alsa.h"
#endif

#include "../linux/snd_oss.h"

static int sound_system_id;
cvar_t *s_system;

qboolean SNDDMA_Init(struct sndinfo *s)
{
	/* alsa is default for linux */
	s_system = Cvar_Get("s_system", "2", CVAR_ARCHIVE);
	s_system->modified = qfalse;

	/* when we select another soundsystem - we have to */
	/* stop the current running - but we need to know */
	/* which one runs... */
	sound_system_id = (int)s_system->value;

	switch ( sound_system_id ) {
		case 0:
			Com_Printf("Using OSS\n");
			return OSS_SNDDMA_Init();
			break;
#ifdef USE_SDL
		case 1:
			Com_Printf("Using SDL\n");
			return SDL_SNDDMA_Init();
			break;
#endif
#ifdef USE_ALSA
		case 2:
			Com_Printf("Using Alsa\n");
			return ALSA_SNDDMA_Init(s);
			break;
#endif
		default:
#ifdef USE_ALSA
			Com_Printf("Unknown soundsystem '%i' (now using ALSA)\n", (int)s_system->value );
			Cvar_Set("s_system", "2");
#else
			Com_Printf("Unknown soundsystem '%i' (now using OSS)\n", (int)s_system->value );
			Cvar_Set("s_system", "1");
#endif
			/* we've nothing started */
			s_system->modified = qfalse;
			return SNDDMA_Init(s);
	}
}

int SNDDMA_GetDMAPos(void)
{
	switch ( sound_system_id ) {
		case 0:
			return OSS_SNDDMA_GetDMAPos();
#ifdef USE_SDL
		case 1:
			return SDL_SNDDMA_GetDMAPos();
#endif
#ifdef USE_ALSA
		case 2:
			return ALSA_SNDDMA_GetDMAPos();
#endif
		default:
			return -1;
	}
}

void SNDDMA_Shutdown(void)
{
	switch ( sound_system_id ) {
		case 0:
			OSS_SNDDMA_Shutdown();
			break;
#ifdef USE_SDL
		case 1:
			SDL_SNDDMA_Shutdown();
			break;
#endif
#ifdef USE_ALSA
		case 2:
			ALSA_SNDDMA_Shutdown();
			break;
#endif
		default:
			break;
	}
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
	switch ( sound_system_id ) {
		case 0:
			OSS_SNDDMA_Submit();
			break;
#ifdef USE_SDL
		case 1:
			SDL_SNDDMA_Submit();
			break;
#endif
#ifdef USE_ALSA
		case 2:
			ALSA_SNDDMA_Submit();
			break;
#endif
		default:
			break;
	}
}

void SNDDMA_BeginPainting (void)
{
	switch ( sound_system_id ) {
		case 0:
			OSS_SNDDMA_BeginPainting();
			break;
#ifdef USE_SDL
		case 1:
			SDL_SNDDMA_BeginPainting();
			break;
#endif
#ifdef USE_ALSA
		case 2:
			ALSA_SNDDMA_BeginPainting();
			break;
#endif
		default:
			break;
	}
}

