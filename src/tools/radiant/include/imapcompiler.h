#ifndef IMAPCOMPILER_H_
#define IMAPCOMPILER_H_

#include "generic/constant.h"
#include <string>

class ICompilerObserver
{
	public:
		virtual ~ICompilerObserver ()
		{
		}

		// notified on each new line of the output
		virtual void notify (const std::string &message) = 0;
};

/**
 * Abstract base class for the map error check module
 */
class IMapCompiler
{

	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "mapcompiler");

		virtual ~IMapCompiler ()
		{
		}

		virtual void performCheck (const std::string& mapName, ICompilerObserver& observer) = 0;

		virtual void fixErrors (const std::string& mapName, ICompilerObserver& observer) = 0;

		virtual void compileMap (const std::string& mapName, ICompilerObserver& observer) = 0;

		virtual void generateMaterial (const std::string& mapName, ICompilerObserver& observer) = 0;
};

// Module definitions

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<IMapCompiler> GlobalMapCompilerModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<IMapCompiler> GlobalMapCompilerModuleRef;

// This is the accessor for the overlay module
inline IMapCompiler& GlobalMapCompiler ()
{
	return GlobalMapCompilerModule::getTable();
}

#endif /*IMAPCOMPILER_H_*/
