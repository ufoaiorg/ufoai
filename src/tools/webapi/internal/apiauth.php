<?php
define ( 'SMF_SETTINGS', '/home/users/mattn/htdocs/ufoai/forum/Settings.php' );

// Path to the api
require_once ('smf_2_api.php');

function getUserId() {
	global $user_info;
	return $user_info ["id"];
}

function auth() {
	$u = getUsername ();
	$p = getPassword ();
	if (! trim ( $u ) || ! trim ( $p )) {
		return false;
	}
	return smfapi_authenticate ( trim ( $u ), trim ( $p ), true );
}
