#include "iscriplib.h"
#include "irender.h"
#include "ieclass.h"

#include "EntityClassScanner.h"

#include "modulesystem/singletonmodule.h"
#include "generic/constant.h"

class EntityClassDefDependencies: public GlobalShaderCacheModuleRef, public GlobalScripLibModuleRef
{
};

class EclassDefAPI
{
		EntityClassScannerUFO* _eclassdef;
	public:
		typedef EntityClassScanner Type;
		STRING_CONSTANT(Name, "def");

		EclassDefAPI ()
		{
			_eclassdef = new EntityClassScannerUFO();
		}
		EntityClassScanner* getTable ()
		{
			return _eclassdef;
		}
};

typedef SingletonModule<EclassDefAPI, EntityClassDefDependencies> EclassDefModule;
typedef Static<EclassDefModule> StaticEclassDefModule;
StaticRegisterModule staticRegisterEclassDef(StaticEclassDefModule::instance());
