/**
 * @file m_node_airfightmap.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../../client.h"
#include "../../campaign/cl_airfightmap.h"
#include "../m_main.h"
#include "../m_draw.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_nodes.h"
#include "m_node_model.h"
#include "m_node_airfightmap.h"

#define MAX_AIRFIGHT_ZOOM	35.0
#define MIN_AIRFIGHT_ZOOM	5.0

static void MN_AirfightmapNodeDraw (menuNode_t *node)
{
	if (curCampaign) {
		/* don't run the campaign in console mode */
		if (cls.key_dest != key_console)
			CL_CampaignRun();	/* advance time */
		AFM_DrawMap(node); /* Draw airfight map */
	}
}

static void MN_AirfightmapNodeRightClick (menuNode_t *node, int x, int y)
{

}

static void MN_AirfightmapNodeMouseWheel (menuNode_t *node, qboolean down, int x, int y)
{
	AFM_StopSmoothMovement();
	ccs.zoom *= pow(0.995, (down ? 10 : -10));
	Com_DPrintf(DEBUG_CLIENT, "zoom level %f \n", ccs.zoom);

	if (ccs.zoom < MIN_AIRFIGHT_ZOOM)
		ccs.zoom = MIN_AIRFIGHT_ZOOM;
	else if (ccs.zoom > MAX_AIRFIGHT_ZOOM) {
		ccs.zoom = MAX_AIRFIGHT_ZOOM;
	}
}

void MN_RegisterAirfightMapNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "airfightmap";
	behaviour->draw = MN_AirfightmapNodeDraw;
	behaviour->rightClick = MN_AirfightmapNodeRightClick;
	behaviour->mouseWheel = MN_AirfightmapNodeMouseWheel;
}
