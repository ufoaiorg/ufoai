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
import logging # TODO: use me
from logging import debug, error, info
import optparse

# path where exists ufo binary
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../..')

INTERACTIVE_REPLY = 'query'

UFO2MAP = 'ufo2map'
if os.name == 'nt':
    UFO2MAP+= '.exe'

JOBS = 1
UFO2MAPFLAGS = ' -v 4 -nice 19 -quant 4 -extra'
URI = 'http://ufoai.ninex.info/maps/2.4'
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

def execute(cmd):
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    data = p.stdout.read()
    return data

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

class Ufo2mapMetadata:

    def __init__(self):
        self.hash_from_source = None
        self.version = None

    def set_content(self, data):
        lines = data.split("\n")
        for line in lines:
            elements = line.split(":", 2)
            if elements[0].strip() == "version":
                self.version = elements[1].strip()
            elif elements[0].strip() == "hash_from_source":
                self.hash_from_source = elements[1].strip()

    def get_content(self):
        data = ""
        data += "version: %s\n" % self.version
        data += "hash_from_source: %s\n" % self.hash_from_source
        return data

    def extract(self):
        """Extract ufo2map information from source and binary files."""

        # TODO maybe useless
        UFO2MAP_DIRSRC = 'src/tools/ufo2map'
        if os.path.exists(UFO2MAP_DIRSRC):
            src = md5()
            files = []
            for i in os.walk(UFO2MAP_DIRSRC):
                if '/.' in i[0]:
                    continue
                files+= [os.path.join(i[0], j) for j in i[2]]
            files.sort()
            for fname in files:
                src.update(open(fname, 'rt').read())
            self.hash_from_source = src.hexdigest()

        # take the version from the more up-to-date
        timestamp = 0
        if os.path.exists(UFO2MAP):
            version = execute("./%s --version" % UFO2MAP)
            # format is ATM "version:1.2.5 revision:1"
            version = version.replace("version", "")
            version = version.replace("revision", "rev")
            version = version.replace(" ", "")
            version = version.replace(":", "")
            version = version.strip()
            # format converted to "1.2.5rev1"
            self.version = version

            stat = os.stat(UFO2MAP)
            timestamp = stat.st_mtime

        UFO2MAP_MAINSRC = 'src/tools/ufo2map/ufo2map.c'
        if os.path.exists(UFO2MAP_MAINSRC):
            file = open(UFO2MAP_MAINSRC, 'rt')
            data = file.read()
            file.close()
            version = ""
            v = re.findall("#define\s+VERSION\s+\"([0-9.]+)\"", data)
            r = re.findall("#define\s+REVISION\s+\"([0-9.]+)\"", data)
            version = v[0]
            if r != []:
                version += "rev" + r[0]

            stat = os.stat(UFO2MAP_MAINSRC)
            t = stat.st_mtime
            if (t > timestamp):
                timestamp = t
                self.version = version

class Object:
    pass

def ask_boolean_question(question):
    if INTERACTIVE_REPLY == 'yes':
        print question, " YES (auto reply)"
        return True
    if INTERACTIVE_REPLY == 'no':
        print question, " NO (auto reply)"
        return False
    return raw_input(question).lower() in ('y', '')

def upgrade(arg):
    """Download the list of maps."""
    global displayAlreadyUpToDate
    maps = {}
    print 'getting list of available maps'
    for l in download(URI + '/Maps').split('\n'):
        i = l.split(' ')
        if len(i) == 3:
            o = Object()
            if i[0] == 'ufo2map':
                o.binhash = i[1]
                o.srchash = i[2]
            else:
                o.bsphash = i[1]
                o.maphash = i[2]
            maps[i[0]] = o
        else:
            print 'Line "%s" corrupted' % l

    ufo2mapMeta = Ufo2mapMetadata()
    ufo2mapMeta.extract()

    # check ufo2map's _source_ md5
    if 'ufo2map' not in maps:
        print 'Mirror corrupted, please try later. If problem persists contact admin.'
        sys.exit(6)
    if maps['ufo2map'].srchash == ufo2mapMeta.hash_from_src:
        print 'ufo2map version ok'
    else:
        print 'WARNING ufo2map version mismatch'
        if not ask_boolean_question('Continue? [Y|n]'):
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


        if md5sum(mappath, True) != maps[i].maphash:
            print '%s version mismatch' % i
            missmatch+= 1
            continue

        if not os.path.exists(bsppath) or md5sum(bsppath, True) != maps[i].bsphash:
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

    ufo2mapMeta = Ufo2mapMetadata()
    ufo2mapMeta.extract()

    version = open(os.path.join(dst, 'UFO2MAP'), 'wt')
    version.write(ufo2mapMeta.get_content())
    version.close()

    # create md5 sums of .map files
    maps = open(os.path.join(dst, 'Maps'), 'w')
    maps.write(' '.join(['ufo2map', ufo2mapMeta.hash_from_source, ufo2mapMeta.hash_from_source]) + '\n')

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
    global displayDownloadStatus
    global displayAlreadyUpToDate
    global INTERACTIVE_REPLY, UFO2MAPFLAGS, JOBS

    print "map-get version " + __version__

    commands = {'upgrade': upgrade,
                'generate': gen}

    parser = optparse.OptionParser(argv)
    parser.add_option('-v', '--verbose', action='store_true', dest='verbose')
    parser.add_option('', '--no-downloadstatus', action='store_false', dest='displayDownloadStatus')
    parser.add_option('', '--hide-uptodate', action='store_false', dest='displayAlreadyUpToDate')
    parser.add_option("", "--reply", action="store", default="query", type="string", dest="reply", help='Allow to auto reply interactive questions, REPLY={yes,no,query}')
    parser.add_option("", "--flags", action="store", default=UFO2MAPFLAGS, type="string", dest="flags", help='Set specific ufo2map flags for map compilation, default value is "' + UFO2MAPFLAGS + '"')
    parser.add_option("-j", "--jobs", action="store", default=JOBS, type="int", dest="jobs", help='Allow JOBS jobs at once, default value is ' + str(JOBS))

    parser.usage = '%prog [options] command\n\n' \
                   'Commands:\n' \
                   ' upgrade - make sure all maps are up to date\n' \
                   ' generate DST - generate or update repository dir DST\n' \
                   '                (probably not what you want)'

    options, args = parser.parse_args()

    logging.basicConfig(level=options.verbose and logging.DEBUG or logging.INFO,
                        format='%(message)s')

    if not (options.reply in ['yes', 'no', 'query']):
        print 'Wrong param --reply={yes,no,query}'
        sys.exit(1)

    displayDownloadStatus = options.displayDownloadStatus
    displayAlreadyUpToDate = options.displayAlreadyUpToDate
    INTERACTIVE_REPLY = options.reply
    UFO2MAPFLAGS = options.flags
    JOBS = options.jobs

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
