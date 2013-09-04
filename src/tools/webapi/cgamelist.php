<?php
include "internal/webapi.php";

$fs = new FileSystem ();
if (hasRequestUserId ())
	$userId = getRequestUserId ();
else
	$userId = getUserId();

$files = $fs->listUserFiles ( $userId );

echo "{";
foreach ( $files as $file ) {
	echo "{userid \"" . $userId . "\" file \"" . $file . "\"}";
}
echo "}";
