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

#include "EntityCreator.h"

#include "ifilter.h"
#include "selectable.h"
#include "inamespace.h"

#include "scenelib.h"
#include "entitylib.h"
#include "eclasslib.h"
#include "pivot.h"

#include "targetable.h"
#include "uniquenames.h"
#include "NameKeys.h"
#include "stream/stringstream.h"

#include "miscmodel.h"
#include "light.h"
#include "group.h"
#include "eclassmodel.h"
#include "generic.h"
#include "miscparticle.h"
#include "miscsound.h"

inline scene::Node& entity_for_eclass (EntityClass* eclass)
{
	if (eclass->name() == "misc_model") {
		return New_MiscModel(eclass);
	} else if (eclass->name() == "misc_sound") {
		return New_MiscSound(eclass);
	} else if (eclass->name() == "misc_particle") {
		return New_MiscParticle(eclass);
	} else if (eclass->name() == "light") {
		return New_Light(eclass);
	} else if (!eclass->fixedsize) {
		return New_Group(eclass);
	} else if (!eclass->modelpath().empty()) {
		return New_EclassModel(eclass);
	} else {
		return New_GenericEntity(eclass);
	}
}

inline Namespaced* Node_getNamespaced (scene::Node& node)
{
	return dynamic_cast<Namespaced*>(&node);
}

inline scene::Node& node_for_eclass (EntityClass* eclass)
{
	scene::Node& node = entity_for_eclass(eclass);
	Node_getEntity(node)->setKeyValue("classname", eclass->name());

	Namespaced* namespaced = Node_getNamespaced(node);
	if (namespaced != 0) {
		namespaced->setNamespace(GlobalNamespace());
	}

	return node;
}

EntityCreator::KeyValueChangedFunc EntityKeyValues::m_entityKeyValueChanged = 0;
EntityCreator::KeyValueChangedFunc KeyValue::m_entityKeyValueChanged = 0;

class ConnectEntities
{
	public:
		Entity* m_e1;
		Entity* m_e2;
		ConnectEntities (Entity* e1, Entity* e2) :
			m_e1(e1), m_e2(e2)
		{
		}
		void connect (const std::string& name)
		{
			m_e1->setKeyValue("target", name);
			m_e2->setKeyValue("targetname", name);
		}
		typedef MemberCaller1<ConnectEntities, const std::string&, &ConnectEntities::connect> ConnectCaller;
};

inline Entity* ScenePath_getEntity (const scene::Path& path)
{
	Entity* entity = Node_getEntity(path.top());
	if (entity == 0) {
		entity = Node_getEntity(path.parent());
	}
	return entity;
}

class UFOEntityCreator: public EntityCreator
{
	public:
		scene::Node& createEntity (EntityClass* eclass)
		{
			return node_for_eclass(eclass);
		}
		void setKeyValueChangedFunc (KeyValueChangedFunc func)
		{
			EntityKeyValues::setKeyValueChangedFunc(func);
		}
		void connectEntities (const scene::Path& path, const scene::Path& targetPath)
		{
			Entity* e1 = ScenePath_getEntity(path);
			Entity* e2 = ScenePath_getEntity(targetPath);

			if (e1 == 0 || e2 == 0) {
				globalErrorStream() << "entityConnectSelected: both of the selected instances must be an entity\n";
				return;
			}

			if (e1 == e2) {
				globalErrorStream()
						<< "entityConnectSelected: the selected instances must not both be from the same entity\n";
				return;
			}

			UndoableCommand undo("entityConnectSelected");

			ConnectEntities connector(e1, e2);
			std::string value = e2->getKeyValue("targetname");
			if (value.empty()) {
				value = e1->getKeyValue("target");
			}
			if (!value.empty()) {
				connector.connect(value);
			} else {
				std::string type = e2->getKeyValue("classname");
				if (type.empty()) {
					type = "t";
				}
				std::string key = type + "1";
				GlobalNamespace().makeUnique(key, ConnectEntities::ConnectCaller(connector));
			}

			SceneChangeNotify();
		}
};

UFOEntityCreator g_UFOEntityCreator;

EntityCreator& GetEntityCreator ()
{
	return g_UFOEntityCreator;
}

#include "preferencesystem.h"

void P_Entity_Construct ()
{
	Light_Construct();

	RenderablePivot::StaticShader::instance() = GlobalShaderCache().capture("$PIVOT");

	GlobalShaderCache().attachRenderable(StaticRenderableConnectionLines::instance());
}

void P_Entity_Destroy ()
{
	GlobalShaderCache().detachRenderable(StaticRenderableConnectionLines::instance());

	GlobalShaderCache().release("$PIVOT");

	Light_Destroy();
}
