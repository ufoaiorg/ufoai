/**
 * @file save_aliencont.h
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

#ifndef SAVE_ALIENCONT_H
#define SAVE_ALIENCONT_H

#define SAVE_ALIENCONT_ALIENCONT "alienCont"
#define SAVE_ALIENCONT_BREATHINGMAILSENT "breathingMailSent"

#define SAVE_ALIENCONT_CONT "cont"
#define SAVE_ALIENCONT_BASEIDX "baseIDX"

#define SAVE_ALIENCONT_ALIEN "alien"
#define SAVE_ALIENCONT_TEAMID "id"
#define SAVE_ALIENCONT_AMOUNTALIVE "amountAlive"
#define SAVE_ALIENCONT_AMOUNTDEAD "amountDead"

#endif

/*
DTD:

<!ELEMENT alienCont cont>
<!ATTLIST alienCont
	breathingMailSent		CDATA		'false'
>

<!ELEMENT cont alien*>
<!ATTLIST cont
	baseIDX					CDATA		#REQUIRED
>

<!ELEMENT alien EMPTY>
<!ATTLIST alien
	id						CDATA		#REQUIRED
	amountAlive				CDATA		'0'
	amountDead				CDATA		'0'
>

*/
