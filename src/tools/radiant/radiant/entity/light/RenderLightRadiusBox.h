#pragma once

#include "render.h"
#include "entitylib.h"

class RenderLightRadiusBox: public OpenGLRenderable {
private:
	const Vector3& m_origin;

	void light_draw_box_lines (const Vector3& origin, const Vector3 points[8]) const
	{
		//draw lines from the center of the bbox to the corners
		glBegin(GL_LINES);

		glVertex3fv(origin);
		glVertex3fv(points[1]);

		glVertex3fv(origin);
		glVertex3fv(points[5]);

		glVertex3fv(origin);
		glVertex3fv(points[2]);

		glVertex3fv(origin);
		glVertex3fv(points[6]);

		glVertex3fv(origin);
		glVertex3fv(points[0]);

		glVertex3fv(origin);
		glVertex3fv(points[4]);

		glVertex3fv(origin);
		glVertex3fv(points[3]);

		glVertex3fv(origin);
		glVertex3fv(points[7]);

		glEnd();
	}

public:
	mutable Vector3 m_points[8];

	RenderLightRadiusBox (const Vector3& origin) :
			m_origin(origin)
	{
	}
	void render (RenderStateFlags state) const
	{
		//draw the bounding box of light based on light_radius key
		if ((state & RENDER_FILL) != 0) {
			aabb_draw_flatshade(m_points);
		} else {
			aabb_draw_wire(m_points);
		}

		//disable if you dont want lines going from the center of the light bbox to the corners
		light_draw_box_lines(m_origin, m_points);
	}
};
