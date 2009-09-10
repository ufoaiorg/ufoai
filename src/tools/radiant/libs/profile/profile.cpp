/**
 * @file profile.cpp
 * @brief Application settings load/save
 * @author Leonardo Zide (leo@lokigames.com)
 */

/*
 Copyright (c) 2001, Loki software, inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list
 of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the name of Loki software nor the names of its contributors may be used
 to endorse or promote products derived from this software without specific prior
 written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "profile.h"

#include <string.h>
#include <stdlib.h>

#include "file.h"

bool read_var (const std::string& filename, const char *section, const char *key, char *value)
{
	char line[1024], *ptr;
	FILE *rc;

	rc = fopen(filename.c_str(), "rt");

	if (rc == NULL)
		return false;

	while (fgets(line, 1024, rc) != 0) {
		// First we find the section
		if (line[0] != '[')
			continue;

		ptr = strchr(line, ']');
		*ptr = '\0';

		if (strcmp(&line[1], section) == 0) {
			while (fgets(line, 1024, rc) != 0) {
				ptr = strchr(line, '=');

				if (ptr == NULL) {
					// reached the end of the section
					fclose(rc);
					return false;
				}
				*ptr = '\0';

				// remove spaces
				while (line[strlen(line) - 1] == ' ')
					line[strlen(line) - 1] = '\0';

				if (strcmp(line, key) == 0) {
					strcpy(value, ptr + 1);
					fclose(rc);

					if (value[strlen(value) - 1] == 10 || value[strlen(value) - 1] == 13 || value[strlen(value) - 1]
							== 32)
						value[strlen(value) - 1] = '\0';

					return true;
				}
			}
		}
	}

	fclose(rc);
	return false;
}

bool save_var (const std::string& filename, const char *section, const char *key, const char *value)
{
	char line[1024], *ptr;
	MemStream old_rc;
	bool found;
	FILE *rc;

	rc = fopen(filename.c_str(), "rb");

	if (rc != NULL) {
		unsigned int len;
		void *buf;

		fseek(rc, 0, SEEK_END);
		len = ftell(rc);
		rewind(rc);
		buf = malloc(len);
		fread(buf, len, 1, rc);
		old_rc.write(reinterpret_cast<MemStream::byte_type*> (buf), len);
		free(buf);
		fclose(rc);
		old_rc.Seek(0, SEEK_SET);
	}

	// TTimo: changed to binary writing. It doesn't seem to affect linux version, and win32 version was happending a lot of '\n'
	rc = fopen(filename.c_str(), "wb");

	if (rc == NULL)
		return false;

	// First we need to find the section
	found = false;
	while (old_rc.ReadString(line, 1024) != NULL) {
		fputs(line, rc);

		if (line[0] == '[') {
			ptr = strchr(line, ']');
			*ptr = '\0';

			if (strcmp(&line[1], section) == 0) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		fputs("\n", rc);
		fprintf(rc, "[%s]\n", section);
	}

	fprintf(rc, "%s=%s\n", key, value);

	while (old_rc.ReadString(line, 1024) != NULL) {
		ptr = strchr(line, '=');

		if (ptr != NULL) {
			*ptr = '\0';

			if (strcmp(line, key) == 0)
				break;

			*ptr = '=';
			fputs(line, rc);
		} else {
			fputs(line, rc);
			break;
		}
	}

	while (old_rc.ReadString(line, 1024) != NULL)
		fputs(line, rc);

	fclose(rc);
	return true;
}
