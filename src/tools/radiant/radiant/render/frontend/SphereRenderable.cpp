#include "SphereRenderable.h"
#include "render.h"

SphereRenderable::SphereRenderable (bool wire, const Vector3& origin) :
		_origin(origin), _wire(wire), _radius(0)
{
	if (!_wire) {
		_shader = GlobalShaderCache().capture("$SPHERE");
	} else {
		_shader = NULL;
	}
}

SphereRenderable::~SphereRenderable ()
{
	if (!_wire) {
		GlobalShaderCache().release("$SPHERE");
	}
}

void SphereRenderable::render (RenderStateFlags state) const
{
	if (_wire)
		drawWire(_origin, _radius, 24);
	else
		drawFill(_origin, _radius, 16);
}

void SphereRenderable::drawFill (const Vector3& origin, float radius, int sides) const
{
	if (radius <= 0)
		return;

	const double dt = c_2pi / static_cast<double>(sides);
	const double dp = c_pi / static_cast<double>(sides);

	glBegin (GL_TRIANGLES);
	for (int i = 0; i <= sides - 1; ++i) {
		for (int j = 0; j <= sides - 2; ++j) {
			const double t = i * dt;
			const double p = (j * dp) - (c_pi / 2.0);

			{
				Vector3 v(origin + vector3_for_spherical(t, p) * radius);
				glVertex3fv(v);
			}

			{
				Vector3 v(origin + vector3_for_spherical(t, p + dp) * radius);
				glVertex3fv(v);
			}

			{
				Vector3 v(origin + vector3_for_spherical(t + dt, p + dp) * radius);
				glVertex3fv(v);
			}

			{
				Vector3 v(origin + vector3_for_spherical(t, p) * radius);
				glVertex3fv(v);
			}

			{
				Vector3 v(origin + vector3_for_spherical(t + dt, p + dp) * radius);
				glVertex3fv(v);
			}

			{
				Vector3 v(origin + vector3_for_spherical(t + dt, p) * radius);
				glVertex3fv(v);
			}
		}
	}

	{
		const double p = (sides - 1) * dp - (c_pi / 2.0);
		for (int i = 0; i <= sides - 1; ++i) {
			const double t = i * dt;

			{
				Vector3 v(origin + vector3_for_spherical(t, p) * radius);
				glVertex3fv(v);
			}

			{
				Vector3 v(origin + vector3_for_spherical(t + dt, p + dp) * radius);
				glVertex3fv(v);
			}

			{
				Vector3 v(origin + vector3_for_spherical(t + dt, p) * radius);
				glVertex3fv(v);
			}
		}
	}
	glEnd();
}

void SphereRenderable::drawWire (const Vector3& origin, float radius, int sides) const
{
	glBegin (GL_LINE_LOOP);
	for (int i = 0; i <= sides; i++) {
		const double ds = sin((i * 2 * c_pi) / sides);
		const double dc = cos((i * 2 * c_pi) / sides);

		glVertex3f(static_cast<float>(origin[0] + radius * dc), static_cast<float>(origin[1] + radius * ds), origin[2]);
	}
	glEnd();

	glBegin(GL_LINE_LOOP);
	for (int i = 0; i <= sides; i++) {
		const double ds = sin((i * 2 * c_pi) / sides);
		const double dc = cos((i * 2 * c_pi) / sides);

		glVertex3f(static_cast<float>(origin[0] + radius * dc), origin[1], static_cast<float>(origin[2] + radius * ds));
	}
	glEnd();

	glBegin(GL_LINE_LOOP);
	for (int i = 0; i <= sides; i++) {
		const double ds = sin((i * 2 * c_pi) / sides);
		const double dc = cos((i * 2 * c_pi) / sides);

		glVertex3f(origin[0], static_cast<float>(origin[1] + radius * dc), static_cast<float>(origin[2] + radius * ds));
	}
	glEnd();
}
