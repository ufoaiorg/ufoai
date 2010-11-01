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

/**
 Loops over all selected brushes and stores their
 world AABBs in the specified array.
 */
class CollectSelectedBrushesBounds: public SelectionSystem::Visitor
{
		AABB* m_bounds; // array of AABBs
		Unsigned m_max; // max AABB-elements in array
		Unsigned& m_count;// count of valid AABBs stored in array

	public:
		CollectSelectedBrushesBounds (AABB* bounds, Unsigned max, Unsigned& count) :
			m_bounds(bounds), m_max(max), m_count(count)
		{
			m_count = 0;
		}

		void visit (scene::Instance& instance) const
		{
			ASSERT_MESSAGE(m_count <= m_max, "Invalid m_count in CollectSelectedBrushesBounds");

			// stop if the array is already full
			if (m_count == m_max)
				return;

			Selectable* selectable = Instance_getSelectable(instance);
			if ((selectable != 0) && instance.isSelected()) {
				// brushes only
				if (Instance_getBrush(instance) != 0) {
					m_bounds[m_count] = instance.worldAABB();
					++m_count;
				}
			}
		}
};

/**
 Selects all objects that intersect one of the bounding AABBs.
 The exact intersection-method is specified through TSelectionPolicy
 */
template<class TSelectionPolicy>
class SelectByBounds: public scene::Graph::Walker
{
		AABB* m_aabbs; // selection aabbs
		Unsigned m_count; // number of aabbs in m_aabbs
		TSelectionPolicy policy; // type that contains a custom intersection method aabb<->aabb

	public:
		SelectByBounds (AABB* aabbs, Unsigned count) :
			m_aabbs(aabbs), m_count(count)
		{
		}

		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Selectable* selectable = Instance_getSelectable(instance);

			/* ignore worldspawn */
			Entity* entity = Node_getEntity(path.top());
			if (entity) {
				if (string_equal(entity->getKeyValue("classname"), "worldspawn"))
					return true;
			}

			if (path.size() > 1 && !path.top().get().isRoot() && selectable != 0) {
				for (Unsigned i = 0; i < m_count; ++i) {
					if (policy.Evaluate(m_aabbs[i], instance)) {
						selectable->setSelected(true);
					}
				}
			}

			return true;
		}

		/**
		 Performs selection operation on the global scenegraph.
		 If delete_bounds_src is true, then the objects which were
		 used as source for the selection aabbs will be deleted.
		 */
		static void DoSelection (bool delete_bounds_src = true)
		{
			if (GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive) {
				// we may not need all AABBs since not all selected objects have to be brushes
				const Unsigned max = (Unsigned) GlobalSelectionSystem().countSelected();
				AABB* aabbs = new AABB[max];

				Unsigned count;
				CollectSelectedBrushesBounds collector(aabbs, max, count);
				GlobalSelectionSystem().foreachSelected(collector);

				// nothing usable in selection
				if (!count) {
					delete[] aabbs;
					return;
				}

				// delete selected objects
				if (delete_bounds_src) { // see deleteSelection
					UndoableCommand undo("deleteSelected");
					Select_Delete();
				}

				// select objects with bounds
				GlobalSceneGraph().traverse(SelectByBounds<TSelectionPolicy> (aabbs, count));

				SceneChangeNotify();
				delete[] aabbs;
			}
		}
};

/**
 SelectionPolicy for SelectByBounds
 Returns true if box and the AABB of instance intersect
 */
class SelectionPolicy_Touching
{
	public:
		bool Evaluate (const AABB& box, scene::Instance& instance) const
		{
			const AABB& other(instance.worldAABB());
			for (int i = 0; i < 3; ++i) {
				if (fabsf(box.origin[i] - other.origin[i]) > (box.extents[i] + other.extents[i]))
					return false;
			}
			return true;
		}
};

/**
 SelectionPolicy for SelectByBounds
 Returns true if the AABB of instance is inside box
 */
class SelectionPolicy_Inside
{
	public:
		bool Evaluate (const AABB& box, scene::Instance& instance) const
		{
			const AABB& other(instance.worldAABB());
			for (int i = 0; i < 3; ++i) {
				if (fabsf(box.origin[i] - other.origin[i]) > (box.extents[i] - other.extents[i]))
					return false;
			}
			return true;
		}
};

class DeleteSelected: public scene::Graph::Walker
{
		mutable bool m_remove;
		mutable bool m_removedChild;
	public:
		DeleteSelected () :
			m_remove(false), m_removedChild(false)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			m_removedChild = false;

			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable != 0 && selectable->isSelected() && path.size() > 1 && !path.top().get().isRoot()) {
				m_remove = true;
				/* don't traverse into child elements */
				return false;
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{

			if (m_removedChild) {
				m_removedChild = false;

				// delete empty entities
				Entity* entity = Node_getEntity(path.top());
				if (entity != 0 && path.top().get_pointer() != Map_FindWorldspawn(g_map) && Node_getTraversable(
						path.top())->empty()) {
					Path_deleteTop(path);
				}
			}

			// node should be removed
			if (m_remove) {
				if (Node_isEntity(path.parent()) != 0) {
					m_removedChild = true;
				}

				m_remove = false;
				Path_deleteTop(path);
			}
		}
};

void Scene_DeleteSelected (scene::Graph& graph)
{
	graph.traverse(DeleteSelected());
	SceneChangeNotify();
}

void Select_Delete (void)
{
	Scene_DeleteSelected(GlobalSceneGraph());
}

class InvertSelectionWalker: public scene::Graph::Walker
{
		SelectionSystem::EMode m_mode;
		mutable Selectable* m_selectable;
	public:
		InvertSelectionWalker (SelectionSystem::EMode mode) :
			m_mode(mode), m_selectable(0)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable) {
				switch (m_mode) {
				case SelectionSystem::eEntity:
					if (Node_isEntity(path.top()) != 0) {
						m_selectable = path.top().get().visible() ? selectable : 0;
					}
					break;
				case SelectionSystem::ePrimitive:
					m_selectable = path.top().get().visible() ? selectable : 0;
					break;
				case SelectionSystem::eComponent:
					// Check if we have a componentselectiontestable instance
					ComponentSelectionTestable* compSelTestable = Instance_getComponentSelectionTestable(instance);

					// Only add it to the list if the instance has components and is already selected
					if (compSelTestable && selectable->isSelected()) {
						m_selectable = path.top().get().visible() ? selectable : 0;
					}
					break;
				}
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			if (m_selectable != 0) {
				m_selectable->invertSelected();
				m_selectable = 0;
			}
		}
};

void Scene_Invert_Selection (scene::Graph& graph)
{
	graph.traverse(InvertSelectionWalker(GlobalSelectionSystem().Mode()));
}

void Select_Invert (void)
{
	Scene_Invert_Selection(GlobalSceneGraph());
}

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
	maxs = bounds.origin + bounds.extents;
	mins = bounds.origin - bounds.extents;
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

void Select_Scale (float x, float y, float z)
{
	GlobalSelectionSystem().scaleSelected(Vector3(x, y, z));
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

inline void matrix4_rotate_by_axis90 (Matrix4& matrix, axis_t axis, sign_t sign)
{
	matrix4_multiply_by_matrix4(matrix, matrix4_rotation_for_axis90(axis, sign));
}

inline void matrix4_pivoted_rotate_by_axis90 (Matrix4& matrix, axis_t axis, sign_t sign, const Vector3& pivotpoint)
{
	matrix.translateBy(pivotpoint);
	matrix4_rotate_by_axis90(matrix, axis, sign);
	matrix.translateBy(-pivotpoint);
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

/**
 * @note TTimo modified to handle shader architecture:
 * expects shader names at input, comparison relies on shader names .. texture names no longer relevant
 */
void FindReplaceTextures (const std::string& pFind, const std::string& pReplace, bool bSelected)
{
	if (!texdef_name_valid(pFind)) {
		g_error("FindReplaceTextures: invalid texture name: '%s', aborted\n", pFind.c_str());
		return;
	}
	if (!texdef_name_valid(pReplace)) {
		g_error("FindReplaceTextures: invalid texture name: '%s', aborted\n", pReplace.c_str());
		return;
	}

	UndoableCommand undo("textureFindReplace -find " + pFind + " -replace " + pReplace);

	if (bSelected) {
		if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
			Scene_BrushFindReplaceShader_Selected(GlobalSceneGraph(), pFind, pReplace);
		}
		Scene_BrushFindReplaceShader_Component_Selected(GlobalSceneGraph(), pFind, pReplace);
	} else {
		Scene_BrushFindReplaceShader(GlobalSceneGraph(), pFind, pReplace);
	}
}

typedef std::vector<const char*> Classnames;

class EntityFindByClassnameWalker: public scene::Graph::Walker
{
	private:
		const Classnames& m_classnames;

		bool classnames_match_entity (Entity &entity) const
		{
			for (Classnames::const_iterator i = m_classnames.begin(); i != m_classnames.end(); ++i) {
				if (string_equal(entity.getKeyValue("classname"), *i)) {
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
		g_message("Searching all faces with texture '%s'\n", name.c_str());
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

void Select_Inside (void)
{
	SelectByBounds<SelectionPolicy_Inside>::DoSelection();
}

void Select_Touching (void)
{
	SelectByBounds<SelectionPolicy_Touching>::DoSelection(false);
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

#include <gtk/gtkbox.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gdk/gdkkeysyms.h>

inline Quaternion quaternion_for_euler_xyz_degrees (const Vector3& eulerXYZ)
{
	const double cx = cos(degrees_to_radians(eulerXYZ[0] * 0.5));
	const double sx = sin(degrees_to_radians(eulerXYZ[0] * 0.5));
	const double cy = cos(degrees_to_radians(eulerXYZ[1] * 0.5));
	const double sy = sin(degrees_to_radians(eulerXYZ[1] * 0.5));
	const double cz = cos(degrees_to_radians(eulerXYZ[2] * 0.5));
	const double sz = sin(degrees_to_radians(eulerXYZ[2] * 0.5));

	return Quaternion(cz * cy * sx - sz * sy * cx, cz * sy * cx + sz * cy * sx, sz * cy * cx - cz * sy * sx, cz * cy
			* cx + sz * sy * sx);
}

struct RotateDialog
{
		GtkSpinButton* x;
		GtkSpinButton* y;
		GtkSpinButton* z;
		GtkWindow *window;
};

static gboolean rotatedlg_apply (GtkWidget *widget, RotateDialog* rotateDialog)
{
	Vector3 eulerXYZ;

	eulerXYZ[0] = static_cast<float> (gtk_spin_button_get_value(rotateDialog->x));
	eulerXYZ[1] = static_cast<float> (gtk_spin_button_get_value(rotateDialog->y));
	eulerXYZ[2] = static_cast<float> (gtk_spin_button_get_value(rotateDialog->z));

	std::stringstream command;
	command << "rotateSelectedEulerXYZ -x " << eulerXYZ[0] << " -y " << eulerXYZ[1] << " -z " << eulerXYZ[2];
	UndoableCommand undo(command.str());

	GlobalSelectionSystem().rotateSelected(quaternion_for_euler_xyz_degrees(eulerXYZ));
	return TRUE;
}

static gboolean rotatedlg_cancel (GtkWidget *widget, RotateDialog* rotateDialog)
{
	gtk_widget_hide(GTK_WIDGET(rotateDialog->window));

	gtk_spin_button_set_value(rotateDialog->x, 0.0f); // reset to 0 on close
	gtk_spin_button_set_value(rotateDialog->y, 0.0f);
	gtk_spin_button_set_value(rotateDialog->z, 0.0f);

	return TRUE;
}

static gboolean rotatedlg_ok (GtkWidget *widget, RotateDialog* rotateDialog)
{
	rotatedlg_apply(widget, rotateDialog);
	rotatedlg_cancel(widget, rotateDialog);
	return TRUE;
}

static gboolean rotatedlg_delete (GtkWidget *widget, GdkEventAny *event, RotateDialog* rotateDialog)
{
	rotatedlg_cancel(widget, rotateDialog);
	return TRUE;
}

static RotateDialog g_rotate_dialog;

void DoRotateDlg (void)
{
	if (g_rotate_dialog.window == NULL) {
		g_rotate_dialog.window = create_dialog_window(GlobalRadiant().getMainWindow(), _("Arbitrary rotation"),
				G_CALLBACK(rotatedlg_delete), &g_rotate_dialog);

		GtkAccelGroup* accel = gtk_accel_group_new();
		gtk_window_add_accel_group(g_rotate_dialog.window, accel);

		{
			GtkHBox* hbox = create_dialog_hbox(4, 4);
			gtk_container_add(GTK_CONTAINER(g_rotate_dialog.window), GTK_WIDGET(hbox));
			{
				GtkTable* table = create_dialog_table(3, 2, 4, 4);
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), TRUE, TRUE, 0);
				{
					GtkWidget* label = gtk_label_new(_("  X  "));
					gtk_widget_show(label);
					gtk_table_attach(table, label, 0, 1, 0, 1, (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
				}
				{
					GtkWidget* label = gtk_label_new(_("  Y  "));
					gtk_widget_show(label);
					gtk_table_attach(table, label, 0, 1, 1, 2, (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
				}
				{
					GtkWidget* label = gtk_label_new(_("  Z  "));
					gtk_widget_show(label);
					gtk_table_attach(table, label, 0, 1, 2, 3, (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
				}
				{
					GtkAdjustment* adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, -359, 359, 1, 10, 0));
					GtkSpinButton* spin = GTK_SPIN_BUTTON(gtk_spin_button_new(adj, 1, 0));
					gtk_widget_show(GTK_WIDGET(spin));
					gtk_table_attach(table, GTK_WIDGET(spin), 1, 2, 0, 1, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
							(GtkAttachOptions) (0), 0, 0);
					gtk_spin_button_set_wrap(spin, TRUE);

					gtk_widget_grab_focus(GTK_WIDGET(spin));

					g_rotate_dialog.x = spin;
				}
				{
					GtkAdjustment* adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, -359, 359, 1, 10, 0));
					GtkSpinButton* spin = GTK_SPIN_BUTTON(gtk_spin_button_new(adj, 1, 0));
					gtk_widget_show(GTK_WIDGET(spin));
					gtk_table_attach(table, GTK_WIDGET(spin), 1, 2, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
							(GtkAttachOptions) (0), 0, 0);
					gtk_spin_button_set_wrap(spin, TRUE);

					g_rotate_dialog.y = spin;
				}
				{
					GtkAdjustment* adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, -359, 359, 1, 10, 0));
					GtkSpinButton* spin = GTK_SPIN_BUTTON(gtk_spin_button_new(adj, 1, 0));
					gtk_widget_show(GTK_WIDGET(spin));
					gtk_table_attach(table, GTK_WIDGET(spin), 1, 2, 2, 3, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
							(GtkAttachOptions) (0), 0, 0);
					gtk_spin_button_set_wrap(spin, TRUE);

					g_rotate_dialog.z = spin;
				}
			}
			{
				GtkVBox* vbox = create_dialog_vbox(4);
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);
				{
					GtkButton* button = create_dialog_button(_("OK"), G_CALLBACK(rotatedlg_ok), &g_rotate_dialog);
					gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
					widget_make_default(GTK_WIDGET(button));
					gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Return, (GdkModifierType) 0,
							(GtkAccelFlags) 0);
				}
				{
					GtkButton* button = create_dialog_button(_("Cancel"), G_CALLBACK(rotatedlg_cancel),
							&g_rotate_dialog);
					gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
					gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Escape, (GdkModifierType) 0,
							(GtkAccelFlags) 0);
				}
				{
					GtkButton* button = create_dialog_button(_("Apply"), G_CALLBACK(rotatedlg_apply), &g_rotate_dialog);
					gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
				}
			}
		}
	}

	gtk_widget_show(GTK_WIDGET(g_rotate_dialog.window));
}

struct ScaleDialog
{
		GtkWidget* x;
		GtkWidget* y;
		GtkWidget* z;
		GtkWindow *window;
};

static gboolean scaledlg_apply (GtkWidget *widget, ScaleDialog* scaleDialog)
{
	float sx, sy, sz;

	sx = static_cast<float> (atof(gtk_entry_get_text(GTK_ENTRY (scaleDialog->x))));
	sy = static_cast<float> (atof(gtk_entry_get_text(GTK_ENTRY (scaleDialog->y))));
	sz = static_cast<float> (atof(gtk_entry_get_text(GTK_ENTRY (scaleDialog->z))));

	std::stringstream command;
	command << "scaleSelected -x " << sx << " -y " << sy << " -z " << sz;
	UndoableCommand undo(command.str());

	Select_Scale(sx, sy, sz);

	return TRUE;
}

static gboolean scaledlg_cancel (GtkWidget *widget, ScaleDialog* scaleDialog)
{
	gtk_widget_hide(GTK_WIDGET(scaleDialog->window));

	gtk_entry_set_text(GTK_ENTRY(scaleDialog->x), "1.0");
	gtk_entry_set_text(GTK_ENTRY(scaleDialog->y), "1.0");
	gtk_entry_set_text(GTK_ENTRY(scaleDialog->z), "1.0");

	return TRUE;
}

static gboolean scaledlg_ok (GtkWidget *widget, ScaleDialog* scaleDialog)
{
	scaledlg_apply(widget, scaleDialog);
	scaledlg_cancel(widget, scaleDialog);
	return TRUE;
}

static gboolean scaledlg_delete (GtkWidget *widget, GdkEventAny *event, ScaleDialog* scaleDialog)
{
	scaledlg_cancel(widget, scaleDialog);
	return TRUE;
}

static ScaleDialog g_scale_dialog;

void DoScaleDlg (void)
{
	if (g_scale_dialog.window == NULL) {
		g_scale_dialog.window = create_dialog_window(GlobalRadiant().getMainWindow(), _("Arbitrary scale"),
				G_CALLBACK(scaledlg_delete), &g_scale_dialog);

		GtkAccelGroup* accel = gtk_accel_group_new();
		gtk_window_add_accel_group(g_scale_dialog.window, accel);

		{
			GtkHBox* hbox = create_dialog_hbox(4, 4);
			gtk_container_add(GTK_CONTAINER(g_scale_dialog.window), GTK_WIDGET(hbox));
			{
				GtkTable* table = create_dialog_table(3, 2, 4, 4);
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), TRUE, TRUE, 0);
				{
					GtkWidget* label = gtk_label_new(_("  X  "));
					gtk_widget_show(label);
					gtk_table_attach(table, label, 0, 1, 0, 1, (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
				}
				{
					GtkWidget* label = gtk_label_new(_("  Y  "));
					gtk_widget_show(label);
					gtk_table_attach(table, label, 0, 1, 1, 2, (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
				}
				{
					GtkWidget* label = gtk_label_new(_("  Z  "));
					gtk_widget_show(label);
					gtk_table_attach(table, label, 0, 1, 2, 3, (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
				}
				{
					GtkWidget* entry = gtk_entry_new();
					gtk_widget_show(entry);
					gtk_entry_set_text(GTK_ENTRY(entry), "1.0");
					gtk_table_attach(table, entry, 1, 2, 0, 1, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
							(GtkAttachOptions) (0), 0, 0);

					gtk_widget_grab_focus(entry);

					g_scale_dialog.x = entry;
				}
				{
					GtkWidget* entry = gtk_entry_new();
					gtk_widget_show(entry);
					gtk_entry_set_text(GTK_ENTRY(entry), "1.0");
					gtk_table_attach(table, entry, 1, 2, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
							(GtkAttachOptions) (0), 0, 0);

					g_scale_dialog.y = entry;
				}
				{
					GtkWidget* entry = gtk_entry_new();
					gtk_widget_show(entry);
					gtk_entry_set_text(GTK_ENTRY(entry), "1.0");
					gtk_table_attach(table, entry, 1, 2, 2, 3, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
							(GtkAttachOptions) (0), 0, 0);

					g_scale_dialog.z = entry;
				}
			}
			{
				GtkVBox* vbox = create_dialog_vbox(4);
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);
				{
					GtkButton* button = create_dialog_button(_("OK"), G_CALLBACK(scaledlg_ok), &g_scale_dialog);
					gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
					widget_make_default(GTK_WIDGET(button));
					gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Return, (GdkModifierType) 0,
							(GtkAccelFlags) 0);
				}
				{
					GtkButton* button = create_dialog_button(_("Cancel"), G_CALLBACK(scaledlg_cancel), &g_scale_dialog);
					gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
					gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Escape, (GdkModifierType) 0,
							(GtkAccelFlags) 0);
				}
				{
					GtkButton* button = create_dialog_button(_("Apply"), G_CALLBACK(scaledlg_apply), &g_scale_dialog);
					gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
				}
			}
		}
	}

	gtk_widget_show(GTK_WIDGET(g_scale_dialog.window));
}
