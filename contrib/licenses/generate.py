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
import shutil
from xml.dom.minidom import parseString
import resources

# config
ABS_URL = None
EOL = "\n"
THUMBNAIL = True
PLOT = True
RESOURCES = resources.Resources()

NON_FREE_LICENSES = set([
    "UNKNOWN", # ambiguous
    "Creative Commons", # ambiguous
    "Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported",
    "Creative Commons Sampling Plus 1.0"
    ])

# filters for files to ignore (lower case)
FILENAME_FILTERS = (re.compile('.txt$'),
    re.compile('\.ufo$'),
    re.compile('\.anm$'),
    re.compile('\.mat$'),
    re.compile('\.pl$'),
    re.compile('\.py$'),
    re.compile('\.map$'),
    re.compile('\.glsl$'),
    re.compile('^\.'),
    re.compile('/\.'),
    re.compile('^makefile'),
    re.compile('copying$'),
    re.compile('\.html$'),
    re.compile('\.cfg$'),
    re.compile('\.lua$'),
    re.compile('\.bsp$'),
    re.compile('\.ump$')
    )

HTML = u"""<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<!-- STYLELINK -->
</head>

<body>
<h1>Licenses in UFO:AI (<!-- TITLE -->)</h1>
Please note that the information are extracted from <a href="http://ufoai.git.sourceforge.net/git/gitweb.cgi?p=ufoai/ufoai;a=blob;f=LICENSES">LICENSES</a> file.<br />
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
# Util
###################################

def fileNameFilter(fname, log=True):
    for i in FILENAME_FILTERS:
        if i.search(fname.lower()):
            if log:
                print 'Ignoriere: ', fname
            return False
    return True

###################################
# Job
###################################

def generateGraph(output, d):
    data = {}
    print 'Ploting "%s"' % d
    times = []

    if not os.path.exists(output + '/licenses/history%s' % d):
        return False

    def test(x):
        if os.path.exists(x):
            return not os.path.isdir(x)
        else:
            return '.' in x

    for time in sorted(os.listdir(output + '/licenses/history%s' % d)):
        if os.path.isdir(output + '/licenses/history%s/%s' % (d, time)):
            continue

        this = cPickle.load(open(output + '/licenses/history%s/%s' % (d, time)))
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

    basedir = output + '/licenses/html' + d
    if not os.path.exists(basedir):
        os.makedirs(basedir)

    plot(output, d, data, times, "plot.png")
    nonfree = {}
    for l in NON_FREE_LICENSES:
        if data.has_key(l):
            nonfree[l] = data[l]
    plot(output, d, nonfree, times, "nonfree.png")
    return True

def plot(output, d, data, times, imagename):
    cmds = 'set terminal png;\n'
    cmds+= 'set object 1 rect from graph 0, graph 0 to graph 1, graph 1 back;\n'
    cmds+= 'set object 1 rect fc rgb "grey" fillstyle solid 1.0;\n'

    ymax = 0
    lastrevision = 0
    for i in data:
        for dot in data[i]:
            if dot[1] > ymax:
                ymax = dot[1]
            if dot[0] > lastrevision:
                lastrevision = dot[0]

    # fix 0 value when the license is no more used
    for i in data:
        lastelement = data[i][len(data[i]) - 1]
        if lastelement[0] < lastrevision:
            data[i].append((lastelement[0] + 1, 0))

    cmds+= 'set data style linespoints;\n'
    cmds+= 'set output "%s/licenses/html%s/%s";\n' % (output, d, imagename)
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

def clean_up(output_path):
    """Delete old html files and create needed directories"""
    print 'Clean up'
    shutil.rmtree(output_path + '/licenses/html')
    os.mkdir(output_path + '/licenses/html')
    for i in os.listdir('base'):
        if os.path.isdir('base/'+i) and not i.startswith('.'):
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

class Analysis(object):
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
        resource = RESOURCES.getResource(self.inputDir)
        self.revision = resource.revision
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
        if not fileNameFilter(fileName, log=False):
            return
        if not fileNameFilter(fileName):
            return
        if os.path.isdir(fileName):
            self.addSubDir(fileName)
        else:
            self.addFile(fileName)

    def addSubDir(self, dirName):
        meta = RESOURCES.getResource(dirName)
        if meta.revision == None:
            print "RETURN", meta.fileName, meta.revision, RESOURCES.mode
            return
        a = Analysis(dirName, deep = self.deep + 1, base = self.base)
        self.subAnalysis.add(a)

        mergeGroup(self.contentByLicense, a.contentByLicense)
        mergeGroup(self.contentByCopyright, a.contentByCopyright)
        mergeGroup(self.contentBySource, a.contentBySource)
        self.count += a.count

    def addFile(self, fileName):
        meta = RESOURCES.getResource(fileName)
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
        content += '<a href="http://ufoai.git.sourceforge.net/git/gitweb.cgi?p=ufoai/ufoai;a=blob;f=%s" title="Download">download</a>' % meta.fileName
        content += ', '
        content += '<a href="http://ufoai.git.sourceforge.net/git/gitweb.cgi?p=ufoai/ufoai;a=history;f=%s" title="History">history</a>' % meta.fileName
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
        html = html.replace("<!-- TITLE -->", "base" + self.getLocalDir())

        # navigation
        navigation = "<ul>" + EOL
        if name != "":
            navigation += '<li><a href="../index.html">back</a></li>' + EOL

        subanalysis = []
        for a in self.subAnalysis:
            subanalysis.append((a.getName(), a))
        subanalysis.sort()
        for a in subanalysis:
            a = a[1]
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
            if generateGraph(output, self.getLocalDir()):
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
                      help="disable plots computation and display")

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

    files = ["style.css", "grid.png"]
    for f in files:
        print "Copying " + f
        shutil.copy (sys.path[0] + "/resources/" + f, output_path + "/licenses/html/" + f)

    # map-texture relations
    RESOURCES.computeTextureUsageInMaps()
    RESOURCES.computeResourceUsageInUFOScripts()

    analyse = Analysis('base')
    analyse.write(output_path)

    print 'bye'

if __name__ == '__main__':
    main()
