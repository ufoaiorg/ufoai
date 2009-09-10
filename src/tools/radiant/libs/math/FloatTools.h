#ifndef FLOATTOOLS_H_
#define FLOATTOOLS_H_

/*	greebo: this contains some handy (?) functions for manipulating float variables
 */

#include "lrint.h"

#include <iostream>
#include <cmath>
#include <float.h>
#include <algorithm>

// =========================================================================================

/// \brief Returns true if \p self is equal to other \p other within \p epsilon.
template<typename Element, typename OtherElement>
inline bool float_equal_epsilon (const Element& self, const OtherElement& other, const Element& epsilon)
{
	return fabs(other - self) < epsilon;
}

/// \brief Returns the value midway between \p self and \p other.
template<typename Element>
inline Element float_mid (const Element& self, const Element& other)
{
	return Element((self + other) * 0.5);
}

/// \brief Returns \p f rounded to the nearest integer. Note that this is not the same behaviour as casting from float to int.
template<typename Element>
inline int float_to_integer (const Element& f)
{
	return lrint(f);
}

/// \brief Returns \p f rounded to the nearest multiple of \p snap.
template<typename Element, typename OtherElement>
inline Element float_snapped (const Element& f, const OtherElement& snap)
{
	return Element(float_to_integer(f / snap) * snap);
}

/// \brief Returns true if \p f has no decimal fraction part.
template<typename Element>
inline bool float_is_integer (const Element& f)
{
	return f == Element(float_to_integer(f));
}

/// \brief Returns \p self modulated by the range [0, \p modulus)
/// \p self must be in the range [\p -modulus, \p modulus)
template<typename Element, typename ModulusElement>
inline Element float_mod_range (const Element& self, const ModulusElement& modulus)
{
	return Element((self < 0.0) ? self + modulus : self);
}

/// \brief Returns \p self modulated by the range [0, \p modulus)
template<typename Element, typename ModulusElement>
inline Element float_mod (const Element& self, const ModulusElement& modulus)
{
	return float_mod_range(Element(fmod(static_cast<double> (self), static_cast<double> (modulus))), modulus);
}

#endif /*FLOATTOOLS_H_*/
