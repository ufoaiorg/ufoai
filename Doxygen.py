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

from buildbot.steps.shell import WarningCountingShellCommand

class Doxygen(WarningCountingShellCommand):
    """
        TODO: Support warnings from both stdout and file output
        TODO: Extract warning file output from config file
        TODO: We count too much warning (when parameters, we count 1 more warning that what it is need)
    """
    # count line containing "warnings", and "parameter" as 1 warnings
    warningPattern = '(.*[wW]arning[: ].*|  parameter.*)'
    name = "compile"
    haltOnFailure = 1
    flunkOnFailure = 1
    description = ["doxygening"]
    descriptionDone = ["doxygen"]
    command = ["doxygen", "doxygenconf"]

    OFFprogressMetrics = ('output',)
    # things to track: number of files compiled, number of directories
    # traversed (assuming 'make' is being used)

    def createSummary(self, log):
        # TODO: grep for the characteristic GCC error lines and
        # assemble them into a pair of buffers
        WarningCountingShellCommand.createSummary(self, log)
