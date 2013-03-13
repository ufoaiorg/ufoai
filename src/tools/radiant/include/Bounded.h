#pragma once

#include "generic/constant.h"

/* FORWARD DECLS */
class AABB;

/**
 * Interface for bounded objects, which maintain a local AABB.
 */
class Bounded
{
	public:
		STRING_CONSTANT(Name, "Bounded");

		virtual ~Bounded() {}

		/**
		 * Return the local AABB for this object.
		 */
		virtual const AABB& localAABB () const = 0;
};
