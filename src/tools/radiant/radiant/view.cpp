/**
 * @file view.cpp
 * @brief Culling stats for the 3d window
 */

/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "view.h"

#include <stdio.h>

int g_count_dots;
int g_count_planes;
int g_count_oriented_planes;
int g_count_bboxs;
int g_count_oriented_bboxs;

void Cull_ResetStats ()
{
	g_count_dots = 0;
	g_count_planes = 0;
	g_count_oriented_planes = 0;
	g_count_bboxs = 0;
	g_count_oriented_bboxs = 0;
}

const char* Cull_GetStats ()
{
	static char g_cull_stats[1024];
	snprintf(g_cull_stats, sizeof(g_cull_stats), "dots: %d | planes %d + %d | bboxs %d + %d", g_count_dots,
			g_count_planes, g_count_oriented_planes, g_count_bboxs, g_count_oriented_bboxs);
	return g_cull_stats;
}
