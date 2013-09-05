<?php
include_once "filetypes/mpt.php";
include_once "filetypes/savx.php";
include_once "filetypes/unknown.php";

const MAXSIZE = 1048576;
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
		return $this->getWebAPIUserDir ( getUserId () );
	}

	/**
	 * ##ROOT##/$cgame$/$userid$/$category$/$file$
	 */
	public function getWebAPIUserDir($userId) {
		return $this->getUserDir( $userId ) . getRequestCategory () . '/';
	}

	public function getUserDir($userId) {
		return WEBAPIDIR . getRequestCGame () . '/' . intval ( $userId ) . '/';
	}

	public function getOwnFile() {
		return $this->getWebAPIDir () . getRequestFile ();
	}

	private function createDir () {
		$dir = $this->getWebAPIUserDir (getUserId ());
		if ( is_dir ( $dir ) ) {
			return;
		}
		if (! mkdir ( $dir, 0777, true ) ) {
			error ( "could not create target directory: " . $dir, 404 );
		}
		$userdir = $this->getUserDir ( getUserId () );
		if (false === chmod ( $userdir, 0777 )) {
			error ( "could not change the mode of " . $userdir, 403 );
		}
		if (false === chmod ( $dir, 0777 )) {
			error ( "could not change the mode of " . $dir, 403 );
		}
	}

	private function move($tmpfile, $targetfile) {
		$this->createDir();
		$dir = $this->getWebAPIDir ();
		$uploadfile = $dir . $targetfile;
		if (! move_uploaded_file ( $tmpfile, $uploadfile )) {
			unlink ( $tmpfile );
			error ( "could not move the file", 406 );
		}
		chmod ( $uploadfile, 0666 );
		$file = $this->getFileType ( $uploadfile );
		if (false === $file->isValid ()) {
			unlink ( $uploadfile );
			error ( "unknown file type: " . $targetfile, 415 );
		}
	}

	public function getFileType($filename) {
		if (false === $this->isValid ( $filename )) {
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
		if ($size > MAXSIZE) {
			error ( "size limit exceeded", 507 );
		}
		$ext = pathinfo ( $name, PATHINFO_EXTENSION );
		if ($ext !== "mpt" && $ext !== "savx") {
			error ( "unsupported extension: " . $ext, 415 );
		}
		if (true === $this->checkDuplicate ( $tmpname, $size )) {
			error ( "duplicate for " . $name, 423 );
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
		if (! file_exists ( $dir ) && ! is_dir ( $dir ))
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
				error ( "could not get the filesize of a file " . $fullfile, 404 );
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
		if ($this->isValid ( $filename ) !== true) {
			error ( "file " . $filename . " is not readable", 404 );
		}
		if (false === unlink ( $filename )) {
			error ( "could not remove file: " . $filename, 404 );
		}
	}
}
