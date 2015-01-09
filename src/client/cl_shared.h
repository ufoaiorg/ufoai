/**
 * @file
 * @brief Share stuff between the different cgame implementations
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "../common/common.h"

#ifdef NO_I18N
#define bindtextdomain(IGNORE1, IGNORE2)
#define bind_textdomain_codeset(IGNORE1, IGNORE2)
#define textdomain(IGNORE1)
#define gettext(String) gettext_noop(String)
#else
/* i18n support via gettext */
#include <libintl.h>
#endif
#include <locale.h>

/* the used textdomain for gettext */
#define TEXT_DOMAIN "ufoai"
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#define INVDEF(containerID) (&csi.ids[(containerID)])

#define XVI_WIDTH		512
#define XVI_HEIGHT		256
#define RADAR_WIDTH		512
#define RADAR_HEIGHT	256

typedef struct geoscapeData_s {
	bool active;
	bool nationOverlay;
	bool xviOverlay;
	bool radarOverlay;
	const char* map;
	date_t date;

	/** this is the data that is used with r_xviTexture */
	byte r_xviAlpha[XVI_WIDTH * XVI_HEIGHT];

	/** this is the data that is used with r_radarTexture */
	byte r_radarPic[RADAR_WIDTH * RADAR_HEIGHT];

	/** this is the data that is used with r_radarTexture */
	byte r_radarSourcePic[RADAR_WIDTH * RADAR_HEIGHT];

	void* geoscapeNode;
} geoscapeData_t;

typedef enum {
	ca_uninitialized,
	ca_disconnected,			/**< not talking to a server */
	ca_connecting,				/**< sending request packets to the server */
	ca_connected,				/**< netchan_t established, waiting for svc_serverdata */
	ca_active					/**< game views should be displayed */
} connstate_t;

#define MapDef_ForeachSingleplayer(var) MapDef_ForeachCondition(var, (var)->singleplayer)
#define MapDef_ForeachSingleplayerCampaign(var) MapDef_ForeachCondition(var, (var)->singleplayer && (var)->campaign)

mapDef_t* Com_GetMapDefinitionByID(const char* mapDefID);

extern memPool_t* cl_genericPool;
