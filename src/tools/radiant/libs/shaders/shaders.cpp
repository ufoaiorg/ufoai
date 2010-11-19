/**
 * @file shaders.cpp
 * @brief Shaders Manager Plugin
 * @note there is an important distinction between SHADER_NOT_FOUND and SHADER_NOTEX:
 * SHADER_NOT_FOUND means we didn't find the raw texture or the shader for this
 * SHADER_NOTEX means we recognize this as a shader script, but we are missing the texture to represent it
 * this was in the initial design of the shader code since early GtkRadiant alpha, and got sort of foxed in 1.2 and put back in
 */

/*
 Copyright (c) 2001, Loki software, inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list
 of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the name of Loki software nor the names of its contributors may be used
 to endorse or promote products derived from this software without specific prior
 written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "shaders.h"

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <list>

#include "ifilesystem.h"
#include "ishadersystem.h"
#include "iscriplib.h"
#include "imaterial.h"
#include "itextures.h"
#include "irender.h"
#include "iregistry.h"

#include "../../radiant/environment.h"

#include <glib/gslist.h>

#include "AutoPtr.h"
#include "debugging/debugging.h"
#include "math/FloatTools.h"
#include "generic/callback.h"
#include "generic/referencecounted.h"
#include "stream/memstream.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"
#include "os/path.h"
#include "os/dir.h"
#include "os/file.h"
#include "stringio.h"
#include "shaderlib.h"
#include "texturelib.h"
#include "moduleobservers.h"
#include "archivelib.h"
#include "imagelib.h"

static Callback g_ActiveShadersChangedNotify;

// Maps of names to corresponding GtkTreeIter* nodes, for both intermediate
// paths and explicitly presented paths
typedef std::map<std::string, bool> LicensesMap;
static LicensesMap licensesMap;

typedef std::string ShaderVariable;
typedef std::string ShaderValue;

// clean a texture name to the qtexture_t name format we use internally
// NOTE: case sensitivity: the engine is case sensitive. we store the shader name with case information and save with case
// information as well. but we assume there won't be any case conflict and so when doing lookups based on shader name,
// we compare as case insensitive. That is Radiant is case insensitive, but knows that the engine is case sensitive.
//++timo FIXME: we need to put code somewhere to detect when two shaders that are case insensitive equal are present
std::string Tokeniser_parseShaderName(Tokeniser& tokeniser) {
	std::string token = tokeniser.getToken();
	if (token.empty())
		return "";

	std::string cleaned = os::standardPath(token);
	return os::stripExtension(cleaned);
}

typedef std::list<ShaderVariable> ShaderParameters;
typedef std::list<ShaderVariable> ShaderArguments;

typedef std::pair<ShaderVariable, ShaderVariable> BlendFuncExpression;

class ShaderTemplate {
		std::size_t m_refcount;
		std::string m_Name;
	public:

		ShaderParameters m_params;

		std::string m_textureName;
		std::string m_diffuse;
		std::string m_bump;
		std::string m_specular;

		int m_nFlags;
		float m_fTrans;

		// alphafunc stuff
		IShader::EAlphaFunc m_AlphaFunc;
		float m_AlphaRef;
		// cull stuff
		IShader::ECull m_Cull;

		ShaderTemplate() :
			m_refcount(0) {
			m_nFlags = 0;
			m_fTrans = 1.0f;
		}

		void IncRef() {
			++m_refcount;
		}
		void DecRef() {
			ASSERT_MESSAGE(m_refcount != 0, "shader reference-count going below zero");
			if (--m_refcount == 0) {
				delete this;
			}
		}

		std::size_t refcount() {
			return m_refcount;
		}

		const std::string& getName() const {
			return m_Name;
		}
		void setName(const std::string& name) {
			m_Name = name;
		}

		void CreateDefault(const std::string& name) {
			m_textureName = name;
			setName(name);
		}
};

typedef SmartPointer<ShaderTemplate> ShaderTemplatePointer;
typedef std::map<std::string, ShaderTemplatePointer> ShaderTemplateMap;

ShaderTemplateMap g_shaders;
ShaderTemplateMap g_shaderTemplates;

class ShaderDefinition {
	public:
		ShaderDefinition(ShaderTemplate* shaderTemplate, const ShaderArguments& args,
				const std::string& filename) :
			shaderTemplate(shaderTemplate), args(args), filename(filename) {
		}
		ShaderTemplate* shaderTemplate;
		ShaderArguments args;
		const std::string filename;
};

typedef std::map<std::string, ShaderDefinition> ShaderDefinitionMap;

ShaderDefinitionMap g_shaderDefinitions;

class CShader: public IShader {
		std::size_t m_refcount;

		const ShaderTemplate& m_template;
		const ShaderArguments& m_args;
		const std::string m_filename;
		// name is shader-name, otherwise texture-name (if not a real shader)
		std::string m_Name;

		qtexture_t* m_pTexture;
		qtexture_t* m_notfound;
		BlendFunc m_blendFunc;

		bool m_bInUse;
		bool m_bIsValid;

		bool searchLicense() {
			// Look up candidate in the map and return true if found
			LicensesMap::iterator it = licensesMap.find(m_template.m_textureName);
			if (it != licensesMap.end())
				return true;

			return false;
		}

	public:
		CShader(const ShaderDefinition& definition) :
			m_refcount(0), m_template(*definition.shaderTemplate), m_args(definition.args),
					m_filename(definition.filename), m_blendFunc(BLEND_SRC_ALPHA,
							BLEND_ONE_MINUS_SRC_ALPHA), m_bInUse(false) {
			m_pTexture = 0;
			m_notfound = 0;

			realise();
		}
		virtual ~CShader() {
			unrealise();

			ASSERT_MESSAGE(m_refcount == 0, "deleting active shader");
		}

		// IShaders implementation -----------------
		void IncRef() {
			++m_refcount;
		}
		void DecRef() {
			ASSERT_MESSAGE(m_refcount != 0, "shader reference-count going below zero");
			if (--m_refcount == 0) {
				delete this;
			}
		}

		std::size_t refcount() {
			return m_refcount;
		}

		// get/set the qtexture_t* Radiant uses to represent this shader object
		qtexture_t* getTexture() const {
			return m_pTexture;
		}
		// get shader name
		const std::string& getName() const {
			return m_Name;
		}
		bool IsValid() const {
			return m_bIsValid;
		}
		void SetIsValid(bool bIsValid) {
			m_bIsValid = bIsValid;
			g_ActiveShadersChangedNotify();
		}
		bool IsInUse() const {
			return m_bInUse;
		}
		void SetInUse(bool bInUse) {
			m_bInUse = bInUse;
			g_ActiveShadersChangedNotify();
		}
		// get the shader flags
		int getFlags() const {
			return m_template.m_nFlags;
		}
		// get the transparency value
		float getTrans() const {
			return m_template.m_fTrans;
		}
		// test if it's a true shader, or a default shader created to wrap around a texture
		bool IsDefault() const {
			return m_filename.empty();
		}
		// get the alphaFunc
		void getAlphaFunc(EAlphaFunc *func, float *ref) {
			*func = m_template.m_AlphaFunc;
			*ref = m_template.m_AlphaRef;
		}
		BlendFunc getBlendFunc() const {
			return m_blendFunc;
		}
		// get the cull type
		ECull getCull() {
			return m_template.m_Cull;
		}

		void realise() {
			m_pTexture = GlobalTexturesCache().capture(m_template.m_textureName);

			if (m_pTexture->texture_number == 0) {
				m_notfound = m_pTexture;
				m_pTexture = GlobalTexturesCache().capture("textures/tex_common/nodraw");
			}

			SetIsValid(searchLicense());
		}

		void unrealise() {
			GlobalTexturesCache().release(m_pTexture);

			if (m_notfound != 0) {
				GlobalTexturesCache().release(m_notfound);
			}
		}

		// set shader name
		void setName(const std::string& name) {
			m_Name = name;
		}

		void forEachLayer(const ShaderLayerCallback& layer) const {
		}

		float getPolygonOffset() const {
			return 0.0f;
		}
};

typedef SmartPointer<CShader> ShaderPointer;
typedef std::map<std::string, ShaderPointer, shader_less_t> shaders_t;

shaders_t g_ActiveShaders;

void ParseLicensesFile(Tokeniser& tokeniser, const std::string& filename) {
	for (;;) {
		std::string token = tokeniser.getToken();
		if (token.empty())
			break;
		if (tokeniser.getLine() > 1) {
			tokeniser.ungetToken();
			break;
		}
	}
	std::size_t lastLine = 1;
	for (;;) {
		std::string token = tokeniser.getToken();
		if (token.empty())
			break;

		if (string::contains(token, "base/textures/") && lastLine != tokeniser.getLine()) {
			licensesMap[os::stripExtension(token.substr(5))] = true;
			lastLine = tokeniser.getLine();
		}
	}
}

static void LoadLicenses(const std::string& filename) {
	const std::string& appPath = GlobalRegistry().get(RKEY_APP_PATH);
	std::string fullpath = appPath + filename;

	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(fullpath));
	if (file) {
		g_message("Parsing licenses file '%s'\n", fullpath.c_str());

		AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().m_pfnNewScriptTokeniser(
				file->getInputStream()));
		ParseLicensesFile(*tokeniser, fullpath);
	} else {
		g_warning("Unable to read licenses '%s'\n", fullpath.c_str());
	}
}

#include "stream/filestream.h"

// will free all GL binded qtextures and shaders
// NOTE: doesn't make much sense out of Radiant exit or called during a reload
void Shaders_Free() {
	for (shaders_t::const_iterator i = g_ActiveShaders.begin(); i != g_ActiveShaders.end(); ++i) {
		ASSERT_MESSAGE(i->second->refcount() == 1, "orphan shader still referenced");
	}
	// empty the actives shaders list
	g_ActiveShaders.clear();
	g_shaders.clear();
	g_shaderTemplates.clear();
	g_shaderDefinitions.clear();
	g_ActiveShadersChangedNotify();
}

static ModuleObservers g_observers;

/** @brief wait until filesystem is realised before loading anything */
static std::size_t g_shaders_unrealised = 1;

bool Shaders_realised() {
	return g_shaders_unrealised == 0;
}

class UFOShaderSystem: public ShaderSystem, public ModuleObserver {
	private:

		shaders_t::iterator g_ActiveShadersIterator;
		const std::string g_texturePrefix;

	public:

		UFOShaderSystem() : g_texturePrefix("textures/")
		{
		}

		void realise() {
			if (--g_shaders_unrealised == 0) {
				/** @todo add config option */
				LoadLicenses("../LICENSES");

				GlobalMaterialSystem()->loadMaterials();
				g_observers.realise();
			}
		}
		void unrealise() {
			if (++g_shaders_unrealised == 1) {
				g_observers.unrealise();
				GlobalMaterialSystem()->freeMaterials();
				Shaders_Free();
			}
		}
		void refresh() {
			unrealise();
			realise();
		}

		CShader* _getShaderForName(const std::string& name) {
			{
				// check if it's already loaded
				shaders_t::iterator i = g_ActiveShaders.find(name);
				if (i != g_ActiveShaders.end())
					return (*i).second;
			}

			// find matching shader definition
			ShaderDefinitionMap::iterator i = g_shaderDefinitions.find(name);
			if (i == g_shaderDefinitions.end()) {
				// shader definition was not found

				// create new shader definition from default shader template
				ShaderTemplatePointer shaderTemplate(new ShaderTemplate());
				shaderTemplate->CreateDefault(name);
				g_shaderTemplates.insert(ShaderTemplateMap::value_type(shaderTemplate->getName(),
						shaderTemplate));

				i = g_shaderDefinitions.insert(ShaderDefinitionMap::value_type(name,
						ShaderDefinition(shaderTemplate.get(), ShaderArguments(), ""))).first;
			}

			// create shader from existing definition
			ShaderPointer pShader(new CShader((*i).second));
			pShader->setName(name);
			g_ActiveShaders.insert(shaders_t::value_type(name, pShader));
			g_ActiveShadersChangedNotify();
			return pShader;
		}

		IShader* getShaderForName(const std::string& name) {
			IShader *pShader = GlobalMaterialSystem()->getMaterialForName(name);
			if (pShader == (IShader*) 0)
				pShader = _getShaderForName(name);
			pShader->IncRef();
			return pShader;
		}

		void foreachShaderName(const ShaderNameCallback& callback) {
			for (ShaderDefinitionMap::const_iterator i = g_shaderDefinitions.begin(); i
					!= g_shaderDefinitions.end(); ++i) {
				callback((*i).first);
			}
			GlobalMaterialSystem()->foreachMaterialName(callback);
		}

		void foreachShaderName(const ShaderSystem::Visitor& visitor) {
			for (ShaderDefinitionMap::const_iterator i = g_shaderDefinitions.begin(); i
					!= g_shaderDefinitions.end(); ++i) {
				const std::string& str = (*i).first;
				visitor.visit(str);
			}
			GlobalMaterialSystem()->foreachMaterialName(visitor);
		}

		void beginActiveShadersIterator() {
			g_ActiveShadersIterator = g_ActiveShaders.begin();
			GlobalMaterialSystem()->beginActiveMaterialsIterator();
		}
		bool endActiveShadersIterator() {
			bool shadersIteratorEnd = g_ActiveShadersIterator == g_ActiveShaders.end();
			if (shadersIteratorEnd) {
				bool materialIteratorEnd = GlobalMaterialSystem()->endActiveMaterialsIterator();
				return materialIteratorEnd;
			}
			return shadersIteratorEnd;
		}
		IShader* dereferenceActiveShadersIterator() {
			if (g_ActiveShadersIterator != g_ActiveShaders.end())
				return static_cast<CShader*> (g_ActiveShadersIterator->second);
			else
				return GlobalMaterialSystem()->dereferenceActiveMaterialsIterator();
		}
		void incrementActiveShadersIterator() {
			if (g_ActiveShadersIterator != g_ActiveShaders.end())
				++g_ActiveShadersIterator;
			else
				GlobalMaterialSystem()->incrementActiveMaterialsIterator();
		}
		void setActiveShadersChangedNotify(const Callback& notify) {
			g_ActiveShadersChangedNotify = notify;
		}

		void attach(ModuleObserver& observer) {
			g_observers.attach(observer);
		}
		void detach(ModuleObserver& observer) {
			g_observers.detach(observer);
		}

		const std::string& getTexturePrefix() const {
			return g_texturePrefix;
		}
};

UFOShaderSystem g_UFOShaderSystem;

ShaderSystem& GetShaderSystem() {
	return g_UFOShaderSystem;
}

void Shaders_Construct() {
	GlobalFileSystem().attach(g_UFOShaderSystem);
}
void Shaders_Destroy() {
	GlobalFileSystem().detach(g_UFOShaderSystem);

	if (Shaders_realised()) {
		Shaders_Free();
	}
}
