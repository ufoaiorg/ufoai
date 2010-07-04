/**
 * @file cl_shared.h
 * @brief Share stuff between the different cgame implementations
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CL_SHARED_H
#define CL_SHARED_H

#include "../common/common.h"

/* i18n support via gettext */
#include <libintl.h>

/* the used textdomain for gettext */
#define TEXT_DOMAIN "ufoai"
#include <locale.h>
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/* Macros for faster access to the inventory-container. */
#define CONTAINER(e, containerID) ((e)->i.c[(containerID)])
#define ARMOUR(e) (CONTAINER(e, csi.idArmour))
#define RIGHT(e) (CONTAINER(e, csi.idRight))
#define LEFT(e)  (CONTAINER(e, csi.idLeft))
#define FLOOR(e) (CONTAINER(e, csi.idFloor))
#define HEADGEAR(e) (CONTAINER(e, csi.idHeadgear))
#define EXTENSION(e) (CONTAINER(e, csi.idExtension))
#define HOLSTER(e) (CONTAINER(e, csi.idHolster))

#define INVDEF(containerID) (&csi.ids[(containerID)])

#endif
