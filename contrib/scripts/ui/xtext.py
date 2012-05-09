#!/usr/bin/env python
#
# @brief extract wiki documentation about behaviour from .h and .c files
# @license Public domain
# @return a MediaWiki document into stdout (http://ufoai.org/wiki/index.php/UI_node_behaviours)
# @todo generate confunc too
#

import os, os.path, sys
from ExtractNodeBehaviour import *

def genBehaviourXText(node):
	result = ""
	label = ""

	result += 'UI%sNodeProperties:\n' % node.name.title()
	if node.extends != None:
		result += '\t{UI%sNodeProperties}\n' % node.extends.title()

	if len(node.properties) > 0 or len(node.callbacks) > 0:
		result += "\t(\n"
		next = ""

		list = node.properties.keys()
		if len(list) != 0:
			list.sort()
			next = ""
			for name in list:
				e = node.properties[name]
				result += "\t\t%s(\"%s\" %s=UIValue)?\n" % (next, e.name, e.name)
				next = "& "

		list = node.callbacks.keys()
		if len(list) != 0:
			list.sort()
			for name in list:
				e = node.callbacks[name]
				result += "\t\t%s(\"%s\" %s=UIFuncStatements)?\n" % (next, e.name, e.name)
				next = "& "
		result += "\t)\n"

	result += '\t;\n'
	return result

def genXText(package):
	result = ""
	list = package.getBehaviourNames()
	list.sort()
	result += """// autogen xtext from source code\n"""
	result += "\n"
	for name in list:
		result += genBehaviourXText(package.getBehaviour(name))
		result += "\n"
	return result

def getXText():
	extract = ExtractNodeBehaviour()
	package = extract.getPackage()
	return genXText(package)

if __name__ == "__main__":
	print getXText()
