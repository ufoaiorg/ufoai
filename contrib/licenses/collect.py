import os, re

BASE_PATH = '../../base'

def trim(fname):
    if fname.startswith(BASE_PATH):
        return fname[len(BASE_PATH)+1:]
    return fname

def static(uri): # XXX move into config
    return 'http://static.ufo.ludwigf.org/' + uri

FFILTERS = (re.compile('.txt$'),
            re.compile('.ufo$'),
            re.compile('.anm$'),
            re.compile('.mat$'),
            re.compile('.pl$'),
            re.compile('.py$'),
            re.compile('^Makefile'),
            re.compile('.html$'),
            re.compile('.cfg$'))

def ffilter(fname):
    for i in FFILTERS:
        if i.search(fname.lower()):
            return False
    return True


class Dir(dict):
    def __init__(self, *args):
        self.name = None
        dict.__init__(self, *args)
    files = property(lambda self: [x for x in self.itervalues() if isinstance(x, File)])
    dirs = property(lambda self: [x for x in self.itervalues() if isinstance(x, Dir)])

    def insert(self, path, obj):
        this, remainder = (path if path.endswith('/') else path+'/').split('/', 1)
        if remainder:
            if not this in self:
                self[this] = Dir()
                self[this].name = this
            self[this].insert(remainder, obj)
        else:
            self[this] = obj

    def find(self, path):
        """Return object at 'path' if 'path' does not exists None is returnd"""
        if path in ('', '/'):
            return self
        this, remainder = (path if path.endswith('/') else path+'/').split('/', 1)
        if not this in self:
            return None
        if remainder:
            return self[this].find(remainder)
        return self[this]

    def _files_r(self):
        """Return all files recursivly"""
        re = self.files
        for d in self.dirs:
            re.extend(d.files_r)
        return re
    files_r = property(_files_r)


class File(object):
    def __init__(self, fname):
        self.license = None
        self.copyright = None
        self.source = None
        self.name = fname

    def show(self):
        # TODO move this somewhere else
        show = u'<b>%s</b> by %s <br /> %s' % (self.name, self.copyright, \
            self.source if self.source != None else '')
        f = self.name.lower()
        if f.endswith('.jpg') or f.endswith('.tga') or f.endswith('.png'):
            show+= '<br /><img src="'+static(thumbpath(self.name))+'" />'
        return show


def get_licenses(dir):
    licenses = {}
    for file in dir.files_r:
        if not file.license in licenses:
            licenses[file.license] = set()
        licenses[file.license].add(file)
    return licenses


# read data
import subprocess as sp

def getLicenseInfo():
    p = sp.Popen(['cat', BASE_PATH+'/LICENSES'], stdout=sp.PIPE)
    raw = [i.split(' | ') for i in p.stdout.read().split('\n') if i != '']

    for row in raw:
        filename, license, author, source = row
        if not os.path.isfile(filename):
            continue
        filename = filename[len(BASE_PATH)+1:]
        fobj = DATA.find(filename)
        if fobj == None:
            fobj = File(filename)
            DATA.insert(filename, fobj)
        setattr(fobj, 'license', license)
        setattr(fobj, 'copyright', author)
        setattr(fobj, 'source', source)


DATA = Dir()

def walk(func, dirname, fnames):
    for fname in fnames:
        if fname.startswith('.'):
            fnames.remove(fname)

    for fname in fnames:
        path = os.path.join(dirname, fname)
        if os.path.isfile(path):
            func(path)

def thumbpath(fname):
    return 'generated/.thumbnails/' + trim(fname) + '.png'

def thumbnail(fname):
    f = fname.lower()
    if f.endswith('.jpg') or f.endswith('.tga') or f.endswith('.png'):
        thumb = thumbpath(fname)
        if not os.path.exists(os.path.dirname(thumb)):
            os.makedirs(os.path.dirname(thumb))

        if os.path.exists(thumb):
            if os.stat(thumb).st_mtime > os.stat(fname).st_mtime:
                return
        sp.Popen(['convert', fname, '-thumbnail', '128x128', thumb]).wait()


def checkin(fname):
    fname = trim(fname)
    if DATA.find(fname) == None and ffilter(os.path.basename(fname)):
        DATA.insert(fname, File(fname))

def update():
    # update preview images
    os.path.walk(BASE_PATH, walk, thumbnail)

    # collect data
    getLicenseInfo()

    # get files that dont have an svn:* prop
    # "svn ls" is pretty slow, so I collect the file that
    # are actually in the filesystem - so
    # WARNING: files that are localy only will show
    # up as I dont check they are in the svn
    os.path.walk(BASE_PATH, walk, checkin)

    # TODO: delete files that does not exists anymore
