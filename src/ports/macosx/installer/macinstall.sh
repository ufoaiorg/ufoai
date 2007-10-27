#!/bin/sh

ROOT_DIR="../../../"
INST_DIR="./"

echo "Deleting old app bundle (if any...)"
rm -rf $(INST_DIR)UFOAI.app

echo "Creating app bundle..."
mkdir -p $(INST_DIR)UFOAI.app/Contents/MacOS
mkdir -p $(INST_DIR)UFOAI.app/Contents/Frameworks
mkdir -p $(INST_DIR)UFOAI.app/Contents/Libraries

echo "Copying content to it..."
cp ufo $(INST_DIR)UFOAI.app/Contents/MacOS

mkdir -p $(INST_DIR)UFOAI.app/base
cp -v $(ROOT_DIR)base/*.pk3 $(INST_DIR)UFOAI.app/base/
cp -r $(ROOT_DIR)base/i18n $(INST_DIR)UFOAI.app/base/i18n
cp -v $(ROOT_DIR)base/game.dylib $(INST_DIR)UFOAI.app/base/
cp -v $(ROOT_DIR)base/*.cfg $(INST_DIR)UFOAI.app/base/

echo "Fixing libraries..."

perl macfixlibs.pl

echo "Creating dmg..."
IMGNAME='ufoai-mac-distrib'
rm -rf $IMGNAME
rm -f $IMGNAME.dmg
mkdir $IMGNAME
mv UFOAI.app $IMGNAME
cp README $IMGNAME
cp CONTRIBUTORS $IMGNAME
cp COPYING $IMGNAME
hdiutil create -srcfolder $IMGNAME $IMGNAME.dmg

echo "done!"
