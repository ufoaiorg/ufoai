#include "iradiant.h"
#include "ifilesystem.h"

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"
#include "modulesystem/modulesmap.h"

#include "generic/constant.h"

#include "EntityClassManager.h"

class EntityClassUFODependencies: public GlobalRadiantModuleRef,
		public GlobalFileSystemModuleRef,
		public GlobalShaderCacheModuleRef
{
		EClassModulesRef m_eclass_modules;
	public:
		EntityClassUFODependencies () :
			m_eclass_modules("def")
		{
		}
		EClassModules& getEClassModules ()
		{
			return m_eclass_modules.get();
		}
};

class EclassManagerAPI
{
		EntityClassManager* _eclassmanager;
	public:
		typedef IEntityClassManager Type;
		STRING_CONSTANT(Name, "*");

		EclassManagerAPI ()
		{
			_eclassmanager = new EntityClassManager();

			GlobalRadiant().attachGameToolsPathObserver(*_eclassmanager);
			GlobalRadiant().attachGameModeObserver(*_eclassmanager);
		}

		~EclassManagerAPI ()
		{
			GlobalRadiant().detachGameModeObserver(*_eclassmanager);
			GlobalRadiant().detachGameToolsPathObserver(*_eclassmanager);

			delete _eclassmanager;
		}

		IEntityClassManager* getTable ()
		{
			return _eclassmanager;
		}
};

typedef SingletonModule<EclassManagerAPI, EntityClassUFODependencies> EclassManagerModule;
typedef Static<EclassManagerModule> StaticEclassManagerModule;
StaticRegisterModule staticRegisterEclassManager(StaticEclassManagerModule::instance());

EntityClassScanner* GlobalEntityClassScanner ()
{
	return StaticEclassManagerModule::instance().getDependencies().getEClassModules().findModule("def");
}
