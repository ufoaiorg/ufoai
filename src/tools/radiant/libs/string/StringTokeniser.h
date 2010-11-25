#ifndef STRING_TOKENISER_H
#define STRING_TOKENISER_H

#include <vector>
#include "string/string.h"

/// \brief A re-entrant string tokeniser similar to strchr.
class StringTokeniser
{
	private:

		std::vector<std::string> _tokens;
		int _token;
		std::size_t _size;
		std::string _end;

	public:

		StringTokeniser (const std::string& string, const std::string& delimiters = " \n\r\t\v") :
			_token(0), _size(0), _end("")
		{
			string::splitBy(string, _tokens, delimiters);
			_size = _tokens.size();
		}
		~StringTokeniser ()
		{
		}

		/// \brief Returns the next token or "" if there are no more tokens available.
		const std::string& getToken ()
		{
			if (_token < _size)
				return _tokens[_token++];
			return _end;
		}
};

#endif /* STRING_TOKENISER_H */
