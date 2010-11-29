#include "iradiant.h"
#include "igl.h"
#include "iregistry.h"
#include "iimage.h"
#include "preferencesystem.h"

#include "modulesystem/modulesmap.h"
#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

#include "TexturesMap.h"

class TexturesDependencies: public GlobalRadiantModuleRef,
		public GlobalOpenGLModuleRef,
		public GlobalRegistryModuleRef,
		public GlobalPreferenceSystemModuleRef
{
		ImageModulesRef m_image_modules;
	public:
		TexturesDependencies () :
			m_image_modules("*")
		{
		}
		ImageModules& getImageModules ()
		{
			return m_image_modules.get();
		}
};

typedef SingletonModule<TexturesMap, TexturesDependencies> TexturesModule;
typedef Static<TexturesModule> StaticTexturesModule;
StaticRegisterModule staticRegisterTextures(StaticTexturesModule::instance());
