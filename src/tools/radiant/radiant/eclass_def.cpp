/**
 * @file eclass_def.cpp
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "iscriplib.h"
#include "ifilesystem.h"
#include "iarchive.h"

#include "eclasslib.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"
#include "modulesystem/moduleregistry.h"
#include "os/path.h"

#define EXTENSION "def"

static const char* EClass_GetExtension ()
{
	return EXTENSION;
}
static void Eclass_ScanFile (EntityClassCollector& collector, const char *filename);

#include "modulesystem/singletonmodule.h"

class EntityClassDefDependencies: public GlobalShaderCacheModuleRef, public GlobalScripLibModuleRef
{
};

class EclassDefAPI
{
		EntityClassScanner m_eclassdef;
	public:
		typedef EntityClassScanner Type;
		STRING_CONSTANT(Name, EXTENSION)
		;

		EclassDefAPI ()
		{
			m_eclassdef.scanFile = &Eclass_ScanFile;
			m_eclassdef.getExtension = &EClass_GetExtension;
		}
		EntityClassScanner* getTable ()
		{
			return &m_eclassdef;
		}
};

typedef SingletonModule<EclassDefAPI, EntityClassDefDependencies> EclassDefModule;
typedef Static<EclassDefModule> StaticEclassDefModule;
StaticRegisterModule staticRegisterEclassDef (StaticEclassDefModule::instance ());

#include "string/string.h"

#include <stdlib.h>

static char com_token[1024];
static bool com_eof;
static const char *debugname;

/**
 * @brief Parse a token out of a string
 */
static const char *COM_Parse (const char *data)
{
	int c;
	int len = 0;

	com_token[0] = 0;

	if (!data)
		return 0;

	// skip whitespace
	skipwhite: while ((c = *data) <= ' ') {
		if (c == 0) {
			com_eof = true;
			return 0; // end of file;
		}
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"') {
		data++;
		do {
			c = *data++;
			if (c == '\"') {
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		} while (1);
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':') {
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do {
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}

static void setSpecialLoad (EntityClass *e, const char* pWhat, CopiedString& p)
{
	const char *pText;
	const char *where = strstr(e->comments(), pWhat);
	if (!where)
		return;

	pText = where + strlen(pWhat);
	if (*pText == '\"')
		pText++;

	where = strchr(pText, '\"');
	if (where) {
		p = StringRange(pText, where);
	} else {
		p = pText;
	}
}

#include "eclasslib.h"

/*

 the classname, color triple, and bounding box are parsed out of comments
 A ? size means take the exact brush size.

 / *QUAKED <classname> (0 0 0) ?
 / *QUAKED <classname> (0 0 0) (-8 -8 -8) (8 8 8)

 Flag names can follow the size description:

 / *QUAKED func_door (0 .5 .8) ? START_OPEN STONE_SOUND DOOR_DONT_LINK GOLD_KEY SILVER_KEY

 */

static EntityClass *Eclass_InitFromText (const char *text)
{
	EntityClass* e = Eclass_Alloc();
	e->free = &Eclass_Free;

	// grab the name
	text = COM_Parse(text);
	e->m_name = com_token;
	debugname = e->name();

	{
		// grab the color, reformat as texture name
		const int r = sscanf(text, " (%f %f %f)", &e->color[0], &e->color[1], &e->color[2]);
		if (r != 3)
			return e;
		eclass_capture_state(e);
	}

	while (*text != ')') {
		if (!*text) {
			return 0;
		}
		text++;
	}
	text++;

	// get the size
	text = COM_Parse(text);
	if (com_token[0] == '(') { // parse the size as two vectors
		e->fixedsize = true;
		const int r = sscanf(text, "%f %f %f) (%f %f %f)", &e->mins[0], &e->mins[1], &e->mins[2], &e->maxs[0],
				&e->maxs[1], &e->maxs[2]);
		if (r != 6) {
			return 0;
		}

		for (int i = 0; i < 2; i++) {
			while (*text != ')') {
				if (!*text) {
					return 0;
				}
				text++;
			}
			text++;
		}
	}

	char parms[256];
	// get the flags that are shown in the entity inspector
	{
		// copy to the first /n
		char* p = parms;
		while (*text && *text != '\n')
			*p++ = *text++;
		*p = 0;
		text++;
	}

	{
		// any remaining words are parm flags
		const char* p = parms;
		for (std::size_t i = 0; i < MAX_FLAGS; i++) {
			p = COM_Parse(p);
			if (!p)
				break;
			strcpy(e->flagnames[i], com_token);
		}
	}

	e->m_comments = text;

	setSpecialLoad(e, "model=", e->m_modelpath);
	StringOutputStream buffer(string_length(e->m_modelpath.c_str()));
	buffer << PathCleaned(e->m_modelpath.c_str());
	e->m_modelpath = buffer.c_str();

	if (!e->fixedsize) {
		EntityClass_insertAttribute(*e, "angle", EntityClassAttribute("direction", "Direction", "0"));
	} else {
		EntityClass_insertAttribute(*e, "angle", EntityClassAttribute("angle", "Yaw Angle", "0"));
	}
	EntityClass_insertAttribute(*e, "model", EntityClassAttribute("model", "Model"));
	EntityClass_insertAttribute(*e, "particle", EntityClassAttribute("particle", "Particle"));
	EntityClass_insertAttribute(*e, "noise", EntityClassAttribute("sound", "Sound"));

	return e;
}

static void Eclass_ScanFile (EntityClassCollector& collector, const char *filename)
{
	EntityClass *e;

	TextFileInputStream inputFile(filename);
	if (inputFile.failed()) {
		g_warning("ScanFile: '%s' not found\n", filename);
		return;
	}
	g_message("ScanFile: '%s'\n", filename);

	enum EParserState
	{
		eParseDefault, eParseSolidus, eParseComment, eParseQuakeED, eParseEntityClass, eParseEntityClassEnd,
	} state = eParseDefault;
	const char* quakeEd = "QUAKED";
	const char* p = 0;
	StringBuffer buffer;
	SingleCharacterInputStream<TextFileInputStream> bufferedInput(inputFile);
	for (;;) {
		char c;
		if (!bufferedInput.readChar(c)) {
			break;
		}

		switch (state) {
		case eParseDefault:
			if (c == '/') {
				state = eParseSolidus;
			}
			break;
		case eParseSolidus:
			if (c == '/') {
				state = eParseComment;
			} else if (c == '*') {
				p = quakeEd;
				state = eParseQuakeED;
			}
			break;
		case eParseComment:
			if (c == '\n') {
				state = eParseDefault;
			}
			break;
		case eParseQuakeED:
			if (c == *p) {
				if (*(++p) == '\0') {
					state = eParseEntityClass;
				}
			} else {
				state = eParseDefault;
			}
			break;
		case eParseEntityClass:
			if (c == '*') {
				state = eParseEntityClassEnd;
			} else {
				buffer.push_back(c);
			}
			break;
		case eParseEntityClassEnd:
			if (c == '/') {
				e = Eclass_InitFromText(buffer.c_str());
				state = eParseDefault;
				if (e)
					collector.insert(e);
				else
					g_warning("Error parsing: '%s' in '%s'\n", debugname, filename);

				buffer.clear();
				state = eParseDefault;
			} else {
				buffer.push_back('*');
				buffer.push_back(c);
				state = eParseEntityClass;
			}
			break;
		}
	}
}
