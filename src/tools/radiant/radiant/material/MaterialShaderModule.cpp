#include "MaterialShaderSystem.h"

#include "ifilesystem.h"
#include "itextures.h"
#include "iradiant.h"
#include "iregistry.h"
#include "iscriplib.h"

#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"

class MaterialShaderSystemAPI
{
		MaterialShaderSystem * _materialShaderSystem;
	public:
		typedef MaterialShaderSystem Type;
		STRING_CONSTANT(Name, "*");

		MaterialShaderSystemAPI ()
		{
			_materialShaderSystem = new MaterialShaderSystem();
			GlobalFileSystem().attach(*_materialShaderSystem);
		}

		~MaterialShaderSystemAPI ()
		{
			GlobalFileSystem().detach(*_materialShaderSystem);
			delete _materialShaderSystem;
		}

		MaterialShaderSystem* getTable ()
		{
			return _materialShaderSystem;
		}
};

/* MaterialShaderSystemAPI dependencies class.
 */

class MaterialShaderSystemDependencies: public GlobalFileSystemModuleRef,
		public GlobalTexturesModuleRef,
		public GlobalRegistryModuleRef,
		public GlobalScripLibModuleRef,
		public GlobalRadiantModuleRef
{
};

typedef SingletonModule<MaterialShaderSystemAPI, MaterialShaderSystemDependencies> MaterialShaderSystemModule;
typedef Static<MaterialShaderSystemModule> StaticMaterialShaderSystemModule;
StaticRegisterModule staticRegisterMaterialShaderSystem(StaticMaterialShaderSystemModule::instance());
