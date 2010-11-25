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

#if !defined(INCLUDED_ISHADERS_H)
#define INCLUDED_ISHADERS_H

#include "generic/constant.h"
#include "generic/callbackfwd.h"
#include <string>
#include "ishader.h"

enum
{
	QER_TRANS = 1 << 0, QER_ALPHATEST = 1 << 1, QER_CLIP = 1 << 2
};

template<typename Element> class BasicVector3;
typedef BasicVector3<float> Vector3;
typedef Vector3 Colour3;

typedef Callback1<const std::string&> ShaderNameCallback;

#include "moduleobserver.h"

/**
 * @note shader and texture names used must be full path.
 * Shaders usable as textures have prefix equal to @c getTexturePrefix()
 */
class ShaderSystem : public ModuleObserver
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "shaders");

		virtual ~ShaderSystem ()
		{
		}

		// ModuleObserver implementation
		virtual void realise () = 0;
		virtual void unrealise () = 0;
		virtual void attach (ModuleObserver& observer) = 0;
		virtual void detach (ModuleObserver& observer) = 0;

		virtual void refresh () = 0;

		/**
		 * activate the shader for a given name and return it
		 * will return the default shader if name is not found
		 */
		virtual IShader* getShaderForName (const std::string& name) = 0;

		virtual void foreachShaderName (const ShaderNameCallback& callback) = 0;

		/** Visitor interface.
		 */
		struct Visitor
		{
				virtual ~Visitor() {}

				virtual void visit (const std::string& shaderName) const = 0;
		};

		virtual void foreachShaderName (const Visitor& visitor) = 0;

		/**
		 * iterate over the list of active shaders
		 */
		virtual void beginActiveShadersIterator () = 0;

		virtual bool endActiveShadersIterator () = 0;

		virtual IShader* dereferenceActiveShadersIterator () = 0;

		virtual void incrementActiveShadersIterator () = 0;

		virtual void setActiveShadersChangedNotify (const Callback& notify) = 0;

		virtual const std::string& getTexturePrefix () const = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<ShaderSystem> GlobalShadersModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<ShaderSystem> GlobalShadersModuleRef;

inline ShaderSystem& GlobalShaderSystem ()
{
	return GlobalShadersModule::getTable();
}

#endif
