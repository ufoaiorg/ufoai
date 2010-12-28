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

#if !defined(INCLUDED_XML_XMLPARSER_H)
#define INCLUDED_XML_XMLPARSER_H

#include <cstdio>
#include <string.h>
#include "ixml.h"
#include <libxml/parser.h>
#include "gtkutil/IConv.h"

class TextInputStream;

class SAXElement : public XMLElement {
public:
	SAXElement(const std::string& name, const char** atts)
			: m_name(name), m_atts(atts) {
	}
	const std::string name() const {
		return m_name;
	}
	const std::string attribute(const std::string& name) const {
		if (m_atts != 0) {
			for (const char** att = m_atts; *att != 0; att += 2) {
				if (string_equal(*att, name)) {
					return *(++att);
				}
			}
		}
		return "";
	}
	void forEachAttribute(XMLAttrVisitor& visitor) const {
		if (m_atts != 0) {
			for (const char** att = m_atts; *att != 0; att += 2) {
				visitor.visit(*att, *(att + 1));
			}
		}
	}
private:
	const std::string m_name;
	const char** m_atts;
} __attribute__ ((deprecated));

#include <stdarg.h>

class FormattedVA {
public:
	const char* m_format;
	va_list& m_arguments;
	FormattedVA(const char* format, va_list& m_arguments)
			: m_format(format), m_arguments(m_arguments) {
	}
};

class Formatted {
public:
	const char* m_format;
	va_list m_arguments;
	Formatted(const char* format, ...)
			: m_format(format) {
		va_start(m_arguments, format);
	}
	~Formatted() {
		va_end(m_arguments);
	}
};

template<typename TextOutputStreamType>
inline TextOutputStreamType& ostream_write(TextOutputStreamType& ostream, const FormattedVA& formatted) {
	char buffer[1024];
	ostream.write(buffer, vsnprintf(buffer, 1023, formatted.m_format, formatted.m_arguments));
	return ostream;
}

template<typename TextOutputStreamType>
inline TextOutputStreamType& ostream_write(TextOutputStreamType& ostream, const Formatted& formatted) {
	char buffer[1024];
	ostream.write(buffer, vsnprintf(buffer, 1023, formatted.m_format, formatted.m_arguments));
	return ostream;
}

class XMLSAXImporter {
	XMLImporter& m_importer;
	xmlSAXHandler m_sax;

	static void startElement(void *user_data, const xmlChar *name, const xmlChar **atts) {
		SAXElement element(reinterpret_cast<const char*>(name), reinterpret_cast<const char**>(atts));
		reinterpret_cast<XMLSAXImporter*>(user_data)->m_importer.pushElement(element);
	}
	static void endElement(void *user_data, const xmlChar *name) {
		reinterpret_cast<XMLSAXImporter*>(user_data)->m_importer.popElement(reinterpret_cast<const char*>(name));
	}
	static void characters(void *user_data, const xmlChar *ch, int len) {
		reinterpret_cast<XMLSAXImporter*>(user_data)->m_importer
		<< gtkutil::IConv::localeFromUTF8(std::string((const char *)ch, len));
	}

	static void warning(void *user_data, const char *msg, ...) {
		va_list args;
		va_start(args, msg);
		globalWarningStream() << "XML WARNING: " << FormattedVA(msg, args);
		va_end(args);
	}
	static void error(void *user_data, const char *msg, ...) {
		va_list args;
		va_start(args, msg);
		globalErrorStream() << "XML ERROR: " << FormattedVA(msg, args);
		va_end(args);
	}

public:
	XMLSAXImporter(XMLImporter& importer) : m_importer(importer) {
		m_sax.internalSubset = 0;
		m_sax.isStandalone = 0;
		m_sax.hasInternalSubset = 0;
		m_sax.hasExternalSubset = 0;
		m_sax.resolveEntity = 0;
		m_sax.getEntity = 0;
		m_sax.entityDecl = 0;
		m_sax.notationDecl = 0;
		m_sax.attributeDecl = 0;
		m_sax.elementDecl = 0;
		m_sax.unparsedEntityDecl = 0;
		m_sax.setDocumentLocator = 0;
		m_sax.startDocument = 0;
		m_sax.endDocument = 0;
		m_sax.startElement = startElement;
		m_sax.endElement = endElement;
		m_sax.reference = 0;
		m_sax.characters = characters;
		m_sax.ignorableWhitespace = 0;
		m_sax.processingInstruction = 0;
		m_sax.comment = 0;
		m_sax.warning = warning;
		m_sax.error = error;
		m_sax.fatalError = 0;
		m_sax.getParameterEntity = 0;
		m_sax.cdataBlock = 0;
		m_sax.externalSubset = 0;
		m_sax.initialized = 1;
	}

	xmlSAXHandler* callbacks() {
		return &m_sax;
	}
	void* context() {
		return this;
	}
} __attribute__ ((deprecated));

#endif
