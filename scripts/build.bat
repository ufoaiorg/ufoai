REM example script to build the maputil.
REM you are probably better off using netbeans (the sun IDE)

@echo off

rem change to directory  with java source in
cd ..\net\ufoai\sf\maputils
for %%i in (*.java) DO (
	rem classpath is the directory containing the net directory
	echo _______________________________
	echo compiling %%i
	javac -classpath ..\..\..\.. %%i
)

rem change to the directory containing the net directory
cd ..\..\..\..

echo _______________________________
echo using svn to create file with revision number in
svn info . > svn_info.txt

echo _______________________________
echo using jar to create archive
rem using jar is naff, it includes ".svn" directories and source in the jar
jar cvfm maputils.jar MANIFEST.MF net svn_info.txt


REM rem download command line 7zip (7za.exe) from http://www.7-zip.org/download.html
REM uncomment the next 3 lines to delecte unwanted files from the jar
REM echo _______________________________
REM echo using 7zip to delete unwanted stuff from archive
REM 7za d -r maputils.jar net\*.java ".svn"

echo _______________________________
echo example run
java -jar mapUtils.jar -h
pause
