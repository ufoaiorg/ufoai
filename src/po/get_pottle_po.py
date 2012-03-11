#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import os
import urllib2
import socket
import platform
import sys
import time
import re

VERSION = "1"
RETRIES = 10
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../..')
POTTLE_BASE = "/pootle"
POTTLE_URL = "http://ufoai.org" + POTTLE_BASE


displayDownloadStatus = True

ignore_languages = set(['en', 'en_BASE'])

# code name from pottle to ufoai file
# TODO: remove it when it is possible
language_mapping = {
    "bg":   "bg_BG",
    "nb":   "no",
    "nl":   "nl_NL",
}

#read the given path

def download(uri):
    global displayDownloadStatus
    request = urllib2.Request(uri)

    p = ' '.join([platform.platform()] + list(platform.dist()))
    request.add_header('User-Agent', 'ufoai_get_po/%s (%s)' % (VERSION, p))

    trynum = 0
    while trynum < RETRIES:
        try:
            f = urllib2.build_opener().open(request)

            re = out = ''
            t = 1
            data = f.read(1024)
            if not displayDownloadStatus:
                print('Downloading ' + uri)
            while data:
                re+= data
                if displayDownloadStatus and sys.stdout.isatty:
                    out = '\r%s %9i KiB' % (uri, len(re) / 1024)
                    sys.stdout.write(out)
                    sys.stdout.flush()
                t = time.time()
                data = f.read(1024)
                t = time.time() - t
            f.close()
            if displayDownloadStatus:
                sys.stdout.write('\r%s\r' % (' '*len(out)))
            break
        except socket.timeout:
            print '...timeout fetching URL'
            trynum += 1
            if trynum >= RETRIES:
                raise
    return re

def find_available_languages():
    data = download(POTTLE_URL + "/projects/ufoai/")
    languages = re.findall("%s/([a-zA-Z_]*)/ufoai/" % POTTLE_BASE, data)
    languages = set(languages)
    languages.remove('projects')
    return languages

def get_language(lang):
    global language_mapping
    url = "%s/%s/ufoai/ufoai.po/download/" % (POTTLE_URL, lang)

    file_lang = lang
    if lang in language_mapping:
        file_lang = language_mapping[lang]
    file_name = "%s/src/po/ufoai-%s.po" % (UFOAI_ROOT, file_lang)

    print 'Download "%s": %s' % (lang, url)
    data = download(url)

    print 'Write "%s": %s' % (lang, file_name)
    file = open(file_name, "wt")
    file.write(data)
    file.close()

def get_all_languages():
    global ignore_languages
    languages = find_available_languages()
    print "Available languages: " + ', '.join(languages)
    for lang in languages:
        if lang in ignore_languages:
            print 'Skip "%s"' % (lang)
            continue
        get_language(lang)

get_all_languages()
