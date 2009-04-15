rem NOTE: This batch file requires you to have the 7-zip command line utility, 7za.exe, in your path.
rem You can get this utility here: http://www.7-zip.org/download.html
rem -r recurse subdirectories. for example, this ensures that 0models.pk3 will contain stuff from "models\aliens" as well as "models".

7za u -tzip 0base.pk3 *.cfg irc_motd.txt -x!".svn"
7za u -r -tzip 0maps.pk3 materials\*.mat maps\*.bsp maps\*.ump -x!".svn"
7za u -r -tzip 0media.pk3 shaders\*.fp shaders\*.vp media\*.ttf -x!".svn"
7za u -r -tzip 0models.pk3 models\*.md2 models\*.md3 models\*.jpg models\*.png models\*.tga models\*.anm models\*.txt models\*.tag -x!".svn"
7za u -r -tzip 0music.pk3 music\*.ogg -x!".svn"
7za u -r -tzip 0pics.pk3 pics\*.jpg pics\*.tga pics\*.png textures\*.jpg textures\*.tga textures\*.png -x!".svn"
7za u -r -tzip 0snd.pk3 sound\*.ogg sound\*.wav -x!".svn"
7za u -r -tzip 0ufos.pk3 ufos\*.ufo -x!".svn"
7za u -r -tzip 0vids.pk3 videos\*.roq -x!".svn"
