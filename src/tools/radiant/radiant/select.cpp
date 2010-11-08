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

#include "select.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include "ientity.h"
#include "iselection.h"
#include "iundo.h"
#include "igrid.h"

#include <vector>

#include "stream/stringstream.h"
#include "signal/isignal.h"
#include "shaderlib.h"
#include "scenelib.h"

#include "gtkutil/idledraw.h"
#include "gtkutil/dialog.h"
#include "gtkutil/widget.h"
#include "brush/brushmanip.h"
#include "brush/brush.h"
#include "selection.h"
#include "sidebar/sidebar.h"
#include "gtkmisc.h"
#include "mainframe.h"
#include "map/map.h"
#include "selection/SceneWalkers.h"

static WorkZone g_select_workzone;

#include "selection/algorithm/SelectionPolicies.h"

class ExpandSelectionToEntitiesWalker: public scene::Graph::Walker
{
		mutable std::size_t m_depth;
	public:
		ExpandSelectionToEntitiesWalker (void) :
			m_depth(0)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			++m_depth;
			if (m_depth == 2) { /* entity depth */
				/* traverse and select children if any one is selected */
				if (instance.childSelected())
					Instance_setSelected(instance, true);
				return Node_getEntity(path.top())->isContainer() && instance.childSelected();
			} else if (m_depth == 3) { /* primitive depth */
				Instance_setSelected(instance, true);
				return false;
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			--m_depth;
		}
};

void Scene_ExpandSelectionToEntities (void)
{
	GlobalSceneGraph().traverse(ExpandSelectionToEntitiesWalker());
}

namespace {
void Selection_UpdateWorkzone (void)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		Select_GetBounds(g_select_workzone.min, g_select_workzone.max);
	}
}
typedef FreeCaller<Selection_UpdateWorkzone> SelectionUpdateWorkzoneCaller;

IdleDraw g_idleWorkzone = IdleDraw(SelectionUpdateWorkzoneCaller());
}

const WorkZone& Select_getWorkZone (void)
{
	g_idleWorkzone.flush();
	return g_select_workzone;
}

void UpdateWorkzone_ForSelection (void)
{
	g_idleWorkzone.queueDraw();
}

// update the workzone to the current selection
void UpdateWorkzone_ForSelectionChanged (const Selectable& selectable)
{
	if (selectable.isSelected()) {
		UpdateWorkzone_ForSelection();
	}
}

void Select_SetShader (const char* shader)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushSetShader_Selected(GlobalSceneGraph(), shader);
	}
	Scene_BrushSetShader_Component_Selected(GlobalSceneGraph(), shader);
}

void Select_SetTexdef (const TextureProjection& projection)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushSetTexdef_Selected(GlobalSceneGraph(), projection);
	}
	Scene_BrushSetTexdef_Component_Selected(GlobalSceneGraph(), projection);
}

/**
 * @todo Set contentflags for whole brush when we are in face selection mode
 */
void Select_SetFlags (const ContentsFlagsValue& flags)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushSetFlags_Selected(GlobalSceneGraph(), flags);
	}
	Scene_BrushSetFlags_Component_Selected(GlobalSceneGraph(), flags);
}

void Select_GetBounds (Vector3& mins, Vector3& maxs)
{
	AABB bounds;
	GlobalSceneGraph().traverse(BoundsSelected(bounds));
	if (bounds.isValid()) {
		maxs = bounds.origin + bounds.extents;
		mins = bounds.origin - bounds.extents;
	} else {
		maxs = Vector3(0,0,0);
		mins = Vector3(0,0,0);
	}
}

void Select_GetMid (Vector3& mid)
{
	AABB bounds;
	GlobalSceneGraph().traverse(BoundsSelected(bounds));
	mid = vector3_snapped(bounds.origin);
}

void Select_FlipAxis (int axis)
{
	Vector3 flip(1, 1, 1);
	flip[axis] = -1;
	GlobalSelectionSystem().scaleSelected(flip);
}

enum axis_t
{
	eAxisX = 0, eAxisY = 1, eAxisZ = 2
};

enum sign_t
{
	eSignPositive = 1, eSignNegative = -1
};

inline Matrix4 matrix4_rotation_for_axis90 (axis_t axis, sign_t sign)
{
	switch (axis) {
	case eAxisX:
		if (sign == eSignPositive) {
			return matrix4_rotation_for_sincos_x(1, 0);
		} else {
			return matrix4_rotation_for_sincos_x(-1, 0);
		}
	case eAxisY:
		if (sign == eSignPositive) {
			return matrix4_rotation_for_sincos_y(1, 0);
		} else {
			return matrix4_rotation_for_sincos_y(-1, 0);
		}
	default://case eAxisZ:
		if (sign == eSignPositive) {
			return matrix4_rotation_for_sincos_z(1, 0);
		} else {
			return matrix4_rotation_for_sincos_z(-1, 0);
		}
	}
}

inline Quaternion quaternion_for_axis90 (axis_t axis, sign_t sign)
{
	switch (axis) {
	case eAxisX:
		if (sign == eSignPositive) {
			return Quaternion(c_half_sqrt2f, 0, 0, c_half_sqrt2f);
		} else {
			return Quaternion(-c_half_sqrt2f, 0, 0, -c_half_sqrt2f);
		}
	case eAxisY:
		if (sign == eSignPositive) {
			return Quaternion(0, c_half_sqrt2f, 0, c_half_sqrt2f);
		} else {
			return Quaternion(0, -c_half_sqrt2f, 0, -c_half_sqrt2f);
		}
	default://case eAxisZ:
		if (sign == eSignPositive) {
			return Quaternion(0, 0, c_half_sqrt2f, c_half_sqrt2f);
		} else {
			return Quaternion(0, 0, -c_half_sqrt2f, -c_half_sqrt2f);
		}
	}
}

void Select_RotateAxis (int axis, float deg)
{
	if (fabs(deg) == 90.f) {
		GlobalSelectionSystem().rotateSelected(quaternion_for_axis90((axis_t) axis, (deg > 0) ? eSignPositive
				: eSignNegative));
	} else {
		switch (axis) {
		case 0:
			GlobalSelectionSystem().rotateSelected(quaternion_for_matrix4_rotation(matrix4_rotation_for_x_degrees(deg)));
			break;
		case 1:
			GlobalSelectionSystem().rotateSelected(quaternion_for_matrix4_rotation(matrix4_rotation_for_y_degrees(deg)));
			break;
		case 2:
			GlobalSelectionSystem().rotateSelected(quaternion_for_matrix4_rotation(matrix4_rotation_for_z_degrees(deg)));
			break;
		}
	}
}

void Select_ShiftTexture (float x, float y)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushShiftTexdef_Selected(GlobalSceneGraph(), x, y);
	}
	Scene_BrushShiftTexdef_Component_Selected(GlobalSceneGraph(), x, y);
}

void Select_ScaleTexture (float x, float y)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushScaleTexdef_Selected(GlobalSceneGraph(), x, y);
	}
	Scene_BrushScaleTexdef_Component_Selected(GlobalSceneGraph(), x, y);
}

void Select_RotateTexture (float amt)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushRotateTexdef_Selected(GlobalSceneGraph(), amt);
	}
	Scene_BrushRotateTexdef_Component_Selected(GlobalSceneGraph(), amt);
}

void Select_FlipTexture(unsigned int flipAxis) {
	UndoableCommand undo("flipTexture");

	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		// Flip the texture of all the brushes (selected as a whole)
		Scene_BrushFlipTexture_Selected(flipAxis);
		//Scene_PatchScaleTexture_Selected(GlobalSceneGraph(), x, y);
	}
	// Now flip all the seperately selected faces
	Scene_BrushFlipTexture_Component_Selected(flipAxis);
	SceneChangeNotify();
}

typedef std::vector<std::string> Classnames;

class EntityFindByClassnameWalker: public scene::Graph::Walker
{
	private:
		const Classnames& m_classnames;

		bool classnames_match_entity (Entity &entity) const
		{
			for (Classnames::const_iterator i = m_classnames.begin(); i != m_classnames.end(); ++i) {
				if (entity.getKeyValue("classname") == *i) {
					return true;
				}
			}
			return false;
		}

	public:
		EntityFindByClassnameWalker (const Classnames& classnames) :
			m_classnames(classnames)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Entity* entity = Node_getEntity(path.top());
			if (entity != 0 && classnames_match_entity(*entity)) {
				Instance_getSelectable(instance)->setSelected(true);
			}
			return true;
		}
};

void Scene_EntitySelectByClassnames (scene::Graph& graph, const Classnames& classnames)
{
	graph.traverse(EntityFindByClassnameWalker(classnames));
}

class EntityGetSelectedClassnamesWalker: public scene::Graph::Walker
{
		Classnames& m_classnames;
	public:
		EntityGetSelectedClassnamesWalker (Classnames& classnames) :
			m_classnames(classnames)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable != 0 && selectable->isSelected()) {
				Entity* entity = Node_getEntity(path.top());
				if (entity != 0) {
					m_classnames.push_back(entity->getKeyValue("classname"));
				}
			}
			return true;
		}
};

void Scene_EntityGetClassnames (scene::Graph& graph, Classnames& classnames)
{
	graph.traverse(EntityGetSelectedClassnamesWalker(classnames));
}

/**
 * @brief Callback selects all faces with same texure as currently selected face.
 *
 */
void Select_AllFacesWithTexture (void)
{
	std::string name;
	Scene_BrushGetShader_Component_Selected(GlobalSceneGraph(), name);
	if (!name.empty()) {
		GlobalSelectionSystem().setSelectedAllComponents(false);
		Scene_BrushFacesSelectByShader_Component(GlobalSceneGraph(), name);
	}
}

void Select_AllOfType (void)
{
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		if (GlobalSelectionSystem().ComponentMode() == SelectionSystem::eFace) {
			GlobalSelectionSystem().setSelectedAllComponents(false);
			Scene_BrushSelectByShader_Component(GlobalSceneGraph(), GlobalTextureBrowser().getSelectedShader());
		}
	} else {
		Classnames classnames;
		Scene_EntityGetClassnames(GlobalSceneGraph(), classnames);
		GlobalSelectionSystem().setSelectedAll(false);
		if (!classnames.empty()) {
			Scene_EntitySelectByClassnames(GlobalSceneGraph(), classnames);
		} else {
			Scene_BrushSelectByShader(GlobalSceneGraph(), GlobalTextureBrowser().getSelectedShader());
		}
	}
}

void Select_FitTexture (float horizontal, float vertical)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushFitTexture_Selected(GlobalSceneGraph(), horizontal, vertical);
	}
	Scene_BrushFitTexture_Component_Selected(GlobalSceneGraph(), horizontal, vertical);

	SceneChangeNotify();
}

inline void hide_node (scene::Node& node, bool hide)
{
	if (hide)
		node.enable(scene::Node::eHidden);
	else
		node.disable(scene::Node::eHidden);
}

class HideSelectedWalker: public scene::Graph::Walker
{
		bool m_hide;
	public:
		HideSelectedWalker (bool hide) :
			m_hide(hide)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable != 0 && selectable->isSelected()) {
				hide_node(path.top(), m_hide);
			}
			return true;
		}
};

void Scene_Hide_Selected (bool hide)
{
	GlobalSceneGraph().traverse(HideSelectedWalker(hide));
}

void Select_Hide (void)
{
	Scene_Hide_Selected(true);
	SceneChangeNotify();
}

void HideSelected (void)
{
	Select_Hide();
	GlobalSelectionSystem().setSelectedAll(false);
}

class HideAllWalker: public scene::Graph::Walker
{
		bool m_hide;
	public:
		HideAllWalker (bool hide) :
			m_hide(hide)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			hide_node(path.top(), m_hide);
			return true;
		}
};

void Scene_Hide_All (bool hide)
{
	GlobalSceneGraph().traverse(HideAllWalker(hide));
}

void Select_ShowAllHidden (void)
{
	Scene_Hide_All(false);
	SceneChangeNotify();
}

void Selection_Flipx (void)
{
	UndoableCommand undo("mirrorSelected -axis x");
	Select_FlipAxis(0);
}

void Selection_Flipy (void)
{
	UndoableCommand undo("mirrorSelected -axis y");
	Select_FlipAxis(1);
}

void Selection_Flipz (void)
{
	UndoableCommand undo("mirrorSelected -axis z");
	Select_FlipAxis(2);
}

void Selection_Rotatex (void)
{
	UndoableCommand undo("rotateSelected -axis x -angle -90");
	Select_RotateAxis(0, -90);
}

void Selection_Rotatey (void)
{
	UndoableCommand undo("rotateSelected -axis y -angle 90");
	Select_RotateAxis(1, 90);
}

void Selection_Rotatez (void)
{
	UndoableCommand undo("rotateSelected -axis z -angle -90");
	Select_RotateAxis(2, -90);
}

void Nudge (int nDim, float fNudge)
{
	Vector3 translate(0, 0, 0);
	translate[nDim] = fNudge;

	GlobalSelectionSystem().translateSelected(translate);
}

void Selection_NudgeZ (float amount)
{
	std::stringstream command;
	command << "nudgeSelected -axis z -amount " << amount;
	UndoableCommand undo(command.str());

	Nudge(2, amount);
}

void Selection_MoveDown (void)
{
	Selection_NudgeZ(-GlobalGrid().getGridSize());
}

void Selection_MoveUp (void)
{
	Selection_NudgeZ(GlobalGrid().getGridSize());
}

void SceneSelectionChange (const Selectable& selectable)
{
	SceneChangeNotify();
}

SignalHandlerId Selection_boundsChanged;

void Selection_Construct (void)
{
	typedef FreeCaller1<const Selectable&, SceneSelectionChange> SceneSelectionChangeCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(SceneSelectionChangeCaller());
	typedef FreeCaller1<const Selectable&, UpdateWorkzone_ForSelectionChanged> UpdateWorkzoneForSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(UpdateWorkzoneForSelectionChangedCaller());
	typedef FreeCaller<UpdateWorkzone_ForSelection> UpdateWorkzoneForSelectionCaller;
	Selection_boundsChanged = GlobalSceneGraph().addBoundsChangedCallback(UpdateWorkzoneForSelectionCaller());
}

void Selection_Destroy (void)
{
	GlobalSceneGraph().removeBoundsChangedCallback(Selection_boundsChanged);
}
