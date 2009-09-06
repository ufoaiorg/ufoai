#!/usr/bin/python

#
# @brief extract node inheritance from .h and .c files
# @license Public domain
# @return a DOT document into stdout (http://www.graphviz.org/)
# @todo generate functions, confunc too
#

import os, os.path, sys
from ExtractNodeBehaviour import *

def genBehaviourDot(node):
	result = ""
	label = ""

	label = '<tr><td align="center">' + node.name + '</td></tr>\n'
	
	if len(node.properties) != 0:
		p = ''
		names = node.properties.keys()
		names.sort()
		for name in names:
			e = node.properties[name]
			if p != '':
				p = p + '<br />'
			p = p + '+ ' + e.name + ': ' + e.type
		label = label + '<tr><td align="left" balign="left">' + p + '</td></tr>'
	
	if len(node.methods) != 0:
		p = ''
		names = node.methods.keys()
		names.sort()
		for name in names:
			e = node.methods[name]
			if p != '':
				p = p + '<br />'
			p = p + '+ ' + e.name + ' ()'
		label = label + '<tr><td align="left" balign="left">' + p + '</td></tr>'

	label = '<table border="0" cellborder="1" cellspacing="0" cellpadding="4">\n' + label + '\n</table>'

	result = result + "\t_%s [rankdir=TB,shape=plaintext,label=<\n%s>]\n" % (node.name, label)
	if node.extends != None:
		result = result + "\t_%s -> _%s\n" % (node.name, node.extends)
	
	return result


def genDot(package):
	print "// contrib/scripts/menu/nodeInheritance.py > inheritance.dot"
	print "// dot -Tpng -oinheritance.png < inheritance.dot"
	print "digraph G {"
	print "\trankdir=LR"
	print "\tnode [shape=box]"
	print "\tedge [arrowhead=onormal]"
	print ""

	list = package.getBehaviourNames()
	list.sort()
	for name in list:
		node = package.getBehaviour(name)
		print genBehaviourDot(node)
	print "}"

extract = ExtractNodeBehaviour()
package = extract.getPackage()
genDot(package)
