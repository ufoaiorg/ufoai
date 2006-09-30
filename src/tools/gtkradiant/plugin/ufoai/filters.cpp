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

#include "filters.h"

#include "ifilter.h"
#include "ientity.h"

#include <list>

class UfoAIFilterWrapper : public FaceFilter
{
	bool m_active;
	UfoAIFilter& m_filter;
	public:
	UfoAIFilterWrapper(UfoAIFilter& filter) : m_filter(filter) {
	}
	void setActive(bool active) {
		m_active = active;
	}
	bool active() {
		return m_active;
	}
	bool filter(const Entity& entity) {
		return m_filter.filter(entity);
	}
};


typedef std::list<UfoAIFilterWrapper> UfoAIFilters;
UfoAIFilters g_UfoAIFilters;

void add_ufoai_filter(UfoAIFilter& filter, int mask)
{
	g_UfoAIFilters.push_back(UfoAIFilterWrapper(filter));
	GlobalFilterSystem().addFilter(g_UfoAIFilters.back(), mask);
	globalOutputStream() << "...add new filter for mask " << mask << "\n";
}

bool ufoai_filtered(Entity& ufoai)
{
	for(UfoAIFilters::iterator i = g_UfoAIFilters.begin(); i != g_UfoAIFilters.end(); ++i) {
		if((*i).active() && (*i).filter(ufoai)) {
			return true;
		}
	}
	return false;
}

