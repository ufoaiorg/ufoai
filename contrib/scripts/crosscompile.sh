#!/bin/sh


JOBS=1
MINGW=i686-pc-mingw32
MINGW_CROSS_ENV=/opt/mingw-cross-env
DIR=$(dirname $(readlink -f ${0}))
cd "${DIR}/../.."

# http://www.nongnu.org/mingw-cross-env/
export PATH=${MINGW_CROSS_ENV}/usr/bin:${MINGW_CROSS_ENV}/usr/${MINGW}/bin/:${PATH}

./configure --host=${MINGW} --disable-uforadiant

make -j ${JOBS}

