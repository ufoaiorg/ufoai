/**
 * @file
 *
 * for Mumble 1.2+  http://mumble.sourceforge.net/Link
 */

#include "../client.h"
#include "s_mumble.h"
#include "s_local.h"

#ifdef ANDROID

void S_MumbleInit (void)
{
}
void S_MumbleLink (void)
{
}
void S_MumbleUnlink (void)
{
}
void S_MumbleUpdate (const vec3_t origin, const vec3_t forward, const vec3_t right, const vec3_t up)
{
}

#else
#include <libmumblelink.h>

static cvar_t* snd_mumble;
static cvar_t* snd_mumble_alltalk;
static cvar_t* snd_mumble_scale;

void S_MumbleInit (void)
{
	snd_mumble = Cvar_GetOrCreate("snd_mumble", "0", CVAR_ARCHIVE | CVAR_LATCH);
	snd_mumble_alltalk = Cvar_GetOrCreate("snd_mumble_alltalk", "0", CVAR_ARCHIVE | CVAR_LATCH);
	snd_mumble_scale = Cvar_GetOrCreate("snd_mumble_scale", "0.0254", CVAR_ARCHIVE);
}

void S_MumbleLink (void)
{
	if (!s_env.initialized)
		return;

	if (!snd_mumble->integer)
		return;

	if (!mumble_islinked()) {
		int ret = mumble_link(GAME_TITLE_LONG);
		Com_Printf("S_MumbleLink: Linking to Mumble application %s\n", ret == 0 ? "ok" : "failed");
	}
}

void S_MumbleUnlink (void)
{
	if (!s_env.initialized)
		return;

	if (!snd_mumble->integer)
		return;

	if (mumble_islinked()) {
		Com_Printf("S_MumbleUnlink: Unlinking from Mumble application\n");
		mumble_unlink();
	}
}

void S_MumbleUpdate (const vec3_t origin, const vec3_t forward, const vec3_t right, const vec3_t up)
{
	if (!s_env.initialized)
		return;

	if (!snd_mumble->integer)
		return;

	vec3_t mp, mf, mt;
	VectorScale(origin, snd_mumble_scale->value, mp);
	VectorSet(mf, forward[0], forward[2], forward[1]);
	VectorSet(mt, up[0], up[2], up[1]);

	if (snd_mumble->integer == 2)
		Com_Printf("S_MumbleUpdate:\n%f, %f, %f\n%f, %f, %f\n%f, %f, %f", mp[0], mp[1], mp[2], mf[0], mf[1], mf[2], mt[0], mt[1], mt[2]);

	mumble_update_coordinates(mp, mf, mt);

#if 0
	const unsigned int playerNum = CL_GetPlayerNum();
	mumble_set_identity(CL_PlayerGetName(playerNum));
#endif

	char context[256];
	if (snd_mumble_alltalk->integer)
		Q_strncpyz(context, "*", sizeof(context));
	else
		Q_strncpyz(context, va("%i", cls.team), sizeof(context));
	mumble_set_context(context, strlen(context) + 1);
}

#endif
