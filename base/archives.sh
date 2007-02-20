#/bin/bash

echo "generating archives"
echo
echo "pics and textures..."
zip -ruq9 0pics.zip pics textures -i \*.jpg \*.tga \*.png \*.bmp \*.pcx
echo "models..."
zip -ruq9 0models.zip models -i \*.md2 \*.jpg \*.tga \*.png \*.txt \*.anm \*.tag
echo "sound and music..."
zip -ruq9 0snd.zip music sound -i \*.txt \*.ogg \*.wav
echo "maps..."
zip -ruq9 0maps.zip maps -i \*.bsp
echo "media..."
zip -ruq9 0media.zip media shaders -i \*.fp \*.vp \*.ttf COPYING
echo "i18n..."
echo "don't include this in the zipfiles - gettext doesn't have support for it"
echo "ufos..."
zip -ruq9 0ufos.zip ufos -i \*.ufo
echo "base..."
zip -uq9 0base.zip *.cfg
