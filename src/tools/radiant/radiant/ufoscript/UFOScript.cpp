#include "iufoscript.h"
#include "iradiant.h"
#include "ieventmanager.h"
#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"
#include "os/path.h"
#include "terrain/Terrain.h"
#include "mapdef/MapDef.h"
#include "generic/callback.h"

#include <string>
#include <set>

namespace
{
	class UFOScriptCollector
	{
		private:

			std::set<std::string>& _list;

		public:
			typedef const std::string& first_argument_type;

			UFOScriptCollector (std::set<std::string>& list) :
				_list(list)
			{
				// don't parse the menus
				GlobalFileSystem().forEachFile("ufos/", "*", makeCallback1(*this), 0);
				std::size_t size = _list.size();
				globalOutputStream() << "Found " << string::toString(size) << " ufo files\n";
			}

			// Functor operator needed for the forEachFile() call
			void operator() (const std::string& file)
			{
				std::string extension = os::getExtension(file);

				if (extension == "ufo") {
					_list.insert(file);
				}
			}
	};
}

UFOScriptSystem::UFOScriptSystem ()
{
}

void UFOScriptSystem::generateTerrainDefinition ()
{
	scripts::Terrain terrain;
	terrain.generateTerrainDefinitionsForTextures();
}

void UFOScriptSystem::editTerrainDefinition ()
{
	scripts::Terrain terrain;
	terrain.showTerrainDefinitionForTexture();
}

void UFOScriptSystem::editMapDefinition ()
{
	scripts::MapDef mapDef;
	if (mapDef.getMapDefForCurrentMap())
		mapDef.showMapDefinition();
	else
		mapDef.add();
}

const std::string UFOScriptSystem::getUFOScriptDir () const
{
	return "ufos/";
}

const UFOScriptSystem::UFOScriptFiles& UFOScriptSystem::getFiles ()
{
	if (!_init) {
		_init = true;
		UFOScriptCollector collector(_ufoFiles);
	}

	return _ufoFiles;
}


void UFOScriptSystem::init ()
{
	GlobalEventManager().addCommand("EditTerrainDefinition", MemberCaller<UFOScriptSystem, &UFOScriptSystem::editTerrainDefinition> (*this));
	GlobalEventManager().addCommand("EditMapDefinition", MemberCaller<UFOScriptSystem, &UFOScriptSystem::editMapDefinition> (*this));
}

class UFOScriptSystemAPI
{
	private:

		UFOScriptSystem* _ufoScriptSystem;

	public:
		typedef UFOScriptSystem Type;
		STRING_CONSTANT(Name, "*");

		UFOScriptSystemAPI () :
			_ufoScriptSystem(0)
		{
			_ufoScriptSystem = new UFOScriptSystem();
		}
		~UFOScriptSystemAPI ()
		{
			delete _ufoScriptSystem;
		}

		UFOScriptSystem* getTable ()
		{
			return _ufoScriptSystem;
		}
};

typedef SingletonModule<UFOScriptSystemAPI> UFOScriptSystemModule;
typedef Static<UFOScriptSystemModule> StaticUFOScriptSystemModule;
StaticRegisterModule staticRegisterUFOScript(StaticUFOScriptSystemModule::instance());
