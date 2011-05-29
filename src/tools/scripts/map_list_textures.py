#!python
#
# Following code is distributed under the GNU General Public License version 2
#
'''
This script will print the list of textures used in the given .map
Python 2.6 required
'''
import sys, glob
import ufomap_io

args = sys.argv
path = ""
if len(args)<2:
    print 'Usage: python map_list_textures.py map_file_name|map_file_wildcard' # [-r]'
    print ' e.g. python map_list_textures.py ../../../base/map/dam.map'
    print ' or python map_list_textures.py ../../../base/map/*.map'
    #print ' -r will enable recursing through subdirectories'
    print 'Results will be printed to stdout, so add ">result_file_name" (without quotes) if you want to save textures list'
    exit
else:
    path = args[1]

recurse = len(args) == 3 and args[2] == "-r"

#expand wildcards, if any
names = glob.glob(path)

texnames = {}

for name in names:
    ufo_map = ufomap_io.read_ufo_map(name)
    for i in ufo_map:
        for j in i.brushes:
            for k in j.faces:
                texnames[k.texture] = True

tmp = texnames.keys()
tmp.sort()
print "\n".join(map(str, tmp))
