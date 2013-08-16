<?php
include "teamconfig.php";

// get all text files with a .mpt extension.
$dir = TEAMDIR . '*.' . FILEEXT;
$teams = glob($dir);

// print each file name
echo "{";
foreach($teams as $team) {
	preg_match_all('!\d+!', basename($team), $matches);
	$id = implode(' ', $matches[0]);
	$data = getData($team);
	# the stored soldiers in this file
	$count = $data["soldiercount"];
	# might contain " - replace with '
	$name = str_replace("\"", "'", $data["name"]);
	echo "{id \"$id\" soldiercount \"$count\" name \"$name\"}";
}
echo "}";
