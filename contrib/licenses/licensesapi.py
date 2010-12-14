#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import sys
from optparse import OptionParser, OptionGroup

class LicenseEntry(object):
    def __init__(self, filename, author = None, license = None, source = None):
        self._filename = filename
        self.author = author
        self.license = license
        self.source = source

    def __str__(self):
        return self.filename

    def get_filename(self):
        return self._filename

    def copy(self, new_filename):
        return LicenseEntry(new_filename, self.author, self.license, self.source)

    # avoid editing filename
    filename = property(get_filename)

class LicenseSet(object):
    METADATA_SEPARATOR = " | "
    ENTRY_SEPARATOR = "\n"

    def __init__(self, filename):
        self.entries = {}
        self.load_from_file(filename)
        pass

    def save_to_file(self, filename):
        file = open(filename, "wt")

        header = ["filename", "license", "author", "source"]
        file.write(self.METADATA_SEPARATOR.join(header))
        file.write(self.ENTRY_SEPARATOR)

        # sort output
        list = []
		for entry in self.get_entries():
            list.append((entry.filename, entry))
        list.sort()

		for entry in list:
            entry = entry[1]
		    element = []
            element.append(entry.filename)
            element.append(entry.license)
            element.append(entry.author)
            element.append(entry.source)

            # remove empty trail
            for i in xrange(0, len(element)):
                if element[i] != None and element[i].strip() == "":
                    element[i] = None
            while len(element) > 0 and element[len(element)-1] == None:
                element.pop(len(element)-1)

            # to string
            for i in xrange(0, len(element)):
                if element[i] == None:
                    element[i] = ""

            # TODO move it on function creating new entries
            for e in element:
                if self.METADATA_SEPARATOR in e:
                    message = "Entry %s contain a reserved metadata separator" % e.filename
                    raise Exception(message)

            message = self.METADATA_SEPARATOR.join(element)
            file.write(message.encode("utf-8"))
            file.write(self.ENTRY_SEPARATOR)

        file.close()

    def load_from_file(self, filename):
        file = open(filename, "rt")
        content = file.read()
        content = unicode(content, "utf-8")
        file.close()
        entries = content.split(self.ENTRY_SEPARATOR)

        # read header
        header = entries.pop(0)
        header = header.split(self.METADATA_SEPARATOR)
        filenameId = header.index('filename')
        authorId = header.index('author')
        licenseId = header.index('license')
        sourceId = header.index('source')

        # read content
        for entry in entries:
            # skip empty entries
            if entry.strip() == "":
                continue

            data = entry.split(self.METADATA_SEPARATOR)
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
        if not filename in self.entries:
            return None
        return self.entries[filename]

    def get_entries(self):
        return self.entries.values()

    def exists_entry(self, entry_name):
        return entry_name in self.entries

    def remove_entry(self, entry_name):
        entry_name = str(entry_name)
        if not self.exists_entry(entry_name):
            message = "Entry %s not found" % entry_name
            raise Exception(message)
        del self.entries[entry_name]

    def move_entry(self, entry, new_filename):
        if not self.exists_entry(entry.filename):
            message = "Entry %s not found" % entry.filename
            raise Exception(message)
        if self.exists_entry(new_filename):
            message = "Entry %s already exists" % new_filename
            raise Exception(message)
        new_entry = entry.copy(new_filename)
        self.remove_entry(entry)
        self.add_entry(new_entry)

    def add_entry(self, entry):
        if self.exists_entry(entry.filename):
            message = "Entry %s already exists" % entry.filename
            raise Exception(message)
        self.entries[entry.filename] = entry
