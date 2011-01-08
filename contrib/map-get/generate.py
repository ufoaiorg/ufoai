#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import sys, os, re
try:
	from hashlib import md5
except ImportError:
	from md5 import md5
import subprocess
import urllib2
from gzip import GzipFile
from tempfile import mkstemp
import optparse
import mapsync
import time

UFO2MAPFLAGS = ' -v 4 -nice 19 -quant 4 -extra'
JOBS = 1

def run(cmd, mandatory=True):
    print cmd
    if os.system(cmd) and mandatory:
        sys.stderr.write('Faild to run: %s' % cmd)
        sys.exit(2)

def gen(dst, changed_maps):
    from random import choice

    if not os.path.exists(dst):
        os.makedirs(dst)

    if not os.access(dst, os.R_OK | os.W_OK):
        print 'Permission denied. "%s"' % argsdst

    # read old md5 list
    old = {}
    path = os.path.join(dst, 'MAPS')
    if os.path.exists(path):
        for i in open(path):
            i = i.split(' ')
            old[i[0]] = i[1]

    # run make maps etc.
    # call separately otherwise linking happens last -> building maps fails
    run('sh configure --enable-release --enable-only-ufo2map')
    run('make clean')
    run('make ufo2map')

    # try to guess if rebuild of all maps is needed.
    old_maps = old.keys()
    if 'ufo2map' in old_maps:
        old_maps.remove('ufo2map')
    try:
        test_map = choice(list(set(old_maps) - set(changed_maps)))
    except IndexError:
        # all maps changed? wow.
        test_map = False

    if test_map and os.path.exists(test_map):
        old_md5 = old[test_map]
        os.unlink(test_map)

        # taken from build/maps.mk
        run('./ufo2map %s %s' % (UFO2MAPFLAGS, test_map[5:-3] + 'map'))
        if md5sum(test_map) != old_md5:
            sys.stderr.write('Compiler must have changed significant! Compiling all maps again.\n')
            for i in old_maps:
                os.unlink(i)

    run('make maps -j %d UFO2MAPFLAGS="%s"' % (JOBS, UFO2MAPFLAGS))
    print

    ufo2mapMeta = mapsync.Ufo2mapMetadata()
    ufo2mapMeta.extract()

    version = open(os.path.join(dst, 'UFO2MAP'), 'wt')
    version.write(ufo2mapMeta.get_content())
    version.close()

    # create md5 sums of .map files
    maps = open(os.path.join(dst, 'Maps'), 'w')

    maps_compiled = 0
    for dirname, dnames, fnames in os.walk('base/maps'):
        if '/.' in dirname:
            # this dir is a / within a hidden one
            continue

        for i in fnames:
            if not i.endswith('.map'):
                continue

            mapfile = os.path.join(dirname, i)
            bspfile = mapfile[:-4] + '.bsp'
            if not os.path.exists(bspfile):
                print "Warning: Cant find .bsp for %s" % mapfile
                continue

            maphash = md5sum(mapfile)
            bsphash = md5sum(bspfile, True)

            if not bspfile in old or bsphash != old[bspfile]:
                print '%s - updating' % bspfile
                maps_compiled+= 1
                if os.path.exists(os.path.join(dst, bspfile)):
                    os.unlink(os.path.join(dst, bspfile))

                # make sure destination directory exists
                dst_dir = os.path.split(os.path.join(dst, bspfile))[0]
                if not os.path.exists(dst_dir):
                    os.makedirs(dst_dir)

                data = open(bspfile).read()
                GzipFile(os.path.join(dst, bspfile) + '.gz', 'w').write(data)
            else:
                print '%s - already up to date' % bspfile

            maps.write(' '.join((bspfile, bsphash, maphash)) + '\n')
    print ' %s maps compiled' % maps_compiled
    return maps_compiled

def main(argv=None):
    global UFO2MAPFLAGS, JOBS

    print "map-get version " + mapsync.__version__

    parser = optparse.OptionParser(argv)
    parser.add_option('-v', '--verbose', action='store_true', dest='verbose')
    parser.add_option("", "--reply", action="store", default="query", type="string", dest="reply", help='Allow to auto reply interactive questions, REPLY={yes,no,query}')
    parser.add_option("", "--flags", action="store", default=UFO2MAPFLAGS, type="string", dest="flags", help='Set specific ufo2map flags for map compilation, default value is "' + UFO2MAPFLAGS + '"')
    parser.add_option("-j", "--jobs", action="store", default=JOBS, type="int", dest="jobs", help='Allow JOBS jobs at once, default value is ' + str(JOBS))
    parser.usage = '%prog [options] destination\n\n'

    options, args = parser.parse_args()

    if not (options.reply in ['yes', 'no', 'query']):
        print 'Wrong param --reply={yes,no,query}'
        sys.exit(1)

    UFO2MAPFLAGS = options.flags
    JOBS = options.jobs

    if len(args) == 0:
        parser.print_help()
        sys.exit(2)

    # search base dir
    while os.path.abspath('.').count(os.path.sep) > 1:
        if os.path.exists('base/maps/'):
            break
        os.chdir('..')

    if not os.path.exists('base/maps/'):
        print 'Can\'t find base/maps dir.'
        print 'You should execute map-get within the root of ufo directory tree.'
        sys.exit(3)

    if len(args) < 1:
        print "You must specify a directory."
        sys.exit(2)

    destination = args[0]
    changed_maps = args[1:]

    branch = mapsync.get_branch()
    destination = os.path.join(destination, branch)

    print 'Destination: %s' % os.path.realpath(destination)
    print '-----------'
    print 'Process will start in 5 seconds'
    time.sleep(5)

    gen(destination, changed_maps)


if __name__ == '__main__':
    main()
