#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

# Copyright (C) 2011 - UFO: Alien Invasion
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

import sys
sys.path.append(sys.path[0] + '/../../contrib/licenses')

import os
import os.path
import shutil
import resources
import sprite_pack

UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../..')

SRC_BASE = os.path.join(UFOAI_ROOT, "base")
DEST_BASE = os.path.join(UFOAI_ROOT, "base_light")

def base_copy(src_base, dest_base, name, ignore_prefix=set([])):

    def ignore_callback(local, content):
        elements = content
        ignore = []
        for c in content:
            ref = os.path.join(name, local, c)
            ref = os.path.relpath(ref, src_base)
            if ref in ignore_prefix:
                print "    skip \"%s\"", os.path.join(src_base, ref)
                ignore.append(c)
        return ignore

    src = os.path.join(src_base, name)
    if not os.path.exists(src):
        sys.stderr.write("File \"%s\" not found. Copy aborted.\n" % src)
        return
    dest = os.path.join(dest_base, name)
    #if not os.path.exists(dest):
    #    os.makedirs(dest)
    if os.path.isdir(src):
        shutil.copytree(src, dest, ignore=ignore_callback)
    else:
        dir = os.path.dirname(dest)
        if not os.path.exists(dir):
            os.makedirs(dir)
        shutil.copy(src, dest)

# TODO remove it from global
resourceset = None
needTextures = set([])

def base_map_copy(src, dest, name):
    global resourceset, needTextures
    """
        Copy maps from a base to another,
        Also move needed textures
    """
    srcfile = os.path.join(src, name)

    if ".ump" in srcfile:
        sys.stderr.write("\"%s\" not copyed. RMA file unsupported.\n" % srcfile)
        return

    if ".map" in srcfile:
        nameId = os.path.relpath(srcfile, UFOAI_ROOT)
        resource = resourceset.getResource(nameId)
        print "Copy \"%s\" (only the bsp)" % resource.fileName
        print "    need %d textures" % len(resource.useTextures)
        for r in resource.useTextures:
            needTextures.add(os.path.relpath(r.fileName, src))
        destfile = os.path.join(dest, name)
        dir = os.path.dirname(destfile)
        if not os.path.exists(dir):
            os.makedirs(dir)
        #shutil.copy(srcfile, destfile)
        srcfile = srcfile.replace(".map", ".bsp")
        destfile = destfile.replace(".map", ".bsp")
        shutil.copy(srcfile, destfile)
        return

    sys.stderr.write("File format \"%s\" format unknown. Copy aborded.\n" % src)

if __name__ == '__main__':
    print "Delete \"%s\"" % DEST_BASE
    if os.path.exists(DEST_BASE):
        shutil.rmtree(DEST_BASE)

    # base
    needs = [
        "autoexec.cfg", "dedicated.cfg", "default.cfg", "keys.cfg",
        "irc_motd.txt", "mapcycle.txt",
        "safemode.cfg", "video-high.cfg", "video-low.cfg", "video-medium.cfg"]
    for name in needs:
        print "Copy \"%s\"" % os.path.join(SRC_BASE, name)
        base_copy(SRC_BASE, DEST_BASE, name)

    # fully copy subdirs
    needs = [
        "ai", "i18n", "materials", "media", "models", "shaders", "sound", "ufos",
        "textures/pt", "textures/tex_common", "textures/tex_lights", "textures/tex_material"]
    for name in needs:
        print "Copy \"%s\"" % os.path.join(SRC_BASE, name)
        base_copy(SRC_BASE, DEST_BASE, name)

    # few music
    fewmusic = ["van_geoscape.ogg", "van_theme.ogg"]
    for name in fewmusic:
        name = os.path.join("music", name)
        print "Copy \"%s\"" % os.path.join(SRC_BASE, name)
        base_copy(SRC_BASE, DEST_BASE, name)

    # few pics, but no sprites
    ignore = set(["pics/buttons", "pics/icons"])
    name = "pics"
    print "Copy \"%s\" (with filters)" % os.path.join(SRC_BASE, name)
    base_copy(SRC_BASE, DEST_BASE, name, ignore_prefix=ignore)

    # TODO remove this OLD pack as soon as possible
    base_copy(SRC_BASE, DEST_BASE, "pics/icons/pack1.png")
    base_copy(SRC_BASE, DEST_BASE, "pics/icons/pack2.png")
    base_copy(SRC_BASE, DEST_BASE, "pics/icons/pack_hud.png")
    base_copy(SRC_BASE, DEST_BASE, "pics/icons/pack_hud2.png")
    base_copy(SRC_BASE, DEST_BASE, "pics/icons/pack_message.png")

    # generate sprite pack
    sprite_pack.gen_sprite_pack(DEST_BASE, 512)

    # few maps
    resourceset = resources.Resources()
    resourceset.computeTextureUsageInMaps()

    # "cemetery.ump"
    fewmaps = ["dam.map", "lake_ice.map", "wilderness.map", "fighter_crash.map", "farm.map", "training_a.map"]
    for name in fewmaps:
        name = os.path.join("maps", name)
        base_map_copy(SRC_BASE, DEST_BASE, name)

    for name in needTextures:
        print "Copy \"%s\"" % os.path.join(SRC_BASE, name)
        base_copy(SRC_BASE, DEST_BASE, name)
