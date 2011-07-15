#include "Parser.h"

#include "iufoscript.h"
#include "ifilesystem.h"
#include "iarchive.h"
#include "AutoPtr.h"
#include <sstream>

namespace scripts
{
	void Parser::skipBlock (Tokeniser &tokeniser)
	{
		int level = 0;
		std::string token = tokeniser.getToken();
		if (token.empty())
			return;

		// most of the script entries have a type and a name
		// before the data block follows, but some have not
		if (token == "{")
			tokeniser.ungetToken();

		for (;;) {
			token = tokeniser.getToken();
			if (token.empty())
				break;

			if (token == "{") {
				level++;
			} else if (token == "}") {
				level--;
			}

			if (level == 0)
				break;
		}
	}

	void Parser::parse (const std::string& filename, Tokeniser &tokeniser, const std::string& id)
	{
		for (;;) {
			const std::string scriptID = tokeniser.getToken();
			if (scriptID.empty()) {
				break;
			} else if (scriptID != id) {
				skipBlock(tokeniser);
			} else {
				int level = 0;
				std::stringstream os;

				const std::string token = tokeniser.getToken();
				if (token.empty())
					break;
				// most of the script entries have a type and a name
				// before the data block follows, but some have not
				else if (token == "{")
					tokeniser.ungetToken();

				DataBlock* block = new DataBlock(filename, tokeniser.getLine(), token);
				for (;;) {
					const std::string token = tokeniser.getToken();
					if (token.empty())
						break;

					if (token == "{") {
						level++;
					} else if (token == "}") {
						level--;
					}

					os << token << " ";

					if (level == 0)
						break;

				}
				block->setData(os.str());
				_entries.push_back(block);
			}
		}
	}

	Parser::Parser (const std::string& id)
	{
		const std::string& scriptDir = GlobalUFOScriptSystem()->getUFOScriptDir();
		const std::set<std::string>& files = GlobalUFOScriptSystem()->getFiles();
		for (UFOScriptSystem::UFOScriptFilesIterator i = files.begin(); i != files.end(); ++i) {
			const std::string& filename = (*i);
			AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(scriptDir + filename));
			if (file) {
				AutoPtr<Tokeniser> reader(GlobalScriptLibrary().createScriptTokeniser(file->getInputStream()));
				parse(filename, *reader, id);
			}
		}
	}

	Parser::~Parser ()
	{
		for (EntriesIterator i = _entries.begin(); i != _entries.end(); ++i) {
			DataBlock* block = *i;
			delete block;
		}
		_entries.clear();
	}

	const std::vector<DataBlock*>& Parser::getEntries ()
	{
		return _entries;
	}

	DataBlock* Parser::getEntryForID (const std::string& id)
	{
		for (Parser::EntriesIterator i = _entries.begin(); i != _entries.end(); ++i) {
			DataBlock* blockData = (*i);
			if (id == blockData->getID()) {
				return blockData;
			}
		}
		return (DataBlock*) 0;
	}
}
