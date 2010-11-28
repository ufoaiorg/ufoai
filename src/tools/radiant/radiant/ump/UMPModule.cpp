#include "UMPSystem.h"

#include "ifilesystem.h"

/* Required code to register the module with the ModuleServer.
 */
#include "modulesystem/singletonmodule.h"

/* UMP dependencies class.
 */
class UMPSystemDependencies: public GlobalFileSystemModuleRef
{
};

typedef SingletonModule<UMPSystem, UMPSystemDependencies> UMPModule;

typedef Static<UMPModule> StaticUMPSystemModule;
StaticRegisterModule staticRegisterUMPSystem(StaticUMPSystemModule::instance());
