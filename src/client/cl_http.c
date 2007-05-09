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

#ifdef HAVE_CURL
#include "client.h"

cvar_t	*cl_http_downloads;
cvar_t	*cl_http_filelists;
cvar_t	*cl_http_proxy;
cvar_t	*cl_http_max_connections;

enum {
	HTTPDL_ABORT_NONE,
	HTTPDL_ABORT_SOFT,
	HTTPDL_ABORT_HARD
};

static CURLM	*multi = NULL;
static int		handleCount = 0;
static int		pendingCount = 0;
static int		abortDownloads = HTTPDL_ABORT_NONE;
static qboolean	downloading_pak = qfalse;
static qboolean	httpDown = qfalse;

void StripHighBits (char *string)
{
	byte c;
	char *p;

	p = string;

	while (string[0]) {
		c = *(string++);

		if (c >= 32 && c <= 127)
			*p++ = c;
	}

	p[0] = '\0';
}

qboolean isvalidchar (int c)
{
	if (!isalnum(c) && c != '_' && c != '-')
		return qfalse;
	return qtrue;
}

/*
===============================
R1Q2 HTTP Downloading Functions
===============================
HTTP downloading is used if the server provides a content
server URL in the connect message. Any missing content the
client needs will then use the HTTP server instead of auto
downloading via UDP. CURL is used to enable multiple files
to be downloaded in parallel to improve performance on high
latency links when small files such as textures are needed.
Since CURL natively supports gzip content encoding, any files
on the HTTP server should ideally be gzipped to conserve
bandwidth.
*/

/**
 * @brief libcurl callback to update progress info. Mainly just used as
 * a way to cancel the transfer if required.
 */
static int CL_HTTP_Progress (void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	dlhandle_t *dl;

	dl = (dlhandle_t *)clientp;

	dl->position = (unsigned)dlnow;

	/* don't care which download shows as long as something does :) */
	if (!abortDownloads) {
		strcpy(cls.downloadname, dl->queueEntry->ufoPath);
		cls.downloadposition = dl->position;

		if (dltotal)
			cls.downloadpercent = (int)((dlnow / dltotal) * 100.0f);
		else
			cls.downloadpercent = 0;
	}

	return abortDownloads;
}

/**
 * @brief libcurl callback to update header info.
 */
static size_t CL_HTTP_Header (void *ptr, size_t size, size_t nmemb, void *stream)
{
	char	headerBuff[1024];
	size_t	bytes;
	size_t	len;

	bytes = size * nmemb;

	if (bytes <= 16)
		return bytes;

	/* memset (headerBuff, 0, sizeof(headerBuff)); */
	/* memcpy (headerBuff, ptr, min(bytes, sizeof(headerBuff)-1)); */
	if (bytes < sizeof(headerBuff)-1)
		len = bytes;
	else
		len = sizeof(headerBuff)-1;

	Q_strncpyz(headerBuff, ptr, len);

	if (!Q_strncasecmp(headerBuff, "Content-Length: ", 16)) {
		dlhandle_t	*dl;

		dl = (dlhandle_t *)stream;

		if (dl->file)
			dl->fileSize = strtoul(headerBuff + 16, NULL, 10);
	}

	return bytes;
}

/**
 * @brief Properly escapes a path with HTTP %encoding. libcurl's function
 * seems to treat '/' and such as illegal chars and encodes almost
 * the entire URL...
 */
static void CL_EscapeHTTPPath (const char *filePath, char *escaped)
{
	int		i;
	size_t	len;
	char	*p;

	p = escaped;

	len = strlen (filePath);
	for (i = 0; i < len; i++) {
		if (!isalnum(filePath[i]) && filePath[i] != ';' && filePath[i] != '/' &&
			filePath[i] != '?' && filePath[i] != ':' && filePath[i] != '@' && filePath[i] != '&' &&
			filePath[i] != '=' && filePath[i] != '+' && filePath[i] != '$' && filePath[i] != ',' &&
			filePath[i] != '[' && filePath[i] != ']' && filePath[i] != '-' && filePath[i] != '_' &&
			filePath[i] != '.' && filePath[i] != '!' && filePath[i] != '~' && filePath[i] != '*' &&
			filePath[i] != '\'' && filePath[i] != '(' && filePath[i] != ')') {
			sprintf(p, "%%%02x", filePath[i]);
			p += 3;
		} else {
			*p = filePath[i];
			p++;
		}
	}
	p[0] = 0;

	/* using ./ in a url is legal, but all browsers condense the path and some IDS / request */
	/* filtering systems act a bit funky if http requests come in with uncondensed paths. */
	len = strlen(escaped);
	p = escaped;
	while ((p = strstr (p, "./"))) {
		memmove(p, p + 2, len - (p - escaped) - 1);
		len -= 2;
	}
}

/**
 * @brief libcurl callback for filelists.
 */
static size_t CL_HTTP_Recv (void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t		bytes;
	dlhandle_t	*dl;

	dl = (dlhandle_t *)stream;

	bytes = size * nmemb;

	if (!dl->fileSize) {
		dl->fileSize = bytes > 131072 ? bytes : 131072;
		dl->tempBuffer = Mem_Alloc((int)dl->fileSize);
	} else if (dl->position + bytes >= dl->fileSize - 1) {
		char		*tmp;

		tmp = dl->tempBuffer;

		dl->tempBuffer = Mem_Alloc((int)(dl->fileSize*2));
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
 * @brief
 */
int CL_CURL_Debug (CURL *c, curl_infotype type, char *data, size_t size, void * ptr)
{
	if (type == CURLINFO_TEXT) {
		char buff[4096];
		if (size > sizeof(buff)-1)
			size = sizeof(buff)-1;
		Q_strncpyz(buff, data, size);
		Com_Printf("DEBUG: %s\n", buff);
	}

	return 0;
}

/**
 * @brief Actually starts a download by adding it to the curl multi handle.
 */
static void CL_StartHTTPDownload (dlqueue_t *entry, dlhandle_t *dl)
{
	size_t		len;
	char		tempFile[MAX_OSPATH];
	char		escapedFilePath[MAX_QPATH*4];

	/* yet another hack to accomodate filelists, how i wish i could push :(
	 * NULL file handle indicates filelist. */
	len = strlen(entry->ufoPath);
	if (len > 9 && !strcmp(entry->ufoPath + len - 9, ".filelist")) {
		dl->file = NULL;
		CL_EscapeHTTPPath(entry->ufoPath, escapedFilePath);
	} else {
		Com_sprintf(dl->filePath, sizeof(dl->filePath), "%s/%s", FS_Gamedir(), entry->ufoPath);

		Com_sprintf(tempFile, sizeof(tempFile), "%s/%s", cl.gamedir, entry->ufoPath);
		CL_EscapeHTTPPath(tempFile, escapedFilePath);

		strcat(dl->filePath, ".tmp");

		FS_CreatePath(dl->filePath);

		/* don't bother with http resume... too annoying if server doesn't support it. */
		dl->file = fopen(dl->filePath, "wb");
		if (!dl->file) {
			Com_Printf("CL_StartHTTPDownload: Couldn't open %s for writing.\n", dl->filePath);
			entry->state = DLQ_STATE_DONE;
			/* CL_RemoveHTTPDownload(entry->ufoPath); */
			return;
		}
	}

	dl->tempBuffer = NULL;
	dl->speed = 0;
	dl->fileSize = 0;
	dl->position = 0;
	dl->queueEntry = entry;

	if (!dl->curl)
		dl->curl = curl_easy_init();

	Com_sprintf(dl->URL, sizeof(dl->URL), "%s%s", cls.downloadServer, escapedFilePath);

	curl_easy_setopt(dl->curl, CURLOPT_ENCODING, "");
	/* curl_easy_setopt(dl->curl, CURLOPT_DEBUGFUNCTION, CL_CURL_Debug); */
	/* curl_easy_setopt(dl->curl, CURLOPT_VERBOSE, 1); */
	curl_easy_setopt(dl->curl, CURLOPT_NOPROGRESS, 0);
	if (dl->file) {
		curl_easy_setopt(dl->curl, CURLOPT_WRITEDATA, dl->file);
		curl_easy_setopt(dl->curl, CURLOPT_WRITEFUNCTION, NULL);
	} else {
		curl_easy_setopt(dl->curl, CURLOPT_WRITEDATA, dl);
		curl_easy_setopt(dl->curl, CURLOPT_WRITEFUNCTION, CL_HTTP_Recv);
	}
	curl_easy_setopt(dl->curl, CURLOPT_PROXY, cl_http_proxy->string);
	curl_easy_setopt(dl->curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(dl->curl, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(dl->curl, CURLOPT_WRITEHEADER, dl);
	curl_easy_setopt(dl->curl, CURLOPT_HEADERFUNCTION, CL_HTTP_Header);
	curl_easy_setopt(dl->curl, CURLOPT_PROGRESSFUNCTION, CL_HTTP_Progress);
	curl_easy_setopt(dl->curl, CURLOPT_PROGRESSDATA, dl);
	curl_easy_setopt(dl->curl, CURLOPT_USERAGENT, Cvar_VariableString ("version"));
	curl_easy_setopt(dl->curl, CURLOPT_REFERER, cls.downloadReferer);
	curl_easy_setopt(dl->curl, CURLOPT_URL, dl->URL);

	if (curl_multi_add_handle(multi, dl->curl) != CURLM_OK) {
		Com_Printf("curl_multi_add_handle: error\n");
		dl->queueEntry->state = DLQ_STATE_DONE;
		return;
	}

	handleCount++;
	/*Com_Printf("started dl: hc = %d\n", handleCount); */
	Com_Printf("CL_StartHTTPDownload: Fetching %s...\n", dl->URL);
	dl->queueEntry->state = DLQ_STATE_RUNNING;
}

/**
 * @brief Init libcurl and multi handle.
 */
void CL_InitHTTPDownloads (void)
{
	curl_global_init(CURL_GLOBAL_NOTHING);
	Com_Printf("%s initialized.\n", curl_version());
}

/**
 * @brief A new server is specified, so we nuke all our state.
 */
void CL_SetHTTPServer (const char *URL)
{
	dlqueue_t *q, *last;

	CL_HTTP_Cleanup(qfalse);

	q = &cls.downloadQueue;

	last = NULL;

	while (q->next) {
		q = q->next;

		if (last)
			Mem_Free(last);

		last = q;
	}

	if (last)
		Mem_Free(last);

	if (multi)
		Com_Error(ERR_DROP, "CL_SetHTTPServer: Still have old handle");

	multi = curl_multi_init ();

	memset(&cls.downloadQueue, 0, sizeof(cls.downloadQueue));

	abortDownloads = HTTPDL_ABORT_NONE;
	handleCount = pendingCount = 0;

	strncpy(cls.downloadServer, URL, sizeof(cls.downloadServer) - 1);
}

void CL_ResetPrecacheCheck(void);

/**
 * @brief Cancel all downloads and nuke the queue.
 */
void CL_CancelHTTPDownloads (qboolean permKill)
{
	dlqueue_t	*q;

	if (permKill)
		abortDownloads = HTTPDL_ABORT_HARD;
	else
		abortDownloads = HTTPDL_ABORT_SOFT;

	q = &cls.downloadQueue;

	while (q->next) {
		q = q->next;
		if (q->state == DLQ_STATE_NOT_STARTED)
			q->state = DLQ_STATE_DONE;
	}

	if (!pendingCount && !handleCount && abortDownloads == HTTPDL_ABORT_HARD)
		cls.downloadServer[0] = 0;

	pendingCount = 0;
}

/**
 * @brief Called from the precache check to queue a download. Return value of
 *qfalse will cause standard UDP downloading to be used instead.
 */
qboolean CL_QueueHTTPDownload (const char *ufoPath)
{
	size_t		len;
	dlqueue_t	*q;
	qboolean	needList;

	/* no http server (or we got booted) */
	if (!cls.downloadServer[0] || abortDownloads || !cl_http_downloads->integer)
		return qfalse;

	needList = qfalse;

	/*  first download queued, so we want the mod filelist */
	if (!cls.downloadQueue.next && cl_http_filelists->integer)
		needList = qtrue;

	q = &cls.downloadQueue;

	while (q->next) {
		q = q->next;

		/* avoid sending duplicate requests */
		if (!strcmp(ufoPath, q->ufoPath))
			return qtrue;
	}

	q->next = Mem_Alloc(sizeof(*q));
	q = q->next;

	q->next = NULL;
	q->state = DLQ_STATE_NOT_STARTED;
	Q_strncpyz(q->ufoPath, ufoPath, sizeof(q->ufoPath)-1);

	if (needList) {
		/* grab the filelist */
		CL_QueueHTTPDownload(va("%s.filelist", cl.gamedir));
	}

	/* special case for map file lists, i really wanted a server-push mechanism for this, but oh well */
	len = strlen(ufoPath);
	if (cl_http_filelists->integer && len > 4 && !Q_stricmp (ufoPath + len - 4, ".bsp")) {
		char listPath[MAX_OSPATH];
		char filePath[MAX_OSPATH];

		Com_sprintf(filePath, sizeof(filePath), "%s/%s", cl.gamedir, ufoPath);

		COM_StripExtension(filePath, listPath);
		strcat(listPath, ".filelist");

		CL_QueueHTTPDownload(listPath);
	}

	/* if a download entry has made it this far, CL_FinishHTTPDownload is guaranteed to be called. */
	pendingCount++;

	return qtrue;
}

/**
 * @brief See if we're still busy with some downloads. Called by precacher just
 * before it loads the map since we could be downloading the map. If we're
 * busy still, it'll wait and CL_FinishHTTPDownload will pick up from where
 * it left.
 */
qboolean CL_PendingHTTPDownloads (void)
{
	dlqueue_t	*q;

	if (!cls.downloadServer[0])
		return qfalse;

	return pendingCount + handleCount;

	q = &cls.downloadQueue;

	while (q->next) {
		q = q->next;
		if (q->state != DLQ_STATE_DONE)
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Validate a path supplied by a filelist.
 */
static void CL_CheckAndQueueDownload (char *path)
{
	size_t		length;
	char		*ext;
	qboolean	pak;
	qboolean	gameLocal;

	StripHighBits(path);

	length = strlen(path);

	if (length >= MAX_QPATH)
		return;

	ext = strrchr(path, '.');

	if (!ext)
		return;

	ext++;

	if (!ext[0])
		return;

	Q_strlwr(ext);

	if (!strcmp(ext, "pk3")) {
		Com_Printf("NOTICE: Filelist is requesting a .pak file (%s)\n", path);
		pak = qtrue;
	} else
		pak = qfalse;

	if (!pak && strcmp(ext, "bsp") && strcmp(ext, "wav") && strcmp(ext, "md2") &&
		strcmp(ext, "md3") && strcmp(ext, "tga") && strcmp(ext, "png") && strcmp(ext, "jpg")) {
		Com_Printf("WARNING: Illegal file type '%s' in filelist.\n", path);
		return;
	}

	if (path[0] == '@') {
		if (pak) {
			Com_Printf("WARNING: @ prefix used on a pak file (%s) in filelist.\n", path);
			return;
		}
		gameLocal = qtrue;
		path++;
		length--;
	} else
		gameLocal = qfalse;

	if (strstr(path, "..") || !isvalidchar(path[0]) || !isvalidchar(path[length-1]) || strstr(path, "//") ||
		strchr(path, '\\') || (!pak && !strchr(path, '/')) || (pak && strchr(path, '/'))) {
		Com_Printf("WARNING: Illegal path '%s' in filelist.\n", path);
		return;
	}

	/* by definition paks are game-local */
	if (gameLocal || pak) {
		qboolean exists;
		FILE *f;
		char gamePath[MAX_OSPATH];

		if (pak) {
			Com_sprintf(gamePath, sizeof(gamePath),"%s/%s", FS_Gamedir(), path);
			f = fopen(gamePath, "rb");
			if (!f)
				exists = qfalse;
			else {
				exists = qtrue;
				fclose(f);
			}
		} else
			exists = FS_CheckFile(path);

		if (!exists) {
			if (CL_QueueHTTPDownload(path)) {
				/* paks get bumped to the top and HTTP switches to single downloading.
				 * this prevents someone on 28k dialup trying to do both the main .pak
				 * and referenced configstrings data at once. */
				if (pak) {
					dlqueue_t *q, *last;

					last = q = &cls.downloadQueue;

					while (q->next) {
						last = q;
						q = q->next;
					}

					last->next = NULL;
					q->next = cls.downloadQueue.next;
					cls.downloadQueue.next = q;
				}
			}
		}
	} else
		CL_CheckOrDownloadFile (path);
}

/**
 * @brief A filelist is in memory, scan and validate it and queue up the files.
 */
static void CL_ParseFileList (dlhandle_t *dl)
{
	char	 *list;
	char	*p;

	if (!cl_http_filelists->integer)
		return;

	list = dl->tempBuffer;

	for (;;) {
		p = strchr (list, '\n');
		if (p) {
			p[0] = 0;
			if (list[0])
				CL_CheckAndQueueDownload(list);
			list = p + 1;
		} else {
			if (list[0])
				CL_CheckAndQueueDownload(list);
			break;
		}
	}

	Mem_Free(dl->tempBuffer);
	dl->tempBuffer = NULL;
}

/**
 * @brief A pak file just downloaded, let's see if we can remove some stuff from
 * the queue which is in the .pak.
 */
static void CL_ReVerifyHTTPQueue (void)
{
	dlqueue_t	*q;

	q = &cls.downloadQueue;

	pendingCount = 0;

	while (q->next) {
		q = q->next;
		if (q->state == DLQ_STATE_NOT_STARTED) {
			if (FS_LoadFile (q->ufoPath, NULL) != -1)
				q->state = DLQ_STATE_DONE;
			else
				pendingCount++;
		}
	}
}

/**
 * @brief UFO is exiting or we're changing servers. Clean up.
 */
void CL_HTTP_Cleanup (qboolean fullShutdown)
{
	dlhandle_t *dl;
	int i;

	if (fullShutdown && httpDown)
		return;

	for (i = 0; i < 4; i++) {
		dl = &cls.HTTPHandles[i];

		if (dl->file) {
			fclose(dl->file);
			remove(dl->filePath);
			dl->file = NULL;
		}

		if (dl->tempBuffer) {
			Mem_Free(dl->tempBuffer);
			dl->tempBuffer = NULL;
		}

		if (dl->curl) {
			if (multi)
				curl_multi_remove_handle(multi, dl->curl);
			curl_easy_cleanup(dl->curl);
			dl->curl = NULL;
		}
	}

	if (multi) {
		curl_multi_cleanup(multi);
		multi = NULL;
	}

	if (fullShutdown) {
		curl_global_cleanup();
		httpDown = qtrue;
	}
}

/**
 * @brief A download finished, find out what it was, whether there were any errors and
 * if so, how severe. If none, rename file and other such stuff.
 */
static void CL_FinishHTTPDownload (void)
{
	size_t		i;
	int			msgs_in_queue;
	CURLMsg		*msg;
	CURLcode	result;
	dlhandle_t	*dl;
	CURL		*curl;
	long		responseCode;
	double		timeTaken;
	double		fileSize;
	char		tempName[MAX_OSPATH];
	qboolean	isFile;

	do {
		msg = curl_multi_info_read (multi, &msgs_in_queue);

		if (!msg) {
			Com_Printf("CL_FinishHTTPDownload: Odd, no message for us...\n");
			return;
		}

		if (msg->msg != CURLMSG_DONE) {
			Com_Printf("CL_FinishHTTPDownload: Got some weird message...\n");
			continue;
		}

		curl = msg->easy_handle;

		/* curl doesn't provide reverse-lookup of the void * ptr, so search for it */
		for (i = 0; i < 4; i++) {
			if (cls.HTTPHandles[i].curl == curl) {
				dl = &cls.HTTPHandles[i];
				break;
			}
		}

		if (i == 4)
			Com_Error(ERR_DROP, "CL_FinishHTTPDownload: Handle not found");

		/* we mark everything as done even if it errored to prevent multiple attempts. */
		dl->queueEntry->state = DLQ_STATE_DONE;

		/* filelist processing is done on read */
		if (dl->file)
			isFile = qtrue;
		else
			isFile = qfalse;

		if (isFile) {
			fclose(dl->file);
			dl->file = NULL;
		}

		/* might be aborted */
		if (pendingCount)
			pendingCount--;
		handleCount--;
		/* Com_Printf("finished dl: hc = %d\n", handleCount); */
		cls.downloadname[0] = 0;
		cls.downloadposition = 0;

		result = msg->data.result;

		switch (result) {
			/* for some reason curl returns CURLE_OK for a 404... */
			case CURLE_HTTP_RETURNED_ERROR:
			case CURLE_OK:

				curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &responseCode);
				if (responseCode == 404) {
					i = strlen (dl->queueEntry->ufoPath);
					if (!strcmp (dl->queueEntry->ufoPath + i - 4, ".pk3"))
						downloading_pak = qfalse;

					if (isFile)
						remove (dl->filePath);
					Com_Printf("HTTP(%s): 404 File Not Found [%d remaining files]\n", dl->queueEntry->ufoPath, pendingCount);
					curl_easy_getinfo (curl, CURLINFO_SIZE_DOWNLOAD, &fileSize);
					if (fileSize > 512) {
						/* ick */
						isFile = qfalse;
						result = CURLE_FILESIZE_EXCEEDED;
						Com_Printf("Oversized 404 body received (%d bytes), aborting HTTP downloading.\n", (int)fileSize);
					} else {
						curl_multi_remove_handle(multi, dl->curl);
						continue;
					}
				} else if (responseCode == 200) {
					if (!isFile && !abortDownloads)
						CL_ParseFileList(dl);
					break;
				}

				/* every other code is treated as fatal, fallthrough here */

			/* fatal error, disable http */
			case CURLE_COULDNT_RESOLVE_HOST:
			case CURLE_COULDNT_CONNECT:
			case CURLE_COULDNT_RESOLVE_PROXY:
				if (isFile)
					remove (dl->filePath);
				Com_Printf("Fatal HTTP error: %s\n", curl_easy_strerror(result));
				curl_multi_remove_handle(multi, dl->curl);
				if (abortDownloads)
					continue;
				CL_CancelHTTPDownloads(qtrue);
				continue;
			default:
				i = strlen (dl->queueEntry->ufoPath);
				if (!strcmp(dl->queueEntry->ufoPath + i - 4, ".pk3"))
					downloading_pak = qfalse;
				if (isFile)
					remove(dl->filePath);
				Com_Printf("HTTP download failed: %s\n", curl_easy_strerror(result));
				curl_multi_remove_handle(multi, dl->curl);
				continue;
		}

		if (isFile) {
			/* rename the temp file */
			Com_sprintf(tempName, sizeof(tempName), "%s/%s", FS_Gamedir(), dl->queueEntry->ufoPath);

			if (!FS_Rename(dl->filePath, tempName))
				Com_Printf("Failed to rename %s for some odd reason...", dl->filePath);

			/* /a pak file is very special... */
			i = strlen (tempName);
			if (!strcmp(tempName + i - 4, ".pk3")) {
				FS_RestartFilesystem();
				CL_ReVerifyHTTPQueue();
				downloading_pak = qfalse;
			}
		}

		/* show some stats */
		curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &timeTaken);
		curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &fileSize);

		/* FIXME:
		 * technically i shouldn't need to do this as curl will auto reuse the
		 * existing handle when you change the URL. however, the handleCount goes
		 * all weird when reusing a download slot in this way. if you can figure
		 * out why, please let me know. */
		curl_multi_remove_handle(multi, dl->curl);

		Com_Printf("HTTP(%s): %.f bytes, %.2fkB/sec [%d remaining files]\n", dl->queueEntry->ufoPath, fileSize, (fileSize / 1024.0) / timeTaken, pendingCount);
	} while (msgs_in_queue > 0);

	if (handleCount == 0) {
		if (abortDownloads == HTTPDL_ABORT_SOFT)
			abortDownloads = HTTPDL_ABORT_NONE;
		else if (abortDownloads == HTTPDL_ABORT_HARD)
			cls.downloadServer[0] = 0;
	}

	/* done current batch, see if we have more to dl - maybe a .bsp needs downloaded */
	if (cls.state == ca_connected && !CL_PendingHTTPDownloads())
		CL_RequestNextDownload();
}

/**
 * @brief Find a free download handle to start another queue entry on.
 */
static dlhandle_t *CL_GetFreeDLHandle (void)
{
	dlhandle_t	*dl;
	int			i;

	for (i = 0; i < 4; i++) {
		dl = &cls.HTTPHandles[i];
		if (!dl->queueEntry || dl->queueEntry->state == DLQ_STATE_DONE)
			return dl;
	}

	return NULL;
}

/**
 * @brief Start another HTTP download if possible.
 */
static void CL_StartNextHTTPDownload (void)
{
	dlqueue_t	*q;

	q = &cls.downloadQueue;

	while (q->next) {
		q = q->next;
		if (q->state == DLQ_STATE_NOT_STARTED) {
			size_t len;

			dlhandle_t *dl;

			dl = CL_GetFreeDLHandle();

			if (!dl)
				return;

			CL_StartHTTPDownload(q, dl);

			/* ugly hack for pak file single downloading */
			len = strlen (q->ufoPath);
			if (len > 4 && !Q_stricmp(q->ufoPath + len - 4, ".pk3"))
				downloading_pak = qtrue;

			break;
		}
	}
}

/**
 * @brief This calls curl_multi_perform do actually do stuff. Called every frame while
 * connecting to minimise latency. Also starts new downloads if we're not doing
 * the maximum already.
 */
void CL_RunHTTPDownloads (void)
{
	int			newHandleCount;
	CURLMcode	ret;

	if (!cls.downloadServer[0])
		return;

	/* Com_Printf("handle %d, pending %d\n", handleCount, pendingCount); */

	/* not enough downloads running, queue some more! */
	if (pendingCount && abortDownloads == HTTPDL_ABORT_NONE &&
		!downloading_pak && handleCount < cl_http_max_connections->integer)
		CL_StartNextHTTPDownload();

	do {
		ret = curl_multi_perform (multi, &newHandleCount);
		if (newHandleCount < handleCount) {
			/* Com_Printf("runnd dl: hc = %d, nc = %d\n", handleCount, newHandleCount); */
			/* hmm, something either finished or errored out. */
			CL_FinishHTTPDownload();
			handleCount = newHandleCount;
		}
	} while (ret == CURLM_CALL_MULTI_PERFORM);

	if (ret != CURLM_OK) {
		Com_Printf("curl_multi_perform error. Aborting HTTP downloads.\n");
		CL_CancelHTTPDownloads(qtrue);
	}

	/* not enough downloads running, queue some more! */
	if (pendingCount && abortDownloads == HTTPDL_ABORT_NONE &&
		!downloading_pak && handleCount < cl_http_max_connections->integer)
		CL_StartNextHTTPDownload();
}

#endif /* HAVE_CURL */
