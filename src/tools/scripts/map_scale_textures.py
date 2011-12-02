#! /usr/bin/env python
#
# Following code is distributed under the GNU General Public License version 2
#
'''
This script will rescale specific texture used in the given .map(s)
Python 2.6 required
'''
import sys, glob, os, fnmatch
import ufomap_io

def scale_textures(texname, mapname, scalex, scaley):
    changed = False
    ufo_map = ufomap_io.read_ufo_map(mapname, True)
    for i in ufo_map:
        for j in i.brushes:
            for k in j.faces:
                if k.texture == texname: #found face with that texture
                    k.scale[0] *= scalex
                    k.scale[1] *= scaley
                    k.offset[0] /= scalex
                    k.offset[1] /= scaley
                    changed = True

    if changed:
        ufomap_io.write_ufo_map(ufo_map, mapname)
        print "Texture found, map updated"

args = sys.argv
argsCount = len(args)
path = ""
recurse = False
scalex = 1.0
scaley = 1.0
texname = ""

brokenArgs = False
if argsCount<4:
    brokenArgs = True
else:
    if args[2] == "-r":
        recurse = True
        texname = args[3]
        if argsCount == 5:
            scalex = scaley = float(args[4])
        elif argsCount == 6:
            scalex = float(args[4])
            scaley = float(args[5])
        else:
            brokenArgs = True
    else:
        texname = args[2]
        if argsCount == 4:
            scalex = scaley = float(args[3])
        elif argsCount == 5:
            scalex = float(args[3])
            scaley = float(args[4])
        else:
            brokenArgs = True

if brokenArgs:
    print 'Usage: python map_scale_textures.py map_file_name|map_file_wildcard texture scalex scaley'
    print ' e.g. python map_scale_textures.py ../../../base/map/dam.map tex_base/lab_wall001 .5 .33'
    print ' or python map_scale_textures.py ../../../base/map/*.map tex_base/lab_wall001 .5 .33'
    print ' to recurse through subdirectories use: python map_scale_textures.py path -r texture scalex scaley'
    print ' which will process all .map files'
    print 'If only scalex is given, scaley will reuse its value'
    exit()

path = args[1]

if recurse:
    for (folder, subFolders, files) in os.walk(path):
        for filename in files:
            if fnmatch.fnmatch(filename, "*.map"):
                scale_textures(texname, os.path.join(folder, filename), scalex, scaley)
    if path.find("prefabs") < 0:
        print "Don't forget prefabs!"
else:
    #expand wildcards, if any
    names = glob.glob(path)

    for name in names:
        scale_textures(texname, name, scalex, scaley)
