#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import sys, os
try:
	from hashlib import md5
except ImportError:
	from md5 import md5
import urllib2
from gzip import GzipFile
from tempfile import mkstemp
import logging # TODO: use me
from logging import debug, error, info
import optparse

# path where exists ufo binary
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../..')

UFO2MAP = 'ufo2map'
if os.name == 'nt':
    UFO2MAP+= '.exe'

UFO2MAPFLAGS = ' -v 2 -nice 19 -extra'
URI = 'http://static.ufo.ludwigf.org/maps'
__version__ = '0.0.4.2'

displayDownloadStatus = False
displayAlreadyUpToDate = True

"""
written by Florian Ludwig <dino@phidev.org>

Changelog
0.0.4.2
 * compile ufo2map as release
 * add .map file hash into Maps

0.0.4 serverside work
 * execute ./configure
 * abort on configure or make errors

0.0.3 windows work
 * URI to maps changed.
 * check if ufo2map exists
 * use ufo2map.exe on windows systems
 * on windows always upgrade, generating is not supported anyway

0.0.2
 * handle corrupted mirror files
 * use optparse
 * allow upgrade even if ufo2map version differs
 * search root of ufo tree
 * display summary
 * generation: guess if ufo2map changed and maps need to be rebuild
"""

# TODO: use os.path.join

def run(cmd, mandatory=True):
    print cmd
    if os.system(cmd) and mandatory:
        sys.stderr.write('Faild to run: %s' % cmd)
        sys.exit(2)

def md5sum(path, binary = False):
    # to be used carefully with big files
    if binary:
    	return md5(open(path, "rb").read()).hexdigest()
    return md5(open(path).read()).hexdigest()


import time
def download(uri):
	global displayDownloadStatus
    request = urllib2.Request(uri)
    import platform
    p = ' '.join([platform.platform()] + list(platform.dist()))
    request.add_header('User-Agent', 'ufoai_map-get/%s (%s)' % (__version__, p))
    try:
        f = urllib2.build_opener().open(request)
    except urllib2.URLError, e:
        error('Err %s' % uri)
        error('%s: %s' % (e.message, e.reason[1]))
        sys.exit(6) # TODO

    re = out = ''
    t = 1
    data = f.read(10240)
    if not displayDownloadStatus:
    	print('downloading ' + uri)
    while data:
        re+= data
        if displayDownloadStatus and sys.stdout.isatty:
            out = '\r%s %9ikb' % (uri, len(re) / 1024)
            sys.stdout.write(out)
            sys.stdout.flush()
        t = time.time()
        data = f.read(10240)
        t = time.time() - t
    f.close()
    if displayDownloadStatus:
    	sys.stdout.write('\r%s\r' % (' '*len(out)))
    return re


def ufo2map_hash():
    """create an md5sum from the source and the binary of ufo2map."""
    # TODO fight against / handle svn:eol-style=native
    src = md5()
    files = []
    for i in os.walk('src/tools/ufo2map'):
        if '/.' in i[0]:
            continue
        files+= [os.path.join(i[0], j) for j in i[2]]
    files.sort()
    for fname in files:
        src.update(open(fname).read())
    src = src.hexdigest()
    bin = 'null'
    if os.path.exists(UFO2MAP):
        bin = md5(open(UFO2MAP).read()).hexdigest()
    debug('ufo2map_hash: %s %s' % (bin, src))
    return [bin, src]


def upgrade(arg):
    """Download the list of maps."""
    global displayAlreadyUpToDate
    maps = {}
    print 'getting list of available maps'
    for i in download(URI + '/Maps').split('\n'):
        i = i.split(' ')
        try:
            maps[i[0]] = (i[1], i[2])
        except:
            pass

    # check ufo2map's _source_ md5
    if 'ufo2map' in maps and maps['ufo2map'][1] == ufo2map_hash()[1]:
        print 'ufo2map version ok'
    elif 'ufo2map' not in maps:
        print 'Mirror corrupted, please try later. If problem persists contact admin.'
        sys.exit(6)
    else:
        print 'WARNING ufo2map version mismatch'
        if not raw_input('Continue? [Y|n]').lower() in ('y', ''):
            sys.exit(3)
    del maps['ufo2map']

	print 'INFO write files into ' + UFOAI_ROOT

    updated = missmatch = uptodate = 0
    mapnames = maps.keys()
    mapnames.sort()
    for i in mapnames:
        map_name = i[:-4] + ".map"
        mappath = UFOAI_ROOT + '/' + map_name
        bsppath = UFOAI_ROOT + '/' + i

        if not os.path.exists(mappath):
            print '%s not found' % map_name
            continue


        if md5sum(mappath) != maps[i][1]:
            print '%s version mismatch' % i
            missmatch+= 1
            continue

        if not os.path.exists(bsppath) or md5sum(bsppath, True) != maps[i][0]:
            fd, name = mkstemp()
            os.write(fd, download('%s/%s.gz' %(URI, i)))
            os.close(fd)
            data = GzipFile(name, 'rb').read()
            os.unlink(name)
            open(bsppath, 'wb').write(data)
            print '%s - updated' % i
            updated+= 1
        else:
            if displayAlreadyUpToDate:
                print '%s - already up to date' % i
            uptodate+= 1

    print '%d upgraded, %d version mismatched, %d already up to date' % (updated, missmatch, uptodate)


def gen(args):
    from random import choice
    if len(args) < 1:
        print "You must specify a directory."
        sys.exit(2)

    dst = args[0]
    changed_maps = args[1:]

    if not os.path.exists(dst):
        print "The path '%s' does not exist." % dst
        sys.exit(5)

    if not os.access(dst, os.R_OK | os.W_OK):
        print 'Permission denied. "%s"' % argsdst

    # read old md5 list
    old = {}
    if os.path.exists(os.path.join(dst, 'Maps')):
        for i in open(os.path.join(dst, 'Maps')):
            i = i.split(' ')
            old[i[0]] = i[1]

    # run make maps etc.
    # call separately otherwise linking happens last -> building maps fails
    run('sh configure --enable-release --disable-client --disable-uforadiant --disable-dedicated')
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

    run('make maps -j 2')
    print

    # create md5 sums of .map files
    maps = open(os.path.join(dst, 'Maps'), 'w')
    maps.write(' '.join(['ufo2map']+ufo2map_hash()) + '\n')

    maps_compiled = 0
    for dirname, dnames, fnames in os.walk('base/maps'):
        if '/.' in dirname:
            # this dir is a / within a hidden one
            continue

        for i in fnames:
            if not i.endswith('.map'):
                continue

            mfile = os.path.join(dirname, i)
            bfile = mfile[:-4] + '.bsp'
            if not os.path.exists(bfile):
                print "Warning: Cant find .bsp for %s" % mfile
                continue

            pmd5 = md5sum(mfile)
            mmd5 = md5sum(mfile, True)
            bmd5 = md5sum(bfile, True)

            if not bfile in old or bmd5 != old[bfile]:
                print '%s - updating' % bfile
                maps_compiled+= 1
                if os.path.exists(os.path.join(dst, bfile)):
                    os.unlink(os.path.join(dst, bfile))

                # make sure destination directory exists
                dst_dir = os.path.split(os.path.join(dst, bfile))[0]
                if not os.path.exists(dst_dir):
                    os.makedirs(dst_dir)

                data = open(bfile).read()
                GzipFile(os.path.join(dst, bfile) + '.gz', 'w').write(data)
            else:
                print '%s - already up to date' % bfile

            maps.write(' '.join((bfile, bmd5, pmd5, mmd5)) + '\n')
    print ' %s maps compiled' % maps_compiled
    return maps_compiled


def main(argv=None):
    global displayDownloadStatus
    global displayAlreadyUpToDate
    commands = {'upgrade': upgrade,
                'generate': gen}

    parser = optparse.OptionParser(argv)
    parser.add_option('-v', '--verbose', action='store_true', dest='verbose')
    parser.add_option('', '--no-downloadstatus', action='store_false', dest='displayDownloadStatus')
    parser.add_option('', '--hide-uptodate', action='store_false', dest='displayAlreadyUpToDate')

    parser.usage = '%prog [options] command\n\n' \
                   'Commands:\n' \
                   ' upgrade - make sure all maps are up to date\n' \
                   ' generate DST - generate or update repository dir DST\n' \
                   '                (probably not what you want)'

    options, args = parser.parse_args()

    logging.basicConfig(level=options.verbose and logging.DEBUG or logging.INFO,
                        format='%(message)s')
    displayDownloadStatus = options.displayDownloadStatus
    displayAlreadyUpToDate = options.displayAlreadyUpToDate

    # on windows always just upgrade
    if os.name == 'nt':
        upgrade([])

    if not len(args) or args[0] not in commands:
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

    commands[args[0]](args[1:])


if __name__ == '__main__':
    main()
