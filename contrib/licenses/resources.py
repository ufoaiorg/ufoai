#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

# Copyright (C) 2006 - Florian Ludwig <dino@phidev.org>
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

import subprocess
import hashlib, os
from xml.dom.minidom import parseString

# TODO i dont think a cache here is need cause the Resources is itself a cache
def get(cmd, cacheable=True):
    if cacheable and CACHING:
        h = hashlib.md5(cmd).hexdigest()
        if h in os.listdir('licenses/cache'):
            print ' getting from cache: ', cmd
            print ' ', h
            return open('licenses/cache/' + h).read()
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    data = p.stdout.read()

    if cacheable and CACHING:
        open('licenses/cache/' + h, 'w').write(data)
        print ' written to cache: ', cmd
    return data


def get_used_tex(m):
    used = []
    for i in open(m):
        i = i.split(')')
        if len(i) < 4:
            continue

        tex = i[3].strip().split(' ')[0]
        if tex not in used:
            used.append(tex)
    return used

def get_used_resources_from_ufo_script(scriptFileName):
    "Return file name from base/ used by the script"
    return set([])

class Resource(object):
    "Identify a file resource and links with other resources"

    def __init__(self, fileName):
        self.fileName = fileName
        self.license = None
        self.copyright = None
        self.source = None
        self.revision = None
        self.usedByMaps = set([])
        self.useTextures = set([])
        self.usedByScripts = set([])
        self.useResources = set([])

    def isImage(self):
        return self.fileName.endswith('.jpg') or self.fileName.endswith('.tga') or self.fileName.endswith('.png')

    def __repr__(self):
        return str((self.license, self.copyright, self.source, self.revision))

# TODO compute a map with 'short' name
class Resources(object):
    "Manage a set of resources"

    def __init__(self):
        self.resources = {}
        self.computedBaseDir = set()

    # TODO can we "propget" more than 1 property? faster
    # TODO using getResource here is a little stupid
    def computeResources(self, dir):
        "Read resources and metadatas for all versionned file of a dir"

        if dir in self.computedBaseDir:
            return

        self.computedBaseDir.add(dir)

        print "Parse '%s' revisions..." % dir
        xml = get('svn info %s --xml -R' % dir, False)
        dom = parseString(xml)
        entries = dom.firstChild.getElementsByTagName('entry')
        for e in entries:
            path = e.getAttribute("path")
            meta = self.getResource(path)
            meta.revision = int(e.getAttribute("revision"))

        print "Parse '%s' licenses..." % dir
        xml = get('svn pg svn:license %s --xml -R' % dir, False)
        dom = parseString(xml)
        entries = dom.firstChild.getElementsByTagName('target')
        for e in entries:
            path = e.getAttribute("path")
            property = e.getElementsByTagName('property')[0]
            assert(property.getAttribute("name") == 'svn:license')
            meta = self.getResource(path)
            meta.license = property.firstChild.nodeValue

        print "Parse '%s' copyright..." % dir
        xml = get('svn pg svn:copyright %s --xml -R' % dir, False)
        dom = parseString(xml)
        entries = dom.firstChild.getElementsByTagName('target')
        for e in entries:
            path = e.getAttribute("path")
            property = e.getElementsByTagName('property')[0]
            assert(property.getAttribute("name") == 'svn:copyright')
            meta = self.getResource(path)
            meta.copyright = property.firstChild.nodeValue

        print "Parse '%s' sources..." % dir
        xml = get('svn pg svn:source %s --xml -R' % dir, False)
        dom = parseString(xml)
        entries = dom.firstChild.getElementsByTagName('target')
        for e in entries:
            path = e.getAttribute("path")
            property = e.getElementsByTagName('property')[0]
            assert(property.getAttribute("name") == 'svn:source')
            meta = self.getResource(path)
            meta.source = property.firstChild.nodeValue

    def getResource(self, fileName):
        "Get a resource from a name without extension"

        # Must we load the basedir?
        if not (fileName in self.resources):
            basedir = fileName.split('/')
            basedir = basedir[0]
            self.computeResources(basedir)

        # Get metadata from cache
        if fileName in self.resources:
            meta = self.resources[fileName]
        # Gen object for non versioned file
        else:
            meta = Resource(fileName)
            self.resources[fileName] = meta

        return meta

    def getResourceByShortImageName(self, fileName):
        "Get a resource from a name without extension"

        if os.path.exists(fileName + ".png"):
            return self.getResource(fileName + ".png")
        if os.path.exists(fileName + ".jpg"):
            return self.getResource(fileName + ".jpg")
        if os.path.exists(fileName + ".tga"):
            return self.getResource(fileName + ".tga")
        return None

    def computeResourceUsageInUFOScripts(self):
        "Read UFO scripts and create relations with other resources"

        for i in os.walk('base/ufos'):
            for ufoname in i[2]:
                if not ufoname.endswith('.ufo'):
                    continue
                ufoname = i[0] + '/' + ufoname
                uforesource = self.getResource(ufoname)

                for name in get_used_resources_from_ufo_script(ufoname):
                    resource = self.getResource(name)
                    resource.usedByScripts.add(uforesource)
                    uforesource.useResources.add(resource)

    def computeTextureUsageInMaps(self):
        "Read maps and create relations with other resources"

        print 'Parse texture usage in maps...'
        files = set([])
        for i in os.walk('base/maps'):
            for mapname in i[2]:
                if not mapname.endswith('.map'):
                    continue
                mapname = i[0] + '/' + mapname
                files.add(mapname)
        for i in os.walk('radiant/prefabs'):
            for mapname in i[2]:
                if not mapname.endswith('.map'):
                    continue
                mapname = i[0] + '/' + mapname
                files.add(mapname)

        for mapname in files:
            mapmeta = self.getResource(mapname)

            for tex in get_used_tex(mapname):
                texname = "base/textures/" + tex
                texmeta = self.getResourceByShortImageName(texname)
                # texture missing (or wrong python parsing)
                if texmeta == None:
                    print "Warning: \"" + texname + "\" from map \"" + mapname + "\" does not exist"
                    continue
                texmeta.usedByMaps.add(mapmeta)
                mapmeta.useTextures.add(texmeta)

