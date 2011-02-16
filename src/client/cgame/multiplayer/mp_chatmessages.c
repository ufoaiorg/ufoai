/**
 * @file mp_chatmessages.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "mp_chatmessages.h"
#include "../../cl_shared.h"
#include "../../ui/ui_main.h"
#include "../../ui/ui_data.h"
#include "../../ui/node/ui_node_text.h"

static linkedList_t *mp_chatMessageStack = NULL;

/**
 * @brief Displays a chat on the hud and add it to the chat buffer
 */
void MP_AddChatMessage (const char *text)
{
	char message[2048];
	Q_strncpyz(message, text, sizeof(message));
	LIST_AddString(&mp_chatMessageStack, Com_Trim(message));
	UI_RegisterLinkedListText(TEXT_CHAT_WINDOW, mp_chatMessageStack);
	UI_ExecuteConfunc("unhide_chatscreen");
}
