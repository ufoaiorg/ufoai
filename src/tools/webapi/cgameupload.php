<?php
include "internal/webapi.php";

const FORMNAME = "cgame";

function main() {
	if (false === auth ()) {
		error ( "Not logged in" );
	}
	$f = $_FILES [FORMNAME];
	if (! isset ( $f )) {
		error ( "No file attached" );
	}
	$size = $f ['size'];
	$tmpname = $f ['tmp_name'];
	if ($size > MAXSIZE) {
		unlink ( $tmpname );
		error ( "max size hit" );
	}
	$name = $f ['name'];
	$fs = new FileSystem ();
	$fs->upload ( $tmpname, $name, $size );
}

main ();
