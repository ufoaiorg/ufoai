/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#include "parse.h"

#include <list>

#include "iradiant.h"
#include "radiant_i18n.h"
#include "ientity.h"
#include "ibrush.h"
#include "ieclass.h"
#include "iscriplib.h"
#include "scenelib.h"
#include "string/string.h"
#include "stringio.h"
#include "eclasslib.h"
#include "gtkutil/ModalProgressDialog.h"
#include "string/string.h"

inline MapImporter* Node_getMapImporter (scene::Node& node)
{
	return dynamic_cast<MapImporter*>(&node);
}

typedef std::list<std::pair<std::string, std::string> > KeyValues;

NodeSmartReference g_nullNode(NewNullNode());

NodeSmartReference Entity_create (EntityCreator& entityTable, EntityClass* entityClass, const KeyValues& keyValues)
{
	scene::Node& entity(entityTable.createEntity(entityClass));
	for (KeyValues::const_iterator i = keyValues.begin(); i != keyValues.end(); ++i) {
		Node_getEntity(entity)->setKeyValue((*i).first, (*i).second);
	}
	return NodeSmartReference(entity);
}

NodeSmartReference Entity_parseTokens (Tokeniser& tokeniser, EntityCreator& entityTable, const PrimitiveParser& parser,
		int index)
{
	NodeSmartReference entity(g_nullNode);
	KeyValues keyValues;
	std::string classname = "";

	int count_primitives = 0;
	while (1) {
		std::string token = tokeniser.getToken();
		if (token.empty()) {
			Tokeniser_unexpectedError(tokeniser, token, "#entity-token");
			return g_nullNode;
		}
		if (token == "}") { // end entity
			if (entity == g_nullNode) {
				// entity does not have brushes
				entity = Entity_create(entityTable, GlobalEntityClassManager().findOrInsert(classname, false),
						keyValues);
			}
			return entity;
		} else if (token == "{") { // begin primitive
			if (entity == g_nullNode) {
				// entity has brushes
				entity
						= Entity_create(entityTable, GlobalEntityClassManager().findOrInsert(classname, true),
								keyValues);
			}

			NodeSmartReference primitive(parser.parsePrimitive(tokeniser));
			if (primitive == g_nullNode || !Node_getMapImporter(primitive)->importTokens(tokeniser)) {
				globalErrorStream() << "brush " << count_primitives << ": parse error\n";
				return g_nullNode;
			}

			scene::Traversable* traversable = Node_getTraversable(entity);
			if (Node_getEntity(entity)->isContainer() && traversable != 0) {
				traversable->insert(primitive);
			} else {
				globalErrorStream() << "entity " << index << ": type " << classname << ": discarding brush "
						<< count_primitives << "\n";
			}
			++count_primitives;
		} else { // epair
			const std::string key = token;
			token = tokeniser.getToken();
			if (token.empty()) {
				Tokeniser_unexpectedError(tokeniser, token, "#epair-value");
				return g_nullNode;
			}
			keyValues.push_back(KeyValues::value_type(key, token));
			if (key == "classname")
				classname = keyValues.back().second;
		}
	}
	// unreachable code
	return g_nullNode;
}

void Map_Read (scene::Node& root, Tokeniser& tokeniser, EntityCreator& entityTable, const PrimitiveParser& parser)
{
	// Create an info display panel to track load progress
	gtkutil::ModalProgressDialog dialog(GlobalRadiant().getMainWindow(), _("Loading map"));

	// Read each entity in the map, until EOF is reached
	for (int entCount = 0; ; entCount++) {
		// Update the dialog text
		dialog.setText("Loading entity " + string::toString(entCount));

		// Check for end of file
		if (tokeniser.getToken().empty())
			break;

		// Create an entity node by parsing from the stream
		NodeSmartReference entity(Entity_parseTokens(tokeniser, entityTable, parser, entCount));

		if (entity == g_nullNode) {
			globalErrorStream() << "entity " << entCount << ": parse error\n";
			return;
		}

		// Insert the new entity into the scene graph
		Node_getTraversable(root)->insert(entity);
	}
}
