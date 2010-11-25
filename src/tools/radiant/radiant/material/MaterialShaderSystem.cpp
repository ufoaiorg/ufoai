#include "MaterialShaderSystem.h"

MaterialShaderSystem::MaterialShaderSystem () :
	g_texturePrefix("textures/")
{
}

void MaterialShaderSystem::realise ()
{
	GlobalMaterialSystem()->loadMaterials();

	/** @todo add config option */
	_licenseParser.openLicenseFile("../LICENSES");

	// notify the observers that this module is initialized now
	g_observers.realise();
}

void MaterialShaderSystem::unrealise ()
{
	// notify the observers that this module is going to get shut down now
	g_observers.unrealise();

	for (MaterialShaders::iterator i = _activeMaterialShaders.begin(); i != _activeMaterialShaders.end(); ++i) {
		ASSERT_MESSAGE(i->second->refcount() == 1, "orphan material still referenced");
	}
	_activeMaterialShaders.clear();
	_materialDefinitions.clear();
	g_ActiveShadersChangedNotify();

	GlobalMaterialSystem()->freeMaterials();
}

void MaterialShaderSystem::attach (ModuleObserver& observer)
{
	g_observers.attach(observer);
}

void MaterialShaderSystem::detach (ModuleObserver& observer)
{
	g_observers.detach(observer);
}

void MaterialShaderSystem::refresh ()
{
	unrealise();
	realise();
}

IShader* MaterialShaderSystem::getShaderForName (const std::string& name)
{
	MaterialShaders::iterator i = _activeMaterialShaders.find(name);
	if (i != _activeMaterialShaders.end()) {
		(*i).second->IncRef();
		return (*i).second;
	}

	MaterialShader *material;
	std::string block = GlobalMaterialSystem()->getBlock(name);
	if (block.empty())
		material = new MaterialShader(name);
	else
		material = new MaterialShader(name, block);

	MaterialDefinitionMap::iterator idef = _materialDefinitions.find(name);
	if (idef == _materialDefinitions.end()) {
		_materialDefinitions[name] = name;
	}

	MaterialPointer pShader(material);
	_activeMaterialShaders.insert(MaterialShaders::value_type(name, pShader));
	pShader->SetIsValid(_licenseParser.isValid(name));

	pShader->IncRef();

	g_ActiveShadersChangedNotify();

	return pShader;
}

void MaterialShaderSystem::foreachShaderName (const ShaderNameCallback& callback)
{
	for (MaterialDefinitionMap::const_iterator i = _materialDefinitions.begin(); i != _materialDefinitions.end(); ++i) {
		const std::string& str = (*i).first;
		callback(str);
	}
}

void MaterialShaderSystem::foreachShaderName (const ShaderSystem::Visitor& visitor)
{
	for (MaterialDefinitionMap::const_iterator i = _materialDefinitions.begin(); i != _materialDefinitions.end(); ++i) {
		const std::string& str = (*i).first;
		visitor.visit(str);
	}
}

void MaterialShaderSystem::beginActiveShadersIterator ()
{
	_activeMaterialsIterator = _activeMaterialShaders.begin();
}

bool MaterialShaderSystem::endActiveShadersIterator ()
{
	return _activeMaterialsIterator == _activeMaterialShaders.end();
}

IShader* MaterialShaderSystem::dereferenceActiveShadersIterator ()
{
	return _activeMaterialsIterator->second;
}

void MaterialShaderSystem::incrementActiveShadersIterator ()
{
	++_activeMaterialsIterator;
}

void MaterialShaderSystem::setActiveShadersChangedNotify (const Callback& notify)
{
	g_ActiveShadersChangedNotify = notify;
}

const std::string& MaterialShaderSystem::getTexturePrefix () const
{
	return g_texturePrefix;
}
