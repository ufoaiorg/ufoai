#include "RoutingRenderable.h"
#include "math/aabb.h"
#include "math/quaternion.h"
#include "igl.h"
#include "entitylib.h"//for aabb_draw_solid
#include "shared.h"

#include "../filters/LevelFilter.h"

#define UNIT_SIZE_HALF (UNIT_SIZE/2)
#define UNIT_SIZE_QUARTER (UNIT_SIZE/4)
#define UNIT_HEIGHT_HALF (UNIT_HEIGHT/2)
#define UNIT_HEIGHT_QUARTER (UNIT_HEIGHT/4)
#define UNIT_HEIGHT_EIGHTH (UNIT_HEIGHT/8)

namespace routing
{
	const Vector3 color_connected_walk = Vector3(0.0, 1.0, 0.0);
	const Vector3 color_connected_crouch = Vector3(1.0, 1.0, 0.0);
	const Vector3 color_connected_not = Vector3(1.0, 0.0, 0.0);
	const Vector3 color_accessible_stand = Vector3(0.0, 1.0, 0.0);
	const Vector3 color_accessible_crouch = Vector3(1.0, 1.0, 0.0);
	const Vector3 color_accessible_not = Vector3(1.0, 0.0, 0.0);
	const Vector3 color_wireframe = Vector3(1.0, 1.0, 1.0);

	const Vector3 diffCenterToZero = Vector3(-UNIT_SIZE_HALF, -UNIT_SIZE_HALF, -UNIT_HEIGHT_HALF);

	RoutingRenderable::RoutingRenderable ()
	{
		_showAllLowerLevels = true;
		_glListID = 0;
	}

	RoutingRenderable::~RoutingRenderable ()
	{
		for (routing::RoutingRenderableEntries::const_iterator i = _entries.begin(); i != _entries.end(); ++i) {
			delete *i;
		}
		checkClearGLCache();
	}

	void RoutingRenderable::render (RenderStateFlags state) const
	{
		int maxDisplayLevel = GlobalLevelFilter().getCurrentLevel();
		const int minDisplayLevel = _showAllLowerLevels ? 0 : std::max(0, maxDisplayLevel - 1);
		if (maxDisplayLevel == 0)
			maxDisplayLevel = PATHFINDING_HEIGHT;
		if (_glListID != 0) {
			for (int level = minDisplayLevel; level < maxDisplayLevel; level++)
				glCallList(_glListID + level);
		} else {
			_glListID = glGenLists(PATHFINDING_HEIGHT);
			for (int level = 0; level < PATHFINDING_HEIGHT; level++) {
				const bool visible = minDisplayLevel <= level && level < maxDisplayLevel;
				glNewList(_glListID + level, visible ? GL_COMPILE_AND_EXECUTE : GL_COMPILE);
				for (routing::RoutingRenderableEntries::const_iterator i = _entries.begin(); i != _entries.end(); ++i) {
					const routing::RoutingRenderableEntry* entry = *i;
					if (entry->isForLevel(level + 1))
						entry->render(state);
				}
				glEndList();
			}
		}
	}

	void RoutingRenderable::setShowAllLowerLevels (bool showAllLowerLevels)
	{
		_showAllLowerLevels = showAllLowerLevels;
	}

	void RoutingRenderable::add (const RoutingLumpEntry& dataEntry)
	{
		routing::RoutingRenderableEntry* entry = new RoutingRenderableEntry(dataEntry);
		_entries.push_back(entry);
		checkClearGLCache();
	}

	/**
	 * @brief clear associated renderable entries list and delete precached glList if any.
	 */
	void RoutingRenderable::clear (void)
	{
		_entries.clear();
		checkClearGLCache();
	}

	/**
	 * @brief if there is any associated glList instance, delete it so it will be recreated on next rendering.
	 * This method should be called whenever the data changes which should be rendered.
	 */
	void RoutingRenderable::checkClearGLCache (void)
	{
		if (_glListID != 0) {
			glDeleteLists(_glListID, PATHFINDING_HEIGHT);
			_glListID = 0;
		}
	}

	bool RoutingRenderableEntry::isForLevel (const int level) const
	{
		return (_data.getLevel() == level);
	}

	/**
	 * @brief render this renderable entry which represents one grid space by drawing
	 * wireframe, accessibility box and connectivity arrows.
	 * @param state render state flags
	 */
	void RoutingRenderableEntry::render (RenderStateFlags state) const
	{
		this->renderWireframe();
		this->renderAccessability(state);
		if (this->_data.getAccessState() != ACC_DISABLED)
			this->renderConnections();
	}

	/**
	 * @brief render marker wireframe for grid
	 */
	void RoutingRenderableEntry::renderWireframe () const
	{
		glColor3fv(color_wireframe);

		Vector3 dx = Vector3(UNIT_SIZE, 0, 0);
		Vector3 dy = Vector3(0, UNIT_SIZE, 0);
		Vector3 dz = Vector3(0, 0, UNIT_HEIGHT);
		Vector3 dxy = dx + dy;
		/**@todo evaluate why aabb_draw_wire is not working here */
		/* lower and upper corners */
		Vector3 l1 = this->_data.getOrigin() + diffCenterToZero;
		Vector3 l2 = l1 + dx;
		Vector3 l3 = l1 + dxy;
		Vector3 l4 = l1 + dy;
		Vector3 u1 = l1 + dz;
		Vector3 u2 = l2 + dz;
		Vector3 u3 = l3 + dz;
		Vector3 u4 = l4 + dz;

		glBegin(GL_LINE_STRIP);
		/* connect continuing lines from point to point */
		glVertex3fv(l1);
		glVertex3fv(l2);
		glVertex3fv(l3);
		glVertex3fv(l4);
		glVertex3fv(l1);
		glVertex3fv(u1);
		glVertex3fv(u2);
		glVertex3fv(u3);
		glVertex3fv(u4);
		glVertex3fv(u1);
		glEnd();

		glBegin(GL_LINES);
		/* draw missing lines from point to point */
		glVertex3fv(l2);
		glVertex3fv(u2);

		glVertex3fv(l3);
		glVertex3fv(u3);

		glVertex3fv(l4);
		glVertex3fv(u4);

		glEnd();
	}

	/**
	 * @brief Convert EAccessState into color representation
	 * @param state state to display
	 * @return color vector
	 */
	static Vector3 getColorForAccessState (EAccessState state)
	{
		switch (state) {
		case ACC_DISABLED:
			return color_accessible_not;
		case ACC_CROUCH:
			return color_accessible_crouch;
		case ACC_STAND:
			return color_accessible_stand;
		default:
			return color_accessible_not;
		}

	}

	/**
	 * @brief render a box for accessability of this grid part
	 */
	void RoutingRenderableEntry::renderAccessability (RenderStateFlags state) const
	{
		/* center of drawn box */
		const Vector3 color = getColorForAccessState(_data.getAccessState());
		glColor3fv(color);
		AABB box = AABB(this->_data.getOrigin() + Vector3(0, 0, -UNIT_HEIGHT_QUARTER - UNIT_HEIGHT_EIGHTH), Vector3(
				UNIT_SIZE_QUARTER, UNIT_SIZE_QUARTER, UNIT_HEIGHT_EIGHTH));
		aabb_draw_solid(box, state);
	}

	/**
	 * @brief Convert EConnectionState into color representation
	 * @param state state to display
	 * @return color vector
	 */
	static Vector3 getColorForConnectivity (EConnectionState state)
	{
		switch (state) {
		case CON_DISABLE:
			return color_connected_not;
		case CON_CROUCHABLE:
			return color_connected_crouch;
		case CON_WALKABLE:
			return color_connected_walk;
		default:
			return color_connected_not;
		}
	}

	/**
	 * @brief render connection arrow for given direction
	 * @param direction direction to draw arrow for
	 */
	void RoutingRenderableEntry::renderConnection (EDirection direction) const
	{
		const Vector3 color = getColorForConnectivity(_data.getConnectionState(direction));
		glColor3fv(color);

		//vector to center of direction arrows
		Vector3 diffCenter = this->_data.getOrigin();//vector3_added(vector3_scaled(this->_data.getOrigin(), grid_scale),Vector3(UNIT_SIZE_HALF, UNIT_SIZE_HALF, UNIT_HEIGHT_HALF));
		//vector tip
		Vector3 difTip = Vector3(0, -(UNIT_SIZE_HALF), 0);
		//vector base corners
		Vector3 difB1 = Vector3(2, -(UNIT_SIZE_QUARTER), 2);
		Vector3 difB2 = Vector3(2, -(UNIT_SIZE_QUARTER), -2);
		Vector3 difB3 = Vector3(-2, -(UNIT_SIZE_QUARTER), -2);
		Vector3 difB4 = Vector3(-2, -(UNIT_SIZE_QUARTER), 2);

		//rotate tip and base corners around {0,0,0} before translate to center
		Quaternion rotation = quaternion_for_z(degrees_to_radians(direction * 45));

		Vector3 tip = diffCenter + quaternion_transformed_point(rotation, difTip);
		Vector3 b1 = diffCenter + quaternion_transformed_point(rotation, difB1);
		Vector3 b2 = diffCenter + quaternion_transformed_point(rotation, difB2);
		Vector3 b3 = diffCenter + quaternion_transformed_point(rotation, difB3);
		Vector3 b4 = diffCenter + quaternion_transformed_point(rotation, difB4);

		/* draw arrow as connected triangles */
		glBegin(GL_TRIANGLE_FAN);
		glVertex3fv(tip);
		glVertex3fv(b1);
		glVertex3fv(b2);
		glVertex3fv(b3);
		glVertex3fv(b4);
		glVertex3fv(b1);
		glEnd();

		/* close the arrow (counterclockwise)*/
		glBegin(GL_QUADS);
		glVertex3fv(b2);
		glVertex3fv(b1);
		glVertex3fv(b4);
		glVertex3fv(b3);
		glEnd();
	}

	/**
	 * @brief draw all connection arrows (one for each direction)
	 */
	void RoutingRenderableEntry::renderConnections (void) const
	{
		for (EDirection dir = routing::DIR_WEST; dir < routing::MAX_DIRECTIONS; dir++) {
			renderConnection(dir);
		}
	}
}
