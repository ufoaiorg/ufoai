#!/bin/bash

set -e
set -x

PREFIX=debian
RELEASE=squeeze
ARCH=i386
DEBMIRROR=http://127.0.0.1:3142/ftp.debian.de/debian

if [ $# -gt 0 ]; then
	ARCH=$1
fi

CHROOT=~/${PREFIX}-${RELEASE}-${ARCH}
CHROOTBIN="chroot"
DEBOOTSTRAPBIN="debootstrap"
CHROOTHOMEPATH="/home/ufo"
CHROOTUFOPATH="${CHROOTHOMEPATH}/ufoai"
UFOGITCHECKOUTROOT="/home/mattn/dev"

createChroot() {
	rm -rf "${CHROOT}"
	mkdir -p "${CHROOT}"

	${DEBOOTSTRAPBIN} --arch ${ARCH} ${RELEASE} "${CHROOT}" ${DEBMIRROR}
	cp /etc/resolv.conf "${CHROOT}"/etc
	#fakeroot mount -t proc proc ${CHROOT}/proc
	${CHROOTBIN} "${CHROOT}" apt-get clean
	${CHROOTBIN} "${CHROOT}" apt-get install -y --force-yes libsdl1.2-dev
	${CHROOTBIN} "${CHROOT}" apt-get clean
	${CHROOTBIN} "${CHROOT}" apt-get install -y --force-yes git-core debhelper libpng-dev libjpeg-dev libgl1-mesa-dev libsdl-ttf2.0-dev libsdl-mixer1.2-dev
	${CHROOTBIN} "${CHROOT}" apt-get clean
	${CHROOTBIN} "${CHROOT}" apt-get install -y --force-yes libcurl4-gnutls-dev libgtk2.0-dev zip libxml2-dev
	${CHROOTBIN} "${CHROOT}" apt-get clean
	${CHROOTBIN} "${CHROOT}" apt-get install -y --force-yes libgtkglext1-dev libtheora-dev dpatch libglib2.0-dev zlib1g-dev gettext libmxml-dev pkg-config
	${CHROOTBIN} "${CHROOT}" apt-get clean
	${CHROOTBIN} "${CHROOT}" apt-get install -y --force-yes libopenal-dev libgtksourceview2.0-dev libsdl-image1.2-dev python2.6 binutils-dev
	${CHROOTBIN} "${CHROOT}" apt-get clean
	${CHROOTBIN} "${CHROOT}" apt-get install -y --force-yes devscripts
	${CHROOTBIN} "${CHROOT}" apt-get clean
	${CHROOTBIN} "${CHROOT}" mkdir -p /home/ufo/dev
#	${CHROOTBIN} "${CHROOT}" wget https://launchpad.net/ubuntu/+archive/primary/+files/libxvidcore4_1.3.2-3_${ARCH}.deb
#	${CHROOTBIN} "${CHROOT}" wget https://launchpad.net/ubuntu/+archive/primary/+files/libxvidcore-dev_1.3.2-3_${ARCH}.deb
#	${CHROOTBIN} "${CHROOT}" dpkg -i libxvidcore4_1.3.2-3_${ARCH}.deb libxvidcore-dev_1.3.2-3_${ARCH}.deb
#	${CHROOTBIN} "${CHROOT}" rm -f libxvid*.deb
	echo "cd ${CHROOTUFOPATH}; ./configure --enable-release; make -j 2; make deb" > ${CHROOT}/usr/bin/compileufo.sh
	chmod +x ${CHROOT}/usr/bin/compileufo.sh
	${CHROOTBIN} ${CHROOT} mkdir -p ${CHROOTHOMEPATH}
	mount --bind ${UFOGITCHECKOUTROOT} ${CHROOT}${CHROOTHOMEPATH}
	${CHROOTBIN} ${CHROOT} compileufo.sh
}


createChroot
