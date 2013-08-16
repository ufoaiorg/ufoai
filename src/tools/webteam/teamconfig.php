<?php
DEBUG=false
if (DEBUG) {
	error_reporting(E_ALL | E_STRICT | E_DEPRECATED);
} else {
	// do not disclose information
	error_reporting(0);
}

#const TEAMDIR = '/home/mgerhardy/.ufoai/2.5-dev/base/save/';
#const TEAMDIR = '/home/users/mattn/htdocs/ufoai/teams/';
const TEAMDIR = '/var/www/teams/';
const FORMNAME = "team";
const FILEEXT = "mpt";
# 1MB
const MAXSIZE = 104448;
const MPTVERSION = 4;

function getData($filename) {
	if (!file_exists($filename)) {
		error("File doesn't exist");
	}
	$file = fopen($filename, "r");
	if (false === $file) {
		error("Could not open file");
	}
	$length = 40;
	$content = fread($file, $length);
	if (false === $content) {
		error("Failed to read file");
	}
	#uint32_t version; /**< which savegame version */
	#uint32_t soldiercount; /** the number of soldiers stored in this savegame */
	#char name[32]; /**< savefile name */
	#uint32_t xmlSize; /** needed, if we store compressed */
	$array = unpack("V1version/V1soldiercount/a32name", $content);
	fclose($file);
	return $array;
}

function error($message) {
	error_log($message);
	#$stdout = fopen('php://stdout', 'w');
	#fwrite($stdout, "$message\n");
	die($message);
}
