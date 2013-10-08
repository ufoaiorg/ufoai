#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

# Copyright (C) 2002-2010 UFO: Alien Invasion.
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.

#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os.path

XSL_LIB = None

def apply_xsl_4suite(xml_file_name, xsl_file_name):
    from Ft.Lib.Uri import OsPathToUri
    from Ft.Xml import InputSource
    from Ft.Xml.Xslt import Processor

    if not os.path.exists(xsl_file_name):
        sys.stderr.write('XSL file "%s" not found' % xsl_file_name)
        return None
    if not os.path.exists(xml_file_name):
        sys.stderr.write('XML file "%s" not found' % xml_file_name)
        return None

    uri = OsPathToUri(xml_file_name)
    xml_source = InputSource.DefaultFactory.fromUri(uri)
    uri = OsPathToUri(xsl_file_name)
    xsl_source = InputSource.DefaultFactory.fromUri(uri)

    transform = Processor.Processor()
    transform.appendStylesheet(xsl_source)
    result = transform.run(xml_source)
    return result

def apply_xsl_libxslt(xml_file_name, xsl_file_name):
    import libxslt
    import libxml2

    # optional transform args
    stylesheetArgs = {}

    style_doc = libxml2.parseFile(xsl_file_name)
    style = libxslt.parseStylesheetDoc(style_doc)

    doc = libxml2.parseFile(xml_file_name)
    result = style.applyStylesheet(doc, stylesheetArgs)
    res = style.saveResultToString(result)

    style.freeStylesheet()
    doc.freeDoc()
    result.freeDoc()
    return res

def check_xsl_lib():
    try:
        import libxslt
        import libxml2
        return 'libxslt'
    except:
        pass
    try:
        from Ft.Lib.Uri import OsPathToUri
        from Ft.Xml import InputSource
        from Ft.Xml.Xslt import Processor
        return '4suite'
    except:
        pass
    return 'no'

def apply_xsl(xml_file_name, xsl_file_name):
    """
        Apply XSL file transformation to an XML file.
        Return the new XML content, else None
        If no xsl lib found, return None and right comment in stderr
    """
    global XSL_LIB

    if XSL_LIB == None:
        XSL_LIB = check_xsl_lib()

    if XSL_LIB == '4suite':
        return apply_xsl_4suite(xml_file_name, xsl_file_name)

    if XSL_LIB == 'libxslt':
        return apply_xsl_libxslt(xml_file_name, xsl_file_name)

    sys.stderr.write("No xsl library found.")
    sys.stderr.write("Install python-4suite-xml module")
    sys.stderr.write("or python-libxml2 and python-libxslt1 module")
    return None


if __name__ == '__main__':
    content = apply_xsl("./ufoai-Results.xml", "./contrib/buildbot/CUnit-Run.xsl")
    file = open("./ufoai-Results.html", "wt")
    file.write(content)
    file.close()
