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
		self.command += "tar cvjf %s %s && " % (self.output, " ".join(self.files))
		self.command += "chmod 644 %s" % self.output
		ShellCommand.start(self)
