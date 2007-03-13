#/bin/bash

echo "generating archives"
echo
echo "pics and textures..."
zip -ru9 0pics.zip pics textures -i \*.jpg \*.tga \*.png \*.bmp \*.pcx
echo "models..."
zip -ru9 0models.zip models -i \*.md2 \*.jpg \*.tga \*.png \*.txt \*.anm \*.tag
echo "sound and music..."
zip -ru9 0snd.zip music sound -i \*.txt \*.ogg \*.wav
echo "maps..."
zip -ru9 0maps.zip maps -i \*.bsp
echo "media..."
zip -ru9 0media.zip media shaders -i \*.fp \*.vp \*.ttf COPYING
echo "i18n..."
echo "don't include this in the zipfiles - gettext doesn't have support for it"
echo "ufos..."
zip -ru9 0ufos.zip ufos -i \*.ufo
echo "base..."
#don't add the user settings
rm config.cfg
zip -u9 0base.zip *.cfg
