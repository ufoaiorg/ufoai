#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import sys, os, re
import subprocess
from licensesapi import *
from optparse import OptionParser, OptionGroup

def error(message):
    """
        Display an error and exit with an error status
    """
    print >> sys.stderr, "Error: " + message
    exit(-1)

def get_licenses_set(licenses):
    """
        Return the set of used licenses
    """
    entries = licenses.get_entries()
    l = set([])
    for e in entries:
        if e.license == None:
            continue
        l.add(e.license)
    return l

def check_filename(filename_requested):
    """
        Check if a filename exists, else exit with an error message
    """
    if not os.path.exists(filename_requested):
        error("The filename \"%s\" do not exists in the project. Action rejected.\n    (use \"--force\" to force the use of this filename)" % filename_requested)

def check_license(licenses, license_requested):
    """
        Check if a license is already used in the project, else exit with an error message
    """
    used = get_licenses_set(licenses)
    if not (license_requested in used):
        error("The license \"%s\" is not yet used in the project. Action rejected.\n    (use \"--force\" to force the use of this new license)\n    (use \"stats\" command to have a list of current licenses)" % license_requested)

def execute(cmd):
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    data = p.stdout.read()
    return data

def command_add(new_entry_name, options):
    licenses = LicenseSet(options.datafile)
    if not options.force:
        check_filename(new_entry_name)

    if options.self:
        if options.license == None:
            l = execute("git config user.licensemedia")
            l = l.strip()
            if l == "":
                error("Default media license is not defined.\n    (use \"git config --add user.licensemedia YOURLICENSENAME\" to define it)")
            else:
                options.license = l
        if options.author == None:
            a = execute("git config user.name")
            a = a.strip()
            if l == "":
                error("Default username is not defined.\n    (use \"git config --add user.name YOURNAME\" to define it)")
            else:
                options.author = a

    if options.license != None and not options.force:
        check_license(licenses, options.license)
    if options.force and licenses.exists_entry(new_entry_name):
        licenses.remove_entry(new_entry_name)
    e = LicenseEntry(new_entry_name, author=options.author, license=options.license, source=options.source)
    licenses.add_entry(e)
    if not options.dryrun:
        licenses.save_to_file(options.datafile)

def command_remove(entry_name, options):
    licenses = LicenseSet(options.datafile)
    licenses.remove_entry(entry_name)
    if not options.dryrun:
        licenses.save_to_file(options.datafile)

def command_edit(entry_name, options):
    licenses = LicenseSet(options.datafile)
    e = licenses.get_entry(entry_name)
    if e == None:
        error("Entry %s not found" % entry_name)
    if options.license != None and not options.force:
        check_license(licenses, options.license)
    if options.author != None:
        e.author = options.author
    if options.license != None:
        e.license = options.license
    if options.source != None:
        e.source = options.source
    if not options.dryrun:
        licenses.save_to_file(options.datafile)

def command_copy(entry_name, new_entry_name, options):
    licenses = LicenseSet(options.datafile)
    if options.force and licenses.exists_entry(new_entry_name):
        licenses.remove_entry(new_entry_name)
    e = licenses.get_entry(entry_name)
    e = e.copy(new_entry_name)
    licenses.add_entry(e)
    if not options.dryrun:
        licenses.save_to_file(options.datafile)

def command_move(entry_name, new_entry_name, options):
    licenses = LicenseSet(options.datafile)
    if not options.force:
        check_filename(new_entry_name)
    if options.force and licenses.exists_entry(new_entry_name):
        licenses.remove_entry(new_entry_name)
    e = licenses.get_entry(entry_name)
    licenses.move_entry(e, new_entry_name)
    if not options.dryrun:
        licenses.save_to_file(options.datafile)

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
                      help="Define the license of an entry (it override --self)")
    group.add_option("-a", "--author", dest="author", default=None,
                      help="Define the author of an entry (it override --self)")
    group.add_option("-s", "--source", dest="source", default=None,
                      help="Define the source of an entry")
    group.add_option("", "--self", dest="self", action="store_true", default=False,
                      help="Define yourself as author and use your default license name. We only can use it for add command. git config user.name and user.licensemedia must be set")
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
    list ENTRYNAME_REGEX
                        List entry name (use metadata options to
                        filter the list)
"""
        exit(0)

    if args[0] == "add":
        if len(args) != 2:
            error("2 arguments expected but \"%s\" found" % str(args))
        new_entry_name = args[1]
        command_add(new_entry_name, options)

    elif args[0] == "remove":
        if len(args) != 2:
            error("2 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        command_remove(entry_name, options)

    elif args[0] == "edit":
        if len(args) != 2:
            error("2 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        command_edit(entry_name, options)

    elif args[0] == "move":
        if len(args) != 3:
            error("3 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        new_entry_name = args[2]
        command_move(entry_name, new_entry_name, options)

    elif args[0] == "copy":
        if len(args) != 3:
            error("3 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        new_entry_name = args[2]
        command_copy(entry_name, new_entry_name, options)

    elif args[0] == "show":
        if len(args) != 2:
            error("2 arguments expected but \"%s\" found" % str(args))
        entry_name = args[1]
        licenses = LicenseSet(options.datafile)
        e = licenses.get_entry(entry_name)
        if e == None:
            error("Entry %s not found" % entry_name)
        print "license: " + str(e.license)
        print "author: " + str(e.author)
        print "source: " + str(e.source)

    elif args[0] == "stats":
        if len(args) != 1:
            error("1 arguments expected but \"%s\" found" % str(args))
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
            error("1 arguments expected but \"%s\" found" % str(args))
        licenses = LicenseSet(options.datafile)
        entries = licenses.get_entries()
        for e in entries:
            if not os.path.exists(e.filename):
                print "Warning: File \"%s\" do not exists" % e.filename

    elif args[0] == "list":
        if len(args) != 2:
            error("2 arguments expected but \"%s\" found" % str(args))
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
        error("Command for args %s unknown" % str(args))
