#include "Pathfinding.h"

/* Required code to register the module with the ModuleServer.
 */
#include "modulesystem/singletonmodule.h"

/* UMP dependencies class.
 */
class PathfindingSystemDependencies
{
};

typedef SingletonModule<routing::Pathfinding, PathfindingSystemDependencies> PathfindingModule;

typedef Static<PathfindingModule> StaticPathfindingSystemModule;
StaticRegisterModule staticRegisterPathfindingSystem (StaticPathfindingSystemModule::instance ());
