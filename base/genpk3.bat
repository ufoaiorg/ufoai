rem NOTE: This batch file requires you to have the 7-zip command line utility in your path.
rem You can get this utility here: http://www.7-zip.org/download.html

7za a -tzip 0base.pk3 *.cfg -x!".svn"
7za a -r -tzip 0maps.pk3 materials maps\*.bsp maps\*.ump -x!".svn"
7za a -r -tzip 0media.pk3 shaders media -x!".svn"
7za a -r -tzip 0models.pk3 models -x!".svn"
7za a -r -tzip 0music.pk3 music -x!".svn"
7za a -r -tzip 0pics.pk3 pics textures -x!".svn"
7za a -r -tzip 0snd.pk3 sound -x!".svn"
7za a -r -tzip 0ufos.pk3 ufos -x!".svn"
7za a -r -tzip 0vids.pk3 videos -x!".svn"
