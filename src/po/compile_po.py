#! /usr/bin/env python

# Compile src/po/*.po to base/i18n using command-line supplied "msgmerge" path
# This is primarily for windows users. "msgmerge" is usually in the same place
# as poedit
#
# by techtonik // rainforce.org

import os, glob, sys, stat, platform

#read the given path
if len(sys.argv) < 2:
    msgfmt = "msgfmt"
    if platform.system() == "Windows": msgfmt = msgfmt + ".exe"
else:
    msgfmt = sys.argv[1]

print "====================================================="
print "po compiler for UFO:AI (http://sf.net/projects/ufoai)"
print "Given path to 'msgfmt' utility as parameter it will  "
print "compile all .po files and in src/po and place them   "
print "into base/i18n/"
print "====================================================="

sys.stdout.write("...checking for msgfmt: "); sys.stdout.flush();
if not os.access(msgfmt, os.X_OK):
    print "is not executable. exiting..."
    print
    print "usage: compile_po.py E:\\path\\to\\poEdit\\bin\\msgfmt.exe"
    sys.exit(1)
print "%s - ok" % msgfmt

podir = "."
podir = os.path.normpath(podir)
if not os.path.isdir(podir):
    print "%s does not exists" % podir
    sys.exit(1)
i18ndir = "../../base/i18n"
i18ndir = os.path.normpath(i18ndir)
if not os.path.isdir(i18ndir):
    print "%s does not exists" % i18ndir
    sys.exit(1)

po = glob.glob(podir + "/*.po")
for f in po:
    name, ext = os.path.splitext(os.path.split(f)[1])
    outpath = os.path.normpath(i18ndir + "/" + name + "/LC_MESSAGES")
    if not os.path.isdir(outpath):
        os.makedirs(outpath)
    outfile = os.path.normpath(outpath + "/" + "ufoai.mo")
    sys.stdout.write("compiling %s to %s -" % (f, outfile)); sys.stdout.flush();

    z = os.system("%s -o %s %s" % (msgfmt, outfile, f))
    if z != 0:
        sys.exit(" msgfmt error. Terminated.")

    print "ok"

print "Done."
