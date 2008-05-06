#!/bin/bash
# example script to build the maputil.
# you are probably better off using netbeans (the sun IDE)
# javac and jar are included in java sdk

fail() {
    echo -e "\n$1"
    exit 1
}

SVN=$(which svn 2> /dev/null)
[[ $SVN ]] || fail "no svn in path"

FIND=$(which find 2> /dev/null)
[[ $FIND ]] || fail "no find in path"

JAVAC=$(which javac 2> /dev/null)
[[ $JAVAC ]] || fail "no javac in path"

JAR=$(which jar 2> /dev/null)
[[ $JAR ]] || fail "no jar in path"

JAVA=$(which java 2> /dev/null)

# change to directory  with java source in
cd ../net/ufoai/sf/maputils
for FILE in *.java; do
	# classpath is the directory containing the net directory
	echo "_______________________________"
	echo "compiling $FILE"
#	$JAVAC -classpath ../../../.. $FILE
done

# change to the directory containing the net directory
cd ../../../..

echo "_______________________________"
echo "using svn to create file with revision number in"
$SVN info . > svn_info.txt

echo "_______________________________"
echo "using jar to create archive"
jar cvfm maputils.jar MANIFEST.MF svn_info.txt $($FIND net -type f ! -wholename '*.svn*' ! -name '*.java')

[[ $JAVA ]] || fail "no java in path, skipping the example run"
echo "_______________________________"
echo "example run"
$JAVA -jar maputils.jar -h
