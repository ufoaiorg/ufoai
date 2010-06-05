#!/usr/bin/python

#
# @brief check the usage of menuNode_s attributes into all menu file (call it from the root of the trunk)
# @license Public domain
# @return an XHTML page into stdout
# @todo reading "attributes" from the nodes.h file
#

import os, os.path, sys

# path where exists ufo binary
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../../..')

attributes = [
	"textalign",
	"text",
	"font",
	"icon",
	"image",
	"cvar",
	"border",
	"bgcolor",
	"bordercolor",
	"color",
	"selectedColor",
	"onClick",
	"onRightClick",
	"onMiddleClick",
	"onWheel",
	"onMouseEnter",
	"onMouseLeave",
	"onWheelUp",
	"onWheelDown",
	"onChange"
]

files = []
dir = UFOAI_ROOT + '/src/client/menu'

for f in os.listdir(dir):
	if ".c" not in f:
		continue

	file = open(dir + '/' + f, "rt")
	data = file.read()
	file.close()

	fd = f, data
	files.append(fd)

dir = UFOAI_ROOT + '/src/client/menu/node'

for f in os.listdir(dir):
	if ".c" not in f:
		continue

	file = open(dir + '/' + f, "rt")
	data = file.read()
	file.close()

	fd = f, data
	files.append(fd)

print "<html>"
print "<body>"
print "<table>"

print "<tr><td></td>\n"
for a in attributes:
	print "<th>" + a + "</th>\n"
print "</tr>"

for fd in files:
	f, data = fd

	print "<tr><th>" + f + "</th>"
	for a in attributes:
		c = data.count("node->" + a)
		style = ""
		if c > 0:
			style = " style=\"background-color:#00FF00\""
		else:
			a = ""
		print "<td" + style + ">" + a + "</td>"
	print "</tr>\n"

print "</table>"



print "<table>"
for a in attributes:

	print "<tr><th>" + a + "</th>"
	print "<td>"
	for fd in files:
		f, data = fd

		c = data.count("node->" + a)
		style = ""
		if c > 0:
			print f + " "
	print "</td>"
	print "</tr>"
print "</table>"


print "</body>"
print "</html>"
