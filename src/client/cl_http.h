/**
 * @file cl_http.h
 * @brief cURL header
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef __CLIENT_HTTP_H__
#define __CLIENT_HTTP_H__

void CL_CancelHTTPDownloads(qboolean permKill);
void CL_InitHTTPDownloads(void);
qboolean CL_QueueHTTPDownload(const char *ufoPath);
void CL_RunHTTPDownloads(void);
qboolean CL_PendingHTTPDownloads(void);
void CL_SetHTTPServer(const char *URL);
void CL_HTTP_Cleanup(qboolean fullShutdown);

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

#endif /* __CLIENT_HTTP_H__ */

#endif /* HAVE_CURL */
