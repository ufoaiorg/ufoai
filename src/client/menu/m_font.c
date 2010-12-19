/**
 * @file m_font.c
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

#include "m_main.h"
#include "m_internal.h"
#include "m_font.h"
#include "m_parse.h"

#include "../client.h"
#include "../../shared/parse.h"
#include "../renderer/r_font.h"

#define MAX_FONTS 16
static int numFonts = 0;
static menuFont_t fonts[MAX_FONTS];

static const value_t fontValues[] = {
	{"font", V_TRANSLATION_STRING, offsetof(menuFont_t, path), 0},
	{"size", V_INT, offsetof(menuFont_t, size), MEMBER_SIZEOF(menuFont_t, size)},
	{"style", V_CLIENT_HUNK_STRING, offsetof(menuFont_t, style), 0},

	{NULL, V_NULL, 0, 0},
};

/**
 * @brief Registers a new TTF font
 * @note The TTF font path is translated via gettext to be able to use different
 * fonts for every translation
 * @param[in] font
 */
static void MN_RegisterFont (const menuFont_t *font)
{
	const char *path = _(font->path);

	if (!path)
		Com_Error(ERR_FATAL, "...font without path (font %s)", font->name);

	if (FS_CheckFile("%s", path) == -1)
		Com_Error(ERR_FATAL, "...font file %s does not exist (font %s)", path, font->name);

	R_FontRegister(font->name, font->size, path, font->style);
}

/**
 * @sa CL_ParseClientData
 */
void MN_ParseFont (const char *name, const char **text)
{
	menuFont_t *font;
	const char *errhead = "MN_ParseFont: unexpected end of file (font";
	const char *token;
	const value_t *v = NULL;

	/* search for font with same name */
	if (MN_GetFontByID(name)) {
		Com_Printf("MN_ParseFont: font \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (numFonts >= MAX_FONTS) {
		Com_Printf("MN_ParseFont: Max fonts reached\n");
		return;
	}

	/* initialize the menu */
	font = &fonts[numFonts];
	memset(font, 0, sizeof(*font));

	font->name = Mem_PoolStrDup(name, mn_sysPool, 0);

	Com_DPrintf(DEBUG_CLIENT, "...found font %s (%i)\n", font->name, numFonts);

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseFont: font \"%s\" without body ignored\n", name);
		return;
	}

	numFonts++;

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (v = fontValues; v->string; v++)
			if (!strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (v->type) {
				case V_TRANSLATION_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)font + (int)v->ofs), mn_sysPool, 0);
					break;
				default:
					Com_EParseValue(font, token, v->type, v->ofs, v->size);
					break;
				}
				break;
			}

		if (!v->string)
			Com_Printf("MN_ParseFont: unknown token \"%s\" ignored (font %s)\n", token, name);
	} while (*text);

	MN_RegisterFont(font);
}

/**
 * @brief Return the font for a specific node or default font
 * @param[in] node Context node
 * @return char pointer with font name (default is f_small)
 */
const char *MN_GetFontFromNode (const menuNode_t *const node)
{
	if (node && node->font) {
		return MN_GetReferenceString(node, node->font);
	}
	return "f_small";
}


/**
 * @brief Return the font for a specific id
 */
const menuFont_t *MN_GetFontByID (const char *name)
{
	int i;

	for (i = 0; i < numFonts; i++)
		if (!strcmp(fonts[i].name, name))
			return &fonts[i];

	return NULL;
}

int MN_FontGetHeight (const char *fontID)
{
	font_t *font = R_GetFont(fontID);
	if (font != NULL)
		return font->height;
	return -1;
}

/**
 * @brief after a video restart we have to reinitialize the fonts
 */
void MN_InitFonts (void)
{
	int i;

	Com_Printf("...registering %i fonts\n", numFonts);
	for (i = 0; i < numFonts; i++)
		MN_RegisterFont(&fonts[i]);
}
