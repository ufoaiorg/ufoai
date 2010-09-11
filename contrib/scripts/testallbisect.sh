!/bin/sh

./configure --disable-dedicated --disable-client --disable-ufomodel --disable-ufo2map --disable-uforadiant
make clean
make all
if [ ! -f ./testall ]; then
	exit 125
fi

./testall
# is it need?
exit $?
