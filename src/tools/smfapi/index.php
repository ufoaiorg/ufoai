<?php

/*
 * Endpoint for the SMF 2 API
 *
 * This file will handle third-party requests to utilize the Small Machines
 * Forum user database, authentication methods or other content. Parameters
 * should be passed via GET or POST, and must be accompanied by a request
 * parameter, which specifies what action or data is being requested.
 *
 * @request			"user_authentication"
 * 	Specifies the action or data requested. For now it just supports one action,
 * 	authentication the user login details.
 *
 * @smf_username		Username on the forum
 *
 * @smf_password		Password on the forum
 *	Should be sent encrypted. See smfapi_authenticate() in smf_2_api.php.
 *
 * WARNING: the api supports retrieving a lot of information, maybe even some
 * private information. Specific functions should only be implemented on an
 * as-needed basis (ie - no abstract function calling should be set up unless
 * the api has been checked for potential data leakage).
 */

// Path to the SMF Settings.php file
// The api will attempt to find this itself, but it's search can take a while,
// so it's faster to set this here. See smf_2_api.php:574
define('SMF_SETTINGS', '');

// Path to the api
require_once('smf_2_api.php');

/*
 * Authenticate username/password
 *
 * @request "user_authentication"
 * @return 0 = not authenticated, 1 = authenticated
 */
if (isset($_REQUEST['request']) && $_REQUEST['request'] == 'user_authentication') {

	// Exit early if we're missing data
	if ((!isset($_REQUEST['username']) || !trim($_REQUEST['username'])) ||
			(!isset($_REQUEST['password']) || !trim($_REQUEST['password'])) {
		echo '0';
		exit();

	// Authenticate user data
	} else {

		// The third parameter indicates the password has been encrypted. It
		// will handle the encryption if passed a non-encrypted password (and
		// set to false), but I don't think it's safe to send an unencrypted
		// password with a GET request.
		if (smfapi_authenticate(trim($_REQUEST['username']), trim($_REQUEST['password']), true)) {
			echo '0';
		} else {
			echo '1';
		}
	}
}
