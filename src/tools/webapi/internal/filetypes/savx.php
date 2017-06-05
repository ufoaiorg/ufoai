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
		parent::__construct ( $filename, 176, "V1version/V1compressed/V1subsystems/V13dummy/Z16gameversion/Z32name/Z32gamedate/Z32realdate" );
	}

	public function getVersion() {
		return intval ( $this->array ['version'] );
	}

	public function isCompressed() {
		return intval ( $this->array ['compressed'] );
	}

	public function getSubsystems() {
		return intval ( $this->array ['subsystems'] );
	}

	public function getGameversion() {
		return $this->array ['gameversion'];
	}

	public function getName() {
		return $this->array ['name'];
	}

	public function getGamedate() {
		return $this->array ['gamedate'];
	}

	public function getRealdate() {
		return $this->array ['realdate'];
	}

	public function isValid () {
		return $this->getVersion () == 4;
	}
}
