<?php
include "internal/teamconfig.php";

// get all text files with a .mpt extension.
$dir = getDir() . '*.' . FILEEXT;
$teams = glob($dir);

// print each file name
echo "{";
foreach($teams as $team) {
	$id = getId($team);
	$data = getData($team);
	# the stored soldiers in this file
	$count = $data["soldiercount"];
	# might contain " - replace with '
	$name = str_replace("\"", "'", $data["name"]);
	echo "{id \"$id\" soldiercount \"$count\" name \"$name\"}";
}
echo "}";
