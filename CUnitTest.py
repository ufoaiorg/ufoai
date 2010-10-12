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
import os.path
from buildbot.steps.shell import ShellCommand

class CUnitTest(ShellCommand):
	name = "test"
	flunkOnFailure = 1
	description = [ "testing" ]
	descriptionDone = [ "test" ]

    def __init__(self, cunit_result_file = "-Results.xml", cunit_xsl_file = "/usr/share/CUnit/CUnit-Run.xsl", **kwargs):
        self.cunit_xsl_file = cunit_xsl_file
        self.cunit_result_file = cunit_result_file

        self.workdir = None
        if "workdir" in kwargs:
            self.workdir = kwargs["workdir"]

        ShellCommand.__init__(self, **kwargs)
		self.addFactoryArguments(
            cunit_xsl_file = cunit_xsl_file,
            cunit_result_file = cunit_result_file)

    def getResultFileName(self):
        result = self.cunit_result_file
        if self.workdir != None and not result.startswith("/"):
            result = self.workdir + '/' + self.cunit_result_file
        return result

    def getXML(self):
        file = open(self.getResultFileName(), "rt")
        content = file.read()
        file.close()
        return content

    def getHTML(self):
        content = xslutil.apply_xsl(self.getResultFileName(), self.cunit_xsl_file)
        return content

    def createSummary(self, log):
        #self.addCompleteLog("log", log.getText())
        if os.path.exists(self.getResultFileName()):
            xml = self.getXML()
            self.addCompleteLog("xml-result", xml)
            try:
                html = self.getHTML()
                self.addHTMLLog("html-result", html)
            except:
                pass
