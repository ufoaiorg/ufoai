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

#include "plugin.h"

#include "iscriplib.h"
#include "ibrush.h"
#include "ifiletypes.h"
#include "ieclass.h"
#include "qerplugin.h"

#include "scenelib.h"
#include "string/string.h"
#include "stringio.h"
#include "generic/constant.h"

#include "modulesystem/singletonmodule.h"

#include "parse.h"
#include "write.h"

class MapDependencies :
			public GlobalRadiantModuleRef,
			public GlobalBrushModuleRef,
			public GlobalFiletypesModuleRef,
			public GlobalScripLibModuleRef,
			public GlobalEntityClassManagerModuleRef,
			public GlobalSceneGraphModuleRef {
public:
	MapDependencies() :
			GlobalBrushModuleRef("ufo"),
			GlobalEntityClassManagerModuleRef("ufo") {
	}
};

class MapUFOAPI : public TypeSystemRef, public MapFormat, public PrimitiveParser {
public:
	typedef MapFormat Type;
	STRING_CONSTANT(Name, "mapufo");

	MapUFOAPI() {
		GlobalFiletypesModule::getTable().addType(Type::Name(), Name(), filetype_t("ufo maps", "*.map"));
		GlobalFiletypesModule::getTable().addType(Type::Name(), Name(), filetype_t("ufo region", "*.reg"));
	}
	MapFormat* getTable() {
		return this;
	}
	scene::Node& parsePrimitive(Tokeniser& tokeniser) const {
		const char* primitive = tokeniser.getToken();
		if (primitive != 0) {
			if (string_equal(primitive, "(")) {
				tokeniser.ungetToken(); // (
				return GlobalBrushModule::getTable().createBrush();
			}
		}

		Tokeniser_unexpectedError(tokeniser, primitive, "#ufo-primitive");
		return g_nullNode;
	}
	void readGraph(scene::Node& root, TextInputStream& inputStream, EntityCreator& entityTable) const {
		Tokeniser& tokeniser = GlobalScripLibModule::getTable().m_pfnNewSimpleTokeniser(inputStream);
		Map_Read(root, tokeniser, entityTable, *this);
		delete &tokeniser;
	}
	void writeGraph(scene::Node& root, GraphTraversalFunc traverse, TextOutputStream& outputStream) const {
		TokenWriter& writer = GlobalScripLibModule::getTable().m_pfnNewSimpleTokenWriter(outputStream);
		Map_Write(root, traverse, writer);
		writer.release();
	}
};

typedef SingletonModule<MapUFOAPI, MapDependencies> MapUFOModule;

MapUFOModule g_MapUFOModule;



#define PARSE_ERROR "error parsing VMF"

inline void parseToken(Tokeniser& tokeniser, const char* token) {
	ASSERT_MESSAGE(Tokeniser_parseToken(tokeniser, token), "error parsing vmf: token not found: " << makeQuoted(token));
}

#include "generic/arrayrange.h"

class VMFBlock;
typedef ArrayConstRange<VMFBlock> VMFBlockArrayRange;


class VMFBlock {
public:
	const char* m_name;
	VMFBlockArrayRange m_children;
	typedef const VMFBlock Value;

	VMFBlock(const char* name, VMFBlockArrayRange children = VMFBlockArrayRange(0, 0)) : m_name(name), m_children(children) {
	}
	const char* name() const {
		return m_name;
	}
	typedef Value* const_iterator;
	const_iterator begin() const {
		return m_children.first;
	}
	const_iterator end() const {
		return m_children.last;
	}
};

const VMFBlock c_vmfNormals("normals");
const VMFBlock c_vmfDistances("distances");
const VMFBlock c_vmfOffsets("offsets");
const VMFBlock c_vmfOffsetNormals("offset_normals");
const VMFBlock c_vmfAlphas("alphas");
const VMFBlock c_vmfTriangleTags("triangle_tags");
const VMFBlock c_vmfAllowedVerts("allowed_verts");
const VMFBlock c_vmfDispInfoChildren[] = { c_vmfNormals, c_vmfDistances, c_vmfOffsets, c_vmfOffsetNormals, c_vmfAlphas, c_vmfTriangleTags, c_vmfAllowedVerts };
const VMFBlock c_vmfDispInfo("dispinfo", ARRAY_RANGE(c_vmfDispInfoChildren));
const VMFBlock c_vmfSideChildren[] = { c_vmfDispInfo };
const VMFBlock c_vmfSide("side", ARRAY_RANGE(c_vmfSideChildren));
const VMFBlock c_vmfEditor("editor");
const VMFBlock c_vmfVersionInfo("versioninfo");
const VMFBlock c_vmfViewSettings("viewsettings");
const VMFBlock c_vmfCordon("cordon");
const VMFBlock c_vmfGroupChildren[] = { c_vmfEditor };
const VMFBlock c_vmfGroup("group", ARRAY_RANGE(c_vmfGroupChildren));
const VMFBlock c_vmfCamera("camera");
const VMFBlock c_vmfCamerasChildren[] = { c_vmfCamera };
const VMFBlock c_vmfCameras("cameras", ARRAY_RANGE(c_vmfCamerasChildren));
VMFBlock c_vmfVisGroup("visgroup");
VMFBlock c_vmfVisGroups("visgroups", VMFBlockArrayRange(&c_vmfVisGroup, &c_vmfVisGroup + 1));
const VMFBlock c_vmfSolidChildren[] = { c_vmfSide, c_vmfEditor };
const VMFBlock c_vmfSolid("solid", ARRAY_RANGE(c_vmfSolidChildren));
const VMFBlock c_vmfConnections("connections");
const VMFBlock c_vmfEntityChildren[] = { c_vmfEditor, c_vmfSolid, c_vmfGroup, c_vmfConnections };
const VMFBlock c_vmfEntity("entity", ARRAY_RANGE(c_vmfEntityChildren));
const VMFBlock c_vmfWorldChildren[] = { c_vmfEditor, c_vmfSolid, c_vmfGroup };
const VMFBlock c_vmfWorld("world", ARRAY_RANGE(c_vmfWorldChildren));
const VMFBlock c_vmfRootChildren[] = { c_vmfVersionInfo, c_vmfViewSettings, c_vmfVisGroups, c_vmfWorld, c_vmfEntity, c_vmfCameras, c_vmfCordon };
const VMFBlock c_vmfRoot("", ARRAY_RANGE(c_vmfRootChildren));

class VMFInit {
public:
	VMFInit() {
		c_vmfVisGroup.m_children = VMFBlockArrayRange(&c_vmfVisGroup, &c_vmfVisGroup + 1);
	}
};

VMFInit g_VMFInit;

int g_vmf_entities;
int g_vmf_brushes;

inline VMFBlock::const_iterator VMFBlock_find(const VMFBlock& block, const char* name) {
	for (VMFBlock::const_iterator i = block.begin(); i != block.end(); ++i) {
		if (string_equal(name, (*i).name())) {
			return i;
		}
	}
	return block.end();
}

void VMF_parseBlock(Tokeniser& tokeniser, const VMFBlock& block) {
	for (;;) {
		const char* key = tokeniser.getToken();
		if (key == 0 || string_equal(key, "}")) {
			tokeniser.ungetToken();
			break;
		}
		CopiedString tmp(key);
		tokeniser.nextLine();
		const char* value = tokeniser.getToken();
		tokeniser.nextLine();
		if (string_equal(value, "{")) {
			VMFBlock::const_iterator i = VMFBlock_find(block, tmp.c_str());
			ASSERT_MESSAGE(i != block.end(), "error parsing vmf block " << makeQuoted(block.name()) << ": unknown block: " << makeQuoted(tmp.c_str()));
			if (string_equal(tmp.c_str(), "solid")) {
				++g_vmf_brushes;
			} else if (string_equal(tmp.c_str(), "entity") || string_equal(tmp.c_str(), "world")) {
				++g_vmf_entities;
			}
			VMF_parseBlock(tokeniser, *i);
			parseToken(tokeniser, "}");
			tokeniser.nextLine();
		} else {
			// was a pair
		}
	}
}

void VMF_Read(scene::Node& root, Tokeniser& tokeniser, EntityCreator& entityTable) {
	g_vmf_entities = g_vmf_brushes = 0;
	VMF_parseBlock(tokeniser, c_vmfRoot);
	globalOutputStream() << g_vmf_entities << " entities\n";
	globalOutputStream() << g_vmf_brushes << " brushes\n";
}

class MapVMFAPI : public TypeSystemRef, public MapFormat {
public:
	typedef MapFormat Type;
	STRING_CONSTANT(Name, "mapvmf");

	MapVMFAPI() {
		GlobalFiletypesModule::getTable().addType(Type::Name(), Name(), filetype_t("vmf maps", "*.vmf"));
		GlobalFiletypesModule::getTable().addType(Type::Name(), Name(), filetype_t("vmf region", "*.reg"));
	}
	MapFormat* getTable() {
		return this;
	}

	void readGraph(scene::Node& root, TextInputStream& inputStream, EntityCreator& entityTable) const {
		Tokeniser& tokeniser = GlobalScripLibModule::getTable().m_pfnNewSimpleTokeniser(inputStream);
		VMF_Read(root, tokeniser, entityTable);
		delete &tokeniser;
	}
	void writeGraph(scene::Node& root, GraphTraversalFunc traverse, TextOutputStream& outputStream) const {
	}
};

typedef SingletonModule<MapVMFAPI, MapDependencies> MapVMFModule;

MapVMFModule g_MapVMFModule;



extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules(ModuleServer& server) {
	initialiseModule(server);

	g_MapUFOModule.selfRegister();
	g_MapVMFModule.selfRegister();
}
