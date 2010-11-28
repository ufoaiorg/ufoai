#include "RadiantUndoSystem.h"

class RadiantUndoSystemDependencies: public GlobalRegistryModuleRef, GlobalPreferenceSystemModuleRef
{
};

/* Required code to register the module with the ModuleServer.
 */

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<undo::RadiantUndoSystem, RadiantUndoSystemDependencies> RadiantUndoSystemModule;
typedef Static<RadiantUndoSystemModule> StaticRadiantUndoSystemModule;
StaticRegisterModule staticRegisterRadiantUndoSystem(StaticRadiantUndoSystemModule::instance());
