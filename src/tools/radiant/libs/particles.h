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
#include "qerplugin.h"
#include <map>
#include <list>

// the list of ufos/*.ufo files we need to work with
GSList *l_ufofiles = 0;

GSList* getUFOFileList() {
	return l_ufofiles;
}

std::list<CopiedString> g_ufoFilenames;

class ParticleDefinition {

private:
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
		if (m_image) {
#if 0
			GlobalTexturesCache().release(m_image);
#endif
			m_image = NULL;
		}
	}
public:
	ParticleDefinition(const char *id) : m_id(id), m_modelName(NULL), m_imageName(NULL), m_image(NULL) {
		reloadParticleTextureName();
		loadParticleTexture();
	}

	ParticleDefinition(const char* id, const char *modelName, const char *imageName)
		: m_id(id), m_modelName(modelName), m_imageName(imageName), m_image(NULL) {
		loadParticleTexture();
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

ParticleDefinitionMap g_particleDefinitions;


void ParseUFOFile(Tokeniser& tokeniser, const char* filename) {
	g_ufoFilenames.push_back(filename);
	filename = g_ufoFilenames.back().c_str();
	tokeniser.nextLine();
	for (;;) {
		const char* token = tokeniser.getToken();

		if (token == 0) {
			break;
		}

		if (string_equal(token, "particle")) {
			char pID[64];
			int kill = 0;
			token = tokeniser.getToken();
			if (token == 0) {
				Tokeniser_unexpectedError(tokeniser, 0, "#id-name");
				return;
			}

			strncpy(pID, token, sizeof(pID) - 1);
			pID[sizeof(pID) - 1] = '\0';

			if (!Tokeniser_parseToken(tokeniser, "{")) {
				globalErrorStream() << "ERROR: expected {.\n";
				return;
			}

			const char *model = NULL;
			const char *image = NULL;
			for (;;) {
				token = tokeniser.getToken();
				if (string_equal(token, "init")) {
					if (!Tokeniser_parseToken(tokeniser, "{")) {
						globalErrorStream() << "ERROR: expected { in init.\n";
						return;
					}
					for (;;) {
						const char* option = tokeniser.getToken();
						if (string_equal(option, "}")) {
							break;
						} else if (string_equal(option, "image")) {
							image = tokeniser.getToken();
							token = "}";
							break;
						} else if (string_equal(option, "model")) {
							model = tokeniser.getToken();
							token = "}";
							break;
						}
					}
				} else if (string_equal(token, "think")) {
					if (!Tokeniser_parseToken(tokeniser, "{")) {
						globalErrorStream() << "ERROR: expected { in think.\n";
						return;
					}
					for (;;) {
						const char* option = tokeniser.getToken();
						if (string_equal(option, "}")) {
							break;
						} else if (string_equal(option, "kill")) {
							kill = 1;
							break;
						}
					}
				} else if (string_equal(token, "run")) {
					if (!Tokeniser_parseToken(tokeniser, "{")) {
						globalErrorStream() << "ERROR: expected { in run.\n";
						return;
					}
					for (;;) {
						const char* option = tokeniser.getToken();
						if (string_equal(option, "}")) {
							break;
						} else if (string_equal(option, "kill")) {
							kill = 1;
							break;
						}
					}
				}
				if (string_equal(token, "}"))
					break;
			}
			if (!kill && (model || image)) {
				// do we already have this particle?
				if (!g_particleDefinitions.insert(ParticleDefinitionMap::value_type(pID, ParticleDefinition(pID, model, image))).second)
					g_warning("Particle '%s' is already in memory, definition in '%s' ignored.\n", pID, filename);
			}
		}
	}
}

void LoadUFOFile(const char* filename) {
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(filename));
	if (file) {
		AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().m_pfnNewScriptTokeniser(file->getInputStream()));
		ParseUFOFile(*tokeniser, filename);
	} else {
		g_warning("Unable to read ufo script file '%s'\n", filename);
	}
}

void addUFOFile(const char* dirstring) {
	bool found = false;

	for (GSList* tmp = l_ufofiles; tmp != 0; tmp = tmp->next) {
		if (string_equal_nocase(dirstring, (char*)tmp->data)) {
			found = true;
			break;
		}
	}

	if (!found) {
		l_ufofiles = g_slist_append(l_ufofiles, strdup(dirstring));
		StringOutputStream ufoname(256);
		ufoname << "ufos/" << dirstring;
		LoadUFOFile(ufoname.c_str());
		ufoname.clear();
	}
}

typedef FreeCaller1<const char*, addUFOFile> AddUFOFileCaller;

void Particles_Init (void)
{
	GlobalFileSystem().forEachFile("ufos/", "ufo", AddUFOFileCaller(), 0);
}

void Particles_Shutdown (void)
{
	while (l_ufofiles != 0) {
		free(l_ufofiles->data);
		l_ufofiles = g_slist_remove(l_ufofiles, l_ufofiles->data);
	}
	g_ufoFilenames.clear();
	g_particleDefinitions.clear();
}

#endif
