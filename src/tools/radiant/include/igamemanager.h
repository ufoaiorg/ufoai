#ifndef INCLUDED_IGAMEMANAGER_H
#define INCLUDED_IGAMEMANAGER_H

#include "generic/constant.h"
#include "moduleobserver.h"

/**
 * The "global" interface of DarkRadiant's camera module.
 */
class IGameManager
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "gamemanager");

		virtual ~IGameManager ()
		{
		}

		virtual const std::string& getKeyValue(const std::string& key) const = 0;

		virtual const std::string& getEnginePath() const = 0;

		virtual void init() = 0;
		virtual void destroy() = 0;
};

// Module definitions

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<IGameManager> GlobalGameManagerModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<IGameManager> GlobalGameManagerModuleRef;

// This is the accessor for the registry
inline IGameManager& GlobalGameManager ()
{
	return GlobalGameManagerModule::getTable();
}

#endif
