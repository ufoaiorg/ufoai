#include "ScriptTokeniser.h"

ScriptTokeniser::ScriptTokeniser (TextInputStream& istream, bool special) :
	m_state(m_stack), m_istream(istream), m_scriptline(1), m_scriptcolumn(1), m_unget(false), m_emit(false), m_special(
			special)
{
	m_stack[0] = Tokenise(&ScriptTokeniser::tokeniseDefault);
	m_eof = !m_istream.readChar(m_current);
	m_token[MAXTOKEN - 1] = '\0';
}

ScriptTokeniser::CharType ScriptTokeniser::charType (const char c)
{
	switch (c) {
	case '\n':
		return eNewline;
	case '"':
		return eCharQuote;
	case '/':
		return eCharSolidus;
	case '*':
		return eCharStar;
	case '{':
	case '(':
	case '}':
	case ')':
	case '[':
	case ']':
	case ',':
	case ':':
		return (m_special) ? eCharSpecial : eCharToken;
	}

	if (c > 32) {
		return eCharToken;
	}
	return eWhitespace;
}

ScriptTokeniser::Tokenise ScriptTokeniser::state ()
{
	return *m_state;
}

void ScriptTokeniser::push (Tokenise state)
{
	ASSERT_MESSAGE(m_state != m_stack + 2, "token parser: illegal stack push");
	*(++m_state) = state;
}

void ScriptTokeniser::pop ()
{
	ASSERT_MESSAGE(m_state != m_stack, "token parser: illegal stack pop");
	--m_state;
}

void ScriptTokeniser::add (const char c)
{
	if (m_write < m_token + MAXTOKEN - 1) {
		*m_write++ = c;
	}
}

void ScriptTokeniser::remove ()
{
	ASSERT_MESSAGE(m_write > m_token, "no char to remove");
	--m_write;
}

bool ScriptTokeniser::tokeniseDefault (char c)
{
	switch (charType(c)) {
	case eNewline:
		break;
	case eCharToken:
	case eCharStar:
		push(Tokenise(&ScriptTokeniser::tokeniseToken));
		add(c);
		break;
	case eCharSpecial:
		push(Tokenise(&ScriptTokeniser::tokeniseSpecial));
		add(c);
		break;
	case eCharQuote:
		push(Tokenise(&ScriptTokeniser::tokeniseQuotedToken));
		break;
	case eCharSolidus:
		push(Tokenise(&ScriptTokeniser::tokeniseSolidus));
		break;
	default:
		break;
	}
	return true;
}

bool ScriptTokeniser::tokeniseToken (char c)
{
	switch (charType(c)) {
	case eNewline:
	case eWhitespace:
	case eCharQuote:
	case eCharSpecial:
		pop();
		m_emit = true; // emit token
		break;
	case eCharSolidus:
	case eCharToken:
	case eCharStar:
		add(c);
		break;
	default:
		break;
	}
	return true;
}

bool ScriptTokeniser::tokeniseQuotedToken (char c)
{
	switch (charType(c)) {
	case eNewline:
		break;
	case eWhitespace:
	case eCharToken:
	case eCharSolidus:
	case eCharStar:
	case eCharSpecial:
		add(c);
		break;
	case eCharQuote:
		pop();
		push(Tokenise(&ScriptTokeniser::tokeniseEndQuote));
		break;
	default:
		break;
	}
	return true;
}

bool ScriptTokeniser::tokeniseSolidus (char c)
{
	switch (charType(c)) {
	case eNewline:
	case eWhitespace:
	case eCharQuote:
	case eCharSpecial:
		pop();
		add('/');
		m_emit = true; // emit single slash
		break;
	case eCharToken:
		pop();
		add('/');
		add(c);
		break;
	case eCharSolidus:
		pop();
		push(Tokenise(&ScriptTokeniser::tokeniseComment));
		break; // don't emit single slash
	case eCharStar:
		pop();
		push(Tokenise(&ScriptTokeniser::tokeniseBlockComment));
		break; // don't emit single slash
	default:
		break;
	}
	return true;
}

bool ScriptTokeniser::tokeniseComment (char c)
{
	if (c == '\n') {
		pop();
		if (state() == Tokenise(&ScriptTokeniser::tokeniseToken)) {
			pop();
			m_emit = true; // emit token immediately preceding comment
		}
	}
	return true;
}

bool ScriptTokeniser::tokeniseBlockComment (char c)
{
	if (c == '*') {
		pop();
		push(Tokenise(&ScriptTokeniser::tokeniseEndBlockComment));
	}
	return true;
}

bool ScriptTokeniser::tokeniseEndBlockComment (char c)
{
	switch (c) {
	case '/':
		pop();
		if (state() == Tokenise(&ScriptTokeniser::tokeniseToken)) {
			pop();
			m_emit = true; // emit token immediately preceding comment
		}
		break; // don't emit comment
	case '*':
		break; // no state change
	default:
		pop();
		push(Tokenise(&ScriptTokeniser::tokeniseBlockComment));
		break;
	}
	return true;
}

bool ScriptTokeniser::tokeniseEndQuote (char c)
{
	pop();
	m_emit = true; // emit quoted token
	return true;
}

bool ScriptTokeniser::tokeniseSpecial (char c)
{
	pop();
	m_emit = true; // emit single-character token
	return true;
}

/// Returns true if a token was successfully parsed.
bool ScriptTokeniser::tokenise ()
{
	m_write = m_token;
	while (!eof()) {
		char c = m_current;

		if (!((*this).*state())(c)) {
			// parse error
			m_eof = true;
			return false;
		}
		if (m_emit) {
			m_emit = false;
			return true;
		}

		if (c == '\n') {
			++m_scriptline;
			m_scriptcolumn = 1;
		} else {
			++m_scriptcolumn;
		}

		m_eof = !m_istream.readChar(m_current);
	}
	return m_write != m_token;
}

const char* ScriptTokeniser::fillToken ()
{
	if (!tokenise()) {
		return 0;
	}

	add('\0');
	return m_token;
}

bool ScriptTokeniser::eof ()
{
	return m_eof;
}

const std::string ScriptTokeniser::getToken ()
{
	if (m_unget) {
		m_unget = false;
		return m_token;
	}

	const char *token = fillToken();
	if (token)
		return std::string(token);

	return "";
}

void ScriptTokeniser::ungetToken ()
{
	ASSERT_MESSAGE(!m_unget, "can't unget more than one token");
	m_unget = true;
}

std::size_t ScriptTokeniser::getLine () const
{
	return m_scriptline;
}

std::size_t ScriptTokeniser::getColumn () const
{
	return m_scriptcolumn;
}
