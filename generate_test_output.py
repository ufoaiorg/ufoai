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

def main():
    if not os.path.exists(UNITTESTS_DIR):
        os.mkdir(UNITTESTS_DIR)

    revision = get_svn_revision()
    if not os.path.exists(CUNIT_OUTPUT):
        sys.exit(1)

    file_name = revision + ".xml"
    shutil.copy (CUNIT_OUTPUT, UNITTESTS_DIR + "/" + file_name)

    index_name = UNITTESTS_DIR + '/index.html'
    content = INDEX_CONTENT % file_name
    file = open(index_name, "wt")
    file.write(content)
    file.close()

if __name__ == '__main__':
    main()
