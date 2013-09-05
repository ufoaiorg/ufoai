<?php
include_once "filetype.php";

class UnknownFileType extends FileType {
	public function __construct($filename) {
		parent::__construct ( $filename, 0, "" );
	}

	public function isValid () {
		return false;
	}

	public function getName() {
		return "Unknown";
	}
}
