<?php
class FileType {
	protected $array;
	protected function __construct($filename, $length, $unpack) {
		if (! file_exists ( $filename )) {
			error ( "File doesn't exist", 404 );
		}
		$file = fopen ( $filename, "r" );
		if (false === $file) {
			error ( "Could not open file", 501 );
		}
		$content = fread ( $file, $length );
		if (false === $content) {
			error ( "Failed to read file", 502 );
		}
		$this->array = unpack ( $unpack, $content );
		fclose ( $file );
	}

	public function isValid () {
		return true;
	}

	public function getName () {
		return "";
	}
}
