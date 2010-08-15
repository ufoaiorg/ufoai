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
EOL = "\n"
THUMBNAIL = True
PLOT = True

NON_FREE_LICENSES = set([
"UNKNOWN", # ambiguous
"Creative Commons", # ambiguous
"Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported",
"Creative Commons Sampling Plus 1.0"
])

CSS = u"""
body { color: #ffffff; background-color: #262626; font-family: verdana, helvetica, arial, sans-serif; margin: 0; padding: 0; }
html > body { font-size: 8.5pt; }
a { color: #ffd800; background-color: transparent; text-decoration: none; }
a:hover { color: #ffffff; background-color: transparent; text-decoration: none; }
li { margin-bottom: 8px; }
.author { }
.thumb { vertical-align:top; margin-right: 1em; border: 1em solid #101010; }
.badlicense { color:red; }
"""

HTML = u"""<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<!-- STYLELINK -->
</head>

<body>
<h1>Licenses in UFO:AI (<!-- TITLE -->)</h1>
Please note that the information are extracted from the svn tags.<br />
Warning: the statics/graphs might be wrong since it would be to expensive to get information if a enrty was a directory in the past or not.
<br />
State as in revision <!-- REVISION -->.
<hr />

<!-- NAVIGATION -->

<!-- CONTENT -->

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
    def __init__(self, fileName):
        self.fileName = fileName
        self.license = None
        self.copyright = None
        self.source = None
        self.revision = None
        self.usedByMaps = set([])
        self.useTextures = set([])

    def isImage(self):
        return self.fileName.endswith('.jpg') or self.fileName.endswith('.tga') or self.fileName.endswith('.png')

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

def getMetadata(fileName):
    global metadata

    if metadata == None:
        computeMetadata()

    ## get metadata from cache
    if fileName in metadata:
        meta = metadata[fileName]
    ## gen new structure
    else:
        meta = Metadata(fileName)
        metadata[fileName] = meta

    return meta

def getMetadataByShortImageName(fileName):
    if os.path.exists(fileName + ".png"):
        return getMetadata(fileName + ".png")
    if os.path.exists(fileName + ".jpg"):
        return getMetadata(fileName + ".jpg")
    if os.path.exists(fileName + ".tga"):
        return getMetadata(fileName + ".tga")
    return None

def computeTextureUsageInMaps():
    print 'Parse texture usage in maps...'
    for i in os.walk('base/maps'):
        for mapname in i[2]:
            if not mapname.endswith('.map'):
                continue
            mapname = i[0] + '/' + mapname
            mapmeta = getMetadata(mapname)

            for tex in get_used_tex(mapname):
                texname = "base/textures/" + tex
                texmeta = getMetadataByShortImageName(texname)
                # texture missing (or wrong python parsing)
                if texmeta == None:
                    print "Warning: \"" + texname + "\" from map \"" + mapname + "\" do not exists"
                    continue
                texmeta.usedByMaps.add(mapmeta)
                mapmeta.useTextures.add(texmeta)

###################################
# Util
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

def fileNameFilter(fname):
    for i in FFILTERS:
        if i.search(fname.lower()):
            print 'Ignoriere: ', fname
            return False
    return True

###################################
# Job
###################################

def generateGraph(d):
    data = {}
    print 'Ploting'
    times = []

    if not os.path.exists('licenses/history%s' % d):
        return False

    def test(x):
        if os.path.exists(x):
            return not os.path.isdir(x)
        else:
            return '.' in x

    for time in sorted(os.listdir('licenses/history%s' % d)):
        if os.path.isdir('licenses/history%s/%s' % (d, time)):
            continue

        this = cPickle.load(open('licenses/history%s/%s' % (d, time)))
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

    basedir = 'licenses/html' + d
    if not os.path.exists(basedir):
        os.makedirs(basedir)

    plot(d, data, times, "plot.png")
    nonfree = {}
    for l in NON_FREE_LICENSES:
        if data.has_key(l):
            nonfree[l] = data[l]
    plot(d, nonfree, times, "nonfree.png")
    return True

def plot(d, data, times, imagename):
    cmds = 'set terminal png;\n'
    cmds+= 'set object 1 rect from graph 0, graph 0 to graph 1, graph 1 back;\n'
    cmds+= 'set object 1 rect fc rgb "grey" fillstyle solid 1.0;\n'

    ymax = 0
    for i in data:
        for y in data[i]:
            if y[1] > ymax:
                ymax = y[1]

    cmds+= 'set data style linespoints;\n'
    cmds+= 'set output "licenses/html%s/%s";\n' % (d, imagename)
    cmds+= 'set xrange [%i to %i];\n' % (min(times), max(times) + 1 + (max(times)-min(times))*0.15)
    cmds+= 'set yrange [0 to %i];\n' % (int(ymax * 1.15))

    cmds+= 'plot '
    p = []
    for i in data:
        plot_data = '\n'.join('%i %i' % (x[0], x[1]) for x in data[i])
        p.append("'%s' title \"%s\" " % ('/tmp/' + hashlib.md5(i).hexdigest(), i))
        open('/tmp/' + hashlib.md5(i).hexdigest(), 'w').write(plot_data)

    if len(p) == 0:
        return

    cmds+= ', '.join(p) + ';\n'
    open('/tmp/cmds', 'w').write(cmds)
    os.system('gnuplot /tmp/cmds')
    # raw_input('press return to continue')


def setup(output_path):
    """Check if output folders etc. are in place"""
    if not os.path.exists(output_path + '/licenses'):
        print 'creating base directory'
        os.makedirs(output_path + '/licenses')

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

def group(dictionary, key, value):
    if not (key in dictionary):
        dictionary[key] = set([])
    dictionary[key].add(value)

def mergeGroup(groupDest, groupSrc):
    for k in groupSrc:
        for v in groupSrc[k]:
            group(groupDest, k, v)

class Analysis:
    def __init__(self, inputDir, deep = 0, base = None):
        self.inputDir = inputDir
        if base == None:
            base = self.inputDir
        self.base = base
        self.deep = deep
        self.subAnalysis = set([])
        self.contentByLicense = {}
        self.contentByCopyright = {}
        self.contentBySource = {}
        meta = getMetadata(self.inputDir)
        self.revision = meta.revision
        self.count = 0
        self.compute()

    def getLocalDir(self):
        return self.inputDir.replace(self.base, "", 1)

    def getName(self):
        n = self.inputDir.replace(self.base, "", 1)
        if n != "" and n[0] == '/':
            n = n[1:]
        return n

    def add(self, fileName):
        if fileName.startswith('.'):
            return
        if not fileNameFilter(fileName):
            return
        if os.path.isdir(fileName):
            self.addSubDir(fileName)
        else:
            self.addFile(fileName)

    def addSubDir(self, dirName):
        if not os.path.exists(dirName + '/.svn'):
            return
        a = Analysis(dirName, deep = self.deep + 1, base = self.base)
        self.subAnalysis.add(a)

        mergeGroup(self.contentByLicense, a.contentByLicense)
        mergeGroup(self.contentByCopyright, a.contentByCopyright)
        mergeGroup(self.contentBySource, a.contentBySource)
        self.count += a.count

    def addFile(self, fileName):
        meta = getMetadata(fileName)
        if meta.revision == None:
            return
        license = meta.license
        if license == None: license = "UNKNOWN"
        group(self.contentByLicense, license, meta)
        group(self.contentByCopyright, meta.copyright, meta)
        group(self.contentBySource, meta.source, meta)
        self.count += 1

    def compute(self):
        for file in os.listdir(self.inputDir):
            self.add(self.inputDir + '/' + file)

    def getContentEntry(self, output, meta):
        global THUMBNAIL
        file = meta.fileName.replace(self.base + '/', "", 1)

        content = "<li>"

        # preview
        if THUMBNAIL and meta.isImage():
            base = '.thumbnails/%s.png' % meta.fileName.replace(self.base + "/", "", 1)
            thumbFileName = output + '/licenses/html/' + base
            thumbURL = ('../' * self.deep) + base
            if not os.path.exists(thumbFileName):
                thumbdir = thumbFileName.split('/')
                thumbdir.pop()
                thumbdir = "/".join(thumbdir)
                if not os.path.exists(thumbdir):
                     os.makedirs(thumbdir)
                os.system('convert %s -thumbnail 128x128 %s' % (meta.fileName, thumbFileName))
            content += '<img class="thumb" src="%s" />' % thumbURL

        # name and links
        content += meta.fileName
        content += ' ('
        content += '<a href="https://ufoai.svn.sourceforge.net/viewvc/*checkout*/ufoai/ufoai/trunk/%s" title="Download">download</a>' % meta.fileName
        content += ', '
        content += '<a href="http://ufoai.svn.sourceforge.net/viewvc/ufoai/ufoai/trunk/%s?view=log" title="History">history</a>' % meta.fileName
        content += ')'

        # author
        copyright = meta.copyright
        if copyright == None:
            copyright = "UNKNOWN"
        content+= u'<br /><span>by %s</span>' % copyright

        # source
        source = meta.source
        if source != None:
            if source.startswith('http://') or source.startswith('ftp://'):
                source = '<a href="%s">%s</a>' % (source, source)
            content+= '<br />Source: ' + source

        if file.endswith('.map'):
            if len(meta.useTextures) > 0:
                list = []
                for m in meta.useTextures:
                    list.append(m.fileName)
                content += '<br /><div>Uses: %s </div>' % ', '.join(list)
            else:
                content += '<br />Note: Map without textures!'
        
        if 'textures/' in meta.fileName and meta.isImage():
            if len(meta.usedByMaps) > 0:
                list = []
                for m in meta.usedByMaps:
                    list.append(m.fileName)
                content+= '<br/><div>Used in: %s </div>' % ', '.join(list)
            elif '_nm.' in meta.fileName or '_gm.' in meta.fileName or '_sm.' in meta.fileName or '_rm.' in meta.fileName:
                content+= '<br/><div>Special map - only indirectly used</div>'
            else:
                content+= '<br/><b>UNUSED</b> (at least no map uses it)'

        content+= '</li>'

        return content

    def getLicenseFileName(self, license):
        name = license.lower()
        name = name.replace(' ', '')
        name = name.replace('-', '')
        name = name.replace(':', '')
        name = name.replace('.', '')
        name = name.replace('(', '')
        name = name.replace(')', '')
        name = name.replace('/', '')
        name = name.replace('_', '')
        name = name.replace('gplcompatible', '')
        name = name.replace('gnugeneralpubliclicense', 'gpl-')
        name = name.replace('generalpubliclicense', 'gpl-')
        name = name.replace('creativecommons', 'cc-')
        name = name.replace('attribution', 'by-')
        name = name.replace('sharealike', 'sa-')
        name = name.replace('license', '')
        name = name.replace('lisence', '')	# type problem
        name = name.replace('and', '_')
        return 'license-' + name + '.html'
        #return hashlib.md5(license).hexdigest() + '.html'

    def writeLicensePage(self, output, license, contents):
        style = '<link rel="stylesheet" type="text/css" href="' + ('../' * self.deep) + 'style.css" />'
        navigation = '<a href="index.html">back</a><br />' + EOL
        title = license
        content = '<h2>%s</h2>' % license

        content += '<ol>' + EOL
        for meta in contents:
            content += self.getContentEntry(output, meta) + EOL
        content += '</ol>'

        html = HTML
        html = html.replace("<!-- NAVIGATION -->", navigation)
        html = html.replace("<!-- STYLELINK -->", style)
        html = html.replace("<!-- TITLE -->", title)
        html = html.replace("<!-- CONTENT -->", content)
        html = html.replace("<!-- REVISION -->", str(self.revision))
        html = html.encode('utf-8')
        fileName = self.getLicenseFileName(license)
        basedir = output + '/licenses/html' + self.getLocalDir()
        if not os.path.exists(basedir):
            os.makedirs(basedir)
        open(basedir + '/' + fileName, 'w').write(html)

    def writeGroups(self, output, name, group):
        basedir = output + '/licenses/html' + self.getLocalDir()
        if not os.path.exists(basedir):
            os.makedirs(basedir)

        html = "<html>\n"
        html += "<head>\n"
        html += '<meta http-equiv="Content-Type" content="text/html; charset=utf-8">\n'
        html += "</head>\n"
        html += "<body><ul>\n"
        for k in group:
            if k == None:
                continue
            html += "<li>" + k + "<br /><small>"
            for m in group[k]:
                html += m.fileName + ' '
            html += "</small></li>\n"
        html += "</ul></body></html>"
        open(basedir + '/' + name + '.html', 'w').write(html.encode('utf-8'))

    def write(self, output):
        name = self.getName()
        html = HTML

        style = '<link rel="stylesheet" type="text/css" href="' + ('../' * self.deep) + 'style.css" />'
        html = html.replace("<!-- STYLELINK -->", style)
        html = html.replace("<!-- TITLE -->", self.getName())

        # navigation
        navigation = "<ul>" + EOL
        if name != "":
            navigation += '<li><a href="../index.html">back</a></li>' + EOL
        for a in self.subAnalysis:
            a.write(output)
            subname = a.getName()
            dir = a.getLocalDir().replace(self.getLocalDir(), "", 1)
            navigation += ('<li><a href=".%s/index.html">%s</a></li>' % (dir, subname)) + EOL
        navigation += "</ul>" + EOL
        html = html.replace("<!-- NAVIGATION -->", navigation)

        content = "<h2>Content by license</h2>" + EOL

        # save license in history
        licenses = {}
        for l in self.contentByLicense:
            licenses[l] = []
            for m in self.contentByLicense[l]:
                n = m.fileName.replace(self.base, "")
                licenses[l].append(n)
        basedir = output + '/licenses/history' + self.getLocalDir()
        if not os.path.exists(basedir):
            os.makedirs(basedir)
        cPickle.dump(licenses, open(basedir + '/' + str(self.revision), 'wt'))
        # generate graph
        if PLOT and self.count > 20:
            if generateGraph(self.getLocalDir()):
                content += HTML_IMAGE

        licenseSorting = []
        for l in self.contentByLicense:
            licenseSorting.append((len(self.contentByLicense[l]), l))
        licenseSorting.sort(reverse=True)

        content += "<ul>" + EOL
        for l in licenseSorting:
            l = l[1]
            classProp = ""
            if l in NON_FREE_LICENSES:
                classProp = " class=\"badlicense\""
            count = len(self.contentByLicense[l])
            self.writeLicensePage(output, l, self.contentByLicense[l])
            url = self.getLicenseFileName(l)
            content += ('<li>%i - <a%s href="%s">%s</a></li>' % (count, classProp, url, l)) + EOL
        content += "</ul>" + EOL
        html = html.replace("<!-- CONTENT -->", content)
        html = html.replace("<!-- REVISION -->", str(self.revision))

        basedir = output + '/licenses/html' + self.getLocalDir()
        if not os.path.exists(basedir):
            os.makedirs(basedir)
        file = open(basedir + '/index.html', 'wt')
        file.write(html)
        file.close()

        if name == "":
            file = open(basedir + '/style.css', 'wt')
            file.write(CSS)
            file.close()

        self.writeGroups(output, "copyrights", self.contentByCopyright)
        self.writeGroups(output, "licenses", self.contentByLicense)
        self.writeGroups(output, "sources", self.contentBySource)

def main():
    global ABS_URL, THUMBNAIL, PLOT # config
    from optparse import OptionParser

    parser = OptionParser()
    parser.add_option("-o", "--output", dest="output",
                      help="Path to output/working directory ", metavar="DIR")
    parser.add_option("-u", "--url", dest="abs_url",
                      help="base URL where the files will be served", metavar="DIR")
    parser.add_option("-t", "--disable-thumbnails", action="store_false", dest="thumbnail",
                      help="disable thumbnails computation and display")
    parser.add_option("", "--disable-plots", action="store_false", dest="plot",
                      help="disable plot computation and display")

    (options, args) = parser.parse_args()

    output_path = options.output
    if not output_path:
        # default path: relative to working directory
        output_path = '.'
    output_path = os.path.abspath(output_path)

    if options.thumbnail != None:
        THUMBNAIL = options.thumbnail
    if options.plot != None:
        PLOT = options.plot

    # @note we use it nowhere, cause everything is relative
    #ABS_URL = options.abs_url
    #if not ABS_URL:
    #    print "-u is required."
    #    sys.exit(1)

    print "-------------------------"
    print "Absolute URL:\t\t", ABS_URL
    print "Absolute output:\t", output_path
    print "Generate thumbnails:\t", THUMBNAIL
    print "Generate plots:\t\t", PLOT
    print "-------------------------"
    
    setup(output_path)
    clean_up(output_path)

    # map-texture relations
    computeTextureUsageInMaps()

    analyse = Analysis('base')
    analyse.write(output_path)

    print 'bye'

if __name__ == '__main__':
    main()

