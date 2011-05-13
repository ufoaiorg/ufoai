/**
 * @file save_nation.h
 * @brief XML tag constants for savegame.
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef SAVE_NATION_H
#define SAVE_NATION_H

#define SAVE_NATION_NATIONS "nations"
#define SAVE_NATION_NATION "nation"
#define SAVE_NATION_ID "id"
#define SAVE_NATION_MONTH "month"
#define SAVE_NATION_MONTH_IDX "IDX"
#define SAVE_NATION_HAPPINESS "happiness"
#define SAVE_NATION_XVI "XVI"

#endif

/*
DTD:

<!ELEMENT nations nation*>

<!ELEMENT nation month*>
<!ATTLIST nation
	id			CDATA		#REQUIRED
>

<!ELEMENT month EMPTY>
<!ATTLIST month
	IDX			CDATA		#REQUIRED
	happiness	CDATA		'0.0'
	XVI			CDATA		'0'
>
*/
