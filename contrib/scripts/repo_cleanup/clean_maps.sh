#!/bin/bash

MAPDIR=${1:-../../../base/maps}

find "$MAPDIR" -type f ! -wholename '*/.svn*' \( -name '*.autosave.*' -o -name '*.bak' \) -exec rm {} \;
