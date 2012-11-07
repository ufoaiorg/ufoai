#!/bin/sh
[ -z "$1" ] && { echo "Usage: $0 apk-file-to-sign.apk"; exit 1; }
zip -d "$1" "META-INF/*"
jarsigner -verbose -keystore ufoai-team.keystore -storepass AliensWillEatYou -sigalg MD5withRSA -digestalg SHA1 "$1" ufoai-team
zipalign 4 "$1" "$1-aligned"
mv -f "$1-aligned" "$1"
