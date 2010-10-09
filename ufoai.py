# code to deliver build status through twisted.words (instant messaging
# protocols: irc, etc)

import os, re, shlex, glob
import ConfigParser

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

def get_config(filename):
	properties = ConfigParser.SafeConfigParser()
	try:
		properties.read(filename)
	except:
		# catch error like that else it can display passwords on stdout
		raise Exception("Can't load %s file" % filename)
	return properties

class Publish(ShellCommand):
	name = "publish"
	haltOnFailure = 1
	flunkOnFailure = 1
	description = [ "publishing" ]
	descriptionDone = [ "publish" ]

	def __init__(self, **kwargs):
		self.src = kwargs["src"]
		del kwargs["src"]
		self.dest = kwargs["dest"]
		del kwargs["dest"]

		ShellCommand.__init__(self, **kwargs)

		self.addFactoryArguments(src = self.src)
		self.addFactoryArguments(dest = self.dest)

	def start(self):
		properties = self.build.getProperties()

		if not properties.has_key("package"):
			return SKIPPED

		self.command = ""
		self.command += "rm -f %s && " % self.dest
		self.command += "mkdir -p $(dirname %s) && " % self.dest
		self.command += "cp %s %s && " % (self.src, self.dest)
		self.command += "chmod 644 %s" % self.dest
		ShellCommand.start(self)

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
		properties = self.build.getProperties()

		if not properties.has_key("package"):
			return SKIPPED

		self.command = ""
		self.command += "cd %s && " % self.workdir
		self.command += "mkdir -p $(dirname %s) && " % self.output
		if self.output.endswith('.zip'):
			self.command += "zip %s %s && " % (self.output, " ".join(self.files))
		elif self.output.endswith('.tar.bz2'):
			self.command += "tar cvjf %s %s && " % (self.output, " ".join(self.files))
		elif self.output.endswith('.tar.gz'):
			self.command += "tar cvzf %s %s && " % (self.output, " ".join(self.files))
		else:
			print 'Output extension from "%s" unknown' % self.output
			return FAILURE

		self.command += "chmod 644 %s" % self.output
		ShellCommand.start(self)

class UFOAICUnitTest(CUnitTest):
	def __init__(self, suite, **kwargs):
		name = suite
		result = name + "-Results.xml"

		self.name = [ "test_" + suite ]
		self.description = [ "testing " + suite ]
		self.descriptionDone = [ "test " + suite ]

		kwargs["name"] = "test " + suite
		kwargs["description"] = [ "testing " + suite ]
		kwargs["descriptionDone"] = [ "test " + suite ]
		kwargs["command"] = ["./testall", "--automated", "--only-" + suite, "--output-prefix=" + name]
		kwargs["cunit_result_file"] = result
		CUnitTest.__init__(self, **kwargs)
		self.addFactoryArguments(suite = suite)

class UFOAICUnitOtherTests(CUnitTest):
	name = "test other tests"
	description = [ "testing other tests" ]
	descriptionDone = [ "test other tests" ]

	def __init__(self, suites, **kwargs):
		name = "other"
		result = name + "-Results.xml"
		command = ["./testall", "--automated", "--output-prefix=" + name]

		for suite in suites:
			command.append("--disable-" + suite)

		kwargs["command"] = command
		kwargs["description"] = "OtherTests"
		kwargs["cunit_result_file"] = result
		CUnitTest.__init__(self, **kwargs)
		self.addFactoryArguments(suites = suites)
