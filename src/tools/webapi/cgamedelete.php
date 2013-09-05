<?php
include "internal/webapi.php";

function main() {
	if (false === auth ()) {
		error ( "Not logged in", 403 );
	}
	$fs = new FileSystem ();
	$file = $fs->getOwnFile ();
	$fs->delete ( $file );
}

main ();
