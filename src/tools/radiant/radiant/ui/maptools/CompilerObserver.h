#ifndef CHECKDIALOGCOMPILEROBSERVER_H_
#define CHECKDIALOGCOMPILEROBSERVER_H_

#include "imapcompiler.h"

#include "string/string.h"
#include "string/WildcardMatcher.h"

namespace ui {

namespace {
enum
{
	CHECK_ENTITY, CHECK_BRUSH, CHECK_MESSAGE, CHECK_COLUMNS
};
}

class CompilerObserver: public ICompilerObserver
{
	private:

		GtkListStore *_store;

		bool isRelevant (const std::string& message)
		{
			if (message.empty() || message.length() < 15)
				return false;

			return WildcardMatcher::matches(message, "*ent:*brush:*");
		}

		int getEntNum (const std::string& message)
		{
			std::size_t posEnt = message.find("ent:");
			if (posEnt == std::string::npos)
				return -1;

			std::size_t posBrush = message.find(" brush:");
			if (posBrush == std::string::npos)
				return -1;

			posEnt += 4;
			return string::toInt(message.substr(posEnt, posBrush - posEnt), -1);
		}

		int getBrushNum (const std::string& message)
		{
			std::size_t posBrush = message.find("brush:");
			if (posBrush == std::string::npos)
				return -1;

			std::size_t posSplit = message.find(" - ");
			if (posSplit == std::string::npos)
				return -1;

			posBrush += 6;
			return string::toInt(message.substr(posBrush, posSplit - posBrush), -1);
		}

		std::string getErrorMessage (const std::string& message)
		{
			std::size_t pos = message.find(" - ");
			if (pos == std::string::npos)
				return "error while parsing the output";
			pos += 3;
			return message.substr(pos, message.length() - pos);
		}

	public:

		CompilerObserver (GtkListStore* store) :
			_store(store)
		{
			/* start with a fresh list */
			gtk_list_store_clear(_store);
		}

		void notify (const std::string &message)
		{
			std::cout << message << "\n";
			if (!isRelevant(message))
				return;

			int entNum = getEntNum(message);
			int brushNum = getBrushNum(message);
			std::string error = getErrorMessage(message);

			GtkTreeIter iter;
			gtk_list_store_append(_store, &iter);
			gtk_list_store_set(_store, &iter, CHECK_ENTITY, entNum, CHECK_BRUSH, brushNum, CHECK_MESSAGE,
					error.c_str(), -1);
		}
};

}

#endif
