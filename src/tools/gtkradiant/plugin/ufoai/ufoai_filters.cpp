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

#include "ufoai_filters.h"

#include "ifilter.h"
#include "ibrush.h"

#include "../../radiant/brush.h"

#include "generic/callback.h"

#include <list>


class ufoai_filter_face_shader : public FaceFilter
{
  const char* m_shader;
public:
  ufoai_filter_face_shader(const char* shader) : m_shader(shader)
  {
  }
  bool filter(const Face& face) const
  {
    return shader_equal(face.GetShader(), m_shader);
  }
};

class ufoai_filter_face_shader_prefix : public FaceFilter
{
  const char* m_prefix;
public:
  ufoai_filter_face_shader_prefix(const char* prefix) : m_prefix(prefix)
  {
  }
  bool filter(const Face& face) const
  {
    return shader_equal_n(face.GetShader(), m_prefix, strlen(m_prefix));
  }
};

class ufoai_filter_face_flags : public FaceFilter
{
  int m_flags;
public:
  ufoai_filter_face_flags(int flags) : m_flags(flags)
  {
  }
  bool filter(const Face& face) const
  {
    return (face.getShader().shaderFlags() & m_flags) != 0;
  }
};

class ufoai_filter_face_contents : public FaceFilter
{
  int m_contents;
public:
  ufoai_filter_face_contents(int contents) : m_contents(contents)
  {
  }
  bool filter(const Face& face) const
  {
    return (face.getShader().m_flags.m_contentFlags & m_contents) != 0;
  }
};



class FaceFilterAny
{
  FaceFilter* m_filter;
  bool& m_filtered;
public:
  FaceFilterAny(FaceFilter* filter, bool& filtered) : m_filter(filter), m_filtered(filtered)
  {
    m_filtered = false;
  }
  void operator()(Face& face) const
  {
    if(m_filter->filter(face))
    {
      m_filtered = true;
    }
  }
};

class filter_brush_any_face : public BrushFilter
{
  FaceFilter* m_filter;
public:
  filter_brush_any_face(FaceFilter* filter) : m_filter(filter)
  {
  }
  bool filter(const Brush& brush) const
  {
    bool filtered;
    Brush_forEachFace(brush, FaceFilterAny(m_filter, filtered));
    return filtered;
  }
};

class FaceFilterAll
{
  FaceFilter* m_filter;
  bool& m_filtered;
public:
  FaceFilterAll(FaceFilter* filter, bool& filtered) : m_filter(filter), m_filtered(filtered)
  {
    m_filtered = true;
  }
  void operator()(Face& face) const
  {
    if(!m_filter->filter(face))
    {
      m_filtered = false;
    }
  }
};

class filter_brush_all_faces : public BrushFilter
{
  FaceFilter* m_filter;
public:
  filter_brush_all_faces(FaceFilter* filter) : m_filter(filter)
  {
  }
  bool filter(const Brush& brush) const
  {
    bool filtered;
    Brush_forEachFace(brush, FaceFilterAll(m_filter, filtered));
    return filtered;
  }
};


class BrushFilterWrapper : public Filter
{
  bool m_active;
  bool m_invert;
  BrushFilter& m_filter;
public:
  BrushFilterWrapper(BrushFilter& filter, bool invert) : m_invert(invert), m_filter(filter)
  {
  }
  void setActive(bool active)
  {
    m_active = active;
  }
  bool active()
  {
    return m_active;
  }
  bool filter(const Brush& brush)
  {
    return m_invert ^ m_filter.filter(brush);
  }
};

typedef std::list<BrushFilterWrapper> BrushFilters;
BrushFilters g_brushFilters;

BrushFilterWrapper* add_ufoaibrush_filter(BrushFilter& filter, int mask, bool invert)
{
  g_brushFilters.push_back(BrushFilterWrapper(filter, invert));
  GlobalFilterSystem().addFilter(g_brushFilters.back(), mask);
  return &g_brushFilters.back();
}

ufoai_filter_face_flags g_ufoai_filter_face_clip(CONTENTS_ACTORCLIP);
filter_brush_all_faces g_filter_brush_clip(&g_ufoai_filter_face_clip);

ufoai_filter_face_flags g_ufoai_filter_face_stepon(CONTENTS_STEPON);
filter_brush_all_faces g_filter_brush_stepon(&g_ufoai_filter_face_stepon);

ufoai_filter_face_flags g_ufoai_filter_face_level1(CONTENTS_LEVEL1);
filter_brush_all_faces g_filter_brush_level1(&g_ufoai_filter_face_level1);

ufoai_filter_face_flags g_ufoai_filter_face_level2(CONTENTS_LEVEL2);
filter_brush_all_faces g_filter_brush_level2(&g_ufoai_filter_face_level2);

ufoai_filter_face_flags g_ufoai_filter_face_level3(CONTENTS_LEVEL3);
filter_brush_all_faces g_filter_brush_level3(&g_ufoai_filter_face_level3);

ufoai_filter_face_flags g_ufoai_filter_face_level4(CONTENTS_LEVEL4);
filter_brush_all_faces g_filter_brush_level4(&g_ufoai_filter_face_level4);

ufoai_filter_face_flags g_ufoai_filter_face_level5(CONTENTS_LEVEL5);
filter_brush_all_faces g_filter_brush_level5(&g_ufoai_filter_face_level5);

ufoai_filter_face_flags g_ufoai_filter_face_level6(CONTENTS_LEVEL6);
filter_brush_all_faces g_filter_brush_level6(&g_ufoai_filter_face_level6);

ufoai_filter_face_flags g_ufoai_filter_face_level7(CONTENTS_LEVEL7);
filter_brush_all_faces g_filter_brush_level7(&g_ufoai_filter_face_level7);

ufoai_filter_face_flags g_ufoai_filter_face_level8(CONTENTS_LEVEL8);
filter_brush_all_faces g_filter_brush_level8(&g_ufoai_filter_face_level8);

BrushFilterWrapper* f_stepon;
BrushFilterWrapper* f_actorclip;
BrushFilterWrapper* f_level1;
BrushFilterWrapper* f_level2;
BrushFilterWrapper* f_level3;
BrushFilterWrapper* f_level4;
BrushFilterWrapper* f_level5;
BrushFilterWrapper* f_level6;
BrushFilterWrapper* f_level7;
BrushFilterWrapper* f_level8;

void init_filters(void)
{
	f_stepon = add_ufoaibrush_filter(g_filter_brush_stepon, EXCLUDE_CLIP, 0);
	f_level1 = add_ufoaibrush_filter(g_filter_brush_level1, EXCLUDE_CLIP, 0);
	f_level2 = add_ufoaibrush_filter(g_filter_brush_level2, EXCLUDE_CLIP, 0);
	f_level3 = add_ufoaibrush_filter(g_filter_brush_level3, EXCLUDE_CLIP, 0);
	f_level4 = add_ufoaibrush_filter(g_filter_brush_level4, EXCLUDE_CLIP, 0);
	f_level5 = add_ufoaibrush_filter(g_filter_brush_level5, EXCLUDE_CLIP, 0);
	f_level6 = add_ufoaibrush_filter(g_filter_brush_level6, EXCLUDE_CLIP, 0);
	f_level7 = add_ufoaibrush_filter(g_filter_brush_level7, EXCLUDE_CLIP, 0);
	f_level8 = add_ufoaibrush_filter(g_filter_brush_level8, EXCLUDE_CLIP, 0);
	f_actorclip = add_ufoaibrush_filter(g_filter_brush_clip, EXCLUDE_CLIP, 0);
	globalOutputStream() << "initialized UFO:AI fiters\n";
}

static int activeLevelFilter = 0;
static int activeSteponFilter = 0;
static int activeActorClipFilter = 0;

/**
 * @brief Deactivates the level filter for the given level
 */
void deactive_filter_level(int level)
{
	globalOutputStream() << "deactive_filter_level: " << level << "\n";
	activeLevelFilter = 0;
	switch (level) {
	case 1:
		f_level1->setActive(0);
		break;
	case 2:
		f_level2->setActive(0);
		break;
	case 3:
		f_level3->setActive(0);
		break;
	case 4:
		f_level4->setActive(0);
		break;
	case 5:
		f_level5->setActive(0);
		break;
	case 6:
		f_level6->setActive(0);
		break;
	case 7:
		f_level7->setActive(0);
		break;
	case 8:
		f_level8->setActive(0);
		break;
	};
}

/**
 * @brief Activates the level filter for the given level
 * @param[in] level Which level to show?
 */
void filter_level(int level)
{
	int flag = 1;
	if (activeLevelFilter > 0)
		deactive_filter_level(activeLevelFilter);
	if (activeLevelFilter != level)
	{
		activeLevelFilter = level;
		flag <<= (level - 1);
		switch (level) {
		case 0:
			f_level1->setActive(1);
			break;
		case 1:
			f_level2->setActive(1);
			break;
		case 2:
			f_level3->setActive(1);
			break;
		case 3:
			f_level4->setActive(1);
			break;
		case 4:
			f_level5->setActive(1);
			break;
		case 5:
			f_level6->setActive(1);
			break;
		case 6:
			f_level7->setActive(1);
			break;
		case 7:
			f_level8->setActive(1);
			break;
		};
		globalOutputStream() << "filter_level: " << activeLevelFilter << " flag: " << flag << "\n";
	}
	else
	{
		globalOutputStream() << "Level filters deactivated\n";
	}
}

/**
 * @brief Deactivates the stepon filter
 * @sa filter_stepon
 */
void deactive_filter_stepon(void)
{
	globalOutputStream() << "deactive_filter_stepon\n";
	activeSteponFilter = 0;
	f_stepon->setActive(activeSteponFilter);
}

/**
 * @brief Activates the stepon filter to hide stepon brushes
 * @sa deactive_filter_stepon
 */
void filter_stepon(void)
{
	if (activeSteponFilter == 1)
		deactive_filter_stepon();
	else
	{
		activeSteponFilter = 1;
		f_stepon->setActive(activeSteponFilter);
		globalOutputStream() << "filter_stepon\n";
	}
}


/**
 * @brief Deactivates the actorclip filter
 * @sa filter_actorclip
 */
void deactive_filter_actorclip(void)
{
	globalOutputStream() << "deactive_filter_actorclip\n";
	activeActorClipFilter = 0;
	f_actorclip->setActive(activeSteponFilter);
}

/**
 * @brief Activates the actorclip filter to hide actorclip brushes
 * @sa deactive_filter_actorclip
 */
void filter_actorclip(void)
{
	if (activeActorClipFilter == 1)
		deactive_filter_actorclip();
	else
	{
		activeActorClipFilter = 1;
		f_actorclip->setActive(activeSteponFilter);
		globalOutputStream() << "filter_actorclip\n";
	}
}

bool brush_filtered(Brush& brush)
{
  for(BrushFilters::iterator i = g_brushFilters.begin(); i != g_brushFilters.end(); ++i)
  {
    if((*i).active() && (*i).filter(brush))
    {
      return true;
    }
  }
  return false;
}

class ClassnameFilter : public Filterable
{
  scene::Node& m_node;
public:
  Brush& m_brush;

  ClassnameFilter(Brush& brush, scene::Node& node) : m_node(node), m_brush(brush)
  {
  }
  ~ClassnameFilter()
  {
  }

  void instanceAttach()
  {
    GlobalFilterSystem().registerFilterable(*this);
  }
  void instanceDetach()
  {
    GlobalFilterSystem().unregisterFilterable(*this);
  }

  void updateFiltered()
  {
    if(brush_filtered(m_brush))
    {
      m_node.enable(scene::Node::eFiltered);
    }
    else
    {
      m_node.disable(scene::Node::eFiltered);
    }
  }

  void classnameChanged(const char* value)
  {
    updateFiltered();
  }
  typedef MemberCaller1<ClassnameFilter, const char*, &ClassnameFilter::classnameChanged> ClassnameChangedCaller;
};
