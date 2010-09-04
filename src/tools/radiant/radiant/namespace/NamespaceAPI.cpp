#include "NamespaceAPI.h"

#include "BasicNamespace.h"

#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"

class NamespaceAPI
{
		Namespace* m_namespace;
	public:
		typedef Namespace Type;
		STRING_CONSTANT(Name, "*")
;		NamespaceAPI (void)
		{
			m_namespace = &g_defaultNamespace;
		}
		Namespace* getTable (void)
		{
			return m_namespace;
		}
	};

typedef SingletonModule<NamespaceAPI> NamespaceModule;
typedef Static<NamespaceModule> StaticNamespaceModule;
StaticRegisterModule staticRegisterDefaultNamespace(StaticNamespaceModule::instance());
