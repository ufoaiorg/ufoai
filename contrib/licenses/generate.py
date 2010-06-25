#!/usr/bin/env python

# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

# Copyright (C) 2006 - Florian Ludwig <dino@phidev.org>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.

#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.


import subprocess
import hashlib, os
import cPickle
import sys
import re
from xml.dom.minidom import parseString

# config
CACHING = True
ABS_URL = None

NON_FREE_LICENSES = [
"UNKNOWN", # ambiguous
"Creative Commons", # ambiguous
"Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported",
"Creative Commons Sampling Plus 1.0"
]

HTML = u"""<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<style>
body { color: #ffffff; background-color: #262626; font-family: verdana, helvetica, arial, sans-serif; margin: 0; padding: 0; }
html > body { font-size: 8.5pt; }
a { color: #ffd800; background-color: transparent; text-decoration: none; }
a:hover { color: #ffffff; background-color: transparent; text-decoration: none; }
li { margin-bottom: 8px; }
.author { }
</style></head>

<body>
<h1>Licenses in UFO:ai (base/%s)</h1>
Please note that the information are extracted from the svn tags.<br />
Warning: the statics/graphs might be wrong since it would be to expensive to get information if a enrty was a directory in the past or not.
<br />
State as in revision %i.
<hr />

%s

</body></html>"""

HTML_IMAGE = u"""<br/><table><tr>
<td><img src="plot.png" caption="Timeline of all game content by revision and licenses" /></td>
<td><img src="nonfree.png" caption="Timeline of non free content by revision and licenses" /></td>
</table><br/>
"""

###################################
# Metadata cache
###################################

metadata = None

class Metadata:
    def __init__(self, filename):
        self.license = None
        self.copyright = None
        self.source = None
        self.revision = None

    def __repr__(self):
        return str((self.license, self.copyright, self.source, self.revision))

## compute metadata for all versionned ./base file
# @todo can we "propget" more than 1 property? faster
def computeMetadata():
    global metadata
    metadata = {}

    print "Parse revisions..."
    xml = get('svn info base --xml -R', False)
    dom = parseString(xml)
    entries = dom.firstChild.getElementsByTagName('entry')
    for e in entries:
        path = e.getAttribute("path")
        meta = getMetadata(path)
        meta.revision = int(e.getAttribute("revision"))
    print "Parse licenses..."
    xml = get('svn pg svn:license base --xml -R', False)
    dom = parseString(xml)
    entries = dom.firstChild.getElementsByTagName('target')
    for e in entries:
        path = e.getAttribute("path")
        property = e.getElementsByTagName('property')[0]
        assert(property.getAttribute("name") == 'svn:license')
        meta = getMetadata(path)
        meta.license = property.firstChild.nodeValue
    print "Parse copyright..."
    xml = get('svn pg svn:copyright base --xml -R', False)
    dom = parseString(xml)
    entries = dom.firstChild.getElementsByTagName('target')
    for e in entries:
        path = e.getAttribute("path")
        property = e.getElementsByTagName('property')[0]
        assert(property.getAttribute("name") == 'svn:copyright')
        meta = getMetadata(path)
        meta.copyright = property.firstChild.nodeValue
    print "Parse sources..."
    xml = get('svn pg svn:source base --xml -R', False)
    dom = parseString(xml)
    entries = dom.firstChild.getElementsByTagName('target')
    for e in entries:
        path = e.getAttribute("path")
        property = e.getElementsByTagName('property')[0]
        assert(property.getAttribute("name") == 'svn:source')
        meta = getMetadata(path)
        meta.source = property.firstChild.nodeValue

def getMetadata(filename):
    global metadata

    if metadata == None:
        computeMetadata()

    ## get metadata from cache
    if filename in metadata:
        meta = metadata[filename]
    ## gen new structure
    else:
        meta = Metadata(filename)
        metadata[filename] = meta

    return meta

###################################
# Job
###################################

def get(cmd, cacheable=True):
    if cacheable and CACHING:
        h = hashlib.md5(cmd).hexdigest()
        if h in os.listdir('licenses/cache'):
            print ' getting from cache: ', cmd
            print ' ', h
            return open('licenses/cache/' + h).read()
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    data = p.stdout.read()

    if cacheable and CACHING:
        open('licenses/cache/' + h, 'w').write(data)
        print ' written to cache: ', cmd
    return data


def get_used_tex(m):
    used = []
    for i in open(m):
        i = i.split(')')
        if len(i) < 4:
            continue

        tex = i[3].strip().split(' ')[0]
        if tex not in used:
            used.append(tex)
    return used

def get_data(d, files):
    print ' getting data for "%s"' % d

    # get current revision
    filename = "base"
    if d != "":
        filename = "base/" + d
    meta = getMetadata(filename)
    rev = meta.revision

    print '  Current Revision r' + str(rev)

    licenses = {}
    for f in files:
        filename = "base/" + d + "/" + f
        if d == "":
            filename = "base/" + f
        meta = getMetadata(filename)
        l = meta.license
        if l == None:
            l = "UNKNOWN"
        if not (l in licenses):
            licenses[l] = []
        licenses[l].append(f)

    return licenses

# filters for files to ignore
FFILTERS = (re.compile('.txt$'),
            re.compile('.ufo$'),
            re.compile('.anm$'),
            re.compile('.mat$'),
            re.compile('.pl$'),
            re.compile('.py$'),
            re.compile('.glsl$'),
            re.compile('^Makefile'),
            re.compile('^COPYING'),
            re.compile('.html$'),
            re.compile('.cfg$'),
            re.compile('.lua$'),
            re.compile('.bsp$'))

def ffilter(fname):
    for i in FFILTERS:
        if i.search(fname.lower()):
            print 'Ignoriere: ', fname
            return False
    return True


def get_all_data():
    print 'get all data'
    result = {}
    allfiles = []

    for i in os.listdir('base'):
        if os.path.isdir('base/'+i) and not i.startswith('.') and os.path.exists('base/%s/.svn' % i):
            files = []
            # get list of files
            for path, dirnames, fnames in os.walk('base/' + i):
                for fname in fnames:
                    if not '/.' in path and ffilter(fname):
                        f = path[len(i)+ 6:]
                        if f != '': f += '/'
                        f += fname
                        files.append(f)
                        allfiles.append(i + '/' + f)
            result[i] = get_data(i, files)

    result[''] = get_data('', allfiles) # mae
    return result

def generate(d, data, texture_map, map_texture):
    licenses = data[d]
    if d == '':
        meta = getMetadata("base")
    else:
        meta = getMetadata("base/" + d)
    rev = meta.revision

    # --------------------------
    print 'Generating html for "%s"' % d
    print ' index.html'

    def test(x):
        if os.path.exists(x):
            return not os.path.isdir(x)
        else:
            return '.' in x

    content = HTML_IMAGE

    l_count = []
    for i in licenses:
        l_count.append((i, len([x for x in licenses[i] if test(x)])))

    def key(a):
        return a[1]

    l_count.sort(key=key, reverse=True)

    # per license files are namend: MD5(license_name).html
    content += '<ul>'
    for i in l_count:
        h = hashlib.md5(i[0]).hexdigest()
        content += u'<li>%i - <a href="%s.html">%s</a></li>' % (i[1], h, i[0])
    content += '</ul>'

    if d != '':
        content = u'<a href="../index.html">Back</a><br/>%s' %  content
    else:
        # print index
        index = u'<b>See also:</b><ul>'
        for i in os.listdir('base'):
            if os.path.isdir('base/'+i) and not i.startswith('.') and os.path.exists('base/%s/.svn' % i):
                index+= u'<li><a href="%s/index.html">%s</a></li>' % (i,i)
        index += '</ul>'

        content = index + u'%s' %  content
        content+= '<hr/>You grab the source code from ufo:ai\'s svn. USE AT OWN RISK.'

    html = HTML % (d, rev, content)
    open('licenses/html/%s/index.html' % d, 'w').write(html)
    for i in licenses:
        h = hashlib.md5(i).hexdigest()
        content = u'<a href="index.html">Back</a><br /><h2>%s</h2><ol>' % i
        for j in licenses[i]:
            if os.path.isdir('base/%s/%s' % (d, j)):
                continue

            # preview file
            img = ''
            if j.endswith('.jpg') or j.endswith('.tga') or j.endswith('.png'):
                thumb = '.thumbnails/%s/%s.png' % (d,j)
                if d != '' and not os.path.exists('licenses/html/%s' % thumb):
                    os.system('convert base/%s/%s -thumbnail 128x128 licenses/html/%s' % (d, j, thumb))
                img = '<img src="%s%s"/>' % (ABS_URL, thumb)

            content+= u'<li>%s<a href="https://ufoai.svn.sourceforge.net/viewvc/*checkout*/ufoai/ufoai/trunk/base/%s/%s" title="Download">%s</a> - <a href="http://ufoai.svn.sourceforge.net/viewvc/ufoai/ufoai/trunk/base/%s/%s?view=log" title="History">%s</a>' % (img, d, j, j, d, j, j)

            filename = "base/" + d + "/" + j
            if d == '':
                filename = "base" + "/" + j
            meta = getMetadata(filename)

            copy = meta.copyright
            if copy == None:
                copy = "UNKNOWN"
            content+= u' <span>by %s</span>' % unicode(copy)

            source = meta.source
            if source == None:
                source = ''
            if source != '':
                if source.startswith('http://') or source.startswith('ftp://'):
                    source = '<a href="%s">%s</a>' % (source, source[7:])
                content+= '<br/>Source: ' + source

            if d == 'maps' and j.endswith('.map'):
                if kill_suffix(j) in map_texture:
                    content+= '<br/><div>Uses: %s </div>' % ', '.join(map_texture[kill_suffix(j)])
                else:
                    content+= 'wtf?'
            elif d == 'textures':
                if kill_suffix(j) in texture_map:
                    content+= '<br/><div>Used in: %s </div>' % ', '.join(texture_map[kill_suffix(j)])
                elif j.find('_nm.') or j.find('_gm.') or j.find('_sm.') or j.find('_rm.'):
                    content+= '<br/><div>Special map - only indirectly used</div>'
                else:
                    content+= '<br/><b>UNUSED</b> (at least no map uses it)'

            content+= '</li>'
        content+= '</ol>'
        html = HTML % (d, rev, content)
        html = html.encode('UTF-8')
        open('licenses/html/%s/%s.html' % (d, h), 'w').write(html)

    cPickle.dump(licenses, open('licenses/history/%s/%i' % (d, rev), 'w'))

    # generate graph!
    data = {}
    print 'Ploting'
    times = []

    for time in sorted(os.listdir('licenses/history/%s' % d)):
        if os.path.isdir('licenses/history/%s/%s' % (d, time)):
            continue

        this = cPickle.load(open('licenses/history/%s/%s' % (d, time)))
        times.append(int(time))
        for i in this:
            if i not in data:
                data[i] = []

            # the >>if '.' in x<< is a hack to check if it is a file..
            # not "good practice" but not easy to get for files/durs that
            # might not exist anymore ;)
            data[i].append([int(time), len([x for x in this[i] if test(x)])])

    def key(x):
        return x[0]

    for i in data:
        data[i].sort(key=key)

    plot(d, data, times, "plot.png")
    nonfree = {}
    for l in NON_FREE_LICENSES:
        if data.has_key(l):
            nonfree[l] = data[l]
    plot(d, nonfree, times, "nonfree.png")


def plot(d, data, times, imagename):
    cmds = 'set terminal png;\n'
    cmds+= 'set data style linespoints;\n'
    cmds+= 'set output "licenses/html/%s/%s";\n' % (d, imagename)
    cmds+= 'set xrange [%i to %i];\n' % (min(times), max(times) + 1 + (max(times)-min(times))*0.15)

    cmds+= 'plot '
    p = []
    for i in data:
        plot_data = '\n'.join('%i %i' % (x[0], x[1]) for x in data[i])
        p.append("'%s' title \"%s\" " % ('/tmp/' + hashlib.md5(i).hexdigest(), i))
        open('/tmp/' + hashlib.md5(i).hexdigest(), 'w').write(plot_data)

    cmds+= ', '.join(p) + ';\n'
    open('/tmp/cmds', 'w').write(cmds)
    os.system('gnuplot /tmp/cmds')
    # raw_input('press return to continue')


def setup(output_path):
    """Check if output folders etc. are in place"""
    if not os.path.exists(output_path + '/licenses'):
        print 'creating bas directory'
        os.mkdir(output_path + '/licenses')
    
    for p in ['cache', 'history', 'html']:
        test_path = '%s/licenses/%s' % (output_path, p)
        if not os.path.exists(test_path):
            print 'creating', os.path.abspath(test_path)
            os.mkdir(test_path)


from shutil import rmtree
def clean_up(output_path):
    """Delete old html files and create needed directories"""
    print 'clean up'
    rmtree(output_path + '/licenses/html')
    os.mkdir(output_path + '/licenses/html')
    for i in os.listdir('base'):
        if os.path.isdir('base/'+i) and not i.startswith('.') and os.path.exists('base/%s/.svn' % i):
            os.mkdir(output_path + '/licenses/html/'+i)
            if not os.path.exists(output_path + '/licenses/history/'+i):
                os.mkdir(output_path + '/licenses/history/'+i)

    for path, dnames, fnames in os.walk('base'):
        if '/.' in path:
            continue
        os.mkdir(output_path + '/licenses/html/.thumbnails/'+path[5:])


def kill_suffix(i):
    return '.'.join(i.split('.')[:-1])


def main():
    global texture_map, map_texture # debugging
    global ABS_URL # config
    from optparse import OptionParser

    parser = OptionParser()
    parser.add_option("-o", "--output", dest="output",
                      help="Path to output/working directory ", metavar="DIR")
    parser.add_option("-u", "--url", dest="abs_url",
                      help="base URL where the files will be served", metavar="DIR")

    (options, args) = parser.parse_args()

    output_path = options.output
    if not output_path:
        # default path: relative to working directory
        output_path = '.'

    ABS_URL = options.abs_url
    if not ABS_URL:
        print "-u is required."
        sys.exit(1)

    setup(output_path)
    clean_up(output_path)

    # do it for the hole thing
    data = get_all_data()

    # get map data / texture usage
    texture_map = {}
    map_texture = {}

    print 'get texture usage'
    for i in os.walk('base/maps'):
        for f in i[2]:
            if not f.endswith('.map'):
                continue

            path = (i[0] +'/' + f)[10:]
            map_texture[kill_suffix(path)] = []
            for tex in get_used_tex(i[0] +'/' + f):
                map_texture[kill_suffix(path)].append(tex)
                if not tex in texture_map:
                    texture_map[tex] = []
                texture_map[tex].append(kill_suffix(path))

    for i in os.listdir('base'):
        if os.path.isdir('base/'+i) and not i.startswith('.') and os.path.exists('base/%s/.svn' % i):
            generate(i, data, texture_map, map_texture)
            print '\n'
    generate('', data, texture_map, map_texture)

    # copy('licenses.py', 'licenses/html')
    print 'bye'


if __name__ == '__main__':
    main()

