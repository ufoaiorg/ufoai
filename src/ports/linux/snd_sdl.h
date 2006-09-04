#if defined (__FreeBSD__)
#include <SDL11/SDL.h>
#include <SDL11/SDL_version.h>
#else
#include <SDL/SDL.h>
#include <SDL/SDL_version.h>
#endif

#include "../../client/client.h"
#include "../../client/snd_loc.h"

qboolean SDL_SNDDMA_Init (void);
int SDL_SNDDMA_GetDMAPos (void);
void SDL_SNDDMA_Shutdown (void);
void SDL_SNDDMA_Submit (void);
void SDL_SNDDMA_BeginPainting(void);
