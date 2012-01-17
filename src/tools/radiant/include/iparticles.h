/**
 * @file iparticles.h
 */

/*
 Copyright (C) 2002-2011 UFO: Alien Invasion.

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
		virtual const std::string& getName () const = 0;

		virtual const std::string& getModel () const = 0;

		virtual const std::string& getImage () const = 0;

		virtual int getHeight () const = 0;

		virtual int getWidth () const = 0;
};

typedef std::map<std::string, IParticleDefinition*> ParticleDefinitionMap;

class IParticleSystem
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "particles");

		virtual ~IParticleSystem ()
		{
		}

		/**
		 * @brief
		 * Visitor interface the for the particle system.
		 *
		 * This defines the Visitor interface which is used in the foreachParticle()
		 * visit methods.
		 */
		class Visitor
		{
			public:
				virtual ~Visitor ()
				{
				}

				/**
				 * @brief Called by the selection system for each visited node.
				 */
				virtual void visit (IParticleDefinition* particle) const = 0;
		};

		/**
		 * @brief Use the provided Visitor object to enumerate each registered particle
		 */
		virtual void foreachParticle (const Visitor& visitor) const = 0;

		virtual const ParticleDefinitionMap& getParticleDefinitions () = 0;

		virtual IParticleDefinition* getParticle(const std::string& particleID) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<IParticleSystem> GlobalParticleModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<IParticleSystem> GlobalParticleModuleRef;

inline IParticleSystem& GlobalParticleSystem ()
{
	return GlobalParticleModule::getTable();
}

#endif
