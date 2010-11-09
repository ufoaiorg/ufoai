#ifndef SKINNEDMODEL_H_
#define SKINNEDMODEL_H_

#include "generic/constant.h"
#include <string>

class SkinnedModel
{
public:
		STRING_CONSTANT(Name, "SkinnedModel");

		// destructor
		virtual ~SkinnedModel ()
		{
		}

		// greebo: Updates the model's surface remaps. Pass the new skin name (can be empty).
		virtual void skinChanged (const std::string& newSkinName) = 0;

		// Returns the name of the currently active skin
		virtual std::string getSkin () const = 0;
};

#endif
