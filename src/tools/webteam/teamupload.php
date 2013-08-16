<?php
include "teamconfig.php";

function move($tmpfile) {
	$id = getFreeId();
	$uploadfile = TEAMDIR . "team" . $id . "." . FILEEXT;
	if (!move_uploaded_file($tmpfile, $uploadfile)) {
		unlink($tmpfile);
		error("could not move the file");
	}
}

function getFreeId() {
	// TODO:
	return 0;
}

function main() {
	$f = $_FILES[FORMNAME];
	if (!isset($f)) {
		error("No file attached");
	}
	$size = $f['size'];
	$tmpname = $f['tmp_name'];
	$name = $f['name'];
	$ext = pathinfo($name, PATHINFO_EXTENSION);
	if ($ext != FILEEXT) {
		error("invalid extension: " . $ext . " for " . $tmpfile);
	}
	$data = getData($tmpname);
	if ($data["version"] != MPTVERSION) {
		error("invalid mpt header");
	}
	if ($size > MAXSIZE) {
		unlink($tmpname);
		error("max size hit");
	}
	move($tmpname);
}

main();
