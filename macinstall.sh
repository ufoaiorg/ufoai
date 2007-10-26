#!/bin/sh

echo "Deleting old app bundle (if any...)"
rm -rf UFO.app

echo "Creating app bundle..."
mkdir -p UFO.app/Contents/MacOS
mkdir -p UFO.app/Contents/Frameworks
mkdir -p UFO.app/Contents/Libraries

echo "Copying content to it..."
cp ufo UFO.app/Contents/MacOS

mkdir UFO.app/base
cp -v base/*.pk3 UFO.app/base/
cp -r base/i18n UFO.app/base/i18n
cp -v base/game.dylib UFO.app/base/
cp -v base/*.cfg UFO.app/base/

echo "Fixing libraries..."

perl macfixlibs.pl

echo "Creating dmg..."
IMGNAME='ufo-mac-distrib'
rm -rf $IMGNAME
rm -f $IMGNAME.dmg
mkdir $IMGNAME
mv UFO.app $IMGNAME
cp README $IMGNAME
cp CONTRIBUTORS $IMGNAME
cp COPYING $IMGNAME
hdiutil create -srcfolder $IMGNAME $IMGNAME.dmg

echo "done!"
