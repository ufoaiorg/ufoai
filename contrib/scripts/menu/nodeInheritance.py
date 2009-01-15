#!/usr/bin/env python

#
# @brief extract node inheritance from .h and .c files
# @license Public domain
# @return a DOT document into stdout (http://www.graphviz.org/)
# @todo generate functions, confunc too
#

import os, os.path, sys

# path where exists ufo binary
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../../..')

dir = UFOAI_ROOT + '/src/client/menu/node'


class NodeBehaviour:

	def __init__(self):
		self.name = ""
		self.extends = "abstractnode"
		self.properties = {}
		pass

	def init(self, dic):
		self.name = dic['name']
		if (self.name == "abstractnode"):
			self.extends = None
		elif 'extends' in dic:
			self.extends = dic['extends']

	def initProperties(self, dic):
		self.properties = dic
		
	def genDot(self):
		result = ""
		label = ""

		label = '<tr><td align="center">' + self.name + '</td></tr>\n'
		
		if len(self.properties) != 0:
			p = ''
			names = self.properties.keys()
			names.sort()
			for name in names:
				type = self.properties[name]
				if p != '':
					p = p + '<br />'
				p = p + '+ ' + name + ': ' + type
			label = label + '<tr><td balign="left">' + p + '</td></tr>'

		label = '<table border="0" cellborder="1" cellspacing="0" cellpadding="4">\n' + label + '\n</table>'
			
		result = result + "\t_%s [rankdir=TB,shape=plaintext,label=<\n%s>]\n" % (self.name, label)
		if self.extends != None:
			result = result + "\t_%s -> _%s\n" % (self.name, self.extends)
		
		return result

# @brief Extract each properties into a text according to the structure name
def extractProperties(filedata, structName):
	signature = "const value_t %s[]" % structName
	properties = filedata.split(signature)
	properties = properties[1]

	i = properties.find('{')
	j = properties.find('};')
	properties = properties[i+1:j]

	result = {}
	for line in properties.split('},'):
		i = line.find('{')
		if i == -1:
			continue
		line = line[i+1:]
		e = line.split(',')

		name = e[0].strip()
		if name[0] == '"':
			name = name[1:len(name)-1]

		if name == 'NULL':
			continue

		type = e[1].strip()
		result[name] = type

	return result

# @brief Extract each body of registration function into a text
def extractRegistrationFunctions(filedata):
	result = []
	
	register = filedata.split("\nvoid MN_Register")
	register.pop(0)
	
	for body in register:
		body = body.split('{')[1]
		body = body.split('}')[0]
		result.append(body)

	return result

def extractData():
	result = []
	
	# all nodes
	for f in os.listdir(dir):
		if ".c" not in f:
			continue

		file = open(dir + '/' + f, "rt")
		data = file.read()
		file.close()

		for code in extractRegistrationFunctions(data):
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
			if 'properties' in dic:
				props = extractProperties(data, dic['properties'])
				n.initProperties(props)
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
	for n in nodeList:
		print n.genDot()
	print "}"

nodeList = extractData()
genDot(nodeList)
