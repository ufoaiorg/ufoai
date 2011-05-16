/**
 * @file cl_http.h
 * @brief cURL header
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef __CLIENT_HTTP_H__
#define __CLIENT_HTTP_H__

#include "../common/http.h"

void CL_CancelHTTPDownloads(qboolean permKill);
qboolean CL_QueueHTTPDownload(const char *ufoPath);
void CL_RunHTTPDownloads(void);
qboolean CL_PendingHTTPDownloads(void);
void CL_SetHTTPServer(const char *URL);
void CL_HTTP_Cleanup(void);
void CL_RequestNextDownload(void);
qboolean CL_CheckOrDownloadFile(const char *filename);

void HTTP_InitStartup(void);

#endif /* __CLIENT_HTTP_H__ */
