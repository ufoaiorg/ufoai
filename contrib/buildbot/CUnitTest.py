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

import xslutil

if __name__ == '__main__':
    # Dummy class, easy to test
    class ShellCommand:
        def __init__(self, command, flunkOnFailure):
            pass
        pass
else:
    from buildbot.steps.shell import ShellCommand



class CUnitTest(ShellCommand):
    def __init__(self, command, cunit_result_file="-Results.xml", cunit_xsl_file="/usr/share/CUnit/CUnit-Run.xsl", flunkOnFailure=False):
        ShellCommand.__init__(self, command, flunkOnFailure)
        self.cunit_result_file = cunit_result_file
        self.cunit_xsl_file = cunit_xsl_file
        pass

    def getXML(self):
        file = open(self.cunit_result_file, "rt")
        content = file.read()
        file.close()
        return content

    def getHTML(self):
        content = xslutil.apply_xsl(self.cunit_result_file, self.cunit_xsl_file)
        return content

    def createSummary(self, log):
        self.addCompleteLog("log", log)
        xml = self.getXML()
        self.addCompleteLog("xml-result", xml)
        html = self.getHTML()
        self.addHTMLLog("html-result", html)


if __name__ == '__main__':
    CUnitTest()


