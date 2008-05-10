#!/bin/bash
# example script to build the maputil.
# you are probably better off using netbeans (the sun IDE)
# javac and jar are included in java sdk

SEPARATOR="_______________________________"

fail() {
    echo -e "\n$1"
    exit 1
}

SVN=$(which svn 2> /dev/null)
[[ $SVN ]] || fail "no svn in path"

FIND=$(which find 2> /dev/null)
[[ $FIND ]] || fail "no find in path"

AWK=$(which awk 2> /dev/null)
[[ $AWK ]] || fail "no awk in path"

JAVAC=$(which javac 2> /dev/null)
[[ $JAVAC ]] || fail "no javac in path"

JAR=$(which jar 2> /dev/null)
[[ $JAR ]] || fail "no jar in path"

JAVA=$(which java 2> /dev/null)

# change to directory with java source
cd ../net/ufoai/sf/maputils
while read FILE; do
    # classpath is the directory containing the net directory
    echo $SEPARATOR
    CLASSSECONDS=$(ls -l --time-style="+%s" "${FILE%\.java}.class" 2>/dev/null | $AWK '{print $6}')
    [[ $CLASSSECONDS ]] && {
	JAVASECONDS=$(ls -l --time-style="+%s" "$FILE" 2>/dev/null | awk '{print $6}')
	if [ "$JAVASECONDS" -lt "$CLASSSECONDS" ]; then
	    echo "skipping $FILE, classfile more recent"
	    continue
	fi
    }
    echo "compiling $FILE"
    COMPILED=yes
    $JAVAC -classpath ../../../.. "$FILE"
done < <(find ./ -type f -name '*.java')

# change to the directory containing the net directory
cd ../../../..

[[ $COMPILED ]] && {

    echo $SEPARATOR
    echo "using svn to create file with revision number"
    $SVN info . > svn_info.txt

    echo $SEPARATOR
    echo "using jar to create archive"
    $JAR cvfm maputils.jar MANIFEST.MF svn_info.txt $($FIND net -type f ! -wholename '*.svn*' ! -name '*.java')
} || {
    echo $SEPARATOR
    echo "skipping jar creation, there have been no changes to .java files"
}
[[ $JAVA ]] || fail "no java in path, skipping the example run"
echo -e "$SEPARATOR\nexample run\n$SEPARATOR"
$JAVA -jar maputils.jar -h
