!/bin/sh

make clean
make testall
if [ ! -f ./testall ]; then
	exit 125
fi

./testall
# is it need?
exit $?
