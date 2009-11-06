/**
 * @file mp_chatmessages.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "../client.h"
#include "../menu/m_main.h"
#include "../menu/m_data.h"
#include "../menu/node/m_node_text.h"
#include "mp_callbacks.h"
#include "mp_chatmessages.h"

/**< @brief buffer that hold all printed chat messages for menu display */
static char *chatBuffer = NULL;

/** @brief Stores all chat messages from a multiplayer game */
typedef struct chatMessage_s {
	char *text;
	struct chatMessage_s *next;
} chatMessage_t;

static chatMessage_t *mp_chatMessageStack;

#define MAX_CHAT_TEXT 4096

/**
 * @brief Displays a chat on the hud and add it to the chat buffer
 */
void MP_AddChatMessage (const char *text)
{
	/* allocate memory for new chat message */
	chatMessage_t *chat = (chatMessage_t *) Mem_PoolAlloc(sizeof(*chat), cl_genericPool, 0);

	/* push the new chat message at the beginning of the stack */
	chat->next = mp_chatMessageStack;
	mp_chatMessageStack = chat;
	chat->text = Mem_PoolStrDup(text, cl_genericPool, 0);

	if (!chatBuffer) {
		chatBuffer = (char*)Mem_PoolAlloc(MAX_CHAT_TEXT, cl_genericPool, 0);
		if (!chatBuffer) {
			Com_Printf("Could not allocate chat buffer\n");
			return;
		}
	}

	*chatBuffer = '\0'; /* clear buffer */
	do {
		if (strlen(chatBuffer) + strlen(chat->text) >= MAX_CHAT_TEXT)
			break;
		Q_strcat(chatBuffer, chat->text, MAX_CHAT_TEXT);
		chat = chat->next;
	} while (chat);

	MN_RegisterText(TEXT_CHAT_WINDOW, chatBuffer);
	Cmd_ExecuteString("unhide_chatscreen");
}
