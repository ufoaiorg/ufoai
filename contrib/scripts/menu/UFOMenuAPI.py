#!/usr/bin/env python

#
# @brief functions to check and update menu_*.ufo files
# @license Public domaine
# @note tested with Python 2.4.1
#

import os, sys, re
import os.path 

# path where exists ufo binary
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../../..')

removeComment = re.compile('(?:\s|^)(//.*$)', re.MULTILINE)
removeEvents = re.compile('(\s)(click|change|rclick|mclick|wheel|in|out|whup|whdown)\s*\{.*?\}', re.DOTALL)
removeFunctions = re.compile('(\s)(confunc|func)(\s+)([a-zA-Z0-9_-]*)(\s*)\{.*?\}', re.DOTALL)
removeOptions = re.compile('(\s)(option)(\s+)([a-zA-Z0-9_-]*)(\s*)\{.*?\}', re.DOTALL)

mouseevent = ["click", "rclick", "mclick", "wheel", "in", "out", "whup", "whdown"]

parseNodes = re.compile('\s*([a-zA-Z0-9_-]+)\s+([a-zA-Z0-9_-]+)\s*\{(.*?)\}', re.DOTALL)
matchParam = re.compile("(\".*?\"|(?!\")\S+)\s+", re.DOTALL)

renameMenuParam = '(\smenu\s+%s\s*\{\s*\{.*?\s)(%s)(\s)'
updateMenuParam = '(\smenu\s+%s\s*\{\s*\{.*?\s%s\s*)([^\"\s]\S*|\".*?\")(\s)'
updateNodeParam = '(\smenu\s+%s\s*\{.*?\s%s\s+%s\s*\{.*?%s\s+)([^\"\s]\S*|\".*?\")(\s)'


class Root:

	def __init__(self):
		self.nodes = NodeObject("", "")
		self.files = {}
		self.fileBases = {}

	def parseUFOParam(self, object, data):
		result = matchParam.findall(data.strip() + " ")
		while len(result) != 0:
			name = result.pop(0)
			value = None
			if name in mouseevent:
				if len(result) > 0:
					value = result.pop(0)
					# is event alone
					if value != '...':
						result.insert(0, value)
					
			else:
				if len(result) > 0:
					value = result.pop(0)
				else:
					print 'no value for the param \"' + name + '\" (' + object.name + ')'
			object.addParam(name, value)

	def parseUFOMenu(self, data, filename):
		data = removeComment.sub('', data)
		data = removeEvents.sub('\\1\\2 ...', data)
		data = removeFunctions.sub('\\1\\2\\3\\4\\5{}', data)
		data = removeOptions.sub('\\1\\2\\3\\4\\5', data)
		data = '\n\n' + data
	
		menus = data.split('\nmenu')
		menus.pop(0)
		for m in menus:
		
			m = m.split('{', 1)
			name = m[0].strip()
			menu = NodeObject("menu", name)
			menu.filename = filename
			menu.root = self
			m = m[1].strip()
			m = m[0:len(m) - 1]
	
			# menu param
			if m[0] == '{':
				m = m.split('}', 1)
				params = m[0][1:]
				m = m[1].strip()
				self.parseUFOParam(menu, params)
			
			# nodes
			nodes = parseNodes.findall(m)
			for node in nodes:
				behaviour, name, param = node
				node = NodeObject(behaviour, name)
				node.filename = filename
				node.root = self
				node.parent = menu
				menu.addChild(node)
				self.parseUFOParam(node, param)
			
			
			self.nodes.addChild(menu)

	def loadFile(self, filename):
		print 'load ' + filename
		file = open(UFOAI_ROOT + '/' + filename, "rt")
		data = file.read()
		file.close()
		
		self.files[filename] = data
		self.fileBases[filename] = data
		
		self.parseUFOMenu(data, filename)
	
	
	def loadAll(self):
		for f in os.listdir(UFOAI_ROOT + '/base/ufos'):
			if "menu_" not in f:
				continue
			self.loadFile('/base/ufos/' + f)

	def save(self):
		for f, data in self.files.iteritems():
			if data == self.fileBases[f]:
				continue

			print 'save ' + f
			file = open(UFOAI_ROOT + '/' + f, "wt")
			file.write(data)
			file.close()

	def renameMenuParam(self, node, param, newname):
		menu = node.name
		filename = node.filename
	
		data = self.files[filename]
		match = re.compile(renameMenuParam % (menu, param), re.DOTALL)
		data = match.sub('\\1' + newname + '\\3', data, 1)
		self.files[filename] = data

	def updateMenuParam(self, node, param, value):
		menu = node.name
		filename = node.filename
	
		data = self.files[filename]
		match = re.compile(updateMenuParam % (menu, param), re.DOTALL)
		data = match.sub('\\1' + value + '\\3', data, 1)
		self.files[filename] = data

	def updateNodeParam(self, node, param, value):
		menu = node.parent.name
		filename = node.filename
	
		data = self.files[filename]
		match = re.compile(updateNodeParam % (menu, node.behaviour, node.name, param), re.DOTALL)
		data = match.sub('\\1' + value + '\\3', data, 1)
		self.files[filename] = data

		
class NodeObject:
	def __init__(self, behaviour, name):
		self.behaviour = behaviour
		self.name = name
		self.param = {}
		self.child = []
		self.childname = {}
		self.mouseevent = None
		
	def addChild(self, node):
		self.child.append(node)
		self.childname[node.name] = node
		
	def addParam(self, name, value):
		if name == 'pos':
			v = value[1:len(value)-1].split(" ")
			self.pos = (int(v[0]), int(v[len(v)-1]))
		elif name == 'size':
			v = value[1:len(value)-1].split(" ")
			self.size = (int(v[0]), int(v[len(v)-1]))
		
		self.param[name] = value
		
	def __print__(self, level = 0):
		tab = '\t' * level
		print tab + self.behaviour + " " + self.name
		for name, value in self.param.iteritems():
			print tab + '\t' + name + ' ' + str(value)
		for child in self.child:
			child.__print__(level + 1)

	def existsParam(self, param):
		return param in self.param
	
	def renameParam(self, old, new):
		if self.behaviour == "menu":
			self.root.renameMenuParam(self, old, new)

	def updateParam(self, old, new):
		if self.behaviour == "menu":
			self.root.updateMenuParam(self, old, new)
		else:
			self.root.updateNodeParam(self, old, new)

	def useMouseEvent(self):
		if self.mouseevent == None:
			self.mouseevent = False
			for e in mouseevent:
				if e in self.param:
					self.mouseevent = True
					break
		return self.mouseevent



def renameMenuOriginToPos():
	root = Root()
	root.loadAll()
	#root.loadFile('base/ufos/menu_options.ufo')
	
	print '-----'
	
	for menu in root.nodes.child:
		if not menu.existsParam("origin"):
			continue
		print menu.name
		menu.renameMenuParam("origin", "pos")
	
	root.save()


def searchWindowpanelWithoutParentPosSize():
	root = Root()
	root.loadAll()
	#root.loadFile('base/ufos/menu_options.ufo')
	
	print '-----'
	
	for menu in root.nodes.child:
		if menu.existsParam("pos") and menu.existsParam("size"):
			continue
		for node in menu.child:
			if node.behaviour != "windowpanel":
				continue
				
			print menu.filename + ' > menu ' + menu.name + ' have no pos or size'


def fixeWindowpanelUnderWindow():
	root = Root()
	#root.loadAll()
	#root.loadFile('base/ufos/menu_singleplayer.ufo')
	#root.loadFile('base/ufos/menu_aircraft.ufo')
	#root.loadFile('base/ufos/menu_geoscape.ufo')
	#root.loadFile('base/ufos/menu_irc.ufo')
	#root.loadFile('base/ufos/menu_lostwon.ufo')
	#root.loadFile('base/ufos/menu_main.ufo')
	root.loadFile('base/ufos/menu_multiplayer.ufo')
	#root.loadFile('base/ufos/menu_options.ufo')
	#root.loadFile('base/ufos/menu_singleplayer.ufo')

	print '-----'
	
	for menu in root.nodes.child:
		if not menu.existsParam("pos") or not menu.existsParam("size"):
			continue
		print 'menu ' + menu.name
		
		# find 	windowpanel
		windowpanel = None
		for node in menu.child:
			if node.behaviour != "windowpanel":
				continue
			windowpanel = node
			break
		
		# menu without background
		if windowpanel == None:
			break

		# all right
		if windowpanel.pos == (0, 0):
			print 'menu ' + menu.name + ' (' + menu.filename + ') at right place'
			break
		
		dx, dy = windowpanel.pos
		x, y = menu.pos[0] + dx, menu.pos[1] + dy
		menu.updateParam("pos", '"' + str(x) + " " + str(y) + '"')
		
		for node in menu.child:
			if not node.existsParam("pos"):
				continue
				
			x, y = node.pos[0] - dx, node.pos[1] - dy
			node.updateParam("pos", '"' + str(x) + " " + str(y) + '"')

	root.save()

def moveOptionMenuContent():
	root = Root()
	root.loadFile('base/ufos/menu_options.ufo')

	print '-----'
	
	for menu in root.nodes.child:
		if menu.name not in ["options_video", "options_sound", "options_keys", "options_pause", "options_game", "options_input"]:
			continue
		print 'menu ' + menu.name
		
		dx, dy = 415, 0
		for node in menu.child:
			if not node.existsParam("pos"):
				continue
				
			x, y = node.pos[0] - dx, node.pos[1] - dy
			node.updateParam("pos", '"' + str(x) + " " + str(y) + '"')

	root.save()

def findNonULCheckBox():
	root = Root()
	root.loadAll()

	print '-----'
	
	for menu in root.nodes.child:
		print 'menu ' + menu.name + " " + menu.filename

		for node in menu.child:
			if node.behaviour != "checkbox":
				continue
			if not node.existsParam("align"):
				continue
			align = node.param["align"]
			## @todo the API must remove quotes 
			if align == "ul" or align == "\"ul\"":
				continue
		
			print node.name + " align=" + align

def moveMultiplayerMenuContent():
	root = Root()
	root.loadFile('base/ufos/menu_multiplayer.ufo')

	print '-----'
	
	for menu in root.nodes.child:
		if menu.name not in ["mp_serverbrowser", "mp_create_server", "mp_team"]:
			continue
		print 'menu ' + menu.name
		
		dx, dy = 415, 0
		for node in menu.child:
			if not node.existsParam("pos"):
				continue
				
			x, y = node.pos[0] - dx, node.pos[1] - dy
			node.updateParam("pos", '"' + str(x) + " " + str(y) + '"')

	root.save()

##
# @brief move the reserch table where we want

##
# Move a range of node into a new position
def moveFullCol(menu, prefix, number, newpos, posref, dexY):
	print "----- " + prefix + " -----"
	dec = menu.childname[prefix + "0"].pos
	dec = dec[0] - posref[0], dec[1] - posref[1]

	for i in xrange(0, number + 1):
		x, y = newpos[0] + dec[0], newpos[1] + dec[1] + i * dexY
		menu.childname[prefix + str(i)].updateParam("pos", '"' + str(x) + " " + str(y) + '"')


def moveFullPanel(menu, nodes, newpos, posref):
	print "----- " + "moveFullPanel" + " -----"
	dec = newpos[0] - posref[0], newpos[1] - posref[1]

	for n in nodes:
		x, y = menu.childname[n].pos
		x, y = x + dec[0], y + dec[1]
		menu.childname[n].updateParam("pos", '"' + str(x) + " " + str(y) + '"')
		

# @note update newpos to set a new position
def moveResearchTable():
	root = Root()
	root.loadFile('base/ufos/menu_research.ufo')

	print '-----'

	menu = root.nodes.childname["research"]
	posref = menu.childname["img_status0"].pos
	newpos = 50, 113
	
	moveFullCol(menu, "img_status", 27, newpos, posref, 25)
	moveFullCol(menu, "txt_item", 27, newpos, posref, 25)
	moveFullCol(menu, "txt_assigned", 27, newpos, posref, 25)
	moveFullCol(menu, "txt_available", 27, newpos, posref, 25)
	moveFullCol(menu, "txt_max", 27, newpos, posref, 25)
	moveFullCol(menu, "bt_rs_change", 27, newpos, posref, 25)	

	root.save()

def moveHireTable():
	root = Root()
	root.loadFile('base/ufos/menu_hire.ufo')
	print '-----'

	menu = root.nodes.childname["employees"]
	posref = menu.childname["bt_employee0"].pos
	newpos = 325, 180
	
	moveFullCol(menu, "bt_employee", 18, newpos, posref, 38)

	root.save()

def moveHirePanel():
	root = Root()
	root.loadFile('base/ufos/menu_hire.ufo')
	print '-----'

	nodes = ['name','bt_edit',
		'rank_img','rank_lbl',
		'pwr_lbl','pwr_val','pwr_brd','pwr_bar',
		'spd_lbl','spd_val','spd_bdr','spd_bar',
		'acc_lbl','acc_val','acc_bdr','acc_bar',
		'mnd_lbl','mnd_val','mnd_bdr','mnd_bar',
		'cls_lbl','cls_val','cls_bdr','cls_bar',
		'hvy_lbl','hvy_val','hvy_bdr','hvy_bar',
		'ass_lbl','ass_val','ass_bdr','ass_bar',
		'snp_lbl','snp_val','snp_bdr','snp_bar',
		'exp_lbl','exp_val','exp_bdr','exp_bar',
		'hp_lbl','hp_val','hp_bdr','hp_bar',
		'missions_lbl','missions_val','kills_lbl','kills_val'
	]

	menu = root.nodes.childname["employees"]
	posref = menu.childname["rank_lbl"].pos
	newpos = 485, 330
	
	moveFullPanel(menu, nodes, newpos, posref)

	root.save()


if __name__ == "__main__":
	#moveHirePanel()
	#root = Root()
	#root.loadAll()
	#root.loadFile('base/ufos/menu_team.ufo')
	pass

