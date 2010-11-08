/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#include "shared.h"
#include "radiant_i18n.h"
#include "iclipper.h"
#include "ieventmanager.h"

#include "brushmanip.h"
#include "BrushModule.h"

#include "gtkutil/widget.h"
#include "../gtkmisc.h"
#include "BrushNode.h"
#include "../map/map.h"
#include "../sidebar/sidebar.h"
#include "../dialog.h"
#include "../settings/preferences.h"
#include "../mainframe.h"
#include "../ui/Icons.h"

#include "construct/Prism.h"
#include "construct/Cone.h"
#include "construct/Cuboid.h"
#include "construct/Rock.h"
#include "construct/Sphere.h"
#include "construct/Terrain.h"

#include <list>

static void Brush_ConstructPrefab (Brush& brush, EBrushPrefab type, const AABB& bounds, std::size_t sides,
		const TextureProjection& projection, const std::string& shader = "textures/tex_common/nodraw")
{
	brushconstruct::BrushConstructor *bc;
	switch (type) {
	case eBrushPrism:
		bc = &brushconstruct::Prism::getInstance();
		break;
	case eBrushCone:
		bc = &brushconstruct::Cone::getInstance();
		break;
	case eBrushSphere:
		bc = &brushconstruct::Sphere::getInstance();
		break;
	case eBrushRock:
		bc = &brushconstruct::Rock::getInstance();
		break;
	case eBrushTerrain:
		bc = &brushconstruct::Terrain::getInstance();
		break;
	default:
		return;
	}

	std::string command = bc->getName() + " -sides " + string::toString(sides);
	UndoableCommand undo(command);

	bc->generate(brush, bounds, sides, projection, shader);
}

class FaceSetTexdef
{
		const TextureProjection& m_projection;
	public:
		FaceSetTexdef (const TextureProjection& projection) :
			m_projection(projection)
		{
		}
		void operator() (Face& face) const
		{
			face.SetTexdef(m_projection);
		}
};

void Scene_BrushSetTexdef_Selected (scene::Graph& graph, const TextureProjection& projection)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetTexdef(projection));
	SceneChangeNotify();
}

void Scene_BrushSetTexdef_Component_Selected (scene::Graph& graph, const TextureProjection& projection)
{
	Scene_ForEachSelectedBrushFace(FaceSetTexdef(projection));
	SceneChangeNotify();
}

class FaceTextureFlipper
{
		unsigned int _flipAxis;
public:
	FaceTextureFlipper(unsigned int flipAxis) :
		_flipAxis(flipAxis)
	{}

	void operator()(Face& face) const {
		face.flipTexture(_flipAxis);
	}
};

void Scene_BrushFlipTexture_Selected(unsigned int flipAxis) {
	Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), FaceTextureFlipper(flipAxis));
}

void Scene_BrushFlipTexture_Component_Selected(unsigned int flipAxis) {
	Scene_ForEachSelectedBrushFace(FaceTextureFlipper(flipAxis));
}

class FaceSetFlags
{
		const ContentsFlagsValue& m_projection;
	public:
		FaceSetFlags (const ContentsFlagsValue& flags) :
			m_projection(flags)
		{
		}
		void operator() (Face& face) const
		{
			face.SetFlags(m_projection);
		}
};

void Scene_BrushSetFlags_Selected (scene::Graph& graph, const ContentsFlagsValue& flags)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetFlags(flags));
	SceneChangeNotify();
}

void Scene_BrushSetFlags_Component_Selected (scene::Graph& graph, const ContentsFlagsValue& flags)
{
	Scene_ForEachSelectedBrushFace(FaceSetFlags(flags));
	SceneChangeNotify();
}

class FaceShiftTexdef
{
		float m_s, m_t;
	public:
		FaceShiftTexdef (float s, float t) :
			m_s(s), m_t(t)
		{
		}
		void operator() (Face& face) const
		{
			face.ShiftTexdef(m_s, m_t);
		}
};

void Scene_BrushShiftTexdef_Selected (scene::Graph& graph, float s, float t)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceShiftTexdef(s, t));
	SceneChangeNotify();
}

void Scene_BrushShiftTexdef_Component_Selected (scene::Graph& graph, float s, float t)
{
	Scene_ForEachSelectedBrushFace(FaceShiftTexdef(s, t));
	SceneChangeNotify();
}

class FaceScaleTexdef
{
		float m_s, m_t;
	public:
		FaceScaleTexdef (float s, float t) :
			m_s(s), m_t(t)
		{
		}
		void operator() (Face& face) const
		{
			face.ScaleTexdef(m_s, m_t);
		}
};

void Scene_BrushScaleTexdef_Selected (scene::Graph& graph, float s, float t)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceScaleTexdef(s, t));
	SceneChangeNotify();
}

void Scene_BrushScaleTexdef_Component_Selected (scene::Graph& graph, float s, float t)
{
	Scene_ForEachSelectedBrushFace(FaceScaleTexdef(s, t));
	SceneChangeNotify();
}

class FaceRotateTexdef
{
		float m_angle;
	public:
		FaceRotateTexdef (float angle) :
			m_angle(angle)
		{
		}
		void operator() (Face& face) const
		{
			face.RotateTexdef(m_angle);
		}
};

void Scene_BrushRotateTexdef_Selected (scene::Graph& graph, float angle)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceRotateTexdef(angle));
	SceneChangeNotify();
}

void Scene_BrushRotateTexdef_Component_Selected (scene::Graph& graph, float angle)
{
	Scene_ForEachSelectedBrushFace(FaceRotateTexdef(angle));
	SceneChangeNotify();
}

class FaceSetShader
{
		const char* m_name;
	public:
		FaceSetShader (const char* name) :
			m_name(name)
		{
		}
		void operator() (Face& face) const
		{
			face.SetShader(m_name);
		}
};

void Scene_BrushSetShader_Selected (scene::Graph& graph, const char* name)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetShader(name));
	SceneChangeNotify();
}

void Scene_BrushSetShader_Component_Selected (scene::Graph& graph, const char* name)
{
	Scene_ForEachSelectedBrushFace(FaceSetShader(name));
	SceneChangeNotify();
}

class FaceSetDetail
{
		bool m_detail;
	public:
		FaceSetDetail (bool detail) :
			m_detail(detail)
		{
		}
		void operator() (Face& face) const
		{
			face.setDetail(m_detail);
		}
};

void Scene_BrushSetDetail_Selected (scene::Graph& graph, bool detail)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetDetail(detail));
	SceneChangeNotify();
}

class FaceFitTexture
{
		float m_s_repeat, m_t_repeat;
	public:
		FaceFitTexture (float s_repeat, float t_repeat) :
			m_s_repeat(s_repeat), m_t_repeat(t_repeat)
		{
		}
		void operator() (Face& face) const
		{
			face.FitTexture(m_s_repeat, m_t_repeat);
		}
};

void Scene_BrushFitTexture_Selected (scene::Graph& graph, float s_repeat, float t_repeat)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceFitTexture(s_repeat, t_repeat));
	SceneChangeNotify();
}

void Scene_BrushFitTexture_Component_Selected (scene::Graph& graph, float s_repeat, float t_repeat)
{
	Scene_ForEachSelectedBrushFace(FaceFitTexture(s_repeat, t_repeat));
	SceneChangeNotify();
}

TextureProjection g_defaultTextureProjection;
const TextureProjection& TextureTransform_getDefault ()
{
	g_defaultTextureProjection.constructDefault();
	return g_defaultTextureProjection;
}

void Scene_BrushConstructPrefab (scene::Graph& graph, EBrushPrefab type, std::size_t sides, const std::string& shader)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();

		Brush* brush = Node_getBrush(path.top());
		if (brush != 0) {
			AABB bounds = brush->localAABB(); // copy bounds because the brush will be modified
			Brush_ConstructPrefab(*brush, type, bounds, sides, TextureTransform_getDefault(), shader);
			SceneChangeNotify();
		}
	}
}

void Scene_BrushResize_Selected (scene::Graph& graph, const AABB& bounds, const std::string& shader)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();

		Brush* brush = Node_getBrush(path.top());
		if (brush != 0) {
			brushconstruct::Cuboid::getInstance().generate(*brush, bounds, 0, TextureTransform_getDefault(), shader);
			SceneChangeNotify();
		}
	}
}

static inline bool Brush_hasShader (const Brush& brush, const std::string& name)
{
	for (Brush::const_iterator i = brush.begin(); i != brush.end(); ++i) {
		if (shader_equal((*i)->GetShader(), name)) {
			return true;
		}
	}
	return false;
}

class BrushSelectByShaderWalker: public scene::Graph::Walker
{
		const std::string m_name;
	public:
		BrushSelectByShaderWalker (const std::string& name) :
			m_name(name)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get().visible()) {
				Brush* brush = Node_getBrush(path.top());
				if (brush != 0 && Brush_hasShader(*brush, m_name)) {
					Instance_getSelectable(instance)->setSelected(true);
				}
			}
			return true;
		}
};

void Scene_BrushSelectByShader (scene::Graph& graph, const std::string& name)
{
	graph.traverse(BrushSelectByShaderWalker(name));
}

class FaceSelectByShader
{
		const std::string m_name;
	public:
		FaceSelectByShader (const std::string& name) :
			m_name(name)
		{
		}
		void operator() (FaceInstance& face) const
		{
			if (shader_equal(face.getFace().GetShader(), m_name)) {
				face.setSelected(SelectionSystem::eFace, true);
			}
		}
};

void Scene_BrushSelectByShader_Component (scene::Graph& graph, const std::string& name)
{
	Scene_ForEachSelectedBrush_ForEachFaceInstance(graph, FaceSelectByShader(name));
}

/**
 * @brief Select all faces in given graph that use given texture
 * @param graph scene graph to select faces from
 * @param name texture name
 */
void Scene_BrushFacesSelectByShader_Component (scene::Graph& graph, const std::string& name)
{
	Scene_ForEachBrush_ForEachFaceInstance(graph, FaceSelectByShader(name));
}

class FaceGetTexdef
{
		TextureProjection& m_projection;
		mutable bool m_done;
	public:
		FaceGetTexdef (TextureProjection& projection) :
			m_projection(projection), m_done(false)
		{
		}
		void operator() (Face& face) const
		{
			if (!m_done) {
				m_done = true;
				face.GetTexdef(m_projection);
			}
		}
};

void Scene_BrushGetTexdef_Selected (scene::Graph& graph, TextureProjection& projection)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceGetTexdef(projection));
}

void Scene_BrushGetTexdef_Component_Selected (scene::Graph& graph, TextureProjection& projection)
{
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		faceInstance.getFace().GetTexdef(projection);
	}
}

void Scene_BrushGetShaderSize_Component_Selected (scene::Graph& graph, size_t& width, size_t& height)
{
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		width = faceInstance.getFace().getShader().width();
		height = faceInstance.getFace().getShader().height();
	}
}

void FaceGetFlags::operator ()(Face& face) const
{
	face.GetFlags(m_flags);
}

/**
 * @sa SurfaceInspector_SetCurrent_FromSelected
 */
void Scene_BrushGetFlags_Selected (scene::Graph& graph, ContentsFlagsValue& flags)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		Scene_ForEachSelectedBrush_ForEachFace(graph, FaceGetFlags(flags));
	}
}

/**
 * @sa SurfaceInspector_SetCurrent_FromSelected
 */
void Scene_BrushGetFlags_Component_Selected (scene::Graph& graph, ContentsFlagsValue& flags)
{
	if (GlobalSelectionSystem().countSelectedComponents() != 0) {
		Scene_ForEachSelectedBrushFace(FaceGetFlags(flags));
	}
}

class FaceGetShader
{
		std::string& m_shader;
		mutable bool m_done;
	public:
		FaceGetShader (std::string& shader) :
			m_shader(shader), m_done(false)
		{
		}
		void operator() (Face& face) const
		{
			if (!m_done) {
				m_done = true;
				m_shader = face.GetShader();
			}
		}
};

void Scene_BrushGetShader_Selected (scene::Graph& graph, std::string& shader)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		BrushInstance* brush = Instance_getBrush(GlobalSelectionSystem().ultimateSelected());
		if (brush != 0) {
			Brush_forEachFace(*brush, FaceGetShader(shader));
		}
	}
}

void Scene_BrushGetShader_Component_Selected (scene::Graph& graph, std::string& shader)
{
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		shader = faceInstance.getFace().GetShader();
	}
}

void Select_MakeDetail ()
{
	UndoableCommand undo("brushSetDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), true);
}

void Select_MakeStructural ()
{
	UndoableCommand undo("brushClearDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), false);
}

class BrushMakeSided
{
		std::size_t m_count;
	public:
		BrushMakeSided (std::size_t count) :
			m_count(count)
		{
		}
		void set ()
		{
			Scene_BrushConstructPrefab(GlobalSceneGraph(), eBrushPrism, m_count, GlobalTextureBrowser().getSelectedShader(
					));
		}
		typedef MemberCaller<BrushMakeSided, &BrushMakeSided::set> SetCaller;
};

BrushMakeSided g_brushmakesided3(3);
BrushMakeSided g_brushmakesided4(4);
BrushMakeSided g_brushmakesided5(5);
BrushMakeSided g_brushmakesided6(6);
BrushMakeSided g_brushmakesided7(7);
BrushMakeSided g_brushmakesided8(8);
BrushMakeSided g_brushmakesided9(9);

#include "radiant_i18n.h"
#include <gdk/gdkkeysyms.h>
#include "gtkutil/dialog.h"
#include "scenelib.h"
#include "../brush/brushmanip.h"
#include "../sidebar/sidebar.h"
#include "../ui/brush/QuerySidesDialog.h"

class BrushPrefab
{
	private:
		EBrushPrefab m_type;

	public:
		BrushPrefab (EBrushPrefab type) :
			m_type(type)
		{
		}

		void set ()
		{
			int sides = 0;
			if (m_type != eBrushTerrain) {
				ui::QuerySidesDialog dialog(3, 9999);
				sides = dialog.queryNumberOfSides();
			}

			if (sides != -1)
				Scene_BrushConstructPrefab(GlobalSceneGraph(), m_type, sides, GlobalTextureBrowser().getSelectedShader());
		}
		typedef MemberCaller<BrushPrefab, &BrushPrefab::set> SetCaller;
};

static BrushPrefab g_brushprism(eBrushPrism);
static BrushPrefab g_brushcone(eBrushCone);
static BrushPrefab g_brushsphere(eBrushSphere);
static BrushPrefab g_brushrock(eBrushRock);
static BrushPrefab g_brushterrain(eBrushTerrain);

void ClipSelected ()
{
	if (GlobalClipper().clipMode()) {
		UndoableCommand undo("clipperClip");
		GlobalClipper().clip();
	}
}

void SplitSelected ()
{
	if (GlobalClipper().clipMode()) {
		UndoableCommand undo("clipperSplit");
		GlobalClipper().splitClip();
	}
}

void FlipClipper ()
{
	GlobalClipper().flipClip();
}

void Brush_registerCommands ()
{
	GlobalEventManager().addRegistryToggle("TogTexLock", RKEY_ENABLE_TEXTURE_LOCK);

	GlobalEventManager().addCommand("BrushPrism", BrushPrefab::SetCaller(g_brushprism));
	GlobalEventManager().addCommand("BrushCone", BrushPrefab::SetCaller(g_brushcone));
	GlobalEventManager().addCommand("BrushSphere", BrushPrefab::SetCaller(g_brushsphere));
	GlobalEventManager().addCommand("BrushRock", BrushPrefab::SetCaller(g_brushrock));
	GlobalEventManager().addCommand("BrushTerrain", BrushPrefab::SetCaller(g_brushterrain));

	GlobalEventManager().addCommand("Brush3Sided", BrushMakeSided::SetCaller(g_brushmakesided3));
	GlobalEventManager().addCommand("Brush4Sided", BrushMakeSided::SetCaller(g_brushmakesided4));
	GlobalEventManager().addCommand("Brush5Sided", BrushMakeSided::SetCaller(g_brushmakesided5));
	GlobalEventManager().addCommand("Brush6Sided", BrushMakeSided::SetCaller(g_brushmakesided6));
	GlobalEventManager().addCommand("Brush7Sided", BrushMakeSided::SetCaller(g_brushmakesided7));
	GlobalEventManager().addCommand("Brush8Sided", BrushMakeSided::SetCaller(g_brushmakesided8));
	GlobalEventManager().addCommand("Brush9Sided", BrushMakeSided::SetCaller(g_brushmakesided9));

	GlobalEventManager().addCommand("ClipSelected", FreeCaller<ClipSelected>());
	GlobalEventManager().addCommand("SplitSelected", FreeCaller<SplitSelected>());
	GlobalEventManager().addCommand("FlipClip", FreeCaller<FlipClipper>());

	GlobalEventManager().addCommand("MakeDetail", FreeCaller<Select_MakeDetail>());
	GlobalEventManager().addCommand("MakeStructural", FreeCaller<Select_MakeStructural>());
}
