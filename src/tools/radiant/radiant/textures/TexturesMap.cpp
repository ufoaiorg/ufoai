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

#include "TexturesMap.h"
#include "texturelib.h"

#include "../image.h" // QERApp_LoadImage

TexturesMap::TextureConstructor::TextureConstructor (TexturesMap* cache) :
	m_cache(cache)
{
}

GLTexture* TexturesMap::TextureConstructor::construct (const TextureKey& key)
{
	GLTexture* texture = new GLTexture(key.first, key.second);
	if (m_cache->realised()) {
		texture->realise();
	}
	return texture;
}

void TexturesMap::TextureConstructor::destroy (GLTexture* texture)
{
	if (m_cache->realised()) {
		texture->unrealise();
	}
	delete texture;
}

TexturesMap::TexturesMap () :
	m_qtextures(TextureConstructor(this)), m_observer(0), m_unrealised(1)
{
}

LoadImageCallback TexturesMap::defaultLoader () const
{
	return LoadImageCallback(0, QERApp_LoadImage);
}

GLTexture* TexturesMap::capture (const std::string& name)
{
	return capture(defaultLoader(), name);
}

GLTexture* TexturesMap::capture (const LoadImageCallback& loader, const std::string& name)
{
	g_debug("textures capture: '%s'\n", name.c_str());
	return m_qtextures.capture(TextureKey(loader, name)).get();
}

void TexturesMap::release (GLTexture* texture)
{
	g_debug("textures release: '%s'\n", texture->getName().c_str());
	m_qtextures.release(TextureKey(texture->load, texture->getName()));
}

void TexturesMap::attach (TexturesCacheObserver& observer)
{
	ASSERT_MESSAGE(m_observer == 0, "TexturesMap::attach: cannot attach observer");
	m_observer = &observer;
}

void TexturesMap::detach (TexturesCacheObserver& observer)
{
	ASSERT_MESSAGE(m_observer == &observer, "TexturesMap::detach: cannot detach observer");
	m_observer = 0;
}

void TexturesMap::realise ()
{
	if (--m_unrealised == 0) {
		for (TextureCache::iterator i = m_qtextures.begin(); i != m_qtextures.end(); ++i) {
			if (!(*i).value.empty()) {
				(*(*i).value).realise();
			}
		}
		if (m_observer != 0) {
			m_observer->realise();
		}
	}
}

void TexturesMap::unrealise ()
{
	if (++m_unrealised == 1) {
		if (m_observer != 0) {
			m_observer->unrealise();
		}
		for (TextureCache::iterator i = m_qtextures.begin(); i != m_qtextures.end(); ++i) {
			if (!(*i).value.empty()) {
				(*(*i).value).unrealise();
			}
		}
	}
}

bool TexturesMap::realised ()
{
	return m_unrealised == 0;
}
