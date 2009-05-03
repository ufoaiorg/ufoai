#!/bin/bash

stop_on_error=1
init=1
dont_install=0
sed_soft="sed"
user=`whoami`

if [ "$user" != "root" ]
then
	echo "You must be root to run this script"
	exit 1
fi

download()
{
	if [ -e $2 ]
	then
		return 0
	else
		wget $1$2
		return $?
	fi
}

mkdir mingw_tmp 2> /dev/null

if [ -d "./mingw_tmp" ]; then
	echo "Start"
else
	echo "Could not create directory mingw_tmp"
	exit 1;
fi

cd mingw_tmp

if [ "$init" -eq "1" ]
then
	# a mirror is also available at mattn.ninex.info
	# e.g. wget http://mattn.ninex.info/any2deb/any2deb_1.0-2_all.deb
	aptitude install fakeroot alien wget
	download http://www.profv.de/any2deb/ any2deb_1.2-2_all.deb
	dpkg -i any2deb_1.2-2_all.deb

	aptitude install mingw32
	#aptitude install upx-ucl
	aptitude install nsis
fi

echo "========================================"
echo " ZLIB"
echo "========================================"
version="1.2.3"
download http://heanet.dl.sourceforge.net/sourceforge/gnuwin32/ zlib-$version-lib.zip
any2deb mingw32-zlib1-dev $version zlib-$version-lib.zip /usr/i586-mingw32msvc >> /dev/null
if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-zlib1-dev_$version-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (zlib)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

echo "========================================"
echo " LIBPNG"
echo "========================================"
version="1.2.24"
download http://heanet.dl.sourceforge.net/sourceforge/gnuwin32/ libpng-$version-lib.zip
any2deb mingw32-libpng12-dev $version libpng-$version-lib.zip /usr/i586-mingw32msvc >> /dev/null
if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-libpng12-dev_$version-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (libpng)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

echo "========================================"
echo " LIBJPEG"
echo "========================================"
#beware the -4
#needed for dpkg version string - otherwise the package name would be wrong
version="6b"
download http://heanet.dl.sourceforge.net/sourceforge/gnuwin32/ jpeg-$version-4-lib.zip
any2deb mingw32-libjpeg-dev $version jpeg-$version-4-lib.zip /usr/i586-mingw32msvc >> /dev/null
if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-libjpeg-dev_6b-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (libjpeg)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

echo "========================================"
echo " LIBICONV"
echo "========================================"
version="1.9.2"
#beware the -1
#needed for dpkg version string - otherwise the package name would be wrong
download http://heanet.dl.sourceforge.net/sourceforge/gnuwin32/ libiconv-$version-1-lib.zip
any2deb mingw32-libiconv-dev $version libiconv-$version-1-lib.zip /usr/i586-mingw32msvc >> /dev/null
if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-libiconv-dev_$version-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (libiconv)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

echo "========================================"
echo " LIBINTL"
echo "========================================"
version="0.14.4"
download http://heanet.dl.sourceforge.net/sourceforge/gnuwin32/ libintl-$version-lib.zip
any2deb mingw32-libintl-dev $version libintl-$version-lib.zip /usr/i586-mingw32msvc >> /dev/null
if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-libintl-dev_$version-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (libintl)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

echo "========================================"
echo " FREETYPE"
echo "========================================"
version="2.3.5"
download http://heanet.dl.sourceforge.net/sourceforge/gnuwin32/ freetype-$version-lib.zip
any2deb mingw32-freetype-dev $version freetype-$version-lib.zip /usr/i586-mingw32msvc >> /dev/null
if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-freetype-dev_$version-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (freetype)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

echo "========================================"
echo " SDL"
echo "========================================"
version="1.2.13"
download http://www.libsdl.org/release/ SDL-devel-$version-mingw32.tar.gz
tar -xz -f SDL-devel-$version-mingw32.tar.gz
mkdir -p build-sdl/usr/i586-mingw32msvc

dir=`pwd`/build-sdl/usr/i586-mingw32msvc/
cd SDL-$version
cp -rv bin include lib share $dir/
sed "s|^prefix=.*|prefix=/usr/i586-mingw32msvc|" < bin/sdl-config > $dir/bin/sdl-config;
chmod 755 $dir/bin/sdl-config;
sed "s|^libdir=.*|libdir=\'/usr/i586-mingw32msvc/lib\'|" <lib/libSDL.la > $dir/lib/libSDL.la; \
cd ..

tar -cz --owner=root --group=root -C build-sdl -f mingw32-libsdl1.2-dev-$version.tgz .
fakeroot alien mingw32-libsdl1.2-dev-$version.tgz >> /dev/null

if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-libsdl1.2-dev_$version-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (SDL)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

echo "========================================"
echo " SDLTTF"
echo "========================================"
version="2.0.9"
#beware the -1
#needed for dpkg version string - otherwise the package name would be wrong
download http://cefiro.homelinux.org/resources/files/SDL_ttf/ SDL_ttf-$version-1-i386-mingw32.tar.gz
any2deb mingw32-libsdl-ttf2.0-dev $version SDL_ttf-$version-1-i386-mingw32.tar.gz /usr/i586-mingw32msvc >> /dev/null
if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-libsdl-ttf2.0-dev_$version-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (libsdl-ttf)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

echo "========================================"
echo " SDLMIXER"
echo "========================================"
version="1.2.8"
#beware the -1
#needed for dpkg version string - otherwise the package name would be wrong
download http://cefiro.homelinux.org/resources/files/SDL_mixer/ SDL_mixer-$version-1-i386-mingw32.tar.gz
any2deb mingw32-libsdl-mixer-dev $version SDL_mixer-$version-1-i386-mingw32.tar.gz /usr/i586-mingw32msvc >> /dev/null
if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-libsdl-mixer-dev_$version-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (libsdl-mixer)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

echo "========================================"
echo " LIBCURL"
echo "========================================"
version="7.17.1"
download http://curl.haxx.se/download/ libcurl-$version-win32-nossl.zip
unzip libcurl-$version-win32-nossl.zip
cd libcurl-$version
tar -czf ../libcurl-$version-win32-nossl.tar.gz .
cd ..
any2deb mingw32-libcurl-dev $version libcurl-$version-win32-nossl.tar.gz /usr/i586-mingw32msvc >> /dev/null
if [ "$dont_install" -ne "1" ]
then
	dpkg -i mingw32-libcurl-dev_$version-2_all.deb
	if [ $? -ne 0 ]
	then
		echo "Fatal error with dpkg (libcurl)"
		if [ "$stop_on_error" -eq "1" ]
		then
			exit 1
		fi
	fi
fi

if [ "$dont_install" -eq "1" ]
then
	echo "NOTE: No packages were installed - deactivate dont_install if you want to install them automatically"
fi
