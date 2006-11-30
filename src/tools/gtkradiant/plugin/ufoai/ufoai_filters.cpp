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

#include <list>


class filter_face_shader : public FaceFilter
{
  const char* m_shader;
public:
  filter_face_shader(const char* shader) : m_shader(shader)
  {
  }
  bool filter(const Face& face) const
  {
    return shader_equal(face.GetShader(), m_shader);
  }
};

class filter_face_shader_prefix : public FaceFilter
{
  const char* m_prefix;
public:
  filter_face_shader_prefix(const char* prefix) : m_prefix(prefix)
  {
  }
  bool filter(const Face& face) const
  {
    return shader_equal_n(face.GetShader(), m_prefix, strlen(m_prefix));
  }
};

class filter_face_flags : public FaceFilter
{
  int m_flags;
public:
  filter_face_flags(int flags) : m_flags(flags)
  {
  }
  bool filter(const Face& face) const
  {
    return (face.getShader().shaderFlags() & m_flags) != 0;
  }
};

class filter_face_contents : public FaceFilter
{
  int m_contents;
public:
  filter_face_contents(int contents) : m_contents(contents)
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

void add_brush_filter(BrushFilter& filter, int mask, bool invert)
{
  g_brushFilters.push_back(BrushFilterWrapper(filter, invert));
  GlobalFilterSystem().addFilter(g_brushFilters.back(), mask);
}

filter_face_flags g_filter_face_clip(CONTENTS_ACTORCLIP);
filter_brush_all_faces g_filter_brush_clip(&g_filter_face_clip);

filter_face_flags g_filter_face_stepon(CONTENTS_STEPON);
filter_brush_all_faces g_filter_brush_stepon(&g_filter_face_stepon);

filter_face_flags g_filter_face_level1(CONTENTS_LEVEL1);
filter_brush_all_faces g_filter_brush_level1(&g_filter_face_level1);

filter_face_flags g_filter_face_level2(CONTENTS_LEVEL2);
filter_brush_all_faces g_filter_brush_level2(&g_filter_face_level2);

filter_face_flags g_filter_face_level3(CONTENTS_LEVEL3);
filter_brush_all_faces g_filter_brush_level3(&g_filter_face_level3);

filter_face_flags g_filter_face_level4(CONTENTS_LEVEL4);
filter_brush_all_faces g_filter_brush_level4(&g_filter_face_level4);

filter_face_flags g_filter_face_level5(CONTENTS_LEVEL5);
filter_brush_all_faces g_filter_brush_level5(&g_filter_face_level5);

filter_face_flags g_filter_face_level6(CONTENTS_LEVEL6);
filter_brush_all_faces g_filter_brush_level6(&g_filter_face_level6);

filter_face_flags g_filter_face_level7(CONTENTS_LEVEL7);
filter_brush_all_faces g_filter_brush_level7(&g_filter_face_level7);

filter_face_flags g_filter_face_level8(CONTENTS_LEVEL8);
filter_brush_all_faces g_filter_brush_level8(&g_filter_face_level8);

void init_filters(void)
{
	add_brush_filter(g_filter_brush_clip, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_stepon, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_level1, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_level2, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_level3, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_level4, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_level5, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_level6, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_level7, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_level8, EXCLUDE_CLIP);
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
	globalOutputStream() << "TODO: Implement me\n";
}

/**
 * @brief Activates the level filter for the given level
 * @param[in] level Which level to show?
 */
void filter_level(int level)
{
	if (activeLevelFilter > 0)
		deactive_filter_level(activeLevelFilter);
	if (activeLevelFilter != level)
	{
		activeLevelFilter = level;
		globalOutputStream() << "filter_level: " << activeLevelFilter << "\n";
		globalOutputStream() << "TODO: Implement me\n";
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
	globalOutputStream() << "TODO: Implement me\n";
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
		globalOutputStream() << "filter_stepon\n";
		globalOutputStream() << "TODO: Implement me\n";
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
	globalOutputStream() << "TODO: Implement me\n";
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
		globalOutputStream() << "filter_actorclip\n";
		globalOutputStream() << "TODO: Implement me\n";
	}
}

