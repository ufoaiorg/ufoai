/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "e_event_explode.h"

/**
 * @brief Decides when the explode event should get executed. This in the impact time.
 */
int CL_ExplodeTime (const struct eventRegister_s* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	if (eventTiming->impactTime > cl.time)
		return eventTiming->impactTime;

	return eventTiming->nextTime;
}

/**
 * @note e.g. func_breakable or func_door with health
 * @sa EV_MODEL_EXPLODE
 */
void CL_Explode (const eventRegister_t* self, dbuffer* msg)
{
	int entnum;
	char sound[MAX_QPATH];

	NET_ReadFormat(msg, self->formatString, &entnum, sound, sizeof(sound));

	le_t* le = LE_Get(entnum);
	if (!le)
		LE_NotFoundError(entnum);

	le->inuse = false;
	if (le->modelnum1 > 0)
		cl.model_clip[le->modelnum1] = nullptr;

	const char* file = CL_ConvertSoundFromEvent(sound, sizeof(sound));
	Com_DPrintf(DEBUG_SOUND, "Play network sample %s at (%f:%f:%f)\n", file, le->origin[0], le->origin[1], le->origin[2]);
	S_LoadAndPlaySample(file, le->origin, SOUND_ATTN_NORM, SND_VOLUME_DEFAULT);

	/* Recalc the client routing table because this le (and the inline model) is now gone */
	CL_RecalcRouting(le);
}
