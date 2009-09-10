/**
 * @file iparticles.h
 */

/*
 Copyright (C) 2002-2009 UFO: Alien Invasion.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#ifndef IPARTICLES_H_
#define IPARTICLES_H_

#include "generic/constant.h"
#include <map>
#include <string>

class IParticleDefinition
{
	public:
		virtual ~IParticleDefinition ()
		{
		}
		virtual std::string& getModel () const = 0;

		virtual std::string& getImage () const = 0;

		virtual void render (int x, int y) const = 0;

		virtual int getHeight () const = 0;

		virtual int getWidth () const = 0;
};

typedef std::map<std::string, IParticleDefinition> ParticleDefinitionMap;

class ParticleSystem
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "particles");

		virtual ~ParticleSystem ()
		{
		}
		virtual ParticleDefinitionMap getParticleDefinitions () = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<ParticleSystem> GlobalParticleModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<ParticleSystem> GlobalParticleModuleRef;

inline ParticleSystem& GlobalParticleSystem ()
{
	return GlobalParticleModule::getTable();
}

#endif
