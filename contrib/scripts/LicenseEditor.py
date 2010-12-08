#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import sys
from Tkinter import *

class LicenseEntry(object):
    def __init__(self, filename, author = None, license = None, source = None):
        self.filename = filename
        self.author = author
        self.license = license
        self.source = source

    def __str__(self):
        return self.filename

class LicenseSet(object):
    def __init__(self, filename):
        self.entries = {}
        self.load_from_file(filename)
        pass

    def load_from_file(self, filename):
        file = open(filename, "rt")
        content = file.read()
        content = unicode(content, "utf-8")
        file.close()
        lines = content.splitlines()

        # read header
        header = lines.pop(0)
        header = header.split(' | ')
        filenameId = header.index('filename')
        authorId = header.index('author')
        licenseId = header.index('license')
        sourceId = header.index('source')

        # read content
        for line in lines:
            data = line.split(' | ')
            filename = data[filenameId].strip()

            resource = LicenseEntry(filename)
            try:
                v = data[authorId].strip()
                if v != "":
                    resource.author = v
            except IndexError:
                pass
            try:
                v = data[licenseId].strip()
                if v != "":
                    resource.license = v
            except IndexError:
                pass
            try:
                v = data[sourceId].strip()
                if v != "":
                    resource.source = v
            except IndexError:
                pass

            self.entries[resource.filename] = resource

    def get_entry(self, filename):
        return self.entries[filename]

    def get_entries(self):
        return self.entries.values()

class GUI(Tk):
	def __init__(self, parent):
		Tk.__init__(self, parent)
		self.title('UFO:AI licenses editor')
		self.parent = parent
		self.initialize()
		self.update()

	def initialize(self):
		frame = Frame(self)
		frame.pack(fill=BOTH, expand=1)

		scroll = Scrollbar(frame)
		scroll.pack(side=RIGHT, fill=Y)
		list = Listbox(frame)
		list.pack(fill=BOTH, expand=1)
		scroll.config(command=list.yview)
		list.config(yscrollcommand=scroll.set)
		self.list = list

    def set_license_data(self, data):
        self.data = data
        self.list.delete(0, END)
        list = []
		for entry in data.get_entries():
            list.append((entry.filename, entry))
        list.sort()
		for entry in list:
		    self.list.insert(END, entry)

if __name__ == "__main__":
	application = GUI(None)
    licenses = LicenseSet("../../LICENSES")
    application.set_license_data(licenses)
	application.mainloop()
