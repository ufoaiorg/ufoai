# code to deliver build status through twisted.words (instant messaging
# protocols: irc, etc)

import os, re, shlex, glob

from zope.interface import Interface, implements
from twisted.internet import protocol, reactor
from twisted.words.protocols import irc
from twisted.python import log, failure
from twisted.application import internet

from buildbot import interfaces, util
from buildbot import version
from buildbot.sourcestamp import SourceStamp
from buildbot.status import base
from buildbot.status.builder import SUCCESS, WARNINGS, FAILURE, EXCEPTION, SKIPPED
from buildbot import scheduler
from buildbot.steps.shell import ShellCommand

from string import join, capitalize, lower
from CUnitTest import CUnitTest

class Package(ShellCommand):
	name = "package"
	haltOnFailure = 1
	flunkOnFailure = 1
	description = [ "packaging" ]
	descriptionDone = [ "package" ]

	def __init__(self, **kwargs):
		self.workdir = kwargs["workdir"]
		del kwargs["workdir"]
		self.output = kwargs["output"]
		del kwargs["output"]
		self.files = kwargs["files"]
		del kwargs["files"]

		ShellCommand.__init__(self, **kwargs)

		self.addFactoryArguments(workdir = self.workdir)
		self.addFactoryArguments(output = self.output)
		self.addFactoryArguments(files = self.files)

	def start(self):
		self.command = ""
		self.command += "cd %s && " % self.workdir
		self.command += "mkdir -p $(dirname %s) && " % self.output
		if self.output.endswith('.zip'):
			self.command += "zip %s %s && " % (self.output, " ".join(self.files))
		elif self.output.endswith('.tar.bz2'):
			self.command += "tar cvjf %s %s && " % (self.output, " ".join(self.files))
		else:
			print 'Output extension from "%s" unknown' % self.output
		self.command += "chmod 644 %s" % self.output
		ShellCommand.start(self)

class UFOAICUnitTest(CUnitTest):
	def __init__(self, suite, **kwargs):
		name = suite
		result = name + "-Results.xml"
		CUnitTest.__init__(self,
			name = "test",
			description = suite,
			command = ["./testall", "--automated", "--only-" + suite, "--output-prefix=" + name],
			flunkOnFailure = True,
			cunit_result_file=result,
			**kwargs)

class UFOAICUnitOtherTests(CUnitTest):
	def __init__(self, suites, **kwargs):
		name = "other"
		result = name + "-Results.xml"
		command = ["./testall", "--automated", "--output-prefix=" + name]
		for suite in suites:
			command.append("--disable-" + suite)
		CUnitTest.__init__(self,
			name = "test",
			description = "OtherTests",
			command = command,
			flunkOnFailure = True,
			cunit_result_file = result,
			**kwargs)
