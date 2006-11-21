#!/bin/bash
# This program uses wget to download the wiki page List_of_msgid.
# It scans all the entries in the language .po file and search for any
#   available translation on the wiki.
# It creates a file (name given by the variable output_file) which contains 
#   the updated .po file. 
# It also generates a log file which allows to check the differences.
#
# For each translation found on the wiki, a file 'sed_commands' containing all the sed
#   commands to update this entry is generated, and applied.
# The variable 'debug' should be set to 1 if you encounter a problem : it makes the log
#   far more verbose, and will help for debbugging.


language=$1
url="http://ufo.myexp.de/"
wiki_url="wiki/index.php/"
index="List_of_msgid"
input_file=$language".po"
output_file=updated_${language}.po
log_file=updated_${language}.log
debug=0


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
		        echo "   there are no translation available for this language in this section"
			return 255
		fi
	done < Language_List
	return 255
}

apply_sed()
{
# Procedure to apply the modifications to the .po file if there is any.
	
	if [ "$debug" = "1" ]
	then
		printf "___________________\n" >> $log_file

		echo "*** sed_commands of $english contains :"  >> $log_file
		cat sed_commands >> $log_file
		printf "\n\n" >> $log_file
	fi

	test=`awk ' BEGIN {FS="\""}
		NR == 2 {print $2}' sed_commands`
	test=${#test}
	if [ $test -eq 0 ]
	then
		test=`wc -l sed_commands | cut -d " " -f 1`
	fi

	if [ $test -gt 2 ]
	then
		sed -f 'sed_commands' $output_file > $output_file.tmp
		BEGIN=$BEGIN-2
		test=`awk 'NR == '$BEGIN' {print $0}' $output_file.tmp`
		if [ "$test" = "#, fuzzy" ]
		then
			sed $BEGIN'd' $output_file.tmp > $output_file.tmp.tmp
			mv -f $output_file.tmp.tmp $output_file.tmp
		fi

		test=`diff $output_file $output_file.tmp | awk 'NR==1 {print $0}'`
		if [ ${#test} -gt 0 ]
		then
		   echo "   $language.po file updated"
		   echo "***  $*  ***" >> $log_file
		   diff $output_file $output_file.tmp >> $log_file
		   mv -f $output_file.tmp $output_file
		   printf "__________________________________________\n\n" >> $log_file
		else 
		   echo "   found same translation in wiki than in $language.po"
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

BEGIN=`grep -n "<!-- start content -->" downloaded_page | cut -d : -f 1`
  sed '1,'$BEGIN'd' downloaded_page > temp

BEGIN=`grep -n "<div class=\"printfooter\">" temp | cut -d : -f 1`
END=`wc -l temp | cut -d " " -f 1`
  sed $BEGIN','$END'd' temp |
  sed ':a;N;$!ba;s/\n//g;s/^[ \t]*//g;s/[ \t]*$//g;s/&amp;/\&/g;s/&nbsp;/ /g;s/\s\+$//;s/<br \/>//g;s/<b>//g;s/<\/b>//g;s/<hr \/>//g;s/<\/p>/<p>/g;s/<i>//g;s/<\/i>//g;s/<\/h1>/<p>/g;s/<\/h2>/<p>/g;s/<\/h3>/<p>/g;s/<\/h4>/<p>/g;s/<dd>/<p>/g;s/<dl>/<p>/g;s/<\/dd>/<p>/g;s/<\/dl>/<p>/g;s/[ \t]*<p>[ \t]*/<p>/g;s/:/: /g;s/[ \t][ \t]*/ /g;s/class=\"image\"/><p></g' > downloaded_page
  rm -f temp

}

set_BEGIN_END()
{
# Initialisation of BEGIN and END values to the first and last line of the concerned msgid in the .po file 
# The first argument is 1 if this is a long description which need several lines of msgstr.
	BEGIN=`grep -n "msgid \"$english\"" $output_file |
	cut -d : -f 1`
	END=`awk 'NR > '$BEGIN' && $0 ~ /^#:[ \t]/ {print NR;exit}' $output_file`
	BEGIN=$BEGIN+1    
	END=$END-2
	 
	echo "${BEGIN},${END}d" > sed_commands
	END=$END+1
	echo -n "${END}imsgstr " >> sed_commands

	if [ $1 = "1" ]
	then
		echo "\"\"" >> sed_commands
	fi
}

download_description()
{
# Procedure looking for the url of the description of $english. It download it, then return 0 if it's OK, 1 if the english text is not on the wiki, and 2 if didn't find any translation in this language.
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
	  content=`awk 'BEGIN {FS="\""}
	    NR > '$number' && $0 ~ /<td>/ {n++}
	    NR > '$number' && n == '$loc'+1 && $0 ~ /^<ul><li>/ {
	      match($0,"<a href=")
	      if (RLENGTH>0) {
		$0=substr($0,RSTART+RLENGTH,length($0)-RSTART-RLENGTH+1);
	        match($0,";action=edit\" class=\"new\"")
	        if (RLENGTH<0) {printf "%s\n",$2}
	     	}
	      exit;
	      }
	    n > $loc {exit}
	    ' ${index}`
	  if [ ${#content} -gt 0 ]
	  then
	  	  echo "   downloading description..."
		  wget -nv -O downloaded_page ${url}${content}
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
	update_one_sentence "0"

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
# Procedure to update all the sentences. It takes the base name as input (intro_sentence or prolog_sentence) 

     set_BEGIN_END $1

     if [ ${BEGIN} -gt 1 ]
     then
	if [ $1 -eq "1" ]
	then
		option=$END"i"
	else
		option=""
	fi

	awk 'BEGIN {FS="]";RS="<p>"}
	  $0 ~ /^[ \t]*$/ {next}
	  test && ($0 ~ /^\[/ || $0 ~ /<.*>/) {exit}
	  $0 ~ /^\['$english'\]/ || test == 1 {
	    gsub(/^\['$english'\][ \t]*/,"")
	    gsub(/[ \t]*$/,"")
	    if (test && test2) {
	      printf "\\n"
              if ('$1' == 1) {printf "\n\\n\n"}
            }
	    if (length($0)>0) {test2=1}
	    printf "%s",$0
	    test=1}
	    ' downloaded_page |
	sed 's/^\['$english'\][ \t]*//g;s/\"/\\\"/g;s/\\/\\\\/g;s/^[ \t]*/'$option'\"/g;s/[ \t]*$/\"/g;s/[ \t][ \t]*/ /g' >> sed_commands
	printf "\n"
	echo "Found sentence msgid : " $english
	apply_sed $english
     fi

}

update_news()
{
# Procedure to update the description of $english (only for news). Takes 2 parameters as input : $1 is the number of <h3> title where the description begins (it will read until next <h3> or any unknown flag, such as the printfooter), and $2 is the total number of <h4> title you want to read inside the part designed by $1.
   set_BEGIN_END "1"
   if [ ${BEGIN} -gt 1 ]
   then
	awk -v section=$1 -v news=$2 -v max_h4=$3 'BEGIN {RS="<p>"}
	  $0 ~ /^[ \t]*$/ {next}
	  $0 ~ /<h2>/ {h2++;next}
	  h2 == section && $0 ~ /<h3>/ {h3++; if (h3>news) {exit};next}
	  h3 == news && $0 ~ /<h4>/ {
	    h4++
	    if (h4>max_h4) {exit}
	    match($0,"<h4>")
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
	  ' downloaded_page | 
	sed 's/^[ \t]*//g;/^[ \t]*$/d;s/\"/\\\"/g;s/\\/\\\\/g' |
	awk '$0 !~ /^[ \t]*$/ {
	   printf "'$END'i\"%s\"\n",$0}' >> sed_commands
	printf "\n"
	echo "Found news msgid : " $english
	apply_sed $english
   fi
}

update_txt()
{
# we scan the file to fill the sed_commands file.
# $1 contains sur h1 section we want to get text from
# $2 is 1 if we want to display h2 titles, 0 otherwise
# $3 is the number of h3 title you want to read before stopping
         set_BEGIN_END "1"
	 awk -v working_h2=$1 -v display_title=$2 -v h3_nb=$3 'BEGIN {already_written_test=0;exit_test=0;RS="<p>"}
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
	      if (h3 == 1) {$0="\\n\n"$0}
	      gsub (/[ \t]*$/,"")
	      $0="\\n\n"$0""}
	    h3 > h3_nb {exit}
	    $0 ~ /<h4>/ {
	        match($0,"<h4>");
	        $0=substr($0,RSTART+RLENGTH,length($0)-RSTART-RLENGTH+1);
	        $0="\\n\n"$0"\\n\n"
      	      }
	    $0 ~ /<.*>/ {
	      if (h3 == h3_nb) {exit}}
	    $0 ~ /^<.*>$/ {next}
	    $0 ~ /^SUB: / {
	      gsub (/[ \t]*$/,"")
	      $0=$0"\\n\n"}
	    $0 ~ /statistics:[ /t]*$/ {
	      gsub (/[ \t]*$/,"")
	      $0="\\n\n"$0"\\n\n"}
	    $0 !~ /^[ \t]*$/ {
	      gsub (/[ \t]*$/,"")
	      if (already_written_test) {printf "\\n\n\\n\n"}
	      printf "%s",$0
	      already_written_test=1;
	      exit_test=0}
	      ' downloaded_page | 
	 sed 's/[[:space:]]*<.*>[[:space:]]*//g;/^[[:space:]]*$/d;s/^[ \t]*//g;s/\"/\\\"/g' | 
	 awk '{printf "'$END'i\"%s\"\n",$0}' |	
	 sed 's/\\/\\\\/g'>> sed_commands
	 apply_sed $english
	 return $?
}
 



# Creation of the log file
printf "Log file for updating .po files.\n    Executed on " > $log_file
echo `date` >> $log_file
printf "__________________________________________\n\n" >> $log_file




# Generation of a file 'Language_List' which contains the available languages (and their position) for each array of the wiki index.
wget -O ${index} "${url}${wiki_url}${index}"
if [ $? -ne 0 ]
then
	echo "Fatal error whith wget" | tee -a $log_file
	exit
fi

awk ' BEGIN {FS="</th><th>"}
  $0 ~ /^<th> msgid <\/th><th> status <\/th><th> en / {
     if (begin > 0) {print begin" "NR-1" en"language}
     begin=NR
     language=""
     for (i=4; i<=NF; i++) {language=language$i} 
     gsub (/[ \t][ \t]*/," ",language)
     }
  END {print begin" "NR" en"language}
  ' ${index} > Language_List
echo "Creating Language_List : done." >> $log_file
if [[ "$debug" = "1" ]]
then
	printf "__________________________________________\n\n" >> $log_file
	echo "Language_List contains :" >> $log_file
	cat Language_List >> $log_file
	printf "__________________________________________\n\n" >> $log_file
fi




# Declaration of the integer variables which will be used.
declare -i number
declare -i pre_txt	# value = 1 if the text should avec a _pre_txt version, 0 else
declare -i loc 		# Contains the column of the array corresponding to the language
declare -i BEGIN	# Contains the first line of the concerned msgid in the .po file
declare -i END		# Contains the last line of the concerned msgid in the .po file
declare -i FIRST_LINE 	# Contains the first interesting line of the wiki index. (used in the procedure download_description() )




# Copy of $input_file to $output_file. 
# $output_file is converted from dos to unix if needed
# Each input of msgstr (i.e. the part before any \n) is put on only 1 line
BEGIN=`grep -nm 1 "#: " $language.po | cut -d : -f 1`
END=`wc -l $language.po | cut -d " " -f 1`
sed $BEGIN','$END's/^\"\(.*\)\"$/\1/g;s/\r//g' $language.po |
awk 'BEGIN {FS=" ";test=0}
  $0 ~ /^[ \t]*$/ {if (test) {printf "\"\n";}; printf "\n"; test=0; next}
  $0 ~ /^#/ || $0 ~ /^\"/ || $0 ~ /^msgid \"/ || $0 ~ /^msgstr \"/ {
  	if (test) {printf "\"\n"}
  	printf "%s\n",$0
	test=0
	next}
  {
  	if (test==0) {printf "\""}
	printf "%s", $0
	test=1
	if (index($NF,"\\n") > 0)
	  {printf "\"\n";test=0;}
  }
' > $output_file
echo "Converted $language.po to $output_file : done." >> $log_file

test=`diff $language.po $output_file | sed '/^</!d;/^<[ \t]*\"/d'`
if [[ ${#test} -gt 0 ]]
then
	echo "Security stop : the line $test has been removed from $language.po and shouldn't have" | tee -a $log_file
	exit
fi

if [[ "$debug" = "1" ]]
then
	printf "__________________________________________\n\n" >> $log_file
	echo "$output_file 300 first lines are :" >> $log_file
	awk 'NR <=300 {print $0}' $output_file >> $log_file
	printf "__________________________________________\n\n" >> $log_file
fi




# Generation of a file 'po_file.tmp' which contains the list of the english msgid
# present in the language .po file.

sed '/^msgid \"/!d;s/^msgid[ \t]*\"\(.*\)[ \t]*\"/\1/g;s/^[ \t]*//g;s/[ \t]*$//g;/^[ \t]*$/d' $language.po > po_file.tmp

echo "Creating po_file.tmp : done." >> $log_file
if [[ "$debug" = "1" ]]
then
	printf "__________________________________________\n\n" >> $log_file
	echo "po_file.tmp 50 first lines are :" >> $log_file
	awk 'NR <=50 {print $0}' po_file.tmp >> $log_file
	printf "__________________________________________\n\n" >> $log_file
fi




#
# MAIN PART OF THE PROGRAM
#
# We first begin to update the intro and prolog sentences, news and campaign descriptions, as they are on only 2 wiki page (it avoids useless downloading)
FIRST_LINE=`awk 'BEGIN {FS=" "};NR==1 {print $1}' Language_List`
FIRST_LINE=$FIRST_LINE-4
if [[ "$debug" = "1" ]]
then
	printf "First line of %s containing data : %s\n\n" ${index}  $FIRST_LINE >> $log_file
fi
printf "__________________________________________\n\n" >> $log_file

base_sentence="intro_sentence"
i=1
english=$base_sentence$i
download_description $english

if [[ $? -eq 0 ]]
then
  clean_html

  update_sentences $base_sentence

  base_sentence="prolog_sentence"
  i=1
  english=$base_sentence$i
  update_sentences $base_sentence
  
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
  
  if [[ "$debug" = "1" ]]
  then
	  echo "Checking news descriptions : done." >> $log_file
  fi
fi

english="txt_standard_campaign"
download_description $english
if [[ $? -eq 0 ]]
then
  clean_html

  for english in "txt_standard_campaign" "txt_hard_campaign" "txt_veryhard_campaign" "txt_easy_campaign" "txt_veryeasy_campaign" "txt_alien_campaign"
  do
     update_one_sentence "1"
  done
  if [[ "$debug" = "1" ]]
  then
	echo "Checking campaign descriptions : done." >> $log_file
  fi
fi

# We set the variable default_pre_txt to the text to write for *_pre_txt if not on the wiki.
english="default_pre_txt"
download_description $english
if [ $? -eq 0 ]
then
   clean_html
   default_pre_txt=`awk 'BEGIN {RS="<p>"}
      $0 ~ /</ {exit}
      $0 !~ /^[ \t]*$/ {printf "%s",$0}
      ' downloaded_page |
   sed 's/[[:space:]]*<.*>[[:space:]]*//g;s/^[ \t]*//g;s/\"/\\\"/g'`
fi
if [ "$default_pre_txt" = "" ]
then
	default_pre_txt="<TODO>"
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
  if [[ "$english" != "intro_sentence"* ]] && [[ "$english" != "prolog_sentence"* ]] && [[ "$english" != "news_"*"_txt" ]] && [[ "$english" != "txt_"*"_campaign" ]]
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
	 echo "Found section title msgid : " $english
         set_BEGIN_END "0"
	 grep -iwm 1 ">$english</a> (en), <a href" ${index} |
	 sed 's/<\/a>//g;/ ('$language')/!d;s/.*>\(.*\) ('$language').*/\1/;s/\"/\\\"/g;s/^[ \t]*/"/g;s/[ \t]*$/"/g;s/[ \t][ \t]*/ /g;s/\\/\\\\/g' >> sed_commands
	 apply_sed $english
    else
      pre_txt=0
      if [[ ${english} = *"_txt" ]]
      then
          test=${english:0:${#english}-4}_pre_txt
          test=`grep -im 1 "$test" po_file.tmp`
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
         clean_html
	 # There are (for now...) 2 types of descriptions : the autopsy and the others.
	 # For autopsy we want the second part of the wiki page
	 # For other descriptions, we want the first part.
	 # We distinguish between both with the variable 'working_section'
         if [[ ${english} = "ufo_"*"_txt" ]] || [[ ${english} = *"_autopsy_txt"* ]] || [[ ${english} = "kerrblade_txt" ]]
	 then
	    # Case of Alien Research : only the second part of the text has to be taken
	    update_txt 3 0 0

	    english=${english:0:${#english}-4}_pre_txt
	    update_txt 2 0 0
	    if [[ $? -eq 0 ]]
	    then
		set_BEGIN_END 0
		printf "\"$default_pre_txt\"" >> sed_commands
	        apply_sed $english
		pre_txt=0
	    fi
	 else
	    update_txt 2 1 3
	 fi
      elif [[ "$test" -eq 1 ]]
      then
         number=`grep -iwnm 1 ">$english</a>" ${index} | cut -d : -f 1`
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
	      awk -v number=$number 'BEGIN {FS="\""}
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
	      sed 's/>\(.*\)<\/a>/\1/' | 
	      sed 's/\"/\\\"/g;s/\\/\\\\/g;s/^[ \t]*/"/g;s/[ \t]*$/"/g;s/[ \t][ \t]*/ /g' >> sed_commands
	      apply_sed $english
      	    fi
	 fi
      fi
    fi
    if [[ $pre_txt -eq 1 ]]
    then
	english=${english:0:${#english}-4}_pre_txt
	set_BEGIN_END 0
	if [ $BEGIN -gt 1 ]
	then
		printf "\"$default_pre_txt\"" >> sed_commands
        	apply_sed $english
	fi
    fi
  fi
done < po_file.tmp

printf "\n\n"
printf "\nNew file %s created.\n" $output_file
printf "The file %s contains the list of the modifications.\n" $log_file
printf "You should CAREFULLY look at this log before uploading the updated file.\n"

rm -f Language_List
rm -f po_file.tmp
rm -f $output_file.tmp
rm -f downloaded_page
rm -f ${index}
rm -f sed_commands
echo "Deleting files : done." >> $log_file

