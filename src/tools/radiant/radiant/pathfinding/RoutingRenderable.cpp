#include "RoutingRenderable.h"
#include "math/aabb.h"
#include "math/quaternion.h"
#include "igl.h"
#include "entitylib.h"//for aabb_draw_solid
namespace routing
{
	RoutingRenderable::RoutingRenderable ()
	{
	}

	RoutingRenderable::~RoutingRenderable ()
	{
		for (routing::RoutingRenderableEntries::const_iterator i = _entries.begin(); i != _entries.end(); ++i) {
			delete *i;
		}
	}

	void RoutingRenderable::render (RenderStateFlags state) const
	{
		for (routing::RoutingRenderableEntries::const_iterator i = _entries.begin(); i != _entries.end(); ++i) {
			const routing::RoutingRenderableEntry* entry = *i;
			entry->render(state);
		}
	}

	void RoutingRenderable::add (const RoutingLumpEntry& dataEntry)
	{
		routing::RoutingRenderableEntry* entry = new RoutingRenderableEntry(dataEntry);
		_entries.push_back(entry);
	}

	void RoutingRenderableEntry::render (RenderStateFlags state) const
	{
		this->renderWireframe();
		this->renderAccessability(state);
		this->renderConnections();
	}

	/**
	 * @brief render marker wireframe for grid
	 */
	void RoutingRenderableEntry::renderWireframe () const
	{
		/** @todo constants for colors */
		const Vector3 color_white = Vector3(1.0, 1.0, 1.0);
		glColor3fv( vector3_to_array (color_white));

		/** @todo use constants for grid sizes */
		Vector3 dx = Vector3(32, 0, 0);
		Vector3 dy = Vector3(0, 32, 0);
		Vector3 dz = Vector3(0, 0, 64);
		Vector3 dxy = vector3_added(dx, dy);

		/* lower and upper corners */
		Vector3 l1 = this->_data.getOrigin();
		Vector3 l2 = vector3_added(l1, dx);
		Vector3 l3 = vector3_added(l1, dxy);
		Vector3 l4 = vector3_added(l1, dy);
		Vector3 u1 = vector3_added(l1, dz);
		Vector3 u2 = vector3_added(l2, dz);
		Vector3 u3 = vector3_added(l3, dz);
		Vector3 u4 = vector3_added(l4, dz);

		glBegin(GL_LINE_STRIP);
		/* connect continuing lines from point to point */
		glVertex3fv(vector3_to_array(l1));
		glVertex3fv(vector3_to_array(l2));
		glVertex3fv(vector3_to_array(l3));
		glVertex3fv(vector3_to_array(l4));
		glVertex3fv(vector3_to_array(l1));
		glVertex3fv(vector3_to_array(u1));
		glVertex3fv(vector3_to_array(u2));
		glVertex3fv(vector3_to_array(u3));
		glVertex3fv(vector3_to_array(u4));
		glVertex3fv(vector3_to_array(u1));
		glEnd();

		glBegin(GL_LINES);
		/* draw missing lines from point to point */
		glVertex3fv(vector3_to_array(l2));
		glVertex3fv(vector3_to_array(u2));

		glVertex3fv(vector3_to_array(l3));
		glVertex3fv(vector3_to_array(u3));

		glVertex3fv(vector3_to_array(l4));
		glVertex3fv(vector3_to_array(u4));

		glEnd();
	}

	/**
	 * @brief render a box for accessability of this grid part
	 */
	void RoutingRenderableEntry::renderAccessability (RenderStateFlags state) const
	{
		/* center of drawn box */
		Vector3 diffCenter = Vector3(16, 16, 8);
		/**@todo render accessability in different colors based on state */
		const Vector3 color_green = Vector3(0.0, 1.0, 0.0);
		glColor3fv( vector3_to_array (color_green));
		AABB box = AABB(vector3_added(this->_data.getOrigin(), diffCenter), Vector3(8, 8, 8));
		aabb_draw_solid(box, state);
	}

	/**
	 * @brief render connection arrow for given direction
	 * @param direction direction to draw arrow for
	 */
	void RoutingRenderableEntry::renderConnection (int direction) const
	{
		/**@todo get color from lump connectivity data */
		const Vector3 color_blue = Vector3(-1.0, -1.0, 1.0);
		glColor3fv( vector3_to_array (color_blue));

		//vector to center of direction arrows
		Vector3 diffCenter = Vector3(16, 16, 32);
		//vector tip
		Vector3 difTip = Vector3(0, -16, 0);
		//vector base corners
		Vector3 difB1 = Vector3(2, -8, 2);
		Vector3 difB2 = Vector3(2, -8, -2);
		Vector3 difB3 = Vector3(-2, -8, -2);
		Vector3 difB4 = Vector3(-2, -8, 2);

		//rotate tip and base corners around {0,0,0} before translate to center
		Quaternion rotation = quaternion_for_z(degrees_to_radians(direction * 45));

		Vector3 tip = vector3_added(diffCenter, quaternion_transformed_point(rotation, difTip));
		Vector3 b1 = vector3_added(diffCenter, quaternion_transformed_point(rotation, difB1));
		Vector3 b2 = vector3_added(diffCenter, quaternion_transformed_point(rotation, difB2));
		Vector3 b3 = vector3_added(diffCenter, quaternion_transformed_point(rotation, difB3));
		Vector3 b4 = vector3_added(diffCenter, quaternion_transformed_point(rotation, difB4));

		/* draw arrow as connected triangles */
		glBegin(GL_TRIANGLE_FAN);
		glVertex3fv(vector3_to_array(tip));
		glVertex3fv(vector3_to_array(b1));
		glVertex3fv(vector3_to_array(b2));
		glVertex3fv(vector3_to_array(b3));
		glVertex3fv(vector3_to_array(b4));
		glVertex3fv(vector3_to_array(b1));
		glEnd();

		/* close the arrow (counterclockwise)*/
		glBegin(GL_QUADS);
		glVertex3fv(vector3_to_array(b2));
		glVertex3fv(vector3_to_array(b1));
		glVertex3fv(vector3_to_array(b4));
		glVertex3fv(vector3_to_array(b3));
		glEnd();
	}

	/**
	 * @brief draw all connection arrows (one for each direction)
	 */
	void RoutingRenderableEntry::renderConnections (void) const
	{
		for (int i = 0; i < 8; i++) {
			renderConnection(i);
		}
	}
}
