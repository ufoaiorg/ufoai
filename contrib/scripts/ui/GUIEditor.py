#!/usr/bin/python

#
# @brief GUI to check UFOAI GUI. Only a viewer for the moment
# @license Public domain
# @note tested with Python 2.4.1
#

import sys
from Tkinter import *
from UFOMenuAPI import Root

#print "Python version", sys.version_info

# @brief Window to view an UFOAI window
class View(Toplevel):
	def __init__(self, parent):
		Toplevel.__init__(self, parent)
		self.title('View')
		self.parent = parent
		self.protocol("WM_DELETE_WINDOW", self.close)

		self.initialize()

	def initialize(self):
		self.canvas = Canvas(self, width=1024, height=768)
		self.canvas.pack(side=TOP)

	def update(self):
		i = self.parent.list.curselection()[0]
		name = self.parent.list.get(i)

		self.canvas.delete(ALL)
		menu = self.parent.content.nodes.childname[name]

		menux, menuy, x2, y2 = 0, 0, 1024, 768
		if menu.existsParam("pos") and menu.existsParam("size"):
			menux, menuy = menu.pos
			w, h = menu.size
			x2, y2 = menux + w, menuy + h
		self.canvas.create_rectangle(menux, menuy, x2, y2, fill="white")

		for node in menu.child:
			if not node.existsParam("pos") or not node.existsParam("size"):
				continue
			print node.name
			x, y = menux + node.pos[0], menuy + node.pos[1]
			w, h = node.size
			x2, y2 = x + w, y + h
			self.canvas.create_rectangle(x, y, x2, y2, outline="black")

	def close(self):
		self.parent.view = None
		self.destroy()

# @brief Main window to select UFOAI windows
class Application(Tk):
	def __init__(self, parent):
		Tk.__init__(self, parent)
		self.title('UFO GUI editor')
		self.parent = parent
		self.initialize()
		self.update()

	def initialize(self):
		frame = Frame(self)
		frame.pack(side=TOP)

		scroll = Scrollbar(frame)
		scroll.pack(side=RIGHT, fill=Y)
		list = Listbox(frame)
		list.pack(side=LEFT, fill=Y)
		scroll.config(command=list.yview)
		list.config(yscrollcommand=scroll.set)
		self.list = list

		button = Button(self, text="Next", command=self.next)
		button.pack(side=TOP)
		#button = Button(self, text="Save")
		#button.pack(side=TOP)
		button = Button(self, text="Quit", command=self.quit)
		button.pack(side=TOP)

		list.bind("<Double-Button-1>", self.updateView)

		self.view = None

	def viewInitialize(self):
		if self.view != None:
			return
		self.view = View(self)

	def updateView(self, event):
		self.viewInitialize()
		self.view.update()

	def next(self):
		sel = self.list.curselection()
		i = -1
		if sel != ():
			i = long(self.list.curselection()[0])
		if i != -1:
			self.list.selection_clear(i)
		if i + 1 < self.list.size():
			self.list.selection_set(i + 1)
			self.viewInitialize()
			self.view.update()

	def update(self):
		self.content = Root()
		self.content.loadAll()

		self.list.delete(0, END)
		menus = self.content.nodes.childname.keys()
		menus.sort()
		for name in menus:
			self.list.insert(END, name)

if __name__ == "__main__":
	application = Application(None)
	application.mainloop()
