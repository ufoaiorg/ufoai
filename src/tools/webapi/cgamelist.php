<?php
include "internal/webapi.php";

$fs = new FileSystem ();
if (hasRequestUserId ())
	$userId = getRequestUserId ();
else
	$userId = getUserId();

$files = $fs->listUserFiles ( $userId );
$category = getRequestCategory();

echo "{";
foreach ( $files as $file ) {
	$type = $fs->getFileType($file);
	if (false === $type->isValid ()) {
		continue;
	}
	$name = $type->getName();
	echo "{userid \"" . $userId . "\" file \"" . basename($file) . "\" name \"" . $name . "\" category \"" . $category . "\"}";
}
echo "}";
