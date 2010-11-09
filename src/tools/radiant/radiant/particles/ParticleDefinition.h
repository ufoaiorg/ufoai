/**
 * @file particles.h
 */

#ifndef PARTICLES_H
#define PARTICLES_H

#include "iparticles.h"

class ParticleDefinition : public IParticleDefinition
{
	private:
		std::string _id;
		std::string _modelName;
		std::string _imageName;
		int _width;
		int _height;

	public:
		ParticleDefinition (const std::string& id) :
			_id(id), _modelName(""), _imageName("")
		{
			_width = 0;
			_height = 0;
		}

		ParticleDefinition (const std::string& id, const std::string& modelName, const std::string& imageName,
				int width, int height) :
			_id(id), _modelName(modelName), _imageName(imageName)
		{
			_width = width;
			_height = height;
		}

		~ParticleDefinition ()
		{
		}

		const std::string& getName () const
		{
			return _id;
		}

		const std::string& getImage () const
		{
			return _imageName;
		}

		const std::string& getModel () const
		{
			return _modelName;
		}

		int getWidth () const
		{
			return _width;
		}

		int getHeight () const
		{
			return _height;
		}
};

#endif
