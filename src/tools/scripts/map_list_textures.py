#! /usr/bin/env python
#
# Following code is distributed under the GNU General Public License version 2
#
'''
This script will print the list of textures used in the given .map
Python 2.6 required
'''
import sys, glob, os, fnmatch
import ufomap_io

def add_textures(texnames, mapname):
    ufo_map = ufomap_io.read_ufo_map(mapname, True)
    for i in ufo_map:
        for j in i.brushes:
            for k in j.faces:
                texnames[k.texture] = True

args = sys.argv
path = ""
if len(args)<2:
    print 'Usage: python map_list_textures.py map_file_name|map_file_wildcard'
    print ' e.g. python map_list_textures.py ../../../base/map/dam.map'
    print ' or python map_list_textures.py ../../../base/map/*.map'
    print ' to recurse through subdirectories use: python map_list_textures.py path -r'
    print ' which will analyze all .map files'
    print 'Results will be printed to stdout, so add ">result_file_name" (without quotes) if you want to save textures list'
    exit
else:
    path = args[1]

recurse = len(args) == 3 and args[2] == "-r"

texnames = {}

if recurse:
    for (folder, subFolders, files) in os.walk(path):
        for filename in files:
            if fnmatch.fnmatch(filename, "*.map"):
                add_textures(texnames, os.path.join(folder, filename))
else:
    #expand wildcards, if any
    names = glob.glob(path)

    for name in names:
        add_textures(texnames, name)

tmp = texnames.keys()
tmp.sort()
print "\n".join(map(str, tmp))
