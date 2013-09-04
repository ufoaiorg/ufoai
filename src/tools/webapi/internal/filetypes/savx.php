<?php
include_once "filetype.php";

class SAVX extends FileType {
	public function __construct($filename) {
		#  4 - uint32_t version
		#  4 - uint32_t compressed
		#  4 - uint32_t subsystems
		# 52 - uint32_t dummy[13]
		# 16 - char gameVersion[16]
		# 32 - char name[32]
		# 32 - char gameDate[32]
		# 32 - char realDate[32]
		parent::__construct ( $filename, 176, "V1version/V1compressed/V1subsystems/V13dummy/a16gameversion/a32name/a32gamedate/a32realdate" );
	}

	public function getVersion() {
		return $_array ['version'];
	}

	public function isCompressed() {
		return $_array ['compressed'];
	}

	public function getSubsystems() {
		return $_array ['subsystems'];
	}

	public function getGameversion() {
		return $_array ['gameversion'];
	}

	public function getName() {
		return $_array ['name'];
	}

	public function getGamedate() {
		return $_array ['gamedate'];
	}

	public function getRealdate() {
		return $_array ['realdate'];
	}

	public function isValid () {
		return getVersion() === 4;
	}
}
