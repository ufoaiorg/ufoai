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

function checkDuplicate($tmpfile, $size) {
	$md5 = md5_file($tmpfile);
	$files = scandir(TEAMDIR);
	foreach ($files as $file) {
		if (is_dir($file)) {
			continue;
		}
		$filesize = filesize($file);
		if (false === $filesize) {
			error("could not get the filesize of a file");
		}
		if ($filesize !== $size) {
			continue;
		}
		if (md5_file($file) === $md5) {
			return true;
		}
	}

	return false;
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
	if (true === checkDuplicate($tmpname, $size)) {
		error("duplicate");
	}
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
