/**
 * @file iparticles.h
 */

/*
 Copyright (C) 1997-2008 UFO:AI Team

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
#include "stringio.h"
#include <map>

class IParticleDefinition
{
	public:
		virtual ~IParticleDefinition ()
		{
		}
		virtual const char *getModel () const
		{
			return (const char *) 0;
		}
		virtual const char *getImage () const
		{
			return (const char *) 0;
		}
		virtual void render (int x, int y) const
		{
		}
		virtual int getHeight () const
		{
			return 0;
		}
		virtual int getWidth () const
		{
			return 0;
		}
};

typedef std::map<CopiedString, IParticleDefinition> ParticleDefinitionMap;

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
