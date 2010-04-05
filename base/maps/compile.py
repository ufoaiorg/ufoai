#!/usr/bin/python

# If you have a "make" program installed, you're likely to prefer the
# Makefile to this script, which cannot run jobs in parallel.

# ported from perl to python by techtonik // php.net

import os, fnmatch, sys, stat, platform

extra = "-extra"

def getFile(root):
    pattern = "*.map"
    antipathpattern = "*prefabs*"
    for path, subdirs, files in os.walk(root):
        if ".svn" in subdirs: subdirs.remove(".svn")
        files.sort()
        if fnmatch.fnmatch(path, antipathpattern):
            continue
        for name in files:
            if fnmatch.fnmatch(name, pattern):
                file = os.path.join(path, name)
                file = file.replace("base/", "").replace("base\\", "")
                yield file


def compile(root):
    total = compiled = 0

    sys.stdout.write("...looking for ufo2map: "); sys.stdout.flush();
    ufo2map = "ufo2map"
    if platform.system() == "Windows": ufo2map = ufo2map + ".exe"
    if not os.access(ufo2map, os.X_OK):
        ufo2map = "../../%s" % ufo2map
        if platform.system() == "Windows":
            ufo2map = os.path.normpath(ufo2map)
        if not os.access(ufo2map, os.X_OK):
            print "not found. exiting..."
            sys.exit(1)
    print "found %s" % ufo2map


    print "...looking for maps in: %s" % root
    for f in getFile(root):
        total += 1
        name, ext = os.path.splitext(f)
        bspf = name + ".bsp"
        if os.path.isfile(bspf):
            if os.stat(f)[stat.ST_MTIME] > os.stat(bspf)[stat.ST_MTIME]:
               print "...removing old .bsp file %s" % bspf
               os.unlink(bspf)
        if not os.path.isfile(bspf):
            print "...compiling %s" % f
            try:
               z = os.system("%s %s %s" % (ufo2map, extra, os.path.normpath(f)))
               if z != 0:
                   print "ufo2map error. Deleting %s" % bspf
                   os.unlink(bspf)
                   sys.exit("Terminated.")
               compiled += 1

            except KeyboardInterrupt:
               print "User terminated. Deleting %s" % bspf
               os.unlink(bspf)
               sys.exit("Terminated.")
            print "...%s maps found so far, %s of them compiled..." % (total, compiled)
    return total, compiled



#read the given dir
if len(sys.argv) < 2:
    mapdir = "base"
else:
    mapdir = sys.argv[1]

print "====================================================="
print "Mapcompiler for UFO:AI (http://sf.net/projects/ufoai)"
print "Giving base/maps as parameter or start from base/maps"
print "will compile all maps were no bsp-file exists or that"
print "were modified since last access"
print "Keep in mind that ufo2map needs to be accessible"
print "====================================================="

if not os.path.isdir(mapdir):
    print "Dir %s does not exists" % mapdir
    sys.exit(1)

print "Found %d maps, %d compiled" % compile(mapdir)

print "Done."
