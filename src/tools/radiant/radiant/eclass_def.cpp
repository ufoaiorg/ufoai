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

#include "radiant_i18n.h"
#include "iscriplib.h"
#include "ifilesystem.h"
#include "iarchive.h"

#include "eclasslib.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"
#include "gtkutil/messagebox.h"
#include "modulesystem/moduleregistry.h"
#include "os/path.h"
#include "os/file.h"
#include "../../../shared/parse.h"

static const char* EClass_GetFilename ()
{
	return "ufos/entities.ufo";
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
		STRING_CONSTANT(Name, "def");

		EclassDefAPI ()
		{
			m_eclassdef.scanFile = &Eclass_ScanFile;
			m_eclassdef.getFilename = &EClass_GetFilename;
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
#include "eclasslib.h"

static void Eclass_ParseFlags (EntityClass *e, const char **text)
{
	for (int i = 0; i < MAX_FLAGS; i++) {
		const char *token = COM_Parse(text);
		if (!*token)
			break;
		strcpy(e->flagnames[i], token);
	}
}

static void Eclass_ParseAttribute (EntityClass *e, const char **text, bool mandatory)
{
	const char *token;
	do {
		token = COM_Parse(text);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!strcmp(token, "model")) {
			EntityClass_insertAttribute(*e, "model", EntityClassAttribute("model", "Model", mandatory));
		} else if (!strcmp(token, "particle")) {
			EntityClass_insertAttribute(*e, "particle", EntityClassAttribute("particle", "Particle", mandatory));
		} else if (!strcmp(token, "noise")) {
			EntityClass_insertAttribute(*e, "noise", EntityClassAttribute("noise", "Sound", mandatory));
		} else if (!strcmp(token, "spawnflags")) {
			gchar *flags;
			token = COM_Parse(text);
			if (!*text)
				return;
			flags = g_strdup(token);
			Eclass_ParseFlags(e, (const char **)&flags);
			g_free(flags);
		}
	} while (*token != '}');
}

static void Eclass_ParseDefault (EntityClass *e, const char **text)
{
	const char *token;
	do {
		token = COM_Parse(text);
		if (!*text)
			break;
		if (*token == '}')
			break;
	} while (*token != '}');
}

static EntityClass *Eclass_InitFromText (const char **text)
{
	EntityClass* e = Eclass_Alloc();
	const char *token;
	e->free = &Eclass_Free;

	// grab the name
	e->m_name = COM_Parse(text);
	if (!*text)
		return 0;
	token = COM_Parse(text);
	if (*token != '{') {
		g_message("Entity '%s' without body ignored\n", e->m_name.c_str());
		return 0;
	}

	g_debug("Parsing entity '%s'\n", e->m_name.c_str());

	do {
		/* get the name type */
		token = COM_Parse(text);
		if (!*text)
			return 0;
		if (*token == '}')
			break;
		if (!strcmp(token, "mandatory")) {
			Eclass_ParseAttribute(e, text, true);
		} else if (!strcmp(token, "optional")) {
			Eclass_ParseAttribute(e, text, false);
		} else if (!strcmp(token, "default")) {
			Eclass_ParseDefault(e, text);
		} else {
			if (!strcmp(token, "color") || !strcmp(token, "_color")) {
				token = COM_Parse(text);
				if (!*text)
					return 0;
				// grab the color, reformat as texture name
				const int r = sscanf(token, "%f %f %f", &e->color[0], &e->color[1], &e->color[2]);
				if (r != 3) {
					g_message("Invalid color token given\n");
					return e;
				}
			} else if (!strcmp(token, "size")) {
				token = COM_Parse(text);
				if (!*text)
					return 0;
				e->fixedsize = true;
				const int r = sscanf(token, "%f %f %f %f %f %f", &e->mins[0], &e->mins[1], &e->mins[2], &e->maxs[0],
						&e->maxs[1], &e->maxs[2]);
				if (r != 6) {
					g_message("Invalid size token given\n");
					return 0;
				}
			} else if (!strcmp(token, "description")) {
				token = COM_Parse(text);
				if (!*text)
					return 0;
				e->m_comments = token;
			} else if (!strcmp(token, "model")) {
				token = COM_Parse(text);
				if (!*text)
					return 0;
				StringOutputStream buffer(string_length(token));
				buffer << PathCleaned(e->m_modelpath.c_str());
				e->m_modelpath = buffer.c_str();
			} else {
				g_message("Invalid token '%s'\n", token);
			}
		}
	} while (*text);

	eclass_capture_state(e);

	if (!e->fixedsize) {
		EntityClass_insertAttribute(*e, "angle", EntityClassAttribute("direction", "Direction", "0"));
	} else {
		EntityClass_insertAttribute(*e, "angle", EntityClassAttribute("angle", "Yaw Angle", "0"));
	}

	g_debug("...added\n");
	return e;
}

static void Eclass_ScanFile (EntityClassCollector& collector, const char *filename)
{
	EntityClass *e;

	TextFileInputStream file(filename);
	if (file.failed()) {
		StringOutputStream buffer;
		buffer << "Could not load " << filename;
		gtk_MessageBox(0, buffer.c_str(), _("Radiant - Error"), eMB_OK, eMB_ICONERROR);
		return;
	}
	g_message("ScanFile: '%s'\n", filename);

	const std::size_t size = file_size(filename);
	char *entities = (char *)malloc(size);
	file.read(entities, size);
	const char **text = (const char **)&entities;

	do {
		const char *token;
		do {
			token = COM_Parse(text);
			if (!*token)
				return;
		} while (strcmp(token, "entity"));

		e = Eclass_InitFromText(text);
		if (e)
			collector.insert(e);
	} while (*entities);
}
