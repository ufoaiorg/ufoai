#include "NamespaceModule.h"

#include "Namespace.h"

#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"

NamespaceAPI::NamespaceAPI (void)
{
	_namespace = new Namespace();
}

NamespaceAPI::~NamespaceAPI (void)
{
	delete _namespace;
}

INamespace* NamespaceAPI::getTable (void)
{
	return _namespace;
}

typedef SingletonModule<NamespaceAPI> NamespaceModule;
typedef Static<NamespaceModule> StaticNamespaceModule;
StaticRegisterModule staticRegisterDefaultNamespace (StaticNamespaceModule::instance ());
