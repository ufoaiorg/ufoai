#ifndef SCRIPT_LIBRARY_H_
#define SCRIPT_LIBRARY_H_

#include "iscriplib.h"
#include "itextstream.h"

class ScriptLibrary: public IScriptLibrary
{
	public:

		typedef IScriptLibrary Type;
		STRING_CONSTANT(Name, "*");

		IScriptLibrary* getTable ()
		{
			return this;
		}

	public:

		Tokeniser* createScriptTokeniser (TextInputStream& istream);
		Tokeniser* createSimpleTokeniser (TextInputStream& istream);
		TokenWriter* createSimpleTokenWriter (TextOutputStream& ostream);
};

#endif /* SCRIPT_LIBRARY_H_ */
