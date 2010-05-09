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
import md5, os, sys
import cPickle
import re

# config
CACHING = True

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
<h1>Licenses in UFO:AI (<a href="http://ufoai.svn.sourceforge.net/viewvc/ufoai/ufoai/trunk/base/%s">base/%s</a>)</h1>
Please note that the information is extracted from the svn tags (svn:copyright, svn:license and svn:source).<br />
Warning: the statistics/graphs might be wrong since it would be too expensive to get information whether an entry was a directory in the past or not.
<br />
State as of revision <a href="http://ufoai.svn.sourceforge.net/viewvc/ufoai?view=rev&revision=%i">%i</a>.
<hr />

%s

</body></html>"""

def get(cmd, cacheable=True):
	if cacheable and CACHING:
		h = md5.md5(cmd).hexdigest()
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


def get_rev(d):
	rev = [i for i in get('svn info base/%s' % d, False).split('\n') if i.startswith('Revision')]
	return int(rev[0][10:])

def get_data(d, files):
	print ' getting data for "%s"' % d

	data = [i.split(' - ', 1) for i in get('svn propget svn:license base/%s -R' % d, False).split('\n') if i != '']
	remove = d != '' and len(d) + 1 or 0
	data = [(i[0][5 + remove:], i[1]) for i in data] # remove "base/.../"

	# get current revision
	rev = get_rev(d)

	print '  Current Revision r' + str(rev)

	licenses = {}
	for i in data:
		if not i[1] in licenses:
			licenses[i[1]] = []
		licenses[i[1]].append(i[0])

	# get unknown licenses
	for i in data:
		if i[0] in files:
			files.remove(i[0])
	licenses['UNKNOWN'] = files
	return licenses

FFILTERS = (re.compile('.txt$'),
			re.compile('.ufo$'),
			re.compile('.anm$'),
			re.compile('.bat$'),
			re.compile('.sh$'),
			re.compile('.def$'),
			re.compile('.win$'),
			re.compile('.ump$'),
			re.compile('.glsl$'),
			re.compile('.mat$'),
			re.compile('.lua$'),
			re.compile('.pl$'),
			re.compile('.py$'),
			re.compile('^Makefile'),
			re.compile('.html$'),
			re.compile('.cfg$'))

def ffilter(fname):
	for i in FFILTERS:
		if i.search(fname.lower()):
			print 'Ignore: ', fname
			return False
	return True

def get_all_data():
	print 'get all data'
	re = {}
	print ' svn list'
	files = filter(ffilter, get('svn list -r %i -R base/' % get_rev('.')).split('\n'))

	print '  done'

	print 'calculate'
	for i in os.listdir('base'):
		if os.path.isdir('base/'+i) and not i.startswith('.')and os.path.exists('base/%s/.svn' % i):
			re[i] = get_data(i, [x[len(i)+1:] for x in files if x.startswith(i + '/')])

	re[''] = get_data('', files) # mae
	return re


def generate(d, data, texture_map, map_texture):
	licenses = data[d]
	rev = get_rev(d)

	# --------------------------
	print 'Generating html for "%s"' % d
	print ' index.html'

	def test(x):
		if os.path.exists(x):
			return not os.path.isdir(x)
		else:
			return '.' in x

	content = '<br/><img src="plot.png" /><br/>'
	l_count = []
	for i in licenses:
		l_count.append((i, len([x for x in licenses[i] if test(x)])))

	def key(a):
		return a[1]

	l_count.sort(key=key, reverse=True)

	# per license files are namend: MD5(license_name).html
	for i in l_count:
		h = md5.md5(i[0]).hexdigest()
		content+= u'<li>%i - <a href="%s.html">%s</a></li>' % (i[1], h, i[0])

	if d != '':
		content = u'<a href="../index.html">Back</a><br/><ul>%s</ul>' %  content
	else:
		# print index
		index = u'<b>See also:</b><br /><ul>'
		for i in os.listdir('base'):
			if os.path.isdir('base/'+i) and not i.startswith('.') and os.path.exists('base/%s/.svn' % i):
				index+= u'<li><a href="%s/index.html">%s</a></li>' % (i,i)
		index+= u'</ul><br />'

		content = index + u'<ul>%s</ul>' %  content
		content+= '<hr/>You can <a href="http://ufoai.svn.sourceforge.net/viewvc/ufoai/ufoai/trunk/contrib/scripts/licenses.py">download</a> the source code. USE AT OWN RISK. NOT USERFRIENDLY.'

	html = HTML % (d, d, rev, rev, content)
	open('licenses/html/%s/index.html' % d, 'w').write(html)

	sources = [i.split(' - ', 1) for i in get('svn propget svn:source base/%s -R' % d, False).split('\n') if i != '']

	print 'Generating stats per license'

	for i in licenses:
		h = md5.md5(i).hexdigest()
		content = u'<a href="index.html">Back</a><br /><h2>%s</h2><ol>\n' % i
		for j in licenses[i]:
			if os.path.isdir('base/%s/%s' % (d, j)):
				continue

			# preview file
			img = ''
			if j.endswith('.jpg') or j.endswith('.tga') or j.endswith('.png'):
				thumb = '.thumbnails/%s/%s.png' % (d, j)
				if not os.path.exists('licenses/html/%s' % thumb):
					os.system('convert base/%s/%s -thumbnail 128x128 licenses/html/%s' % (d, j, thumb))
				img = '<img alt="%s" src="../%s" width="128" height="128" /> ' % (j, thumb)

			content+= u'\t<li> %s <a href="https://ufoai.svn.sourceforge.net/viewvc/*checkout*/ufoai/ufoai/trunk/base/%s/%s">%s</a>'  % (img, d, j ,j)
			copy = get('svn propget svn:copyright base/%s/%s' % (d, j), False).strip()
			copy = copy == '' and 'UNKNOWN' or copy
			content+= u' <span class="author">by %s</span>' % unicode(copy.decode('utf-8'))

			if j in sources:
				source = sources[j]
			else:
				source = ''
			if source != '':
				if source.startswith('http://'):
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
				else:
					content+= '<br/><b>UNUSED</b> (at least no map uses it)'

			content+= '</li>\n'
		content+= '</ol>'
		html = HTML % (d, d, rev, rev, content)
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

	import copy
	for i in data:
		data[i].sort(key=key)

	plot(d, data, times)


def plot(d, data, times):
	cmds = 'set terminal png;\n'
	cmds+= 'set data style linespoints;\n'
	cmds+= 'set output "licenses/html/%s/plot.png";\n' % d
	cmds+= 'set xrange [%i to %i];\n' % (min(times), max(times) + (max(times)-min(times))*0.15)

	cmds+= 'plot '
	p = []
	for i in data:
		plot_data = '\n'.join('%i %i' % (x[0], x[1]) for x in data[i])
		p.append("'%s' title \"%s\" " % ('/tmp/' + md5.md5(i).hexdigest(), i))
		open('/tmp/' + md5.md5(i).hexdigest(), 'w').write(plot_data)

	cmds+= ', '.join(p) + ';\n'
	open('/tmp/cmds', 'w').write(cmds)
	os.system('gnuplot /tmp/cmds')
	# raw_input('press return to continue')


from shutil import rmtree, copy
def clean_up():
	print 'clean up'
	rmtree('licenses/html')
	os.mkdir('licenses/html')
	for i in os.listdir('base'):
		if os.path.isdir('base/'+i) and not i.startswith('.') and os.path.exists('base/%s/.svn' % i):
			os.mkdir('licenses/html/'+i)
			if not os.path.exists('licenses/history/'+i):
				os.mkdir('licenses/history/'+i)

	for i in os.walk('base'):
		print i[0]
		if '/.' in i[0]:
			continue

		os.mkdir('licenses/html/.thumbnails/'+i[0][5:])



def setup():
	if not os.path.exists('licenses/'):
		os.mkdir('licenses/')

	if not os.path.exists('licenses/html/'):
		os.mkdir('licenses/html/')

	if not os.path.exists('licenses/history/'):
		os.mkdir('licenses/history/')

	if not os.path.exists('licenses/cache/'):
		os.mkdir('licenses/cache/')

	if os.system('gnuplot -h > /dev/null'):
		print 'you must have gnuplot installed'
		sys.exit(1)

	if os.system('convert -version > /dev/null'):
		print 'you must have imagemagick\'s convert installed'
		sys.exit(1)


def kill_suffix(i):
	return '.'.join(i.split('.')[:-1])


def main():
	global texture_map, map_texture #debugging
	setup()
	clean_up()

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

	print 'bye'

main()


