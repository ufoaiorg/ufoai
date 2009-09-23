/**
 * @file particles.cpp
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

#include "particles.h"
#include <stdlib.h>

// the list of ufos/*.ufo files we need to work with
static GSList *l_ufofiles = 0;
static std::list<CopiedString> g_ufoFilenames;

ParticleDefinitionMap g_particleDefinitions;

static const char * const blend_names[] = { "replace", "one", "blend", "add", "filter", "invfilter" };

static int GetBlendMode (const char *token)
{
	int i;

	for (i = 0; i < BLEND_LAST; i++) {
		if (string_equal(token, blend_names[i])) {
			return i;
		}
	}
	return BLEND_REPLACE;
}

static void ParseUFOFile (Tokeniser& tokeniser, const char* filename)
{
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
				globalErrorStream() << "ERROR: expected {. parsing file " << filename << "\n";
				return;
			}

			char model[128] = "";
			char image[128] = "";
			int blend = BLEND_REPLACE;
			int width = 0;
			int height = 0;
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
							strncpy(image, tokeniser.getToken(), sizeof(image));
						} else if (string_equal(option, "model")) {
							strncpy(model, tokeniser.getToken(), sizeof(model));
						} else if (string_equal(option, "blend")) {
							blend = GetBlendMode(tokeniser.getToken());
						} else if (string_equal(option, "size")) {
							const char *sizeVector2 = tokeniser.getToken();
							char size[64];
							char *seperator;
							strncpy(size, sizeVector2, sizeof(size));
							seperator = size;
							while (seperator != '\0') {
								seperator++;
								if (*seperator == ' ') {
									*seperator = '\0';
									width = atoi(size);
									height = atoi(++seperator);
									break;
								}
							}
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
			if (!kill && (model[0] != '\0' || image[0] != '\0')) {
				// do we already have this particle?
				ParticleDefinition *particleDefinition =
						new ParticleDefinition(pID, model, image, blend, width, height);
				if (!g_particleDefinitions.insert(ParticleDefinitionMap::value_type(pID, *particleDefinition)).second)
					g_warning("Particle '%s' is already in memory, definition in '%s' ignored.\n", pID, filename);
				delete particleDefinition;
			}
		}
	}
}

void LoadUFOFile (const char* filename)
{
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(filename));
	if (file) {
		AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().m_pfnNewScriptTokeniser(file->getInputStream()));
		ParseUFOFile(*tokeniser, filename);
	} else {
		g_warning("Unable to read ufo script file '%s'\n", filename);
	}
}

void addUFOFile (const char* dirstring)
{
	bool found = false;

	for (GSList* tmp = l_ufofiles; tmp != 0; tmp = tmp->next) {
		if (string_equal_nocase(dirstring, (char*) tmp->data)) {
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

void Particles_Construct (void)
{
	GlobalFileSystem().forEachFile("ufos/", "ufo", AddUFOFileCaller(), 0);
}

void Particles_Destroy (void)
{
	while (l_ufofiles != 0) {
		g_free(l_ufofiles->data);
		l_ufofiles = g_slist_remove(l_ufofiles, l_ufofiles->data);
	}
	g_ufoFilenames.clear();
	g_particleDefinitions.clear();
}
