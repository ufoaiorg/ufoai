<?php
include "internal/webapi.php";

function main() {
	if (false === auth ()) {
		error ( "Not logged in" );
	}
	$fs = new FileSystem ();
	$file = $fs->getOwnFile ();
	$fs->remove ( $file );
}

main ();
