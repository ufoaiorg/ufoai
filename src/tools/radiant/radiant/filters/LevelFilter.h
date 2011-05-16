/**
 * @file LevelFilter.h
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

#ifndef LEVELFILTER_H_
#define LEVELFILTER_H_

#include "ientity.h"
#include "../brush/Brush.h"

class LevelFilter
{
	private:

		void filter_level (int level);
		void filter_level1 ();
		void filter_level2 ();
		void filter_level3 ();
		void filter_level4 ();
		void filter_level5 ();
		void filter_level6 ();
		void filter_level7 ();
		void filter_level8 ();

	private:

		typedef std::vector<std::string> EntityClassNameList;
		typedef std::list<Brush*> BrushList;
		typedef std::list<Entity*> EntityList;

		class EntityFindByName: public scene::Graph::Walker
		{
				const std::string& m_name;
				EntityList& m_entitylist;
				/* this starts at 1 << level */
				int m_flag;
				int m_hide;
			public:
				EntityFindByName (const std::string& name, EntityList& entitylist, int flag, bool hide);
				bool pre (const scene::Path& path, scene::Instance& instance) const;
		};

		class ForEachFace: public BrushVisitor
		{
				Brush &m_brush;
			public:
				mutable int m_contentFlagsVis;
				mutable int m_surfaceFlagsVis;

				ForEachFace (Brush& brush);

				void visit (Face& face) const;
		};

		class BrushGetLevel: public scene::Graph::Walker
		{
				BrushList& m_brushlist;
				int m_flag;
				mutable bool m_hide;
			public:
				BrushGetLevel (BrushList& brushlist, int flag, bool hide);
				bool pre (const scene::Path& path, scene::Instance& instance) const;
		};

	private:

		int currentActiveLevel;
		EntityClassNameList _classNameList;

	public:

		LevelFilter ();

		void registerCommands (void);

		int getCurrentLevel (void);
};

inline LevelFilter& GlobalLevelFilter ()
{
	static LevelFilter _levelFilter;
	return _levelFilter;
}

#endif /* LEVELFILTERS_H_ */
