/**
 * @file cl_shader.c
 * @brief Parses and loads shaders and materials from files and passes them to the renderer.
 *
 *
 * Reads in a shader program from a file, puts it in a struct and tell the renderer about the location of this struct.
 * This is to allow the adding of shaders without recompiling the binaries.
 * You can add shaders for each texture; new maps can bring new textures.
 * Without a scriptable shader interface we have to rebuild the renderer to get a shader for a specific texture.
 * They are here and not in the renderer because the file parsers are part of the client.
 * @todo Add command to re-initialise shaders to allow interactive debugging.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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


#include "cl_shader.h"
#include "client.h"
#include "../renderer/r_shader.h"

int r_numShaders;
shader_t r_shaders[MAX_SHADERS];

static const value_t shader_values[] = {
	{"filename", V_CLIENT_HUNK_STRING, offsetof(shader_t, filename), 0},
	{"normal", V_CLIENT_HUNK_STRING, offsetof(shader_t, normal), 0},
	{"distort", V_CLIENT_HUNK_STRING, offsetof(shader_t, distort), 0},
	{"frag", V_BOOL, offsetof(shader_t, frag), MEMBER_SIZEOF(shader_t, frag)},
	{"vertex", V_BOOL, offsetof(shader_t, vertex), MEMBER_SIZEOF(shader_t, vertex)},
	{"glsl", V_BOOL, offsetof(shader_t, glsl), MEMBER_SIZEOF(shader_t, glsl)},
	{"glmode", V_BLEND, offsetof(shader_t, glMode), MEMBER_SIZEOF(shader_t, glMode)},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parses all shader script from ufo script files
 * @note Called from CL_ParseClientData
 * @sa CL_ParseClientData
 */
void CL_ParseShaders (const char *name, const char **text)
{
	shader_t *entry;
	const value_t *v;
	const char *errhead = "CL_ParseShaders: unexpected end of file (names ";
	const char *token;
	int i;

	for (i = 0; i < r_numShaders; i++) {
		if (!Q_strcmp(r_shaders[i].name, name)) {
			Com_Printf("CL_ParseShaders: Second shader with same name found (%s) - second ignored\n", name);
			return;
		}
	}

	if (r_numShaders >= MAX_SHADERS) {
		Com_Printf("CL_ParseShaders: shader \"%s\" ignored - too many shaders\n", name);
		return;
	}

	/* get name list body body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("CL_ParseShaders: shader \"%s\" without body ignored\n", name);
		return;
	}

	/* new entry */
	entry = &r_shaders[r_numShaders++];
	memset(entry, 0, sizeof(*entry));

	/* default value */
	entry->glMode = BLEND_FILTER;

	entry->name = Mem_PoolStrDup(name, cl_genericPool, CL_TAG_NONE);
	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* get values */
		for (v = shader_values; v->string; v++)
			if (!Q_strcmp(token, v->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (v->type) {
				case V_NULL:
					break;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)entry + (int)v->ofs), cl_genericPool, CL_TAG_NONE);
					break;
				default:
					Com_ParseValue(entry, token, v->type, v->ofs, v->size);
					break;
				}
				break;
			}

		if (!v->string)
			Com_Printf("CL_ParseShaders: Ignoring unknown param value '%s'\n", token);
	} while (*text);
}
