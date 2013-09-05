<?php
include "internal/webapi.php";

const FORMNAME = "cgame";

function main() {
	if (false === auth ()) {
		error ( "Not logged in", 403 );
	}
	$f = $_FILES [FORMNAME];
	if (! isset ( $f )) {
		error ( "No file attached", 400 );
	}
	$size = $f ['size'];
	$tmpname = $f ['tmp_name'];
	if ($size > MAXSIZE) {
		unlink ( $tmpname );
		error ( "max size hit", 422 );
	}
	$name = $f ['name'];
	$fs = new FileSystem ();
	$fs->upload ( $tmpname, $name, $size );
}

main ();
