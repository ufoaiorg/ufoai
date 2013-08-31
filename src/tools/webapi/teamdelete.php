<?php
include "internal/teamconfig.php";
include "internal/auth.php";

function getTeamRequestId() {
	return intval($_REQUEST['teamid']);
}

function main() {
	if (false === auth()) {
		error("Not logged in");
	}
	$teamfile = getDir() . getUserId() . "-team" . sprintf("%02d", getTeamRequestId()) . "." . FILEEXT;
	if (is_readable($teamfile) !== true) {
		error("file is not readable");
	}
	unlink($teamfile);
}

main();
