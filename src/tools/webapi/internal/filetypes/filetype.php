<?php
class FileType {
	protected $_array;
	protected function __construct($filename, $length, $unpack) {
		if (! file_exists ( $filename )) {
			error ( "File doesn't exist" );
		}
		$file = fopen ( $filename, "r" );
		if (false === $file) {
			error ( "Could not open file" );
		}
		$content = fread ( $file, $length );
		if (false === $content) {
			error ( "Failed to read file" );
		}
		$_array = unpack ( $unpack, $content );
		fclose ( $file );
	}

	public function isValid () {
		return true;
	}
}
