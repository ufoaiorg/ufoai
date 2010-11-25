#include "ishadersystem.h"
#include "imaterial.h"
#include "ishader.h"
#include "iregistry.h"
#include "ishaderlayer.h"

#include "generic/callback.h"
#include "moduleobservers.h"

#include "MaterialShader.h"
#include "LicenseParser.h"

class MaterialShaderSystem: public ShaderSystem, public RegistryKeyObserver
{
	private:
		const std::string _texturePrefix;

		typedef SmartPointer<MaterialShader> MaterialPointer;
		typedef std::map<std::string, MaterialPointer, shader_less_t> MaterialShaders;
		MaterialShaders _activeMaterialShaders;

		typedef std::map<std::string, std::string> MaterialBlockMap;
		MaterialBlockMap _blocks;

		// this is a map of every ever loaded texture/material
		typedef std::map<std::string, std::string> MaterialDefinitionMap;
		MaterialDefinitionMap _materialDefinitions;

		MaterialShaders::iterator _activeMaterialsIterator;

		ModuleObservers g_observers;

		Callback g_ActiveShadersChangedNotify;

		LicenseParser _licenseParser;

	public:
		MaterialShaderSystem ();

		void realise ();
		void unrealise ();
		void refresh ();

		IShader* getShaderForName (const std::string& name);

		void foreachShaderName (const ShaderNameCallback& callback);

		void foreachShaderName (const ShaderSystem::Visitor& visitor);

		void beginActiveShadersIterator ();
		bool endActiveShadersIterator ();
		IShader* dereferenceActiveShadersIterator ();
		void incrementActiveShadersIterator ();
		void setActiveShadersChangedNotify (const Callback& notify);

		void attach (ModuleObserver& observer);
		void detach (ModuleObserver& observer);

		const std::string& getTexturePrefix () const;

		void keyChanged (const std::string& changedKey, const std::string& newValue);
};
