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
 * @brief Gets a specific url and store into
 * @note Make sure, that you free the strings that is returned by this function
 */
char* HTTP_GetURL (const char *url)
{
	static qboolean downloading = qfalse;
	dlhandle_t dl;

	if (downloading) {
		Com_Printf("Warning: There is still another download running (can't fetch %s) '%s'\n",
				url, dl.URL);
		return NULL;
	}

	OBJZERO(dl);

	downloading = qtrue;
	dl.curl = curl_easy_init();

	Q_strncpyz(dl.URL, url, sizeof(dl.URL));
	curl_easy_setopt(dl.curl, CURLOPT_CONNECTTIMEOUT, http_timeout->integer);
	curl_easy_setopt(dl.curl, CURLOPT_ENCODING, "");
	curl_easy_setopt(dl.curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(dl.curl, CURLOPT_WRITEDATA, &dl);
	curl_easy_setopt(dl.curl, CURLOPT_WRITEFUNCTION, HTTP_Recv);
	curl_easy_setopt(dl.curl, CURLOPT_PROXY, http_proxy->string);
	curl_easy_setopt(dl.curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(dl.curl, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(dl.curl, CURLOPT_WRITEHEADER, &dl);
	curl_easy_setopt(dl.curl, CURLOPT_HEADERFUNCTION, HTTP_Header);
	curl_easy_setopt(dl.curl, CURLOPT_USERAGENT, Cvar_GetString("version"));
	curl_easy_setopt(dl.curl, CURLOPT_URL, dl.URL);

	/* get it */
	curl_easy_perform(dl.curl);

	/* clean up */
	curl_easy_cleanup(dl.curl);

	downloading = qfalse;

	return dl.tempBuffer;
}

void HTTP_PutFile (const char *formName, const char *fileName, const char *url)
{
	CURL *curl;
	struct curl_httppost* post = NULL;
	struct curl_httppost* last = NULL;

	curl = curl_easy_init();

	curl_formadd(&post, &last, CURLFORM_PTRNAME, formName, CURLFORM_FILE, fileName, CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, GAME_TITLE" "UFO_VERSION);
	curl_easy_setopt(curl, CURLOPT_URL, url);

	curl_easy_perform(curl);

	curl_easy_cleanup(curl);
}

/**
 * @brief UFO is exiting or we're changing servers. Clean up.
 */
void HTTP_Cleanup (void)
{
	curl_global_cleanup();
}
