#pragma once

#include "inamespace.h"

#include "generic/constant.h"

class NamespaceAPI
{
		INamespace* _namespace;
	public:
		typedef INamespace Type;
		STRING_CONSTANT(Name, "*");

		NamespaceAPI ();

		~NamespaceAPI ();

		INamespace* getTable ();
};
