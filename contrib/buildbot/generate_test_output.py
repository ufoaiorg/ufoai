#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import sys, os, shutil
import subprocess
from xml.dom.minidom import parseString

CUNIT_OUTPUT = "./ufoai-Results.xml"
PUBLIC_HTML = "/home/users/mattn/buildbot/public_html"
if not os.path.exists(PUBLIC_HTML):
    PUBLIC_HTML = "."
UNITTESTS_DIR = PUBLIC_HTML + "/unittests"
SCRIPT_DIR = sys.path[0]

INDEX_CONTENT = '<html><head><title>Redirection</title><meta http-equiv="refresh" content="0; URL=%s" /></head><body></body></html>'


def get(cmd):
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    data = p.stdout.read()
    return data

def get_svn_revision():
    xml = get("svn info --xml")
    dom = parseString(xml)
    entries = dom.firstChild.getElementsByTagName('entry')
    entry = entries[0]
    return entry.getAttribute("revision")

def apply_xsl(xml_file_name, xsl_file_name):
    try:
        from Ft.Lib.Uri import OsPathToUri
        from Ft.Xml import InputSource
        from Ft.Xml.Xslt import Processor
    except:
        sys.stderr.write("python-4suite-xml module is missing.")
        return

    """
        Apply XSL file transformation to an XML file.
        Return the new XML content, else None
    """
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


def main():
    if not os.path.exists(UNITTESTS_DIR):
        os.mkdir(UNITTESTS_DIR)

    revision = get_svn_revision()
    if not os.path.exists(CUNIT_OUTPUT):
        sys.exit(1)

    xml_file_name = revision + ".xml"
    shutil.copy(CUNIT_OUTPUT, UNITTESTS_DIR + "/" + xml_file_name)
    file_name = xml_file_name
    print '----link----'
    print "unittests/" + xml_file_name

    # TODO use /usr/share/CUnit/ it should contain all .dtd .xsl
    html_file_name = revision + ".html"
    shutil.copy(CUNIT_OUTPUT, SCRIPT_DIR + "/" + xml_file_name)
    content = apply_xsl(SCRIPT_DIR + "/" + xml_file_name, SCRIPT_DIR + "/" + "CUnit-Run.xsl")
    os.remove(SCRIPT_DIR + "/" + xml_file_name)
    if content != None:
        file = open(UNITTESTS_DIR + '/' + html_file_name, "wt")
        file.write(content)
        file.close()
        file_name = html_file_name
        print '----html----'
        print content
    else:
        sys.stderr.write("Warning: HTML output not generated.")

    latest_name = UNITTESTS_DIR + '/latest.html'
    content = INDEX_CONTENT % file_name
    file = open(latest_name, "wt")
    file.write(content)
    file.close()

    latest_name = UNITTESTS_DIR + '/LATEST'
    file = open(latest_name, "wt")
    file.write(xml_file_name)
    file.close()

if __name__ == '__main__':
    main()

