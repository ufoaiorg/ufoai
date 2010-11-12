#include "XMLRegistry.h"

/* XMLRegistry dependencies class.
 */

class XMLRegistryDependencies
{
};

/* Required code to register the module with the ModuleServer.
 */

#include "modulesystem/singletonmodule.h"

typedef SingletonModule<XMLRegistry, XMLRegistryDependencies> XMLRegistryModule;

typedef Static<XMLRegistryModule> StaticXMLRegistrySystemModule;
StaticRegisterModule staticRegisterXMLRegistrySystem(StaticXMLRegistrySystemModule::instance());
