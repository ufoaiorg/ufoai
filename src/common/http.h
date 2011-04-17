/**
 * @file http.h
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#ifndef HTTP_H
#define HTTP_H

#include "common.h"
#define CURL_STATICLIB
#include <curl/curl.h>

typedef enum {
	DLQ_STATE_NOT_STARTED,
	DLQ_STATE_RUNNING,
	DLQ_STATE_DONE
} dlq_state;

typedef struct dlqueue_s {
	struct dlqueue_s	*next;
	char				ufoPath[MAX_QPATH];
	dlq_state			state;
} dlqueue_t;

typedef struct dlhandle_s {
	CURL		*curl;
	char		filePath[MAX_OSPATH];
	FILE		*file;
	dlqueue_t	*queueEntry;
	size_t		fileSize;
	size_t		position;
	double		speed;
	char		URL[576];
	char		*tempBuffer;
} dlhandle_t;

typedef struct upparam_s {
	const char *name;
	const char *value;
	struct upparam_s* next;
} upparam_t;

typedef void (*http_callback_t) (const char *response);

void HTTP_GetURL(const char *url, http_callback_t callback);
void HTTP_PutFile(const char *formName, const char *fileName, const char *url, const upparam_t *params);
size_t HTTP_Recv(void *ptr, size_t size, size_t nmemb, void *stream);
size_t HTTP_Header(void *ptr, size_t size, size_t nmemb, void *stream);
void HTTP_Cleanup(void);
qboolean HTTP_ExtractComponents(const char *url, char *server, size_t serverLength, char *path, size_t pathLength, int *port);

#endif /* HTTP_H */
