#! /usr/bin/env python
#
# Following code is distributed under the GNU General Public License version 2
#
'''
This script will find all maps using given texture
Python 2.6 required
'''
import sys, os, fnmatch
import ufomap_io

def check_for_texture(texname, mapname):
    ufo_map = ufomap_io.read_ufo_map(mapname)
    for i in ufo_map:
        for j in i.brushes:
            for k in j.faces:
                if k.texture == texname: #found face with that texture
                    return True
    return False

args = sys.argv

if len(args) != 3:
    print 'Usage: python maps_using_texture.py path texture'
    print ' e.g. python maps_using_texture.py ../../../base/maps/ tex_base/lab_wall001'
    print ' which will process all .map files in given directory and its subdirectories'
    exit()

path = args[1]
texname = args[2]
maps = []

for (folder, subFolders, files) in os.walk(path):
    for filename in files:
        if fnmatch.fnmatch(filename, "*.map"):
            fullpath = os.path.join(folder, filename)
            if check_for_texture(texname, fullpath):
                maps.append(fullpath)

if len(maps) == 0:
    print "This texture is unused"
else:
    for i in maps:
        print i
