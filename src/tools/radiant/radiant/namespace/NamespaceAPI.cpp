#include "NamespaceAPI.h"

#include "BasicNamespace.h"

#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"

NamespaceAPI::NamespaceAPI (void)
{
	m_namespace = &g_defaultNamespace;
}
Namespace* NamespaceAPI::getTable (void)
{
	return m_namespace;
}

typedef SingletonModule<NamespaceAPI> NamespaceModule;
typedef Static<NamespaceModule> StaticNamespaceModule;
StaticRegisterModule staticRegisterDefaultNamespace (StaticNamespaceModule::instance ());
