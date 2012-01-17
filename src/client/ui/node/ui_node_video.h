/**
 * @file ui_node_video.h
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_UI_UI_NODE_VIDEO_H
#define CLIENT_UI_UI_NODE_VIDEO_H

#include "../ui_nodes.h"
#include "../../cinematic/cl_cinematic.h"

void UI_RegisterVideoNode(struct uiBehaviour_s *behaviour);

#define UI_VIDEOEXTRADATA_TYPE videoExtraData_t
#define UI_VIDEOEXTRADATA(node) UI_EXTRADATA(node, UI_VIDEOEXTRADATA_TYPE)
#define UI_VIDEOEXTRADATACONST(node) UI_EXTRADATACONST(node, UI_VIDEOEXTRADATA_TYPE)

struct uiAction_s;

typedef struct {
	cinematic_t cin;
	qboolean nosound;
	struct uiAction_s *onEnd;
} videoExtraData_t;

#endif
