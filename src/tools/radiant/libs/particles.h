/**
 * @file particles.h
 */

#ifndef PARTICLES_H
#define PARTICLES_H

#include "imagelib.h"
#include "archivelib.h"
#include "autoptr.h"
#include "stringio.h"
#include "ifilesystem.h"
#include "itextures.h"
#include "stream/stringstream.h"
#include "iradiant.h"
#include <map>
#include <list>

class ParticleDefinition {

private:
	bool m_copy;
	void loadParticleTexture (void) {
		if (m_image)
			return;

		if (m_imageName) {
#if 1
			StringOutputStream name(256);
			name << GlobalRadiant().getEnginePath() << GlobalRadiant().getRequiredGameDescriptionKeyValue("basegame") << "/" << "pics/" << m_imageName << ".tga";
			m_image = GlobalTexturesCache().capture(name.c_str());
			if (!m_image) {
				name << GlobalRadiant().getEnginePath() << GlobalRadiant().getRequiredGameDescriptionKeyValue("basegame") << "/" << "pics/" << m_imageName << ".jpg";
				m_image = GlobalTexturesCache().capture(name.c_str());
				if (!m_image)
					g_warning("Particle image: %s wasn't found\n", m_imageName);
			}
#endif
		}
	}

	void freeParticleTexture (void) {
		if (m_image && !m_copy) {
#if 1
			GlobalTexturesCache().release(m_image);
#endif
			m_image = NULL;
		}
	}
public:
	ParticleDefinition(const char *id) : m_copy(false), m_id(id), m_modelName(NULL), m_imageName(NULL), m_image(NULL) {
		reloadParticleTextureName();
		loadParticleTexture();
	}

	ParticleDefinition(const char* id, const char *modelName, const char *imageName)
		: m_copy(false), m_id(id), m_modelName(modelName), m_imageName(imageName), m_image(NULL) {
		loadParticleTexture();
	}

	ParticleDefinition(ParticleDefinition const& copy): m_copy(true), m_id(copy.m_id), m_modelName(copy.m_modelName), m_imageName(copy.m_imageName), m_image(copy.m_image) {
		/* copy constructor */
	}

	~ParticleDefinition () {
		freeParticleTexture();
	}

	const char *m_id;
	const char *m_modelName;
	const char *m_imageName;
	qtexture_t *m_image;

	void reloadParticleTextureName (void) {
		// TODO: Reload particle def and parse image name
		m_imageName = NULL;
	}

	void particleChanged(const char* id) {
		m_id = id;
		g_message("ID is now: %s\n", m_id);
		g_warning("TODO: Implement particle changing\n");
		freeParticleTexture();
		reloadParticleTextureName();
		loadParticleTexture();
	}
	typedef MemberCaller1<ParticleDefinition, const char*, &ParticleDefinition::particleChanged> ParticleChangedCaller;
};

typedef std::map<CopiedString, ParticleDefinition> ParticleDefinitionMap;

extern ParticleDefinitionMap g_particleDefinitions;

void Particles_Init(void);
void Particles_Shutdown(void);

#endif
