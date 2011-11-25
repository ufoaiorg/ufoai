#ifndef SCRIPTVALUE_H_
#define SCRIPTVALUE_H_

#include <vector>
#include <string>

namespace scripts
{
	// value class for a script parameter
	class ScriptValue
	{
		private:

			std::string _id;

			std::size_t _offset;

			int _type;

		public:

			ScriptValue (const std::string& id, const std::size_t offset, int type) :
				_id(id), _offset(offset), _type(type)
			{
			}

			ScriptValue ()
			{
			}

			const std::string& getID () const
			{
				return _id;
			}

			std::size_t getOffset () const
			{
				return _offset;
			}

			int getType () const
			{
				return _type;
			}
	};

	// Container for script parameters
	class ScriptValues
	{
		public:

			typedef std::vector<ScriptValue> ScriptValueVector;

			typedef ScriptValueVector::const_iterator ScriptValueVectorConstIterator;

		private:

			ScriptValueVector _scriptValues;

		public:

			ScriptValues ();

			virtual ~ScriptValues ();

			ScriptValueVector getScriptValues () const;

			void addScriptValue (ScriptValue value);

			ScriptValueVectorConstIterator end () const;

			ScriptValueVectorConstIterator begin () const;

			std::size_t size () const;
	};
}

#endif /* SCRIPTVALUE_H_ */
