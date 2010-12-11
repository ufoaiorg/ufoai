#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import sys, os, re
from licensesapi import *
from optparse import OptionParser, OptionGroup

if __name__ == "__main__":

    parser = OptionParser(usage="usage: %prog [OPTIONS] COMMAND [ARGS]", add_help_option=False)
    parser.add_option("-h", "--help", dest="help", action="store_true", default=False,
                      help="Show this help message and exit")
    parser.add_option("-n", "--dry-run", dest="dryrun", action="store_true", default=False,
                      help="Do everything except saving the result")
    parser.add_option("", "--datafile", dest="datafile", default="LICENSES", metavar="FILE",
                      help="Define the file containing licenses information [default: %default]")

    group = OptionGroup(parser, "Metadata edition")
    group.add_option("-l", "--license", dest="license", default=None,
                      help="Define the license of an entry")
    group.add_option("-a", "--author", dest="author", default=None,
                      help="Define the author of an entry")
    group.add_option("-s", "--source", dest="source", default=None,
                      help="Define the source of an entry")
    group.add_option("-f", "--force", dest="force", action="store_true", default=False,
                      help="Force move, add or copy when target already exists")
    parser.add_option_group(group)

    (options, args) = parser.parse_args()

    if len(args) < 1 or options.help:
        parser.print_help()
        print """
Commands:
    add ENTRYNAME       Add an entry (use metadata edition options)
    remove ENTRYNAME    Remove an entry
    edit ENTRYNAME      Edit an entry  (use metadata edition options)
    move ENTRYNAME NEWENTRYNAME
                        Move an entry to another location
    copy ENTRYNAME NEWENTRYNAME
                        Copy an entry to another location
    show ENTRYNAME      Show properties of an entry
    stats               Display license stats
    check               Check if all entry files exists
    show ENTRYNAME_REGEX
                        List entry name (use metadata options to
                        filter the list)
"""
        exit(0)

    if args[0] == "add":
        if len(args) != 2:
            raise Exception("2 arguments expected but \"%s\" found" % str(args))
        new_entry_name = args[1]
        licenses = LicenseSet(options.datafile)
        if options.force and licenses.exists_entry(new_entry_name):
            licenses.remove_entry(new_entry_name)
        e = LicenseEntry(new_entry_name, author=options.author, license=options.license, source=options.source)
        licenses.add_entry(e)
        if not options.dryrun:
            licenses.save_to_file(options.datafile)

    elif args[0] == "remove":
        if len(args) != 2:
            raise Exception("2 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        licenses = LicenseSet(options.datafile)
        licenses.remove_entry(entry_name)
        if not options.dryrun:
            licenses.save_to_file(options.datafile)

    elif args[0] == "edit":
        if len(args) != 2:
            raise Exception("2 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        licenses = LicenseSet(options.datafile)
        e = licenses.get_entry(entry_name)
        if e == None:
            raise Exception("Entry %s not found" % entry_name)
        if options.author != None:
            e.author = options.author
        if options.license != None:
            e.license = options.license
        if options.source != None:
            e.source = options.source
        if not options.dryrun:
            licenses.save_to_file(options.datafile)

    elif args[0] == "move":
        if len(args) != 3:
            raise Exception("3 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        new_entry_name = args[2]
        licenses = LicenseSet(options.datafile)
        if options.force and licenses.exists_entry(new_entry_name):
            licenses.remove_entry(new_entry_name)
        e = licenses.get_entry(entry_name)
        licenses.move_entry(e, new_entry_name)
        if not options.dryrun:
            licenses.save_to_file(options.datafile)

    elif args[0] == "copy":
        if len(args) != 3:
            raise Exception("3 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        new_entry_name = args[2]
        licenses = LicenseSet(options.datafile)
        if options.force and licenses.exists_entry(new_entry_name):
            licenses.remove_entry(new_entry_name)
        e = licenses.get_entry(entry_name)
        e = e.copy(new_entry_name)
        licenses.add_entry(e)
        if not options.dryrun:
            licenses.save_to_file(options.datafile)

    elif args[0] == "show":
        if len(args) != 2:
            raise Exception("2 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        licenses = LicenseSet(options.datafile)
        e = licenses.get_entry(entry_name)
        print "license: " + str(e.license)
        print "author: " + str(e.author)
        print "source: " + str(e.source)

    elif args[0] == "stats":
        if len(args) != 1:
            raise Exception("1 arguments expected but \"%s\" found" % str(args))
        licenses = LicenseSet(options.datafile)
        entries = licenses.get_entries()
        print "Global:"
        print "    Number of entries: % 4d" % len(entries)
        print
        print "Licenses:"
        stats = {}
        for e in entries:
            license = e.license
            if license == None:
                license = "Undefined"
            if not (license in stats):
                stats[license] = 0
            stats[license] += 1
        size = 0
        for s in stats:
            if len(s) > size:
                size = len(s)
        list = stats.keys()
        list.sort()
        for s in list:
            print "    %s:%s % 5d" % (s, " " * (size-len(s)), stats[s])

    elif args[0] == "check":
        if len(args) != 1:
            raise Exception("1 arguments expected but \"%s\" found" % str(args))
        licenses = LicenseSet(options.datafile)
        entries = licenses.get_entries()
        for e in entries:
            if not os.path.exists(e.filename):
                print "Warning: File \"%s\" do not exists" % e.filename

    elif args[0] == "list":
        if len(args) != 2:
            raise Exception("2 arguments expected but \"%s\" found" % str(args))
        check_name = re.compile(args[1])
        check_license = re.compile(".*")
        check_author = re.compile(".*")
        check_source = re.compile(".*")
        if options.author != None:
            if options.author == "":
                check_author = re.compile("$^")
            else:
                check_author = re.compile(options.author)
        if options.source != None:
            if options.source == "":
                check_source = re.compile("$^")
            else:
                check_source = re.compile(options.source)
        if options.license != None:
            if options.license == "":
                check_license = re.compile("$^")
            else:
                check_license = re.compile(options.license)
        licenses = LicenseSet(options.datafile)
        entries = licenses.get_entries()
        count = 0
        for e in entries:
            if not check_name.match(e.filename):
                continue
            license = e.license
            if license == None: license = ""
            author = e.author
            if author == None: author = ""
            source = e.source
            if source == None: source = ""
            if not check_name.match(e.filename):
                continue
            if not check_license.match(license):
                continue
            if not check_author.match(author):
                continue
            if not check_source.match(source):
                continue
            print "%s" % e.filename
            count += 1
        if count > 1:
            print "%d entries found." % count
        else:
            print "%d entry found." % count

    else:
        raise Exception("Command for args %s unknown" % str(args))
