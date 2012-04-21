#!/bin/bash
# This program uses wget to download the wiki page List_of_msgid.
# It scans all the entries in the language .po file and search for any
#   available translation on the wiki.
# It creates a file (name given by the variable output_file) which contains
#   the updated .po file.
# It also generates a log file which allows to check the differences.
#
# For each translation found on the wiki, a file 'sed_commands_${language0}' containing all the sed
#   commands to update this entry is generated, and applied.
# The variable 'debug' should be set to 1 if you encounter a problem : it makes the log
#   far more verbose, and will help for debbugging.
# The variable 'elseenglish' should be set to 1 if you want to replace all untranslated msgstr with the english version


language=$1
url="http://ufoai.org"
wiki_url="/wiki/index.php/List_of_msgid/"
chapters="Research Armour Equipment Buildings Aircraft Aircraft_Equipment UGVs UGV_Equipment Aliens Campaigns Story Mail_Headers Mail_Bodies"
index="List_of_msgid_"${language}
input_file="ufoai-"$language".po"	# temporary file used -- downloaded from wiki
output_file=updated_${language}.po	# name of the output file (contains updated strings)
log_file=updated_${language}.log	# name of the log file
debug=0			#set to 1 if you want the log file to be more verbose
onlyDescription=1	#set to 0 if you want to update also name of the items
elseenglish=1

if [ -z $language ]
then
	echo "Usage : $0 lang"
	echo "   where lang is the prefix of one .po file (en, de, fr, ...)"
	exit
fi

if [ ! -e $input_file ]
then
	echo "Could not find the file "$input_file" in this directory."
fi

language0=$language

# we use gnu sed and gnu awk a lot of times in this script. For linux, those programs are called 'sed' and 'awk', but on other system (fresbsd), they are called gsed and gawk. We set sed_soft and awk_soft to the name of those programs.
sed_soft="sed"
awk_soft="awk"
test=`printf "rnt\n" | $sed_soft 's/\r//g;s/\n//g;s/\t//g'`
if [ "$test" != "rnt" ]
then
	which gsed > /dev/null
	if [ $? -eq 0 ]
	then
		sed_soft="gsed"
		awk_soft="gawk"
	else
		printf "Sorry, but this program needs a version of gnu sed and gnu awk to run (named sed or gsed and awk or gawk).\nPlease install gsed and gawk(this is different from sed and awk for other Unixes than Linux).\n"
		exit
	fi

	test=`printf "rnt\n" | $sed_soft 's/\r//g;s/\n//g;s/\t//g'`
	if [ "$test" != "rnt" ]
	then
		printf "Sorry, but your gnu sed and gnu awk does not act properly: exiting.\n"
		exit
	fi
fi

find_line()
{
# Procedure to find the column containing the corresponding language translation
	while read l_line
	do
		declare -i begin
		declare -i end
		begin=`echo "$l_line" | cut -d " " -f 1`
		end=`echo "$l_line" | cut -d " " -f 2`
		if [ $1 -ge $begin ] && [ $1 -le $end ]
		then
			declare -i j
			j=0
			liste=`echo "$l_line" | cut -d " " -f 3-`
			for language_select in $liste
			do
				if [ $language_select = $language ]
				then
					return $j
				fi
				j=$j+1
			done
				echo "   there are no translations available for this language in this section"
			return 255
		fi
	done < Language_List_${language0}
	return 255
}

apply_sed()
{
# Procedure to apply the modifications to the .po file if there is any.

	if [ "$debug" = "1" ]
	then
		printf "___________________\n" >> $log_file

		echo "*** sed_commands_${language0} of $english contains :"  >> $log_file
		cat sed_commands_${language0} >> $log_file
		printf "\n\n" >> $log_file
	fi

	test=`$awk_soft ' BEGIN {FS="\""}
		NR == 2 {print $2}' sed_commands_${language0}`
	test=${#test}
	if [ $test -eq 0 ]
	then
		#test=`wc -l sed_commands_${language0} | $sed_soft 's/^[ \t]*//g' | cut -d " " -f 1`
		test=`$awk_soft 'NR == 3 {print $0}' sed_commands_${language0}`
		test=${#test}
	fi

	if [ $test -gt 0 ]
	then
		test=`$awk_soft 'NR == '$BEGIN' {print $0}' $output_file | grep "msgid_plural"`
		if [ ${#test} -gt 0 ]
		then
				test=0
		else
				test=1
		fi
	fi


	if [ $test -gt 0 ]
	then
		$sed_soft -f "sed_commands_${language0}" $output_file > $output_file.tmp
		BEGIN=$BEGIN-2
		test=`$awk_soft 'NR == '$BEGIN' {print $0}' $output_file.tmp`
		if [ "$test" = "#, fuzzy" ]
		then
			$sed_soft $BEGIN'd' $output_file.tmp > $output_file.tmp.tmp
			mv -f $output_file.tmp.tmp $output_file.tmp
		fi

		test=`diff $output_file $output_file.tmp | $awk_soft 'NR==1 {print $0}'`
		if [ ${#test} -gt 0 ]
		then
			echo "   $input_file file updated"
			echo "***  $*  ***" >> $log_file
			diff $output_file $output_file.tmp >> $log_file
			mv -f $output_file.tmp $output_file
			printf "__________________________________________\n\n" >> $log_file
		else
			echo "   found same translation in wiki and in $input_file"
			fi
		return 1
	else
		echo "   didn't find any associated translation"
		return 0
	fi
}

clean_html()
{
# Procedure to clean up the download file of useless html tag. We keep the <p> tags to separate the different paragraphs.
# Warning : Never use clean_html after set_BEGIN_END or it will change the values of BEGIN and END

	BEGIN=`grep -n "<!-- start content -->" downloaded_page_${language0} | cut -d : -f 1`
	$sed_soft '1,'$BEGIN'd' downloaded_page_${language0} > downloaded_page_${language0}.tmp

	BEGIN=`grep -n "<div class=\"printfooter\">" downloaded_page_${language0}.tmp | cut -d : -f 1`
	END=`wc -l downloaded_page_${language0}.tmp | $sed_soft 's/^[ \t]*//g' | cut -d " " -f 1`
	$sed_soft $BEGIN','$END'd' downloaded_page_${language0}.tmp |
	$sed_soft 's/(Extra:.*researched)[ \t]*//g;s/[ \t]*(End Extra)//g' |
	$sed_soft ':a;N;$!ba;s/\n//g;s/^[ \t]*//g;s/[ \t]*$//g;s/&amp;/\&/g;s/&#160;/ /g;s/&nbsp;/ /g;s/\s\+$//;s/<br \/>//g;s/<b>//g;s/<\/b>//g;s/<hr \/>//g;s/<\/p>/<p>/g;s/<i>//g;s/<\/i>//g;s/<\/h1>/<p>/g;s/<\/h2>/<p>/g;s/<\/h3>/<p>/g;s/<\/h4>/<p>/g;s/<dd>/<p>/g;s/<dl>/<p>/g;s/<\/dd>/<p>/g;s/<\/dl>/<p>/g;s/[ \t]*<p>[ \t]*/<p>/g;s/:/: /g;s/[ \t][ \t]*/ /g;s/class=\"image\"/><p></g;s/<\/span>//g' > downloaded_page_${language0}
	rm -f downloaded_page_${language0}.tmp

}

set_BEGIN_END()
{
# Initialisation of BEGIN and END values to the first and last line of the concerned msgid in the .po file
# The first argument is 1 if this is a long description which need several lines of msgstr.
	BEGIN=`grep -n "msgid \"$english\"" $output_file | cut -d : -f 1`
	END=`$awk_soft 'NR > '$BEGIN' && $0 ~ /^[ \t]*\r?$/ {print NR;exit}' $output_file`
	BEGIN=$BEGIN+1
	END=$END-1

	echo "${BEGIN},${END}d" > sed_commands_${language0}
	END=$END+1
	echo -n "${END}imsgstr " >> sed_commands_${language0}

	if [ $1 = "1" ]
	then
		echo "\"\"" >> sed_commands_${language0}
	fi
}

download_description()
{
# Procedure looking for the url of the description of $english. It downloads it, then return 0 if it's OK, 1 if the msgid is not on the wiki, and 2 if didn't find any translation in this language.
    number=`grep -iwnm 1 "<td> $english" ${index} | cut -d : -f 1`
    if [ $number -ge $FIRST_LINE ]
    then
        # Case of a long description (*_txt)
		printf "\n"
		echo "Found long msgid : "$english
		find_line "$number"
		loc=$?+1
		if [ $loc -ne 256 ]
		then
			content=`$awk_soft 'BEGIN {FS="\""}
			NR > '$number' && $0 ~ /<td>/ {n++}
			NR > '$number' && n == '$loc'+1 && $0 ~ /^<ul><li>/ {
				match($0,"<a href=")
				if (RLENGTH>0) {
					$0=substr($0,RSTART+RLENGTH,length($0)-RSTART-RLENGTH+1);
					match($0,"&amp;action=edit&amp;redlink=1\"")
					if (RLENGTH<0) {printf "%s\n",$2}
				}
				exit;
				}
			n > $loc {exit}
			' ${index}`
			if [ ${#content} -gt 0 ]
			then
				echo "   downloading description..."
				wget -nv -O downloaded_page_${language0} ${url}${content}
				if [ $? -ne 0 ]
				then
					echo "Error whith wget" > $log_file
					return 1
				fi
				return 0
			else
				echo "   didn't find any associated translation"
				return 2
			fi
		else
				echo "   didn't find any associated translation"
				return 2
		fi
    fi
    return 1
}

update_sentences()
{
# Procedure to update all the sentences. It takes the base name as input (intro_sentence or prolog_sentence)
	declare -i i
	i=1
	english=$*$i

	while true
	do
		update_one_sentence "1"

		i=$i+1
		english=$*$i
		number=`grep -iwnm 1 "$english" ${index} | cut -d : -f 1`
		if [ $number -lt $FIRST_LINE ]
		then
			break
		fi
	done
}

update_one_sentence()
{
# Procedure to update all the sentences. $1 is 1 for long descriptions, 0 for short ones.
# WARNING : for some reasons, it does not seem to work when $1 is set to 0.
# The name of the msgid to update is read in $english
	set_BEGIN_END $1

    if [ ${BEGIN} -gt 1 ]
    then
		if [ $1 -eq "1" ]
		then
			option=$END"i"
		else
			option=""
		fi
	$awk_soft 'BEGIN {FS="]";RS="<p>"}
		$0 ~ /^[ \t]*$/ {next}
		test && ($0 ~ /^\[/ || $0 ~ /<.*>/) {exit}
		$0 ~ /\['$english'\]/ || test == 1 {
			gsub(/.*\['$english'\][ \t]*<\/a>/,"")
			gsub(/[ \t]*$/,"")
			if (test && test2) {
				printf "\\n"
				if ('$1' == 1) {printf "\n\\n\n"}
				}
			if (length($0)>0) {test2=1}
			printf "%s",$0
			test=1}
		' downloaded_page_${language0} |
	$sed_soft 's/^\['$english'\][ \t]*//g;s/\"/\\\"/g;s/\\/\\\\/g;/^[ \t]*$/d;s/^[ \t]*/'$option'\"/g;s/[ \t]*$/\"/g;s/[ \t][ \t]*/ /g' >> sed_commands_${language0}
	printf "\n"
	echo "Found sentence msgid : " $english
	apply_sed $english
    fi
}

update_news()
{
# Procedure to update the description of $english (only for news). Takes 3 parameters as input : $1 is the number of h1 section where the text is written, $2 is the number of <h3> title where the description begins (it will read until next <h3> or any unknown flag, such as the printfooter), and $3 is the total number of <h4> title you want to read inside the part designed by $2.
	set_BEGIN_END "1"
	if [ ${BEGIN} -gt 1 ]
	then
		$awk_soft -v section=$1 -v news=$2 -v max_h4=$3 'BEGIN {RS="<p>"}
			$0 ~ /^[ \t]*$/ {next}
			$0 ~ /<h2>/ {h2++;next}
			h2 == section && $0 ~ /<h3>/ {h3++; if (h3>news) {exit};next}
			h3 == news && $0 ~ /<h4>/ {
				h4++
				if (h4>max_h4) {exit}
				match($0,/">[ ]*/)
				$0=substr($0,RSTART+RLENGTH,length($0)-RSTART-RLENGTH+1);
				if (h4>1) {$0="\\n\n\\n\n"$0}
				gsub (/[ \t]*$/,":")
				}
			h3 == news && $0 ~ /<.*>/ {exit}
			h3 == news || test == 1{
				gsub(/[ \t]*$/,"")
				if (test) {printf "\\n\n\\n\n"}
				printf "%s",$0
				test=1}
			END {printf "\n"}
		' downloaded_page_${language0} |
		$sed_soft 's/^[ \t]*//g;/^[ \t]*$/d;s/\"/\\\"/g;s/\\/\\\\/g' |
		$awk_soft '$0 !~ /^[ \t]*$/ {
			printf "'$END'i\"%s\"\n",$0}' >> sed_commands_${language0}
		printf "\n"
		echo "Found news msgid : " $english
		apply_sed $english
	fi
}

update_txt()
{
# we scan the file to fill the sed_commands_${language0} file.
# $1 contains sur h1 section we want to get text from
# $2 is 1 if we want to display h2 titles, 0 otherwise
# $3 is the number of h3 title you want to read before stopping
# $4 is 1 if you want to skip the 4 line header, 0 if you want to keep it
	set_BEGIN_END "1"
	$awk_soft -v working_h2=$1 -v display_title=$2 -v h3_nb=$3 -v skip_header=$4 'BEGIN {already_written_test=0;exit_test=0;line=0;RS="<p>"}
		h2 == working_h2 && $0 ~ /<h1>/ {exit}
		$0 ~ /<h2>/ {h2++}
		h2 < working_h2 {next}
		h2 > working_h2 {exit}
		$0 ~ /<h2>/ {
			if (display_title) {
				match($0,"<h2>");
				$0=substr($0,RSTART+RLENGTH,length($0)-RSTART-RLENGTH+1);
			}
			else {next}
			}
		$0 ~ /<h3>/ {h3++;
			match($0,"<h3>");
			$0=substr($0,RSTART+RLENGTH,length($0)-RSTART-RLENGTH+1);
			match($0,"\">");
			$0=substr($0,RSTART+RLENGTH,length($0)-RSTART-RLENGTH+1);
			if (h3 == 1 && already_written_test) {$0="\\n\n"$0}
			gsub (/[ \t]*$/,"")
			if (already_written_test) {$0="\\n\n"$0""}
			}
		h3 > h3_nb {exit}
		$0 ~ /<h4>/ {
			match($0,"<h4>");
			$0=substr($0,RSTART+RLENGTH,length($0)-RSTART-RLENGTH+1);
			$0="\\n\n"$0"\\n\n"
			}
		$0 ~ /<.*>/ {
			if (h3 == h3_nb) {exit}}
		$0 ~ /^<.*>$/ {next}
		$0 ~ /statistics:[ \t]*$/ {
			gsub (/[ \t]*$/,"")
			$0="\\n\n"$0"\\n\n"}
		$0 !~ /^[ \t\n]*$/ {
			if (line < 4 && skip_header ==1) {line++;next}
			gsub (/[ \t]*$/,"")
			if (already_written_test) {printf "\\n\n\\n\n"}
			printf "%s",$0
			line++
			already_written_test=1;
			exit_test=0}
	' downloaded_page_${language0} |
	$sed_soft 's/[[:space:]]*<.*>[[:space:]]*//g;/^[[:space:]]*$/d;s/^[ \t]*//g;s/\"/\\\"/g' |
	$awk_soft '{printf "'$END'i\"%s\"\n",$0}' |
	$sed_soft 's/\\/\\\\/g' >> sed_commands_${language0}

	apply_sed $english
	return $?
}

update_campaign()
{
	set_BEGIN_END "1"
	$awk_soft 'BEGIN {already_written_test=0;RS="<p>"}
		$0 ~ /mw-content-ltr/ {next}
		$0 ~ /<!-- NewPP limit/ {exit}
		{ if (already_written_test == 0) {already_written_test = 1}
		else {$0="\\n\n"$0}
		printf "%s",$0 }
	' downloaded_page_${language0} |
	$sed_soft 's/[[:space:]]*<.*>[[:space:]]*//g;/^[[:space:]]*$/d;s/^[ \t]*//g;s/\"/\\\"/g' |
	$awk_soft '{printf "'$END'i\"%s\"\n",$0}' |
	$sed_soft 's/\\/\\\\/g' >> sed_commands_${language0}

	apply_sed $english
	return $?
}

create_language_file()
{
# Generation of a file 'Language_List_${language0}' which contains the available languages (and their position) for each array of the wiki index.
# Every line describe one tabular in ${index} file (there should be one tabular for each element of ${chapter}) :
#	The first number of a line is the first line of ${index} file of the tabular containing all strings in all languages
#	The second number is the line of ${index} file which indicates the end of the previous tabular
#	Then, all languages translated in the tabular is written, in the order of column appearance

	if [ -f Language_List_${language0} ]
	then
		rm Language_List_${language0}
		touch Language_List_${language0}
	fi
	declare -i id=0
	declare -i language_test=0
	declare -i last_line_check=0
	declare list_language

	while read ll_line
	do
		id=${id}+1
		if [[ "$ll_line" = *"en (source)"* ]]
		then
			language_test=1
			continue
		fi

		if [ "${language_test}" -eq 1 ]
		then
			declare temp_string=$ll_line
			temp_string=`echo "$temp_string" | cut -d ">" -f 2`
			temp_string=`echo "$temp_string" | cut -d "<" -f 1`
			list_language=${list_language}${temp_string}
		fi

		if [[ "${language_test}" -eq 1 ]] && [[ "$ll_line" = *"th><"* ]]
		then
			printf ${id}" " >> Language_List_${language0}
			language_test=0
			last_line_check=1
		fi

		if [[ "${last_line_check}" -eq 1 ]] && [[ "$ll_line" = *"table>"* ]]
		then
			echo ${id}" en"$list_language >> Language_List_${language0}
			list_language=""
			last_line_check=0
		fi
	done < ${index}

	echo "Creating Language_List_${language0} : done." >> $log_file
	if [[ "$debug" = "1" ]]
	then
		printf "__________________________________________\n\n" >> $log_file
		echo "Language_List_${language0} contains :" >> $log_file
		cat Language_List_${language0} >> $log_file
		printf "__________________________________________\n\n" >> $log_file
	fi
}


# Creation of the log file
printf "Log file for updating .po files.\n    Executed on " > $log_file
echo `date` >> $log_file
printf "__________________________________________\n\n" >> $log_file




# Generation of a file '$index' which contains the chapters pages on the wiki.
for i in `echo $chapters`; do
	wget -O ${i}"_"${language} "${url}${wiki_url}${i}";
	if [ $? -ne 0 ]
	then
		echo "Fatal error whith wget" | tee -a $log_file
		exit
	fi;
done
if [ -f $index ]
then
	rm ${index}
	touch ${index}
fi

for i in `echo $chapters`; do
	cat ${i}"_"${language} >> ${index};
	rm ${i}"_"${language}
done

create_language_file


# Declaration of the integer variables which will be used.
declare -i number
declare -i pre_txt     # value = 1 if the text should avec a _pre_txt version, 0 else
declare -i loc         # Contains the column of the array corresponding to the language
declare -i BEGIN       # Contains the first line of the concerned msgid in the .po file
declare -i END         # Contains the last line of the concerned msgid in the .po file
declare -i FIRST_LINE  # Contains the first interesting line of the wiki index. (used in the procedure download_description() )




# Copy of $input_file to $output_file.
# $output_file is converted from dos to unix if needed
# Each input of msgstr (i.e. the part before any \n) is put on only 1 line
BEGIN=`grep -nm 1 "msgid " $input_file | cut -d : -f 1`
END=`wc -l $input_file | $sed_soft 's/^[ \t]*//g' | cut -d " " -f 1`
$sed_soft $BEGIN','$END's/^\"\(.*\)\"$/\1/g;s/\r//g' $input_file |
$awk_soft 'BEGIN {FS=" ";test=0}
	$0 ~ /^[ \t]*$/ {if (test) {printf "\"\n";}; printf "\n"; test=0; next}
    $0 ~ /^#/ || $0 ~ /^\"/ || $0 ~ /^msgid/ || $0 ~ /^msgstr/ {
		if (test) {printf "\"\n"}
		printf "%s\n",$0
		test=0
		next}
	{	if (test==0) {printf "\""}
		printf "%s", $0
		test=1
		if (index($NF,"\\n") > 0)
			{printf "\"\n";test=0;}
		}
' > $output_file
echo "Converted $input_file to $output_file : done." >> $log_file

#This test doesn't work for files too big... I suppress it.
#test=`diff $input_file $output_file | $sed_soft '/^</!d;/^<[ \t]*\"/d'`
#if [[ ${#test} -gt 0 ]]
#then
#	echo $test
#	echo "Security stop : the previous line(s) has been removed from $input_file and shouldn't have" | tee -a $log_file
#	exit
#fi

if [[ "$debug" = "1" ]]
then
	printf "__________________________________________\n\n" >> $log_file
	echo "$output_file 300 first lines are :" >> $log_file
	$awk_soft 'NR <=300 {print $0}' $output_file >> $log_file
	printf "__________________________________________\n\n" >> $log_file
fi




# Generation of a file 'po_file_${language0}.tmp' which contains the list of the english msgid
# present in the language .po file.

$sed_soft '/^msgid \"/!d;s/^msgid[ \t]*\"\(.*\)[ \t]*\"/\1/g;s/^[ \t]*//g;s/[ \t]*$//g;/^[ \t]*$/d' $input_file > po_file_${language0}.tmp

echo "Creating po_file_${language0}.tmp : done." >> $log_file
if [[ "$debug" = "1" ]]
then
	printf "__________________________________________\n\n" >> $log_file
	echo "po_file_${language0}.tmp 50 first lines are :" >> $log_file
	$awk_soft 'NR <=50 {print $0}' po_file_${language0}.tmp >> $log_file
	printf "__________________________________________\n\n" >> $log_file
fi




# Initialisation of the file unfound_msgid_${language0}.tmp which will contain the list of unfound msgids
rm -rf unfound_msgid_${language0}.tmp
touch unfound_msgid_${language0}.tmp

#
# MAIN PART OF THE PROGRAM
#
# We first begin to update the intro and prolog sentences, news and campaign descriptions, as they are on only 2 wiki page (it avoids useless downloading)
FIRST_LINE=`$awk_soft 'BEGIN {FS=" "};NR==1 {print $1}' Language_List_${language0}`
FIRST_LINE=$FIRST_LINE-4
if [[ "$debug" = "1" ]]
then
	printf "First line of %s containing data : %s\n\n" ${index}  $FIRST_LINE >> $log_file
fi
printf "__________________________________________\n\n" >> $log_file

# We set the variable default_pre_txt to the text to write for *_pre_txt if not on the wiki.
english="default_pre_txt"
download_description $english
if [ $? -eq 0 ]
then
	clean_html
	default_pre_txt=`$awk_soft 'BEGIN {already_written_test=0;RS="<p>"}
		$0 ~ /mw-content-ltr/ {next}
		$0 ~ /<!-- NewPP limit/ {exit}
		{ printf "%s",$0 }
	' downloaded_page_${language0} |
	$sed_soft 's/[[:space:]]*<.*>[[:space:]]*//g;/^[[:space:]]*$/d;s/^[ \t]*//g;s/\"/\\\"/g'`
fi
if [ "$default_pre_txt" = "" ]
then
	default_pre_txt="<TODO>"
fi

base_sentence="intro_sentence"
i=1
english=$base_sentence$i
download_description $english
if [[ $? -eq 0 ]]
then
	clean_html

	update_sentences $base_sentence

#	base_sentence="prolog_sentence"
#	i=1
#	english=$base_sentence$i
#	update_sentences $base_sentence

	if [[ "$debug" = "1" ]]
	then
		echo "Checking introduction sentences : done." >> $log_file
	fi

	english="news_initial_political_situation_txt"
	update_news "3" "1" "1"

	english="news_after_the_war_txt"
	update_news "3" "2" "8"

	english="news_phalanx_and_mumbai_aftermath_txt"
	update_news "3" "3" "1"

	english="news_new_twist_txt"
	download_description
	test=$?
	if [[ "$test" -eq 0 ]]
	then
		clean_html
		pre_txt=1
		update_txt 2 0 0 $pre_txt
		pre_txt=0
	fi

	english="news_new_twist_alt_txt"
	download_description
	test=$?
	if [[ "$test" -eq 0 ]]
	then
		clean_html
		pre_txt=1
		update_txt 2 0 0 $pre_txt
		pre_txt=0
	fi

	if [[ "$debug" = "1" ]]
	then
		echo "Checking news descriptions : done." >> $log_file
	fi
fi


for english in "standard_campaign_txt" "hard_campaign_txt" "veryhard_campaign_txt" "easy_campaign_txt" "veryeasy_campaign_txt"
do
	download_description $english
	if [[ $? -eq 0 ]]
	then
		clean_html
		update_campaign
	fi
done

if [[ "$debug" = "1" ]]
then
	echo "Checking campaign : done." >> $log_file
fi

english="mail_aircraft_landed"
download_description $english
if [[ $? -eq 0 ]]
then
	clean_html

	for english in "mail_stunned_alien_died" "mail_prolog" "mail_alien_ufo_crashed" "mail_alien_ufo_recovered" "mail_aircraft_landed" "mail_aircraft_bingo_fuel" "mail_aircraft_ready" "mail_aircraft_lost_target" "mail_aircraft_new_at_base" "mail_aircraft_lost_target" "mail_alien_activity_reported" "mail_alien_ufo_downed" "mail_alien_response_too_late" "mail_alien_new_radar_contact" "mail_alien_lost_radar_contact" "mail_alien_base_discovered" "mail_general_mission_summary" "mail_general_base_attack_report" "mail_general_new_base" "mail_general_construction_finished" "mail_general_equipment_received" "mail_general_transfer_received" "mail_general_ufo_in_hangar" "mail_general_monthly_report" "mail_production_finished" "mail_production_not_enough_resources" "mail_production_not_enough_money" "mail_prolog" "mail_stunned_alien_died"
	do
		update_one_sentence "1"
	done
	if [[ "$debug" = "1" ]]
	then
		echo "Checking mail bodies : done." >> $log_file
	fi
fi


# Then, we scan all the entries of the .po file and look for a translation on the web.
# There are several possibilities :
# 1. The entry is the translation of a section of the wiki index
# 2. The entry is a intro/prolog/news/campaign text, and so has already been updated
# 3. The entry is an other _txt entry and so we have to download the wiki page containing the translation
# 4. The entry is a short sentence, the translation is on the index page (no need to download a new page)
# 5. The entry has no translation on the wiki. In this case, we just go to the next entry.
printf "__________________________________________\n\n" >> $log_file
if [[ "$debug" = "1" ]]
then
	echo "Entering main part of the program..." >> $log_file
fi

while read english
do
	if [[ "$onlyDescription" = "1" ]] && [[ "$english" != *"_txt" ]]
	then
		# no need to check, we care only about description
		printf "."
		continue
	fi

	if [[ "$english" != "intro_sentence"* ]] && [[ "$english" != "prolog_sentence"* ]] && [[ "$english" != "news_"*"_txt" ]] && [[ "$english" != *"_campaign_txt" ]] && [[ "$english" != "Aliens" ]] && [[ "$english" != "mail_"* ]]
	then
		printf "."
		if [[ "$debug" = "1" ]]
		then
			echo "Looking for a translation of $english..." >> $log_file
		fi
		number=`grep -iwnm 1 ">$english</a> (en)" ${index} | cut -d : -f 1`
		if [[ ${number} -gt 0 ]]
		then
		# This is case 1. (translation of a title of the wiki)
			printf "\n"
			echo "Found section title msgid: " $english
			set_BEGIN_END "0"
			grep -iwm 1 ">$english</a> (en), <a href" ${index} |
			$sed_soft 's/<\/a>//g;/ ('$language')/!d;s/.*>\(.*\) ('$language').*/\1/;s/\"/\\\"/g;s/^[ \t]*/"/g;s/[ \t]*$/"/g;s/[ \t][ \t]*/ /g;s/\\/\\\\/g' >> sed_commands_${language0}
			apply_sed $english
		else
			pre_txt=0
			if [[ ${english} = *"_txt" ]]
			then
				test=${english:0:${#english}-4}_pre_txt
				test=`grep -iwm 1 "$test" po_file_${language0}.tmp`
				if [[ ${#test} -gt 0 ]]
				then
					pre_txt=1
				fi
			fi
			download_description
			test=$?
			if [[ "$test" -eq 0 ]]
			then
			# This is case 3. (_txt entry)
			# There are (for now...) 2 types of descriptions : the email-type and the others.
			# For email type texts, we want to download 2 sections.
			# For other descriptions, we want only 1 section.

				clean_html
				if [[ $pre_txt -eq 1 ]]
				then
				# Case of Research with proposal part: only the second part of the text has to be taken
				update_txt 3 0 0 1

				english=${english:0:${#english}-4}_pre_txt
				update_txt 2 0 0 1
					if [[ $? -eq 0 ]]
					then
						set_BEGIN_END 0
						printf "\"$default_pre_txt\"" >> sed_commands_${language0}
						apply_sed $english
						pre_txt=0
					fi
				elif [[ "$english" = "b_antimatter_txt" ]]
				then
					# Exception: text is in e-mail type, but without pre_txt
					update_txt 2 0 3 1
				else
					update_txt 2 1 3 0
				fi
			elif [[ "$test" -eq 2 ]] && [[ "$elseenglish" -eq 1 ]] && [[ "$language" != "en" ]]
			then
			# This is the case when the msgid is in the wiki, but not the desired translation: check if we can use english text
				language0=$language
				language="en"
				download_description
				test=$?
				if [[ "$test" -eq 0 ]]
				then
					# There is a text in english, use it for this language
					clean_html
					if [[ $pre_txt -eq 1 ]]
					then
					update_txt 3 0 0 1

					english=${english:0:${#english}-4}_pre_txt
					update_txt 2 0 0 1
						if [[ $? -eq 0 ]]
						then
							set_BEGIN_END 0
							printf "\"$default_pre_txt\"" >> sed_commands_${language0}
							apply_sed $english
							pre_txt=0
						fi
					elif [[ "$english" = "b_antimatter_txt" ]]
					then
						# Exception: text is in e-mail type, but without pre_txt
						update_txt 2 0 3 1
					else
						update_txt 2 1 3 0
					fi
				fi
				language=$language0
			elif [[ "$test" -eq 1 ]] && [[ "$onlyDescription" = "0" ]]
			then
				test_artifact="0"
				number=`grep -iwnm 1 ">$english</a>" ${index} | cut -d : -f 1`
				if [[ $number -lt $FIRST_LINE ]]
				then
					# This part looks for $english where "-" would be replaced by "--" (like in UFO -- Scout)
					test=`echo $english | sed 's/-/--/g'`
					number=`grep -iwnm 1 ">$test</a>" ${index} | cut -d : -f 1`
				fi
				if [[ $number -lt $FIRST_LINE ]]
				then
					# This part looks for " -- $english" in case this is a Alien Artifact msgid
					test_artifact="1"
					number=`grep -inm 1 " -- $english</a>" ${index} | cut -d : -f 1`
				fi

				if [[ $number -ge $FIRST_LINE ]]
				then
				# This is case 4. (short entry)
					printf "\n"
					echo "Found short msgid : " $english
					set_BEGIN_END "0"
					find_line "$number"
					loc=$?
					if [[ $loc -ne 255 ]]
					then
						$awk_soft -v number=$number 'BEGIN {FS="\""}
							NR > number && $0 ~ /<td>/ {n++}
							NR >= number && n == '$loc' && $0 ~ /^<ul><li>/ {
								match($0,"title=\"")
								if (RLENGTH>0) {$0=substr($0,RSTART+RLENGTH,length($0)-RSTART-RLENGTH+1);
									printf "%s",$2;
									}
								exit;
								}
							n > $loc {exit}
						' ${index} |
						$sed_soft 's/>\(.*\)<\/a>/\1/;s/--/-/g' |
						$awk_soft '
							'$test_artifact' == 1 {gsub(/^.* -/,"")}
							{print $0}' |
						$sed_soft 's/\"/\\\"/g;s/\\/\\\\/g;s/^[ \t]*/"/g;s/[ \t]*$/"/g;s/[ \t][ \t]*/ /g' >> sed_commands_${language0}
						apply_sed $english
					fi
				else
					printf "%s\n" "$english" >> unfound_msgid_${language0}.tmp
				fi
			fi
		fi
		if [[ $pre_txt -eq 1 ]]
		then
			english=${english:0:${#english}-4}_pre_txt
			set_BEGIN_END 0
			if [ $BEGIN -gt 1 ]
			then
				printf "\"$default_pre_txt\"" >> sed_commands_${language0}
				apply_sed $english
			fi
		fi
	fi
done < po_file_${language0}.tmp

printf "\n"
#Copying all unfound msgids to log file
if [[ "$debug" = "1" ]]
then
	printf "__________________________________________\n\n" >> $log_file
	printf "List of unfound msgids in the wiki page :\n" >> $log_file
	cat unfound_msgid_${language0}.tmp >> $log_file
fi
printf "__________________________________________\n\n" | tee -a $log_file

printf "\n\n"
printf "List of modified long text msgids in %s :\n" $input_file | tee -a $log_file
grep "\*\*\* " $log_file | $sed_soft '/_txt/!d' | tee -a $log_file
printf "__________________________________________\n\n" >> $log_file

printf "\n\n"
printf "\nNew file %s created.\n" $output_file
printf "The file %s contains the list of the modifications.\n" $log_file
printf "You should CAREFULLY look at this log before uploading the updated file.\n"

rm -f Language_List_${language0}
rm -f po_file_${language0}.tmp
rm -f unfound_msgid_${language0}.tmp
rm -f $output_file.tmp
rm -f downloaded_page_${language0}
rm -f ${index}
rm -f sed_commands_${language0}
echo "Deleting files : done." >> $log_file
