#include "ScriptTokenWriter.h"
#include "string/string.h"

ScriptTokenWriter::ScriptTokenWriter (TextOutputStream& ostream) :
	m_ostream(ostream), m_separator('\n')
{
}
ScriptTokenWriter::~ScriptTokenWriter ()
{
	writeSeparator();
}

void ScriptTokenWriter::nextLine ()
{
	m_separator = '\n';
}
void ScriptTokenWriter::writeToken (const std::string& token)
{
	ASSERT_MESSAGE(strchr(token.c_str(), ' ') == 0, "token contains whitespace: ");
	writeSeparator();
	m_ostream << token;
}
void ScriptTokenWriter::writeString (const std::string& string)
{
	writeSeparator();
	m_ostream << "\"" << string << "\"";
}
void ScriptTokenWriter::writeInteger (int i)
{
	writeSeparator();
	m_ostream << i;
}
void ScriptTokenWriter::writeUnsigned (std::size_t i)
{
	writeSeparator();
	m_ostream << string::toString(i);
}
void ScriptTokenWriter::writeFloat (double f)
{
	writeSeparator();
	m_ostream << Decimal(f);
}

void ScriptTokenWriter::writeSeparator ()
{
	m_ostream << m_separator;
	m_separator = ' ';
}
