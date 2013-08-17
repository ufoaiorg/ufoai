<?php
include "teamconfig.php";

function move($tmpfile) {
	$id = getFreeId();
	$uploadfile = getDir() . "team" . $id . "." . FILEEXT;
	if (!move_uploaded_file($tmpfile, $uploadfile)) {
		unlink($tmpfile);
		error("could not move the file");
	}
}

function checkDuplicate($tmpfile, $size) {
	$md5 = md5_file($tmpfile);
	$files = scandir(getDir());
	foreach ($files as $file) {
		$fullfile = getDir() . $file;
		if (is_dir($fullfile)) {
			continue;
		}
		$filesize = filesize($fullfile);
		if (false === $filesize) {
			error("could not get the filesize of a file $fullfile");
		}
		if ($filesize !== $size) {
			continue;
		}
		if (md5_file($fullfile) === $md5) {
			return true;
		}
	}

	return false;
}

function getFreeId() {
	$files = scandir(getDir(), 1);
	$file = reset($files);
	if (false === $file)
		return "0000000000";
	return getId($file);
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
