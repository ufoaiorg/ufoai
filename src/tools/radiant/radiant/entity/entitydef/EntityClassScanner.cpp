#include "EntityClassScanner.h"

#include "ifilesystem.h"
#include "iarchive.h"

#include "string/string.h"
#include "eclasslib.h"
#include "gtkutil/dialog.h"
#include "AutoPtr.h"
#include "os/path.h"

const std::string EntityClassScannerUFO::getFilename () const
{
	return "ufos/entities.ufo";
}

void EntityClassScannerUFO::parseFlags (EntityClass *e, const char **text)
{
	e->flagnames.clear();
	string::splitBy(*text, e->flagnames);
	for (int i = e->flagnames.size(); i < MAX_FLAGS; i++) {
		e->flagnames.push_back("");
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
void EntityClassScannerUFO::parseAttribute (EntityClass *e, const entityKeyDef_t *keydef)
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
	int typeFlag = keydef->flags & ED_KEY_TYPE;

	std::string type = attributeName;

	if (attributeName == "noise")
		type = "sound";
	else if (attributeName == "_color" || attributeName == "color")
		type = "colour";
	else if (attributeName == "target")
		type = "entity";
	else if (typeFlag & ED_TYPE_FLOAT) {
		if (keydef->vLen == 3)
			type = "vector3";
		else if (keydef->vLen == 1)
			type = "float";
	} else if (typeFlag & ED_TYPE_INT) {
		// integer
	} else if (typeFlag == 0 || (typeFlag & ED_TYPE_STRING)) {
		// string
	}

	EntityClassAttribute attribute = EntityClassAttribute(type, attributeName, mandatory, value, desc);
	EntityClass_insertAttribute(*e, attributeName, attribute);
}

/**
 * Creates a new entityclass for given parsed definition
 * @param entityDef parsed definition information to use
 * @return a new entity class or 0 if something was wrong with definition
 */
EntityClass *EntityClassScannerUFO::initFromDefinition (const entityDef_t *definition)
{
	g_debug("Creating entity class for entity definition '%s'\n", definition->classname);

	EntityClass* e = Eclass_Alloc();
	e->free = &Eclass_Free;
	e->m_name = definition->classname;

	for (int idx = 0; idx < definition->numKeyDefs; idx++) {
		const entityKeyDef_t *keydef = &definition->keyDefs[idx];
		const std::string keyName = keydef->name;

		if (keyName == "color") {
			//not using _color as this is a valid attribute flag
			// grab the color, reformat as texture name
			const int r = sscanf(keydef->desc, "%f %f %f", &e->color[0], &e->color[1], &e->color[2]);
			if (r != 3) {
				g_message("Invalid color token given\n");
				return 0;
			}
		} else if (keyName == "size") {
			e->fixedsize = true;
			const int r = sscanf(keydef->desc, "%f %f %f %f %f %f", &e->mins[0], &e->mins[1], &e->mins[2], &e->maxs[0],
					&e->maxs[1], &e->maxs[2]);
			if (r != 6) {
				g_message("Invalid size token given\n");
				return 0;
			}
		} else if (keyName == "description") {
			e->m_comments = keydef->desc;
		} else if (keyName == "spawnflags") {
			if (keydef->flags & ED_ABSTRACT) {
				/* there are two keydefs, abstract holds the valid levelflags, the other one default value and type */
				const char *flags = keydef->desc;
				parseFlags(e, &flags);
			} else {
				parseAttribute(e, keydef);
			}
		} else if (keyName == "classname") {
			/* ignore, read from head */
			continue;
		} else if (keyName == "model") {
			/** @todo what does that modelpath stuff do? it does not read anything from keydef */
			e->m_modelpath = os::standardPath(e->m_modelpath);
			const bool mandatory = (keydef->flags & ED_MANDATORY);
			EntityClass_insertAttribute(*e, "model", EntityClassAttribute("model", "model", mandatory));
		} else {
			/* all other keys are valid attribute keys */
			parseAttribute(e, keydef);
		}
	}

#if 0
	/**
	 * @todo direction and angle are 2 types used for different display
	 * (see entityinspector DirectionAttribute and AngleAttribute)
	 * the problem is that different entities have "angle" property, but different defines what values are valid
	 * which is actually not reflected by this code. Perhaps we should introduce different types for these representations.
	 */
	EntityClassAttribute *angle = e->getAttribute("angle");
	if (angle) {
		if (e->fixedsize) {
			angle->name = _("Yaw Angle");
		} else {
			angle->name = _("Direction");
		}
	}
#endif
	eclass_capture_state(e);

	return e;
}

void EntityClassScannerUFO::scanFile (EntityClassCollector& collector)
{
	const std::string& filename = getFilename();

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
		ED_Free();
		return;
	}
	for (int i = 0; i < numEntityDefs; i++) {
		EntityClass *e = initFromDefinition(&entityDefs[i]);
		if (e)
			collector.insert(e);
	}
	free(entities);
	ED_Free();
}
