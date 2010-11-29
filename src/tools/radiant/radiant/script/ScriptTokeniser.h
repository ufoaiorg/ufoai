/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef INCLUDED_SCRIPT_SCRIPTTOKENISER_H
#define INCLUDED_SCRIPT_SCRIPTTOKENISER_H

#include "iscriplib.h"

#define	MAXTOKEN	1024

class ScriptTokeniser: public Tokeniser
{
		enum CharType
		{
			eWhitespace, eCharToken, eNewline, eCharQuote, eCharSolidus, eCharStar, eCharSpecial
		};

		typedef bool (ScriptTokeniser::*Tokenise) (char c);

		Tokenise m_stack[3];
		Tokenise* m_state;
		SingleCharacterInputStream<TextInputStream> m_istream;
		std::size_t m_scriptline;
		std::size_t m_scriptcolumn;

		char m_token[MAXTOKEN];
		char* m_write;

		char m_current;
		bool m_eof;
		bool m_unget;
		bool m_emit;

		bool m_special;

		CharType charType (const char c);

		Tokenise state ();
		void push (Tokenise state);
		void pop ();
		void add (const char c);
		void remove ();

		bool tokeniseDefault (char c);
		bool tokeniseToken (char c);
		bool tokeniseQuotedToken (char c);
		bool tokeniseSolidus (char c);
		bool tokeniseComment (char c);
		bool tokeniseBlockComment (char c);
		bool tokeniseEndBlockComment (char c);
		bool tokeniseEndQuote (char c);
		bool tokeniseSpecial (char c);

		/// Returns true if a token was successfully parsed.
		bool tokenise ();

		const char* fillToken ();

		bool eof ();

	public:
		ScriptTokeniser (TextInputStream& istream, bool special);

		const std::string getToken ();

		void ungetToken ();
		std::size_t getLine () const;
		std::size_t getColumn () const;
};

#endif
