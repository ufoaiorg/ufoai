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
#include "gtkutil/dialog.h"
#include "modulesystem/moduleregistry.h"
#include "os/path.h"
#include "AutoPtr.h"
#include "ifilesystem.h"
#include "archivelib.h"
#include "script/scripttokeniser.h"
#include "../../../shared/parse.h"
#include "../../../shared/entitiesdef.h"

#include "string/string.h"
#include <stdlib.h>
#include "eclasslib.h"

static const std::string EClass_GetFilename (void)
{
	return "ufos/entities.ufo";
}

static void Eclass_ParseFlags (EntityClass *e, const char **text)
{
	for (int i = 0; i < MAX_FLAGS; i++) {
		const char *token = Com_Parse(text);
		if (!*token)
			break;
		strcpy(e->flagnames[i], token);
	}
}

/**
 * Add a new EntityClassAttribute to entity class based on given definition
 * @param e entity class to add attribute to
 * @param keydef definition parsed from entities.ufo with information about type,
 * default values and mandatory state.
 * @note opposed to old parsing code this adds attributes for all definitions,
 * not only for the ones that get special treatment in gui.
 */
static void Eclass_ParseAttribute (EntityClass *e, entityKeyDef_t *keydef)
{
	const bool mandatory = (keydef->flags & ED_MANDATORY);
	/* we use attribute key as its type, only for some types this is really used */
	std::string attributeName = keydef->name;
	std::string value = "";
	std::string desc = "";
	if (keydef->defaultVal)
		value = keydef->defaultVal;
	if (keydef->desc)
		desc = keydef->desc;
	EntityClassAttribute attribute = EntityClassAttribute(attributeName, _(attributeName.c_str()), mandatory, value, desc);
	EntityClass_insertAttribute(*e, attributeName, attribute);
}

/**
 * Creates a new entityclass for given parsed definition
 * @param entityDef parsed definition information to use
 * @return a new entity class or 0 if something was wrong with definition
 */
static EntityClass *Eclass_InitFromDefinition (entityDef_t *definition)
{
	g_debug("Creating entity class for entity definition '%s'\n", definition->classname);

	EntityClass* e = Eclass_Alloc();
	e->free = &Eclass_Free;
	e->m_name = definition->classname;

	for (int idx = 0; idx < definition->numKeyDefs; idx++) {
		entityKeyDef_t keydef = definition->keyDefs[idx];
		const char *keyName = keydef.name;

		if (!strcmp(keyName, "color")) {
			//not using _color as this is a valid attribute flag
			// grab the color, reformat as texture name
			const int r = sscanf(keydef.desc, "%f %f %f", &e->color[0], &e->color[1], &e->color[2]);
			if (r != 3) {
				g_message("Invalid color token given\n");
				return 0;
			}
		} else if (!strcmp(keyName, "size")) {
			e->fixedsize = true;
			const int r = sscanf(keydef.desc, "%f %f %f %f %f %f", &e->mins[0], &e->mins[1], &e->mins[2], &e->maxs[0],
					&e->maxs[1], &e->maxs[2]);
			if (r != 6) {
				g_message("Invalid size token given\n");
				return 0;
			}
		} else if (!strcmp(keyName, "description")) {
			e->m_comments = keydef.desc;
		} else if (!strcmp(keyName, "spawnflags")) {
			if (keydef.flags & ED_ABSTRACT) {
				/* there are two keydefs, abstract holds the valid levelflags, the other one default value and type */
				const char *flags = keydef.desc;
				Eclass_ParseFlags(e, &flags);
			} else {
				Eclass_ParseAttribute(e, &keydef);
			}
		} else if (!strcmp(keyName, "classname")) {
			/* ignore, read from head */
			continue;
		} else if (!strcmp(keyName, "model")) {
			/** @todo what does that modelpath stuff do? it does not read anything from keydef */
			e->m_modelpath = os::standardPath(e->m_modelpath);
			const bool mandatory = (keydef.flags & ED_MANDATORY);
			EntityClass_insertAttribute(*e, "model", EntityClassAttribute("model", "Model", mandatory));
		} else {
			/* all other keys are valid attribute keys */
			Eclass_ParseAttribute(e, &keydef);
		}
	}

	/**
	 * @todo direction and angle are 2 types used for different display
	 * (see entityinspector DirectionAttribute and AngleAttribute)
	 * the problem is that different entities have "angle" property, but different defines what values are valid
	 * which is actually not reflected by this code. Perhaps we should introduce different types for these representations.
	 */
	EntityClassAttribute *angle = e->getAttribute("angle");
	if (angle) {
		if (e->fixedsize) {
			angle->m_name = _("Yaw Angle");
		} else {
			angle->m_name = _("Direction");
		}
	}
	eclass_capture_state(e);

	return e;

}

static void Eclass_ScanFile (EntityClassCollector& collector, const std::string& filename)
{
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(filename));
	if (!file) {
		std::string buffer = "Could not load " + filename;
		gtkutil::errorDialog(buffer);
		return;
	}

	const std::size_t size = file->size();
	char *entities = (char *) malloc(size + 1);
	TextInputStream &stream = file->getInputStream();
	const std::size_t realsize = stream.read(entities, size);
	entities[realsize] = '\0';
	if (ED_Parse(entities) == ED_ERROR) {
		std::string buffer = "Parsing of entities definition file failed, returned error was " + std::string(
				ED_GetLastError());
		gtkutil::errorDialog(buffer);
		free(entities);
		return;
	}
	for (int i = 0; i < numEntityDefs; i++) {
		EntityClass *e = Eclass_InitFromDefinition(&entityDefs[i]);
		if (e)
			collector.insert(e);
	}
	free(entities);
}

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
StaticRegisterModule staticRegisterEclassDef(StaticEclassDefModule::instance());
