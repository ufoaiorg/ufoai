#ifndef XMLFILTERRULE_H_
#define XMLFILTERRULE_H_

#include "string/string.h"

namespace filters
{

/** Data class encapsulating a single filter rule. Several of these rules may make
 * up a single XMLFilter.
 */

struct XMLFilterRule {

	std::string type; 	// "texture", "entityclass" or "object"
	std::string match; 	// the match expression wildcard
	bool show;			// true for action="show", false for action="hide"
	int surfaceflags;	// true if this filter should match again
	int contentflags;	// true if this filter should match again

	// Constructor
	XMLFilterRule(const std::string& t, const std::string& m, bool s)
	: type(t), match(m), show(s), surfaceflags(0), contentflags(0)
	{
		if (t == "surfaceflags") {
			surfaceflags = string::toInt(m);
		} else if (t == "contentflags") {
			contentflags = string::toInt(m);
		}
	}
};

}

#endif /*XMLFILTERRULE_H_*/
