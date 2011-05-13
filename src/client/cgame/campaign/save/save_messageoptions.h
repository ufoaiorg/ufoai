/**
 * @file save_messageoptions.h
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

#ifndef SAVE_MESSAGEOPTIONS_H
#define SAVE_MESSAGEOPTIONS_H

#define SAVE_MESSAGEOPTIONS_MESSAGEOPTIONS "messageOptions"

#define SAVE_MESSAGEOPTIONS_TYPE "type"
#define SAVE_MESSAGEOPTIONS_NAME "name"
#define SAVE_MESSAGEOPTIONS_NOTIFY "notify"
#define SAVE_MESSAGEOPTIONS_PAUSE "pause"
#define SAVE_MESSAGEOPTIONS_SOUND "sound"

#endif

/*
DTD:

<!ELEMENT messageoptions type*>
<!ELEMENT type EMPTY>
<!ATTLIST type
	name		CDATA		#REQUIRED
	notify		CDATA		'false'
	pause		CDATA		'false'
	sound		CDATA		'false'
>

*/
