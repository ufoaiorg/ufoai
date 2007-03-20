#/bin/bash

echo "generating archives"
echo
echo "pics and textures..."
(find pics -type f -print; find textures -type f -print) | egrep -v '(\/\.svn\/)' | awk '/\.jpg$/||/\.tga$/||/\.pcx$/||/\.bmp$/||/\.png/ {print}' | xargs zip -u9@ 0pics.zip
#zip -ru9 0pics.zip pics textures -i \*.jpg \*.tga \*.png \*.bmp \*.pcx
echo "models..."
find models -type f -print | egrep -v '(\/\.svn\/)' | egrep -v '(\/misc_source_files\/)' | awk '/\.md2$/||/\.jpg$/||/\.png$/||/\.tga$/||/\.anm$/||/\.txt$/||/\.tag/ {print}' | xargs zip -u9@ 0models.zip
#zip -ru9 0models.zip models -i \*.md2 \*.jpg \*.tga \*.png \*.txt \*.anm \*.tag
echo "sound and music..."
(find music -type f -print; find sound -type f -print) | egrep -v '(\/\.svn\/)' | awk '/\.ogg$/||/\.txt$/||/\.wav/ {print}' | xargs zip -u9@ 0snd.zip
#zip -ru9 0snd.zip music sound -i \*.txt \*.ogg \*.wav
echo "maps..."
find maps -type f -print | egrep -v '(\/\.svn\/)' | awk '/\.bsp$/||/\.ump/ {print}' | xargs zip -u9@ 0maps.zip
#zip -ru9 0maps.zip maps -i \*.bsp
echo "media..."
(find media -type f -print; find shaders -type f -print) | egrep -v '(\/\.svn\/)' | awk '/\.fp$/||/\.vp$/||/\.ttf/ {print}' | xargs zip -u9@ 0media.zip
#zip -ru9 0media.zip media shaders -i \*.fp \*.vp \*.ttf COPYING
echo "i18n..."
echo "don't include this in the zipfiles - gettext doesn't have support for it"
echo "ufos..."
find ufos -type f -print | egrep -v '(\/\.svn\/)' | awk '/\.ufo/ {print}' | xargs zip -u9@ 0ufos.zip
#zip -ru9 0ufos.zip ufos -i \*.ufo
echo "base..."
#don't add the user settings
rm -f config.cfg
zip -u9 0base.zip *.cfg
echo "...finished"
