#pragma once

#include "irender.h"
#include "generic/callback.h"
#include "string/string.h"

class SphereRenderable: public OpenGLRenderable {
protected:
	const Vector3& _origin;
	bool _wire;
	Shader* _shader;

	void drawFill (const Vector3& origin, float radius, int sides) const;
	void drawWire (const Vector3& origin, float radius, int sides) const;

public:
	float _radius;

public:
	SphereRenderable (bool wire, const Vector3& origin);
	virtual ~SphereRenderable ();

	virtual void render (RenderStateFlags state) const;

	operator Shader* () const
	{
		return _shader;
	}

	operator Shader* ()
	{
		return _shader;
	}

	void valueChanged (const std::string& value)
	{
		_radius = string::toInt(value);
	}
	typedef MemberCaller1<SphereRenderable, const std::string&, &SphereRenderable::valueChanged> RadiusChangedCaller;
};
