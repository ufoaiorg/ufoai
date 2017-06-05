<?php
include_once "filetype.php";

class MPT extends FileType {
	public function __construct($filename) {
		// 4 - uint32_t version
		// 4 - uint32_t soldiercount
		// 32 - char name[32]
		parent::__construct ( $filename, 40, "V1version/V1soldiercount/Z32name" );
	}

	public function getVersion() {
		return intval ( $this->array ['version'] );
	}

	public function getSoldierCount() {
		return intval ( $this->array ['soldiercount'] );
	}

	public function getName() {
		return $this->array ['name'];
	}

	public function isValid () {
		return $this->getVersion () == 4;
	}
}
