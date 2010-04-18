#!/bin/sh

set -x

ROOTDIR=../../../

VERSION_UC=`grep UFO_VERSION ${ROOTDIR}src/common/common.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/' | sed -e 's/\"//g'`
VERSION_LC=`echo ${VERSION_UC} | sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;'`

DESTDIR="image"

rm -rf ${DESTDIR}
mkdir ${DESTDIR}
mkdir ${DESTDIR}/guis
mkdir ${DESTDIR}/scripts
mkdir ${DESTDIR}/data
mkdir ${DESTDIR}/data/base
mkdir ${DESTDIR}/data/base/i18n

for mo in `ls ${ROOTDIR}src/po/*.mo | grep -v "update.*\.mo"`; do
	mo=`basename ${mo}`
	lang=`echo $mo | sed -e 's/\.mo$//'`
	mkdir ${DESTDIR}/data/base/i18n/${lang}
	mkdir ${DESTDIR}/data/base/i18n/${lang}/LC_MESSAGES
	cp ${ROOTDIR}src/po/${po} ${DESTDIR}/data/base/i18n/${lang}/LC_MESSAGES/${mo}
done

for pk3 in `ls ${ROOTDIR}base/*.pk3`; do
	pk3=`basename ${pk3}`
	cp ${ROOTDIR}/base/${pk3} ${DESTDIR}/data/base/${pk3}
done

cp ${ROOTDIR}/ufo ${DESTDIR}/data/ufo
cp ${ROOTDIR}/ufoded ${DESTDIR}/data/ufoded
cp ${ROOTDIR}/ufo2map ${DESTDIR}/data/ufo2map
cp ${ROOTDIR}/base/game.so ${DESTDIR}/data/base/game.so

cp ${ROOTDIR}/ufo.x86_64 ${DESTDIR}/data/ufo.x86_64
cp ${ROOTDIR}/ufoded.x86_64 ${DESTDIR}/data/ufoded.x86_64
cp ${ROOTDIR}/ufo2map.x86_64 ${DESTDIR}/data/ufo2map.x86_64
cp ${ROOTDIR}/base/game_x86_64.so ${DESTDIR}/data/base/game_x86_64.so

cp ${ROOTDIR}/COPYING ${DESTDIR}/data

# this is a little nasty, but it works!
TOTAL_INSTALL_SIZE=`du -sb ${DESTDIR}/data | tail -1 | cut -f1`
sed s/@TOTAL_INSTALL_SIZE@/${TOTAL_INSTALL_SIZE}/g scripts/config.lua.in > scripts/config.lua

for lib in guis/*.so; do
	cp $lib ${DESTDIR}/guis
done

for lua in scripts/*.lua; do
    ./mojoluac $LUASTRIPOPT -o ${DESTDIR}/${lua}c $lua
done

# Make a .zip archive of the Base Archive dirs and nuke the originals...
cd image
zip -9r ../pdata.zip *
cd ..
rm -rf image

# Append the .zip archive to the mojosetup binary, so it's "self-extracting."
cat mojosetup pdata.zip >> ufoai-${VERSION_LC}-linux.run
chmod +x ufoai-${VERSION_LC}-linux.run
rm -f pdata.zip

rm -f scripts/config.lua

exit 0
