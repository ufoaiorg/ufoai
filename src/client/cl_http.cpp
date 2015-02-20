/**
 * @file
 * @author R1CH
 * @brief HTTP downloading is used if the server provides a content server URL in the
 * connect message. Any missing content the client needs will then use the
 * HTTP server. CURL is used to enable multiple files to be downloaded in parallel
 * to improve performance on high latency links when small files such as textures
 * are needed. Since CURL natively supports gzip content encoding, any files
 * on the HTTP server should ideally be gzipped to conserve bandwidth.
 * @sa CL_ConnectionlessPacket
 * @sa SVC_DirectConnect
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

#include "client.h"
#include "cl_http.h"
#include "battlescape/cl_parse.h"

#ifndef NO_HTTP

static cvar_t* cl_http_downloads;
static cvar_t* cl_http_filelists;
static cvar_t* cl_http_max_connections;

enum {
	HTTPDL_ABORT_NONE,
	HTTPDL_ABORT_SOFT,
	HTTPDL_ABORT_HARD
};

static CURLM	*multi = nullptr;
static int		handleCount = 0;
static int		pendingCount = 0;
static int		abortDownloads = HTTPDL_ABORT_NONE;
static bool	downloadingPK3 = false;

static void StripHighBits (char* string)
{
	char* p = string;

	while (string[0]) {
		const unsigned char c = *(string++);

		if (c >= 32 && c <= 127)
			*p++ = c;
	}

	p[0] = '\0';
}

static inline bool isvalidchar (int c)
{
	if (!isalnum(c) && c != '_' && c != '-')
		return false;
	return true;
}

/**
 * @brief libcurl callback to update progress info. Mainly just used as
 * a way to cancel the transfer if required.
 */
static int CL_HTTP_Progress (void* clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	dlhandle_t* dl;

	dl = (dlhandle_t*)clientp;

	dl->position = (unsigned)dlnow;

	/* don't care which download shows as long as something does :) */
	if (!abortDownloads) {
		Q_strncpyz(cls.downloadName, dl->queueEntry->ufoPath, sizeof(cls.downloadName));
		cls.downloadPosition = dl->position;

		if (dltotal > 0.0)
			cls.downloadPercent = (int)((dlnow / dltotal) * 100.0f);
		else
			cls.downloadPercent = 0;
	}

	return abortDownloads;
}

/**
 * @brief Properly escapes a path with HTTP %encoding. libcurl's function
 * seems to treat '/' and such as illegal chars and encodes almost
 * the entire URL...
 */
static void CL_EscapeHTTPPath (const char* filePath, char* escaped)
{
	char *p = escaped;

	size_t len = strlen(filePath);
	for (int i = 0; i < len; i++) {
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
 * @brief Actually starts a download by adding it to the curl multi handle.
 */
static void CL_StartHTTPDownload (dlqueue_t* entry, dlhandle_t* dl)
{
	char escapedFilePath[MAX_QPATH * 4];
	const char* extension = Com_GetExtension(entry->ufoPath);

	/* yet another hack to accomodate filelists, how i wish i could push :(
	 * nullptr file handle indicates filelist. */
	if (extension != nullptr && Q_streq(extension, "filelist")) {
		dl->file = nullptr;
		CL_EscapeHTTPPath(entry->ufoPath, escapedFilePath);
	} else {
		char tempFile[MAX_OSPATH];
		/** @todo use the FS_OpenFile function here */
		Com_sprintf(dl->filePath, sizeof(dl->filePath), "%s/%s", FS_Gamedir(), entry->ufoPath);

		Com_sprintf(tempFile, sizeof(tempFile), BASEDIRNAME "/%s", entry->ufoPath);
		CL_EscapeHTTPPath(tempFile, escapedFilePath);

		strcat(dl->filePath, ".tmp");

		FS_CreatePath(dl->filePath);

		/* don't bother with http resume... too annoying if server doesn't support it. */
		dl->file = Sys_Fopen(dl->filePath, "wb");
		if (!dl->file) {
			Com_Printf("CL_StartHTTPDownload: Couldn't open %s for writing.\n", dl->filePath);
			entry->state = DLQ_STATE_DONE;
			/* CL_RemoveHTTPDownload(entry->ufoPath); */
			return;
		}
	}

	dl->tempBuffer = nullptr;
	dl->speed = 0;
	dl->fileSize = 0;
	dl->position = 0;
	dl->queueEntry = entry;

	if (!dl->curl)
		dl->curl = curl_easy_init();

	Com_sprintf(dl->URL, sizeof(dl->URL), "%s%s", cls.downloadServer, escapedFilePath);

	curl_easy_setopt(dl->curl, CURLOPT_ENCODING, "");
#ifdef PARANOID
	curl_easy_setopt(dl->curl, CURLOPT_VERBOSE, 1);
#endif
	curl_easy_setopt(dl->curl, CURLOPT_NOPROGRESS, 0);
	if (dl->file) {
		curl_easy_setopt(dl->curl, CURLOPT_WRITEDATA, dl->file);
		curl_easy_setopt(dl->curl, CURLOPT_WRITEFUNCTION, nullptr);
	} else {
		curl_easy_setopt(dl->curl, CURLOPT_WRITEDATA, dl);
		curl_easy_setopt(dl->curl, CURLOPT_WRITEFUNCTION, HTTP_Recv);
	}
	curl_easy_setopt(dl->curl, CURLOPT_CONNECTTIMEOUT, http_timeout->integer);
	curl_easy_setopt(dl->curl, CURLOPT_TIMEOUT, http_timeout->integer);
	curl_easy_setopt(dl->curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(dl->curl, CURLOPT_PROXY, http_proxy->string);
	curl_easy_setopt(dl->curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(dl->curl, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(dl->curl, CURLOPT_WRITEHEADER, dl);
	curl_easy_setopt(dl->curl, CURLOPT_HEADERFUNCTION, HTTP_Header);
	curl_easy_setopt(dl->curl, CURLOPT_PROGRESSFUNCTION, CL_HTTP_Progress);
	curl_easy_setopt(dl->curl, CURLOPT_PROGRESSDATA, dl);
	curl_easy_setopt(dl->curl, CURLOPT_USERAGENT, GAME_TITLE " " UFO_VERSION);
	curl_easy_setopt(dl->curl, CURLOPT_REFERER, cls.downloadReferer);
	curl_easy_setopt(dl->curl, CURLOPT_URL, dl->URL);
	curl_easy_setopt(dl->curl, CURLOPT_NOSIGNAL, 1);

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
 * @brief A new server is specified, so we nuke all our state.
 */
void CL_SetHTTPServer (const char* URL)
{
	CL_HTTP_Cleanup();

	for (dlqueue_t* q = cls.downloadQueue; q;) {
		dlqueue_t* const del = q;
		q = q->next;
		Mem_Free(del);
	}
	cls.downloadQueue = 0;

	if (multi)
		Com_Error(ERR_DROP, "CL_SetHTTPServer: Still have old handle");

	multi = curl_multi_init();

	abortDownloads = HTTPDL_ABORT_NONE;
	handleCount = pendingCount = 0;

	Q_strncpyz(cls.downloadServer, URL, sizeof(cls.downloadServer));
}

/**
 * @brief Cancel all downloads and nuke the queue.
 */
void CL_CancelHTTPDownloads (bool permKill)
{
	if (permKill)
		abortDownloads = HTTPDL_ABORT_HARD;
	else
		abortDownloads = HTTPDL_ABORT_SOFT;

	for (dlqueue_t* q = cls.downloadQueue; q; q = q->next) {
		if (q->state == DLQ_STATE_NOT_STARTED)
			q->state = DLQ_STATE_DONE;
	}

	if (!pendingCount && !handleCount && abortDownloads == HTTPDL_ABORT_HARD)
		cls.downloadServer[0] = 0;

	pendingCount = 0;
}

/**
 * @brief Find a free download handle to start another queue entry on.
 */
static dlhandle_t* CL_GetFreeDLHandle (void)
{
	for (int i = 0; i < 4; i++) {
		dlhandle_t* dl = &cls.HTTPHandles[i];
		if (!dl->queueEntry || dl->queueEntry->state == DLQ_STATE_DONE)
			return dl;
	}

	return nullptr;
}

/**
 * @brief Called from the precache check to queue a download.
 * @sa CL_CheckOrDownloadFile
 */
bool CL_QueueHTTPDownload (const char* ufoPath)
{
	/* no http server (or we got booted) */
	if (!cls.downloadServer[0] || abortDownloads || !cl_http_downloads->integer)
		return false;

	dlqueue_t** anchor = &cls.downloadQueue;
	for (; *anchor; anchor = &(*anchor)->next) {
		/* avoid sending duplicate requests */
		if (Q_streq(ufoPath, (*anchor)->ufoPath))
			return true;
	}

	dlqueue_t* const q = Mem_AllocType(dlqueue_t);
	q->next = nullptr;
	q->state = DLQ_STATE_NOT_STARTED;
	Q_strncpyz(q->ufoPath, ufoPath, sizeof(q->ufoPath));
	*anchor = q;

	/* special case for map file lists */
	if (cl_http_filelists->integer) {
		const char* extension = Com_GetExtension(ufoPath);
		if (extension != nullptr && !Q_strcasecmp(extension, "bsp")) {
			char listPath[MAX_OSPATH];
			const size_t len = strlen(ufoPath);
			Com_sprintf(listPath, sizeof(listPath), BASEDIRNAME"/%.*s.filelist", (int)(len - 4), ufoPath);
			CL_QueueHTTPDownload(listPath);
		}
	}

	/* if a download entry has made it this far, CL_FinishHTTPDownload is guaranteed to be called. */
	pendingCount++;

	return true;
}

/**
 * @brief See if we're still busy with some downloads. Called by precacher just
 * before it loads the map since we could be downloading the map. If we're
 * busy still, it'll wait and CL_FinishHTTPDownload will pick up from where
 * it left.
 */
bool CL_PendingHTTPDownloads (void)
{
	if (cls.downloadServer[0] == '\0')
		return false;

	return pendingCount + handleCount;
}

/**
 * @return true if the file exists, otherwise it attempts to start a download via curl
 * @sa CL_CheckAndQueueDownload
 * @sa CL_RequestNextDownload
 */
bool CL_CheckOrDownloadFile (const char* filename)
{
	static char lastfilename[MAX_OSPATH] = "";

	if (Q_strnull(filename))
		return true;

	/* r1: don't attempt same file many times */
	if (Q_streq(filename, lastfilename))
		return true;

	Q_strncpyz(lastfilename, filename, sizeof(lastfilename));

	if (strstr(filename, "..")) {
		Com_Printf("Refusing to check a path with .. (%s)\n", filename);
		return true;
	}

	if (strchr(filename, ' ')) {
		Com_Printf("Refusing to check a path containing spaces (%s)\n", filename);
		return true;
	}

	if (strchr(filename, ':')) {
		Com_Printf("Refusing to check a path containing a colon (%s)\n", filename);
		return true;
	}

	if (filename[0] == '/') {
		Com_Printf("Refusing to check a path starting with / (%s)\n", filename);
		return true;
	}

	if (FS_LoadFile(filename, nullptr) != -1) {
		/* it exists, no need to download */
		return true;
	}

	if (CL_QueueHTTPDownload(filename))
		return false;

	return true;
}

/**
 * @brief Validate a path supplied by a filelist.
 * @param[in,out] path Pointer to file (path) to download (high bits will be stripped).
 * @sa CL_QueueHTTPDownload
 * @sa CL_ParseFileList
 */
static void CL_CheckAndQueueDownload (char* path)
{
	size_t length;
	const char* ext;
	bool pak;
	bool gameLocal;

	StripHighBits(path);

	length = strlen(path);

	if (length >= MAX_QPATH)
		return;

	ext = Com_GetExtension(path);
	if (ext == nullptr)
		return;

	if (Q_streq(ext, "pk3")) {
		Com_Printf("NOTICE: Filelist is requesting a .pk3 file (%s)\n", path);
		pak = true;
	} else
		pak = false;

	if (!pak &&
			!Q_streq(ext, "bsp") &&
			!Q_streq(ext, "wav") &&
			!Q_streq(ext, "md2") &&
			!Q_streq(ext, "ogg") &&
			!Q_streq(ext, "md3") &&
			!Q_streq(ext, "png") &&
			!Q_streq(ext, "jpg") &&
			!Q_streq(ext, "obj") &&
			!Q_streq(ext, "mat") &&
			!Q_streq(ext, "ump")) {
		Com_Printf("WARNING: Illegal file type '%s' in filelist.\n", path);
		return;
	}

	if (path[0] == '@') {
		if (pak) {
			Com_Printf("WARNING: @ prefix used on a pk3 file (%s) in filelist.\n", path);
			return;
		}
		gameLocal = true;
		path++;
		length--;
	} else
		gameLocal = false;

	if (strstr(path, "..") || !isvalidchar(path[0]) || !isvalidchar(path[length - 1]) || strstr(path, "//") ||
		strchr(path, '\\') || (!pak && !strchr(path, '/')) || (pak && strchr(path, '/'))) {
		Com_Printf("WARNING: Illegal path '%s' in filelist.\n", path);
		return;
	}

	/* by definition pk3s are game-local */
	if (gameLocal || pak) {
		bool exists;

		/* search the user homedir to find the pk3 file */
		if (pak) {
			char gamePath[MAX_OSPATH];
			FILE* f;
			Com_sprintf(gamePath, sizeof(gamePath), "%s/%s", FS_Gamedir(), path);
			f = Sys_Fopen(gamePath, "rb");
			if (!f)
				exists = false;
			else {
				exists = true;
				fclose(f);
			}
		} else
			exists = FS_CheckFile("%s", path);

		if (!exists) {
			if (CL_QueueHTTPDownload(path)) {
				/* pk3s get bumped to the top and HTTP switches to single downloading.
				 * this prevents someone on 28k dialup trying to do both the main .pk3
				 * and referenced configstrings data at once. */
				if (pak) {
					dlqueue_t** anchor = &cls.downloadQueue;
					while ((*anchor)->next) anchor = &(*anchor)->next;
					/* Remove the last element from the end of the list ... */
					dlqueue_t* const d = *anchor;
					*anchor            = 0;
					/* ... and prepend it to the list. */
					d->next            = cls.downloadQueue;
					cls.downloadQueue  = d;
				}
			}
		}
	} else
		CL_CheckOrDownloadFile(path);
}

/**
 * @brief A filelist is in memory, scan and validate it and queue up the files.
 */
static void CL_ParseFileList (dlhandle_t* dl)
{
	char* list;

	if (!cl_http_filelists->integer)
		return;

	list = dl->tempBuffer;

	for (;;) {
		char* p = strchr(list, '\n');
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
	dl->tempBuffer = nullptr;
}

/**
 * @brief A pk3 file just downloaded, let's see if we can remove some stuff from
 * the queue which is in the .pk3.
 */
static void CL_ReVerifyHTTPQueue (void)
{
	pendingCount = 0;

	for (dlqueue_t* q = cls.downloadQueue; q; q = q->next) {
		if (q->state == DLQ_STATE_NOT_STARTED) {
			if (FS_LoadFile(q->ufoPath, nullptr) != -1)
				q->state = DLQ_STATE_DONE;
			else
				pendingCount++;
		}
	}
}

/**
 * @brief UFO is exiting or we're changing servers. Clean up.
 */
void CL_HTTP_Cleanup (void)
{
	for (int i = 0; i < 4; i++) {
		dlhandle_t* dl = &cls.HTTPHandles[i];

		if (dl->file) {
			fclose(dl->file);
			remove(dl->filePath);
			dl->file = nullptr;
		}

		Mem_Free(dl->tempBuffer);
		dl->tempBuffer = nullptr;

		if (dl->curl) {
			if (multi)
				curl_multi_remove_handle(multi, dl->curl);
			curl_easy_cleanup(dl->curl);
			dl->curl = nullptr;
		}

		dl->queueEntry = nullptr;
	}

	if (multi) {
		curl_multi_cleanup(multi);
		multi = nullptr;
	}
}

/**
 * @brief A download finished, find out what it was, whether there were any errors and
 * if so, how severe. If none, rename file and other such stuff.
 */
static void CL_FinishHTTPDownload (void)
{
	int messagesInQueue, i;
	CURLcode result;
	CURL* curl;
	long responseCode;
	double timeTaken, fileSize;
	char tempName[MAX_OSPATH];
	bool isFile;

	do {
		CURLMsg* msg = curl_multi_info_read(multi, &messagesInQueue);
		dlhandle_t* dl = nullptr;

		if (!msg) {
			Com_Printf("CL_FinishHTTPDownload: Odd, no message for us...\n");
			return;
		}

		if (msg->msg != CURLMSG_DONE) {
			Com_Printf("CL_FinishHTTPDownload: Got some weird message...\n");
			continue;
		}

		curl = msg->easy_handle;

		/* curl doesn't provide reverse-lookup of the void*  ptr, so search for it */
		for (i = 0; i < 4; i++) {
			if (cls.HTTPHandles[i].curl == curl) {
				dl = &cls.HTTPHandles[i];
				break;
			}
		}

		if (!dl)
			Com_Error(ERR_DROP, "CL_FinishHTTPDownload: Handle not found");

		/* we mark everything as done even if it errored to prevent multiple attempts. */
		dl->queueEntry->state = DLQ_STATE_DONE;

		/* filelist processing is done on read */
		if (dl->file)
			isFile = true;
		else
			isFile = false;

		if (isFile) {
			fclose(dl->file);
			dl->file = nullptr;
		}

		/* might be aborted */
		if (pendingCount)
			pendingCount--;
		handleCount--;
		/* Com_Printf("finished dl: hc = %d\n", handleCount); */
		cls.downloadName[0] = 0;
		cls.downloadPosition = 0;

		result = msg->data.result;

		switch (result) {
		/* for some reason curl returns CURLE_OK for a 404... */
		case CURLE_HTTP_RETURNED_ERROR:
		case CURLE_OK:
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
			if (responseCode == 404) {
				const char* extension = Com_GetExtension(dl->queueEntry->ufoPath);
				if (extension != nullptr && Q_streq(extension, "pk3"))
					downloadingPK3 = false;

				if (isFile)
					FS_RemoveFile(dl->filePath);
				Com_Printf("HTTP(%s): 404 File Not Found [%d remaining files]\n", dl->queueEntry->ufoPath, pendingCount);
				curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &fileSize);
				if (fileSize > 512) {
					/* ick */
					isFile = false;
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
				FS_RemoveFile(dl->filePath);
			Com_Printf("Fatal HTTP error: %s\n", curl_easy_strerror(result));
			curl_multi_remove_handle(multi, dl->curl);
			if (abortDownloads)
				continue;
			CL_CancelHTTPDownloads(true);
			continue;
		default:
			i = strlen(dl->queueEntry->ufoPath);
			if (Q_streq(dl->queueEntry->ufoPath + i - 4, ".pk3"))
				downloadingPK3 = false;
			if (isFile)
				FS_RemoveFile(dl->filePath);
			Com_Printf("HTTP download failed: %s\n", curl_easy_strerror(result));
			curl_multi_remove_handle(multi, dl->curl);
			continue;
		}

		if (isFile) {
			/* rename the temp file */
			Com_sprintf(tempName, sizeof(tempName), "%s/%s", FS_Gamedir(), dl->queueEntry->ufoPath);

			if (!FS_RenameFile(dl->filePath, tempName, false))
				Com_Printf("Failed to rename %s for some odd reason...", dl->filePath);

			/* a pk3 file is very special... */
			i = strlen(tempName);
			if (Q_streq(tempName + i - 4, ".pk3")) {
				FS_RestartFilesystem(nullptr);
				CL_ReVerifyHTTPQueue();
				downloadingPK3 = false;
			}
		}

		/* show some stats */
		curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &timeTaken);
		curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &fileSize);

		/** @todo
		 * technically i shouldn't need to do this as curl will auto reuse the
		 * existing handle when you change the URL. however, the handleCount goes
		 * all weird when reusing a download slot in this way. if you can figure
		 * out why, please let me know. */
		curl_multi_remove_handle(multi, dl->curl);

		Com_Printf("HTTP(%s): %.f bytes, %.2fkB/sec [%d remaining files]\n",
			dl->queueEntry->ufoPath, fileSize, (fileSize / 1024.0) / timeTaken, pendingCount);
	} while (messagesInQueue > 0);

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
 * @brief Start another HTTP download if possible.
 * @sa CL_RunHTTPDownloads
 */
static void CL_StartNextHTTPDownload (void)
{
	for (dlqueue_t* q = cls.downloadQueue; q; q = q->next) {
		if (q->state == DLQ_STATE_NOT_STARTED) {
			size_t len;
			dlhandle_t* dl = CL_GetFreeDLHandle();
			if (!dl)
				return;

			CL_StartHTTPDownload(q, dl);

			/* ugly hack for pk3 file single downloading */
			len = strlen(q->ufoPath);
			if (len > 4 && !Q_strcasecmp(q->ufoPath + len - 4, ".pk3"))
				downloadingPK3 = true;

			break;
		}
	}
}

/**
 * @brief This calls curl_multi_perform do actually do stuff. Called every frame while
 * connecting to minimise latency. Also starts new downloads if we're not doing
 * the maximum already.
 * @sa CL_Frame
 */
void CL_RunHTTPDownloads (void)
{
	CURLMcode ret;

	if (!cls.downloadServer[0])
		return;

	/* Com_Printf("handle %d, pending %d\n", handleCount, pendingCount); */

	/* not enough downloads running, queue some more! */
	if (pendingCount && abortDownloads == HTTPDL_ABORT_NONE &&
		!downloadingPK3 && handleCount < cl_http_max_connections->integer)
		CL_StartNextHTTPDownload();

	do {
		int newHandleCount;
		ret = curl_multi_perform(multi, &newHandleCount);
		if (newHandleCount < handleCount) {
			/* Com_Printf("runnd dl: hc = %d, nc = %d\n", handleCount, newHandleCount); */
			/* hmm, something either finished or errored out. */
			CL_FinishHTTPDownload();
			handleCount = newHandleCount;
		}
	} while (ret == CURLM_CALL_MULTI_PERFORM);

	if (ret != CURLM_OK) {
		Com_Printf("curl_multi_perform error. Aborting HTTP downloads.\n");
		CL_CancelHTTPDownloads(true);
	}

	/* not enough downloads running, queue some more! */
	if (pendingCount && abortDownloads == HTTPDL_ABORT_NONE &&
		!downloadingPK3 && handleCount < cl_http_max_connections->integer)
		CL_StartNextHTTPDownload();
}

void HTTP_InitStartup (void)
{
	cl_http_filelists = Cvar_Get("cl_http_filelists", "1");
	cl_http_downloads = Cvar_Get("cl_http_downloads", "1", 0, "Try to download files via http");
	cl_http_max_connections = Cvar_Get("cl_http_max_connections", "1");
}
#else
void CL_CancelHTTPDownloads(bool permKill) {}
bool CL_QueueHTTPDownload(const char* ufoPath) {return false;}
void CL_RunHTTPDownloads(void) {}
bool CL_PendingHTTPDownloads(void) {return false;}
void CL_SetHTTPServer(const char* URL) {}
void CL_HTTP_Cleanup(void) {}
void CL_RequestNextDownload(void) {}
bool CL_CheckOrDownloadFile(const char* filename) {return false;}

void HTTP_InitStartup(void){}
#endif
