<?php
include_once "filetypes/mpt.php";
include_once "filetypes/savx.php";
include_once "filetypes/unknown.php";

const MAXSIZE = 104448;
const MAX_FILE_PER_CGAME = 10;
const WEBAPIDIR = '/home/users/mattn/htdocs/ufoai/cgame/';

class FileSystem {
	public function isValid($filename) {
		return is_readable ( $filename );
	}

	/**
	 * @brief Get your own user directory
	 */
	public function getWebAPIDir() {
		return $this->getWebAPIUserDir(getUserId());
	}

	/**
	 * ##ROOT##/$cgame$/$userid$/$category$/$file$
	 */
	public function getWebAPIUserDir($userId) {
		return WEBAPIDIR . getRequestCGame() . '/' . intval($userId) . '/' . getRequestCategory() . '/';
	}

	public function getOwnFile() {
		return $this->getWebAPIDir () . getRequestFile ();
	}

	private function move($tmpfile, $targetfile) {
		$dir = $this->getWebAPIDir ();
		if (!mkdir($dir, 0777, true)) {
			error ( "could not create target directory" );
		}
		$uploadfile = $dir . $targetfile;
		if (! move_uploaded_file ( $tmpfile, $uploadfile )) {
			unlink ( $tmpfile );
			error ( "could not move the file" );
		}
		$file = $this->getFileType ( $targetfile );
		if (false === $file->isValid ()) {
			unlink ( $tmpfile );
			error ( "unknown file type: " . $name );
		}
	}

	public function getFileType($filename) {
		if ( false === $this->isValid ( $filename ) ) {
			return new UnknownFileType ();
		}

		$ext = pathinfo ( $filename, PATHINFO_EXTENSION );
		if ($ext == "mpt") {
			$file = new MPT ( $filename );
		} else if ($ext == "savx") {
			$file = new SAVX ( $filename );
		} else {
			$file = new UnknownFileType ();
		}
		return $file;
	}

	public function upload($tmpname, $name, $size) {
		if (true === $this->checkDuplicate ( $tmpname, $size )) {
			error ( "duplicate" );
		}
		$this->move ( $tmpname, $name );
	}

	public function listUserFiles($userId) {
		$dir = $this->getWebAPIUserDir ($userId) . '*.*';
		return glob ( $dir );
	}

	public function listOwnFiles() {
		return $this->listUserFiles ( getUserId() );
	}

	private function checkDuplicate($tmpfile, $size) {
		$dir = $this->getWebAPIDir ();
		if ( ! file_exists ( $dir ) and ! is_dir ( $dir ) )
			return false;
		$md5 = md5_file ( $tmpfile );
		$files = scandir ( $dir );
		foreach ( $files as $file ) {
			$fullfile = $dir . $file;
			if (is_dir ( $fullfile )) {
				continue;
			}
			$filesize = filesize ( $fullfile );
			if (false === $filesize) {
				error ( "could not get the filesize of a file " . $fullfile );
			}
			if ($filesize !== $size) {
				continue;
			}
			if (md5_file ( $fullfile ) === $md5) {
				return true;
			}
		}
		return false;
	}

	public function delete($filename) {
		if ($this->isValid ( $file ) !== true) {
			error ( "file is not readable" );
		}
		unlink ( $filename );
	}
}
