<?php

define('SMF_SETTINGS', '/home/users/mattn/htdocs/ufoai/forum/Settings.php');

// Path to the api
require_once('smf_2_api.php');

function auth() {
	if ((!isset($_REQUEST['username']) || !trim($_REQUEST['username'])) ||
		(!isset($_REQUEST['password']) || !trim($_REQUEST['password']))) {
		return false;
	}
	return smfapi_authenticate(trim($_REQUEST['username']), trim($_REQUEST['password']), true);
}

