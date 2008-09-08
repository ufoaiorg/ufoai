#!/bin/bash

RELATIVE_TRUNKDIR=../../..

./fix_eol.sh $RELATIVE_TRUNKDIR
./fix_eol.sh $RELATIVE_TRUNKDIR/../data_source

./fix_mimetypes.sh $RELATIVE_TRUNKDIR
./fix_mimetypes.sh $RELATIVE_TRUNKDIR/../data_source

./set_map_ignores.sh

./list_missing_textures.sh ../../../base/maps
./list_missing_textures.sh ../../../../data_source/maps

./check_texture_names.sh
