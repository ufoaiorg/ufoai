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

#include "http.h"
#include "../shared/shared.h"
#include "../shared/mutex.h"

/**
 * @brief Extract the servername, the port and the path part of the given url
 * @param url The url to extract the data from
 * @param server The server target buffer
 * @param serverLength The length of the buffer
 * @param path The path target buffer
 * @param pathLength The length of the buffer
 * @param port The port
 * @return @c true if the extracting went well, @c false if an error occurred
 */
qboolean HTTP_ExtractComponents (const char *url, char *server, size_t serverLength, char *path, size_t pathLength, int *port)
{
	char *s, *buf;
	const char *proto = "http://";
	const size_t protoLength = strlen(proto);
	char buffer[1024];
	int i;

	if (Q_strncasecmp(proto, url, protoLength))
		return qfalse;

	Q_strncpyz(buffer, url, sizeof(buffer));
	buf = buffer;

	buf += protoLength;
	i = 0;
	for (s = server; *buf != '\0' && *buf != ':' && *buf != '/';) {
		if (i >= serverLength - 1)
			return qfalse;
		i++;
		*s++ = *buf++;
	}
	*s = '\0';

	if (*buf == ':') {
		buf++;
		if (sscanf(buf, "%d", port) != 1)
			return qfalse;

		for (buf++; *buf != '\0' && *buf != '/'; buf++) {
		}
	} else {
		*port = 80;
	}

	Q_strncpyz(path, buf, pathLength);

	return qtrue;
}

/**
 * @brief libcurl callback to update header info.
 */
size_t HTTP_Header (void *ptr, size_t size, size_t nmemb, void *stream)
{
	char headerBuff[1024];
	size_t bytes;
	size_t len;

	bytes = size * nmemb;

	if (bytes <= 16)
		return bytes;

	if (bytes < sizeof(headerBuff))
		len = bytes + 1;
	else
		len = sizeof(headerBuff);

	Q_strncpyz(headerBuff, (const char *)ptr, len);

	if (!Q_strncasecmp(headerBuff, "Content-Length: ", 16)) {
		dlhandle_t *dl;

		dl = (dlhandle_t *)stream;

		if (dl->file)
			dl->fileSize = strtoul(headerBuff + 16, NULL, 10);
	}

	return bytes;
}

/**
 * @brief libcurl callback for HTTP_GetURL
 */
size_t HTTP_Recv (void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t bytes;
	dlhandle_t *dl;

	dl = (dlhandle_t *)stream;

	bytes = size * nmemb;

	if (!dl->fileSize) {
		dl->fileSize = bytes > 131072 ? bytes : 131072;
		dl->tempBuffer = (char *)Mem_Alloc((int)dl->fileSize);
	} else if (dl->position + bytes >= dl->fileSize - 1) {
		char *tmp;

		tmp = dl->tempBuffer;

		dl->tempBuffer = (char *)Mem_Alloc((int)(dl->fileSize * 2));
		memcpy(dl->tempBuffer, tmp, dl->fileSize);
		Mem_Free(tmp);
		dl->fileSize *= 2;
	}

	memcpy(dl->tempBuffer + dl->position, ptr, bytes);
	dl->position += bytes;
	dl->tempBuffer[dl->position] = 0;

	return bytes;
}

/**
 * @brief Converts the hostname into an ip to work around a bug in libcurl (resp. the resolver) that
 * uses alarm for timeouts (this is in conflict with our signal handlers and longjmp environment)
 * @param[in] url The url to convert
 * @param[out] buf The resolved url or empty if an error occurred
 * @param[in] size The size of the target buffer
 */
static void HTTP_ResolvURL (const char *url, char *buf, size_t size)
{
	char server[512];
	char ipServer[MAX_VAR];
	int port;
	char uriPath[512];

	buf[0] = '\0';

	if (!HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port))
		Com_Error(ERR_DROP, "invalid url given: %s", url);

	NET_ResolvNode(server, ipServer, sizeof(ipServer));
	if (ipServer[0] != '\0')
		Com_sprintf(buf, size, "http://%s:%i%s", ipServer, port, uriPath);
}

/**
 * @brief Gets a specific url
 * @note Make sure, that you free the string that is returned by this function
 */
static char* HTTP_GetURLInternal (const char *url)
{
	dlhandle_t dl;
	char buf[576];

	OBJZERO(dl);

	HTTP_ResolvURL(url, buf, sizeof(buf));
	if (buf[0] == '\0')
		return NULL;
	Q_strncpyz(dl.URL, url, sizeof(dl.URL));

	dl.curl = curl_easy_init();
	curl_easy_setopt(dl.curl, CURLOPT_CONNECTTIMEOUT, http_timeout->integer);
	curl_easy_setopt(dl.curl, CURLOPT_TIMEOUT, http_timeout->integer);
	curl_easy_setopt(dl.curl, CURLOPT_ENCODING, "");
	curl_easy_setopt(dl.curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(dl.curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(dl.curl, CURLOPT_WRITEDATA, &dl);
	curl_easy_setopt(dl.curl, CURLOPT_WRITEFUNCTION, HTTP_Recv);
	curl_easy_setopt(dl.curl, CURLOPT_PROXY, http_proxy->string);
	curl_easy_setopt(dl.curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(dl.curl, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(dl.curl, CURLOPT_WRITEHEADER, &dl);
	curl_easy_setopt(dl.curl, CURLOPT_HEADERFUNCTION, HTTP_Header);
	curl_easy_setopt(dl.curl, CURLOPT_USERAGENT, GAME_TITLE" "UFO_VERSION);
	curl_easy_setopt(dl.curl, CURLOPT_URL, dl.URL);
	curl_easy_setopt(dl.curl, CURLOPT_NOSIGNAL, 1);

	/* get it */
	curl_easy_perform(dl.curl);

	/* clean up */
	curl_easy_cleanup(dl.curl);

	return dl.tempBuffer;
}

void HTTP_PutFile (const char *formName, const char *fileName, const char *url, const upparam_t *params)
{
	CURL *curl;
	struct curl_httppost* post = NULL;
	struct curl_httppost* last = NULL;
	char buf[576];

	HTTP_ResolvURL(url, buf, sizeof(buf));
	if (buf[0] == '\0')
		return;

	curl = curl_easy_init();

	while (params) {
		curl_formadd(&post, &last, CURLFORM_PTRNAME, params->name, CURLFORM_PTRCONTENTS, params->value, CURLFORM_END);
		params = params->next;
	}

	curl_formadd(&post, &last, CURLFORM_PTRNAME, formName, CURLFORM_FILE, fileName, CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, http_timeout->integer);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, http_timeout->integer);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, GAME_TITLE" "UFO_VERSION);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_perform(curl);

	curl_easy_cleanup(curl);
}

void HTTP_GetURL (const char *url, http_callback_t callback)
{
	char *response = HTTP_GetURLInternal(url);
	if (callback != NULL)
		callback(response);
	Mem_Free(response);
}

/**
 * @brief UFO is exiting or we're changing servers. Clean up.
 */
void HTTP_Cleanup (void)
{
	curl_global_cleanup();
}
