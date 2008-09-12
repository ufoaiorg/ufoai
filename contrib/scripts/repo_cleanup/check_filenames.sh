#!/bin/bash

CHECKDIR=${1:-../../..}

# this will also find stuff like something.README; our longest extension seems to be "blend", so we could limit
#  [:upper:] to 1-5 characters... if somebody could explain how to make classes work in find -regex

# searching for filenames with uppercase in extension
find $CHECKDIR ! -wholename '*/.svn*' -type f -name '*.*[[:upper:]]*'
# searching for files & directories with spaces in the name
find $CHECKDIR ! -wholename '*/.svn*' -wholename '*.*[[:space:]]*'
