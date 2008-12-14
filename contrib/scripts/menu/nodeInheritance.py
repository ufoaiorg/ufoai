#!/usr/bin/env python

#
# @brief extract node inheritance from .h and .c files
# @license Public domain,e
# @return a DOT document into stdout (http://www.graphviz.org/)
# @todo generate properties, functions, confunc too
#

import os, os.path, sys

# path where exists ufo binary
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../../..')

dir = UFOAI_ROOT + '/src/client/menu/node'


class NodeBehaviour:

	def __init__(self):
		self.name = ""
		self.extends = "node"
		pass

	def init(self, dic):
		self.name = dic['name']
		if 'extends' in dic:
			self.extends = dic['extends']
		
	def genDot(self):
		result = ""
		print "\t_%s [label=\"%s\"]" % (self.name, self.name)
		print "\t_%s -> _%s" % (self.name, self.extends)
		
		return result

		
def extractData():
	result = []
	for f in os.listdir(dir):
		if ".c" not in f:
			continue

		file = open(dir + '/' + f, "rt")
		data = file.read()
		file.close()

		data = data.split("\nvoid MN_Register")
		data.pop(0)
		
		for code in data:
			code = code.split('{')[1]
			code = code.split('}')[0]
			
			lines = code.split('\n')
			dic = {}
			for l in lines:
				l = l.replace("behaviour->", "")
				l = l.replace(";", "")
				e = l.split('=')
				if len(e) != 2:
					continue
				v = e[1].strip()
				if v[0] == '"':
					v = v[1:len(v)-1]
				dic[e[0].strip()] = v
			
			n = NodeBehaviour()
			n.init(dic)
			result.append(n)

	return result
			
def genDot(nodeList):
	print "// contrib/scripts/menu/nodeInheritance.py > inheritance.dot"
	print "// dot -Tpng -oinheritance.png < inheritance.dot"
	print "digraph G {"
	print "\trankdir=LR"
	print "\tnode [shape=box]"
	print "\tedge [arrowhead=onormal]"
	print ""
	print "\t_node [label=\"node\"]"
	print ""
	for n in nodeList:
		print n.genDot()
	print "}"

nodeList = extractData()
genDot(nodeList)

