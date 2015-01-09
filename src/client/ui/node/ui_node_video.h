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

#pragma once

#include "../ui_nodes.h"
#include "../../cinematic/cl_cinematic.h"

class uiVideoNode : public uiLocatedNode {
	void draw(uiNode_t* node) override;
	void drawOverWindow(uiNode_t* node) override;
	void onWindowOpened(uiNode_t* node, linkedList_t* params) override;
	void onWindowClosed(uiNode_t* node) override;
};

void UI_RegisterVideoNode(uiBehaviour_t* behaviour);

#define UI_VIDEOEXTRADATA_TYPE videoExtraData_t
#define UI_VIDEOEXTRADATA(node) UI_EXTRADATA(node, UI_VIDEOEXTRADATA_TYPE)
#define UI_VIDEOEXTRADATACONST(node) UI_EXTRADATACONST(node, UI_VIDEOEXTRADATA_TYPE)

struct uiAction_s;

typedef struct {
	const char* source;
	cinematic_t cin;
	bool nosound;
	struct uiAction_s* onEnd;
} videoExtraData_t;
