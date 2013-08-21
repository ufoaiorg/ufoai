<?php

define('SMF_SETTINGS', '/home/users/mattn/htdocs/ufoai/forum/Settings.php');

// Path to the api
require_once('smf_2_api.php');

function getUsername() {
	return $_REQUEST['username'];
}

function auth() {
	$u = getUsername();
	$p = $_REQUEST['password'];
	if (!isset($u) || !trim($u) || !isset($p) || !trim($p)) {
		return false;
	}
	return smfapi_authenticate(trim($u), trim($u), true);
}

