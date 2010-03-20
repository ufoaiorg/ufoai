#ifndef PARSER_H_
#define PARSER_H_

#include <string>
#include <vector>
#include "DataBlock.h"

#include "iscriplib.h"

namespace scripts
{
	class Parser
	{
		private:

			std::vector<DataBlock*> _entries;

			void skipBlock (Tokeniser &tokeniser);
			void parse (const std::string& filename, Tokeniser &tokeniser, const std::string& id);

		public:

			typedef std::vector<DataBlock*>::iterator EntriesIterator;

			/**
			 * @brief Constructor that already parses all ufo script files and searches for entries
			 * with the given id
			 * @param id The id to fill the entries list with
			 */
			Parser (const std::string& id);

			virtual ~Parser ();

			/**
			 * @return The list of blocks for the entries the parser has found
			 */
			const std::vector<DataBlock*>& getEntries ();

			/**
			 * @return The data block for the given id or null if not found
			 */
			DataBlock* getEntryForID (const std::string& id);
	};
}

#endif /* PARSER_H_ */
