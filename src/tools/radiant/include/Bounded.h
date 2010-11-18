#ifndef BOUNDED_H_
#define BOUNDED_H_

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

#endif /*BOUNDED_H_*/
