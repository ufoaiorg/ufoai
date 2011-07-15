#include "ScriptValues.h"

namespace scripts
{
	ScriptValues::ScriptValues ()
	{
	}

	ScriptValues::~ScriptValues ()
	{
	}

	std::vector<ScriptValue> ScriptValues::getScriptValues () const
	{
		return _scriptValues;
	}

	void ScriptValues::addScriptValue (ScriptValue value)
	{
	}

	ScriptValues::ScriptValueVectorConstIterator ScriptValues::begin () const
	{
		return _scriptValues.begin();
	}

	ScriptValues::ScriptValueVectorConstIterator ScriptValues::end () const
	{
		return _scriptValues.end();
	}

	std::size_t ScriptValues::size () const
	{
		return _scriptValues.size();
	}
}
