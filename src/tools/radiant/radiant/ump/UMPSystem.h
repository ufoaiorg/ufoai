#include "iump.h"
#include <map>

class UMPSystem: public IUMPSystem
{
	public:
		// Radiant Module stuff
		typedef IUMPSystem Type;
		STRING_CONSTANT(Name, "*");

		// Return the static instance
		IUMPSystem* getTable ()
		{
			return this;
		}

	private:

		class UMPCollector
		{
			private:
				std::set<std::string>& _list;

			public:
				typedef const std::string& first_argument_type;

				UMPCollector (std::set<std::string> &list);

				// Functor operator needed for the forEachFile() call
				void operator() (const std::string& file);
		};


		typedef std::map<std::string, std::string> UMPFileMap;
		UMPFileMap _umpFileMap;

		std::set<std::string> _umpFiles;
		typedef std::set<std::string>::iterator UMPFilesIterator;

	public:

		void editUMPDefinition ();

		/**
		 * @return A vector with ump filesnames
		 */
		const std::set<std::string> getFiles () const;

		void init ();

		/**
		 * @return The ump filename for the given map
		 */
		std::string getUMPFilename (const std::string& map);
};
