#include "GameManager.h"

#include "ifiletypes.h"
#include "iregistry.h"

/* GameManager dependencies class.
 */

class GameManagerDependencies: public GlobalFiletypesModuleRef, public GlobalRegistryModuleRef
{
};

/* Required code to register the module with the ModuleServer.
 */

#include "modulesystem/singletonmodule.h"

typedef SingletonModule<ui::GameManager, GameManagerDependencies> GameManagerModule;

typedef Static<GameManagerModule> StaticGameManagerSystemModule;
StaticRegisterModule staticRegisterGameManagerSystem(StaticGameManagerSystemModule::instance());
