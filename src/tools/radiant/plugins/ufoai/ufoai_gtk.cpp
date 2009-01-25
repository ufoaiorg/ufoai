/*
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

#include "ufoai_gtk.h"
#include "ufoai_filters.h"
#include "iscenegraph.h"

#define NUM_TOOLBARBUTTONS 8

std::size_t ToolbarButtonCount (void)
{
	return NUM_TOOLBARBUTTONS;
}

class UFOAIToolbarButton: public IToolbarButton
{
	public:
		virtual const char* getImage () const
		{
			switch (mIndex) {
			case 0:
				return "ufoai_level1.bmp";
			case 1:
				return "ufoai_level2.bmp";
			case 2:
				return "ufoai_level3.bmp";
			case 3:
				return "ufoai_level4.bmp";
			case 4:
				return "ufoai_level5.bmp";
			case 5:
				return "ufoai_level6.bmp";
			case 6:
				return "ufoai_level7.bmp";
			case 7:
				return "ufoai_level8.bmp";
			}
			return NULL;
		}
		virtual EType getType () const
		{
			switch (mIndex) {
			/*		case 3: return eButton;*/
			default:
				return eButton;
			}
		}
		virtual const char* getText () const
		{
			switch (mIndex) {
			case 0:
				return "Level 1";
			case 1:
				return "Level 2";
			case 2:
				return "Level 3";
			case 3:
				return "Level 4";
			case 4:
				return "Level 5";
			case 5:
				return "Level 6";
			case 6:
				return "Level 7";
			case 7:
				return "Level 8";
			}
			return NULL;
		}
		virtual const char* getTooltip () const
		{
			switch (mIndex) {
			case 0:
				return "Show only level 1";
			case 1:
				return "Show only level 2";
			case 2:
				return "Show only level 3";
			case 3:
				return "Show only level 4";
			case 4:
				return "Show only level 5";
			case 5:
				return "Show only level 6";
			case 6:
				return "Show only level 7";
			case 7:
				return "Show only level 8";
			}
			return NULL;
		}

		virtual void activate () const
		{
			switch (mIndex) {
			case 0:
				filter_level(CONTENTS_LEVEL_1);
				break;
			case 1:
				filter_level(CONTENTS_LEVEL_2);
				break;
			case 2:
				filter_level(CONTENTS_LEVEL_3);
				break;
			case 3:
				filter_level(CONTENTS_LEVEL_4);
				break;
			case 4:
				filter_level(CONTENTS_LEVEL_5);
				break;
			case 5:
				filter_level(CONTENTS_LEVEL_6);
				break;
			case 6:
				filter_level(CONTENTS_LEVEL_7);
				break;
			case 7:
				filter_level(CONTENTS_LEVEL_8);
				break;
			}
			SceneChangeNotify();
		}

		std::size_t mIndex;
};

UFOAIToolbarButton g_ufoaiToolbarButtons[NUM_TOOLBARBUTTONS];

const IToolbarButton* GetToolbarButton (std::size_t index)
{
	g_ufoaiToolbarButtons[index].mIndex = index;
	return &g_ufoaiToolbarButtons[index];
}
