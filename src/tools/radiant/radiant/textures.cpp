/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "textures.h"
#include "itextures.h"
#include "iregistry.h"
#include "iradiant.h"
#include "igl.h"
#include "radiant_i18n.h"

#include "AutoPtr.h"
#include "texturelib.h"
#include "container/hashfunc.h"
#include "container/cache.h"
#include "image.h"
#include "preferencesystem.h"
#include "textures/TextureManipulator.h"

shaders::TextureManipulator* g_manipulator;

/**
 * @brief This function does the actual processing of raw RGBA data into a GL texture.
 * @note It will also resample to power-of-two dimensions, generate the mipmaps and adjust gamma.
 */
void LoadTextureRGBA (qtexture_t* q, Image* image)
{
	q->surfaceFlags = image->getSurfaceFlags();
	q->contentFlags = image->getContentFlags();
	q->value = image->getValue();
	q->width = image->getWidth();
	q->height = image->getHeight();

	Image* processed = g_manipulator->getProcessedImage(image);

	q->color = g_manipulator->getFlatshadeColour(processed);

	glGenTextures(1, &q->texture_number);

	glBindTexture(GL_TEXTURE_2D, q->texture_number);

	// Tell OpenGL how to use the mip maps we will be creating here
	g_manipulator->setTextureParameters();

	// Now create the mipmaps; conveniently, there exists an openGL method for this
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, processed->getWidth(), processed->getHeight(),
					  GL_RGBA, GL_UNSIGNED_BYTE, processed->getRGBAPixels());

	glBindTexture(GL_TEXTURE_2D, 0);

	// if we had to resize it
	if (image != processed)
		delete processed;
}

typedef std::pair<LoadImageCallback, std::string> TextureKey;

void qtexture_realise (qtexture_t& texture, const TextureKey& key)
{
	texture.texture_number = 0;
	/* skip empty names and normalmaps */
	if (!key.second.empty() && !strstr(key.second.c_str(), "_nm")) {
		AutoPtr<Image> image(key.first.loadImage(key.second));
		if (image) {
			LoadTextureRGBA(&texture, image);
		} else {
			g_warning("Texture load failed: \"%s\"\n", key.second.c_str());
		}
	}
}

void qtexture_unrealise (qtexture_t& texture)
{
	if (GlobalOpenGL().contextValid && texture.texture_number != 0) {
		glDeleteTextures(1, &texture.texture_number);
	}
}

class TextureKeyEqualNoCase
{
	public:
		bool operator() (const TextureKey& key, const TextureKey& other) const
		{
			return key.first == other.first && string_equal_nocase(key.second, other.second);
		}
};

class TextureKeyHashNoCase
{
	public:
		typedef hash_t hash_type;
		hash_t operator() (const TextureKey& key) const
		{
			return hash_combine(string_hash_nocase(key.second.c_str()), pod_hash(key.first));
		}
};

#define DEBUG_TEXTURES 0

class TexturesMap: public TexturesCache
{
		class TextureConstructor
		{
				TexturesMap* m_cache;
			public:
				explicit TextureConstructor (TexturesMap* cache) :
					m_cache(cache)
				{
				}
				qtexture_t* construct (const TextureKey& key)
				{
					qtexture_t* texture = new qtexture_t(key.first, key.second);
					if (m_cache->realised()) {
						qtexture_realise(*texture, key);
					}
					return texture;
				}
				void destroy (qtexture_t* texture)
				{
					if (m_cache->realised()) {
						qtexture_unrealise(*texture);
					}
					delete texture;
				}
		};

		typedef HashedCache<TextureKey, qtexture_t, TextureKeyHashNoCase, TextureKeyEqualNoCase, TextureConstructor>
				qtextures_t;
		qtextures_t m_qtextures;
		TexturesCacheObserver* m_observer;
		std::size_t m_unrealised;

	public:
		TexturesMap () :
			m_qtextures(TextureConstructor(this)), m_observer(0), m_unrealised(1)
		{
		}
		typedef qtextures_t::iterator iterator;

		iterator begin ()
		{
			return m_qtextures.begin();
		}
		iterator end ()
		{
			return m_qtextures.end();
		}

		LoadImageCallback defaultLoader () const
		{
			return LoadImageCallback(0, QERApp_LoadImage);
		}
		Image* loadImage (const std::string& name)
		{
			return defaultLoader().loadImage(name);
		}
		qtexture_t* capture (const std::string& name)
		{
			return capture(defaultLoader(), name);
		}
		qtexture_t* capture (const LoadImageCallback& loader, const std::string& name)
		{
			g_debug("textures capture: '%s'\n", name.c_str());
			return m_qtextures.capture(TextureKey(loader, name)).get();
		}
		void release (qtexture_t* texture)
		{
			g_debug("textures release: '%s'\n", texture->name.c_str());
			m_qtextures.release(TextureKey(texture->load, texture->name));
		}
		void attach (TexturesCacheObserver& observer)
		{
			ASSERT_MESSAGE(m_observer == 0, "TexturesMap::attach: cannot attach observer");
			m_observer = &observer;
		}
		void detach (TexturesCacheObserver& observer)
		{
			ASSERT_MESSAGE(m_observer == &observer, "TexturesMap::detach: cannot detach observer");
			m_observer = 0;
		}
		void realise ()
		{
			if (--m_unrealised == 0) {
				for (qtextures_t::iterator i = m_qtextures.begin(); i != m_qtextures.end(); ++i) {
					if (!(*i).value.empty()) {
						qtexture_realise(*(*i).value, (*i).key);
					}
				}
				if (m_observer != 0) {
					m_observer->realise();
				}
			}
		}
		void unrealise ()
		{
			if (++m_unrealised == 1) {
				if (m_observer != 0) {
					m_observer->unrealise();
				}
				for (qtextures_t::iterator i = m_qtextures.begin(); i != m_qtextures.end(); ++i) {
					if (!(*i).value.empty()) {
						qtexture_unrealise(*(*i).value);
					}
				}
			}
		}
		bool realised ()
		{
			return m_unrealised == 0;
		}
};

TexturesMap* g_texturesmap;

TexturesCache& GetTexturesCache ()
{
	return *g_texturesmap;
}

void Textures_Realise ()
{
	g_texturesmap->realise();
}

void Textures_Unrealise ()
{
	g_texturesmap->unrealise();
}

void Textures_Construct ()
{
	g_texturesmap = new TexturesMap;
	g_manipulator = new shaders::TextureManipulator;
}
void Textures_Destroy ()
{
	delete g_texturesmap;
	delete g_manipulator;
}

#include "modulesystem/modulesmap.h"
#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

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

class TexturesAPI
{
		TexturesCache* m_textures;
	public:
		typedef TexturesCache Type;
		STRING_CONSTANT(Name, "*");

		TexturesAPI ()
		{
			Textures_Construct();

			m_textures = &GetTexturesCache();
		}
		~TexturesAPI ()
		{
			Textures_Destroy();
		}
		TexturesCache* getTable ()
		{
			return m_textures;
		}
};

typedef SingletonModule<TexturesAPI, TexturesDependencies> TexturesModule;
typedef Static<TexturesModule> StaticTexturesModule;
StaticRegisterModule staticRegisterTextures(StaticTexturesModule::instance());
