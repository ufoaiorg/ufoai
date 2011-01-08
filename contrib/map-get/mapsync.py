#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import sys, os, re
try:
	from hashlib import md5
except ImportError:
	from md5 import md5
import subprocess
import urllib2

__version__ = '0.0.5.0'

UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../..')

UFO2MAP_DIRSRC = UFOAI_ROOT + '/src/tools/ufo2map'
UFO2MAP_MAINSRC = UFOAI_ROOT + '/src/tools/ufo2map/ufo2map.c'
UFO2MAP = UFOAI_ROOT + '/ufo2map'

if os.name == 'nt':
    UFO2MAP+= '.exe'

def md5sum(path, binary = False):
    # to be used carefully with big files
    if binary:
        return md5(open(path, "rb").read()).hexdigest()
    return md5(open(path).read()).hexdigest()

def execute(cmd):
    """
    Execute a sheel command and return the log
    """
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    data = p.stdout.read()
    return data

def get_branch():
    """
    Return the branch version from the source file
    """
    filename = UFOAI_ROOT + '/src/common/common.h'
    if not os.path.exists(filename):
        raise Exception("File '%s' not found. No way to know the current game branch.")

    file = open(filename, 'rt')
    data = file.read()
    file.close()
    v = re.findall("#define\s+UFO_VERSION\s+\"([0-9]+\.[0-9]+).*\"", data)
    branch = v[0]
    return branch

class Ufo2mapMetadata:
    """
    Class identify ufo2map version of the client or remote/reference server
    """

    def __init__(self):
        self.hash_from_source = None
        self.version_from_source = None
        self.version_from_binary = None
        self.version = None

    def set_content(self, data):
        lines = data.split("\n")
        for line in lines:
            elements = line.split(":", 2)
            if elements[0].strip() == "version":
                self.version = elements[1].strip()
            elif elements[0].strip() == "hash_from_source":
                self.hash_from_source = elements[1].strip()

    def get_content(self):
        data = ""
        data += "version: %s\n" % self.version
        data += "hash_from_source: %s\n" % self.hash_from_source
        return data

    def get_full_content(self):
        data = ""
        data += "version_from_binary: %s\n" % self.version_from_binary
        data += "version_from_source: %s\n" % self.version_from_source
        data += "hash_from_source: %s\n" % self.hash_from_source
        data += "version: %s\n" % self.version
        return data

    def extract(self):
        """Extract ufo2map information from source and binary files."""

        # TODO maybe useless
        if os.path.exists(UFO2MAP_DIRSRC):
            src = md5()
            files = []
            for i in os.walk(UFO2MAP_DIRSRC):
                if '/.' in i[0]:
                    continue
                files+= [os.path.join(i[0], j) for j in i[2]]
            files.sort()
            for fname in files:
                src.update(open(fname, 'rt').read())
            self.hash_from_source = src.hexdigest()

        # take the version from the more up-to-date
        timestamp = 0
        if os.path.exists(UFO2MAP):
            version = execute("%s --version" % UFO2MAP)
            # format is ATM "version:1.2.5 revision:1"
            version = version.replace("version", "")
            version = version.replace("revision", "rev")
            version = version.replace(" ", "")
            version = version.replace(":", "")
            version = version.strip()
            # format converted to "1.2.5rev1"
            self.version = version
            self.version_from_binary = version

            stat = os.stat(UFO2MAP)
            timestamp = stat.st_mtime

        if os.path.exists(UFO2MAP_MAINSRC):
            file = open(UFO2MAP_MAINSRC, 'rt')
            data = file.read()
            file.close()
            version = ""
            v = re.findall("#define\s+VERSION\s+\"([0-9.]+)\"", data)
            r = re.findall("#define\s+REVISION\s+\"([0-9.]+)\"", data)
            version = v[0]
            if r != []:
                version += "rev" + r[0]
            self.version_from_source = version

            stat = os.stat(UFO2MAP_MAINSRC)
            t = stat.st_mtime
            if (t > timestamp):
                timestamp = t
                self.version = version
