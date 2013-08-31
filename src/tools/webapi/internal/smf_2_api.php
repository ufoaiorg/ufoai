<?php

/**
 * Simple Machines Forum(SMF) API for SMF 2.0
 *
 * Use this to integrate your SMF version 2.0 forum with 3rd party software
 * If you need help using this script or integrating your forum with other
 * software, feel free to contact andre@r2bconcepts.com
 *
 * @package   SMF 2.0 API
 * @author    Simple Machines http://www.simplemachines.org
 * @author    Andre Nickatina <andre@r2bconcepts.com>
 * @copyright 2011 Simple Machines
 * @link      http://www.simplemachines.org Simple Machines
 * @link      http://www.r2bconcepts.com Red2Black Concepts
 * @license   http://www.simplemachines.org/about/smf/license.php BSD
 * @version   0.1.2
 *
 * NOTICE OF LICENSE
 ***********************************************************************************
 * This file, and ONLY this file is released under the terms of the BSD License.   *
 *                                                                                 *
 * Redistribution and use in source and binary forms, with or without              *
 * modification, are permitted provided that the following conditions are met:     *
 *                                                                                 *
 * Redistributions of source code must retain the above copyright notice, this     *
 * list of conditions and the following disclaimer.                                *
 * Redistributions in binary form must reproduce the above copyright notice, this  *
 * list of conditions and the following disclaimer in the documentation and/or     *
 * other materials provided with the distribution.                                 *
 * Neither the name of Simple Machines LLC nor the names of its contributors may   *
 * be used to endorse or promote products derived from this software without       *
 * specific prior written permission.                                              *
 *                                                                                 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"     *
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE       *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE      *
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE        *
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR             *
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE *
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)     *
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT      *
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT   *
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. *
 **********************************************************************************/

/*
    This file includes functions that may help integration with other scripts
	and programs, such as portals. It is independent of SMF, and meant to run
	without disturbing your script. It defines several functions, most of
	which start with the smfapi_ prefix. These are:


    array  smfapi_getUserByEmail(string $email)
        - returns all user info from the db in an array

    array  smfapi_getUserById(int $id)
        - returns all user info from the db in an array

    array  smfapi_getUserByUsername(string $username)
        - returns all user info from the db in an array

    array  smfapi_getUserData(mixed $identifier)
        - returns all user info from the db in an array
        - will accept email address, username or member id

    bool   smfapi_login(mixed $identifier, int $cookieLength)
        - sets cookie and session for user specified
        - will accept email address, username or member id
        - does no authentication; do that before calling this

    bool   smfapi_authenticate(mixed $username, string $password, bool $encrypted)
        - authenticates a username/password combo
        - will accept email address, username or member id

    bool   smfapi_logout(string $username)
        - logs the specified user out
        - will accept email address, username or member id

    bool   smfapi_deleteMembers(int || int array $users)
        - deletes member(s) by their int member id
        - will return true unless $users empty
        - will accept email address, username or member id or a mixed array

    int    smfapi_registerMember(array $regOptions)
        - register a member
        - $regOptions will contain the variables from the db
        - dump out the results of smfapi_getUserData($user) to see them all
        - required variables are: 'member_name' (unique), 'email' (unique), 'password'

    bool   smfapi_logError(string $error_message, string $error_type, string $file, int $line)
        - logs an error message to the smf error log
        - $error_type will be one of the following: 'general', 'critical', 'database', 'undefined_vars', 'user', 'template' or 'debug'
        - just use __FILE__ and __LINE__ as $file and $line unless you have other ambitions

    true   smfapi_reloadSettings()
        - loads the $modSettings array
        - adds the following functions to the $smcFunc array:
             'entity_fix', 'htmlspecialchars', 'htmltrim', 'strlen', 'strpos', 'substr', 'strtolower', strtoupper', 'truncate', 'ucfirst' and 'ucwords'

    true   smfapi_loadUserSettings(mixed $identifier)
        - loads the $user_info array for user or guest
        - will accept email address, username or member id
        - if member data not found, will try cookie then session

    true   smfapi_loadSession()
        - starts the session

    *Session functions*
    true   smfapi_sessionOpen()
    true   smfapi_sessionClose()
    bool   smfapi_sessionRead()
    bool   smfapi_sessionWrite()
    bool   smfapi_sessionDestroy()
    mixed  smfapi_sessionGC()

    bool   smfapi_loadDatabase()
        - loads the db connection
        - adds the following fuctions to the $smcFunc array:
            'db_query', 'db_quote', 'db_fetch_assoc', 'db_fetch_row', 'db_free_result', 'db_insert', 'db_insert_id', 'db_num_rows',
            'db_data_seek', 'db_num_fields', 'db_escape_string', 'db_unescape_string', 'db_server_info', 'db_affected_rows',
            'db_transaction', 'db_error', 'db_select_db', 'db_title', 'db_sybase', 'db_case_sensitive' and 'db_escape_wildcard_string'

    void   smfapi_cachePutData(string $key, mixed $value, int $ttl)
        - puts data in the cache

    mixed  smfapi_cacheGetData(string $key, int $ttl)
        - gets data from the cache

    bool   smfapi_updateMemberData(mixed $member, array $data)
        - change member data (email, password, name, etc.)
        - will accept email address, username or member id
        - data will be an associative array ('email_address' => 'newemail@address.com') etc.

    true   smfapi_smfSeedGenerator()
        - generates random seed

    bool   smfapi_updateSettings(array $changeArray, bool $update)
        - updates settings in $modSettings array and puts them in db
        - called from smfapi_updateStats(), smfapi_deleteMessages() and smfapi_smfSeedGenerator()

    true   smfapi_setLoginCookie(int $cookie_length, int $id, string $password)
        - called by smfapi_login() to set the cookie

    array  smfapi_urlParts(bool $local, bool $global)
        - called by smfapi_setLoginCookie() to parse the url

    bool   smfapi_updateStats(string $type, int $parameter1, string $parameter2)
        - update forum member stats
        - called when registering or deleting a member

    string smfapi_unHtmlspecialchars(string $string)
        - fixes strings with special characters
        - called when encrypting the password for checking

    bool   smfapi_deleteMessages(array $personal_messages, string $folder, int || array $owner)
        - called by smfapi_deleteMembers()

    string smfapi_generateValidationCode()
        - used to generate a 10 char alpha validation code during registration

    bool   smfapi_isOnline(mixed $username)
        - check if a user is online
        - will accept email address, username or member id

    bool   smfapi_logOnline(mixed $username)
        - log a user online

    array  smfapi_getMatchingFile(array $files, string $search)
        - find a file from an array
        - used to find Settings.php in case this script is not with it

    array  smfapi_getDirectoryContents(string $directory, array $exempt, array $files)
        - gets the contents of a directory and all subdirectories
        - called by smfapi_getMatchingFile

    ---------------------------------------------------------------------------
	It also defines the following important variables:

	$smcFunc => Array
    (
        [db_query] => smf_db_query
        [db_quote] => smf_db_quote
        [db_fetch_assoc] => mysql_fetch_assoc
        [db_fetch_row] => mysql_fetch_row
        [db_free_result] => mysql_free_result
        [db_insert] => smf_db_insert
        [db_insert_id] => smf_db_insert_id
        [db_num_rows] => mysql_num_rows
        [db_data_seek] => mysql_data_seek
        [db_num_fields] => mysql_num_fields
        [db_escape_string] => addslashes
        [db_unescape_string] => stripslashes
        [db_server_info] => mysql_get_server_info
        [db_affected_rows] => smf_db_affected_rows
        [db_transaction] => smf_db_transaction
        [db_error] => mysql_error
        [db_select_db] => mysql_select_db
        [db_title] =>
        [db_sybase] =>
        [db_case_sensitive] =>
        [db_escape_wildcard_string] => smf_db_escape_wildcard_string
        [entity_fix] =>
        [htmlspecialchars] =>
        [htmltrim] =>
        [strlen] =>
        [strpos] =>
        [substr] =>
        [strtolower] =>
        [strtoupper] =>
        [truncate] =>
        [ucfirst] =>
        [ucwords] =>
    )

	$modSettings => Array
    (
        [smfVersion] =>
        [news] =>
        [compactTopicPagesContiguous] =>
        [compactTopicPagesEnable] =>
        [enableStickyTopics] =>
        [todayMod] =>
        [karmaMode] =>
        [karmaTimeRestrictAdmins] =>
        [enablePreviousNext] =>
        [pollMode] =>
        [enableVBStyleLogin] =>
        [enableCompressedOutput] =>
        [karmaWaitTime] =>
        [karmaMinPosts] =>
        [karmaLabel] =>
        [karmaSmiteLabel] =>
        [karmaApplaudLabel] =>
        [attachmentSizeLimit] =>
        [attachmentPostLimit] =>
        [attachmentNumPerPostLimit] =>
        [attachmentDirSizeLimit] =>
        [attachmentUploadDir] =>
        [attachmentExtensions] =>
        [attachmentCheckExtensions] =>
        [attachmentShowImages] =>
        [attachmentEnable] =>
        [attachmentEncryptFilenames] =>
        [attachmentThumbnails] =>
        [attachmentThumbWidth] =>
        [attachmentThumbHeight] =>
        [censorIgnoreCase] =>
        [mostOnline] =>
        [mostOnlineToday] =>
        [mostDate] =>
        [allow_disableAnnounce] =>
        [trackStats] =>
        [userLanguage] =>
        [titlesEnable] =>
        [topicSummaryPosts] =>
        [enableErrorLogging] =>
        [max_image_width] =>
        [max_image_height] =>
        [onlineEnable] =>
        [cal_enabled] =>
        [cal_maxyear] =>
        [cal_minyear] =>
        [cal_daysaslink] =>
        [cal_defaultboard] =>
        [cal_showholidays] =>
        [cal_showbdays] =>
        [cal_showevents] =>
        [cal_showweeknum] =>
        [cal_maxspan] =>
        [smtp_host] =>
        [smtp_port] =>
        [smtp_username] =>
        [smtp_password] =>
        [mail_type] =>
        [timeLoadPageEnable] =>
        [totalMembers] =>
        [totalTopics] =>
        [totalMessages] =>
        [simpleSearch] =>
        [censor_vulgar] =>
        [censor_proper] =>
        [enablePostHTML] =>
        [theme_allow] =>
        [theme_default] =>
        [theme_guests] =>
        [enableEmbeddedFlash] =>
        [xmlnews_enable] =>
        [xmlnews_maxlen] =>
        [hotTopicPosts] =>
        [hotTopicVeryPosts] =>
        [registration_method] =>
        [send_validation_onChange] =>
        [send_welcomeEmail] =>
        [allow_editDisplayName] =>
        [allow_hideOnline] =>
        [guest_hideContacts] =>
        [spamWaitTime] =>
        [pm_spam_settings] =>
        [reserveWord] =>
        [reserveCase] =>
        [reserveUser] =>
        [reserveName] =>
        [reserveNames] =>
        [autoLinkUrls] =>
        [banLastUpdated] =>
        [smileys_dir] =>
        [smileys_url] =>
        [avatar_directory] =>
        [avatar_url] =>
        [avatar_max_height_external] =>
        [avatar_max_width_external] =>
        [avatar_action_too_large] =>
        [avatar_max_height_upload] =>
        [avatar_max_width_upload] =>
        [avatar_resize_upload] =>
        [avatar_download_png] =>
        [failed_login_threshold] =>
        [oldTopicDays] =>
        [edit_wait_time] =>
        [edit_disable_time] =>
        [autoFixDatabase] =>
        [allow_guestAccess] =>
        [time_format] =>
        [number_format] =>
        [enableBBC] =>
        [max_messageLength] =>
        [signature_settings] =>
        [autoOptMaxOnline] =>
        [defaultMaxMessages] =>
        [defaultMaxTopics] =>
        [defaultMaxMembers] =>
        [enableParticipation] =>
        [recycle_enable] =>
        [recycle_board] =>
        [maxMsgID] =>
        [enableAllMessages] =>
        [fixLongWords] =>
        [knownThemes] =>
        [who_enabled] =>
        [time_offset] =>
        [cookieTime] =>
        [lastActive] =>
        [smiley_sets_known] =>
        [smiley_sets_names] =>
        [smiley_sets_default] =>
        [cal_days_for_index] =>
        [requireAgreement] =>
        [unapprovedMembers] =>
        [default_personal_text] =>
        [package_make_backups] =>
        [databaseSession_enable] =>
        [databaseSession_loose] =>
        [databaseSession_lifetime] =>
        [search_cache_size] =>
        [search_results_per_page] =>
        [search_weight_frequency] =>
        [search_weight_age] =>
        [search_weight_length] =>
        [search_weight_subject] =>
        [search_weight_first_message] =>
        [search_max_results] =>
        [search_floodcontrol_time] =>
        [permission_enable_deny] =>
        [permission_enable_postgroups] =>
        [mail_next_send] =>
        [mail_recent] =>
        [settings_updated] =>
        [next_task_time] =>
        [warning_settings] =>
        [warning_watch] =>
        [warning_moderate] =>
        [warning_mute] =>
        [admin_features] =>
        [last_mod_report_action] =>
        [pruningOptions] =>
        [cache_enable] =>
        [reg_verification] =>
        [visual_verification_type] =>
        [enable_buddylist] =>
        [birthday_email] =>
        [dont_repeat_theme_core] =>
        [dont_repeat_smileys_20] =>
        [dont_repeat_buddylists] =>
        [attachment_image_reencode] =>
        [attachment_image_paranoid] =>
        [attachment_thumb_png] =>
        [avatar_reencode] =>
        [avatar_paranoid] =>
        [global_character_set] =>
        [localCookies] =>
        [default_timezone] =>
        [memberlist_updated] =>
        [latestMember] =>
        [latestRealName] =>
        [rand_seed] =>
        [mostOnlineUpdated] =>
    )

    $user_info => Array
    (
        [groups] => Array
            (
                [0] =>
                [1] =>
            )

        [possibly_robot] =>
        [id] =>
        [username] =>
        [name] =>
        [email] =>
        [passwd] =>
        [language] =>
        [is_guest] =>
        [is_admin] =>
        [theme] =>
        [last_login] =>
        [ip] =>
        [ip2] =>
        [posts] =>
        [time_format] =>
        [time_offset] =>
        [avatar] => Array
            (
                [url] =>
                [filename] =>
                [custom_dir] =>
                [id_attach] =>
            )

        [smiley_set] =>
        [messages] =>
        [unread_messages] =>
        [total_time_logged_in] =>
        [buddies] => Array
            (
            )

        [ignoreboards] => Array
            (
            )

        [ignoreusers] => Array
            (
            )

        [warning] =>
        [permissions] => Array
            (
            )

    )

    For even *more* member data use the function smfapi_getUserData()
    It will return an array with the following:

    $userdata => Array
    (
        [id_member] =>
        [member_name] =>
        [date_registered] =>
        [posts] =>
        [id_group] =>
        [lngfile] =>
        [last_login] =>
        [real_name] =>
        [instant_messages] =>
        [unread_messages] =>
        [new_pm] =>
        [buddy_list] =>
        [pm_ignore_list] =>
        [pm_prefs] =>
        [mod_prefs] =>
        [message_labels] =>
        [passwd] =>
        [openid_uri] =>
        [email_address] =>
        [personal_text] =>
        [gender] =>
        [birthdate] =>
        [website_title] =>
        [website_url] =>
        [location] =>
        [icq] =>
        [aim] =>
        [yim] =>
        [msn] =>
        [hide_email] =>
        [show_online] =>
        [time_format] =>
        [signature] =>
        [time_offset] =>
        [avatar] =>
        [pm_email_notify] =>
        [karma_bad] =>
        [karma_good] =>
        [usertitle] =>
        [notify_announcements] =>
        [notify_regularity] =>
        [notify_send_body] =>
        [notify_types] =>
        [member_ip] =>
        [member_ip2] =>
        [secret_question] =>
        [secret_answer] =>
        [id_theme] =>
        [is_activated] =>
        [validation_code] =>
        [id_msg_last_visit] =>
        [additional_groups] =>
        [smiley_set] =>
        [id_post_group] =>
        [total_time_logged_in] =>
        [password_salt] =>
        [ignore_boards] =>
        [warning] =>
        [passwd_flood] =>
        [pm_receive_from] =>
    )

*/

// don't do anything if SMF is already loaded
if (defined('SMF'))
	return true;

define('SMF', 'API');

// we're going to want a few globals... these are all set later
// set from this script
global $time_start, $scripturl, $context, $settings_path;

// set from Settings.php
global $maintenance, $mtitle, $mmessage, $mbname, $language, $boardurl;
global $webmaster_email, $cookiename, $db_type, $db_server, $db_name, $db_user;
global $db_passwd, $db_prefix, $db_persist, $db_error_send, $boarddir, $sourcedir;
global $cachedir, $db_last_error, $db_character_set;

// set from smfapi_loadDatabase()
global $db_connection, $smcFunc;

// set from smfapi_reloadSettings()
global $modSettings;

// set from smfapi_loadSession()
global $sc;

// set from smfapi_loadUserSettings()
global $user_info;

// turn off magic quotes
if (function_exists('set_magic_quotes_runtime')) {
    // remember the current configuration so it can be set back
    $api_magic_quotes_runtime = function_exists('get_magic_quotes_gpc') && get_magic_quotes_runtime();
	@set_magic_quotes_runtime(0);
}

$time_start = microtime();

// just being safe...
foreach (array('db_character_set', 'cachedir') as $variable) {
	if (isset($GLOBALS[$variable])) {
		unset($GLOBALS[$variable]);
    }
}

$saveFile = dirname(__FILE__) . '/smfapi_settings.txt';
if (file_exists($saveFile)) {
    $settings_path = base64_decode(file_get_contents($saveFile));
}

// specify the settings path here if it's not in smf root and you want to speed things up
$settings_path = SMF_SETTINGS;

// get the forum's settings for database and file paths
if (file_exists(dirname(__FILE__) . '/Settings.php')) {
    require_once(dirname(__FILE__) . '/Settings.php');
} elseif (isset($settings_path)) {
    require_once($settings_path);
} else {
    $directory = $_SERVER['DOCUMENT_ROOT'] . '/';
    $exempt = array('.', '..');
    $files = smfapi_getDirectoryContents($directory, $exempt);
    $matches = smfapi_getMatchingFile($files, 'Settings.php');

    // we're going to search for it...
	@set_time_limit(600);
	// try to get some more memory
	if (@ini_get('memory_limit') < 128) {
		@ini_set('memory_limit', '128M');
    }

    if (1 == count($matches)) {
        require_once($matches[0]);
        $settings_path = $matches[0];
        file_put_contents($saveFile, base64_encode($settings_path));
    } elseif (1 < count($matches)) {
        $matches = smfapi_getMatchingFile($files, 'Settings_bak.php');
        $matches[0] = str_replace('_bak.php', '.php', $matches[0]);
        require_once($matches[0]);
        $settings_path = $matches[0];
        file_put_contents($saveFile, base64_encode($settings_path));
    } else {
        return false;
    }
}

$scripturl = $boardurl . '/index.php';

// make absolutely sure the cache directory is defined
if ((empty($cachedir) || !file_exists($cachedir)) && file_exists($boarddir . '/cache')) {
	$cachedir = $boarddir . '/cache';
}

// don't do john didley if the forum's been shut down competely
if (2 == $maintenance) {
	return;
}

// fix for using the current directory as a path
if (substr($sourcedir, 0, 1) == '.' && substr($sourcedir, 1, 1) != '.') {
	$sourcedir = dirname(__FILE__) . substr($sourcedir, 1);
}

// using a pre 5.1 php version?
if (-1 == @version_compare(PHP_VERSION, '5.1')) {
    //safe to include, will check if functions exist before declaring
	require_once($sourcedir . '/Subs-Compat.php');
}

// create a variable to store some SMF specific functions in
$smcFunc = array();

// we won't put anything in this
$context = array();

// initate the database connection and define some database functions to use
smfapi_loadDatabase();

// load settings
smfapi_reloadSettings();

// create random seed if it's not already created
if (empty($modSettings['rand_seed']) || mt_rand(1, 250) == 69) {
	smfapi_smfSeedGenerator();
}

// start the session if there isn't one already...
smfapi_loadSession();


// load the user and their cookie, as well as their settings.
smfapi_loadUserSettings();

/**
 * Gets the user's info from their email address
 *
 * Will take the users email address and return an array containing all the
 * user's information in the db. Will return false on failure
 *
 * @param  string $email the user's email address
 * @return array $results containing the user info || bool false
 * @since  0.1.0
 */
function smfapi_getUserByEmail($email='')
{
    global $smcFunc;

    if ('' == $email || !is_string($email) || 2 > count(explode('@', $email))) {
        return false;
    }

    $request = $smcFunc['db_query']('', '
			SELECT *
			FROM {db_prefix}members
			WHERE email_address = {string:email_address}
			LIMIT 1',
			array(
				'email_address' => $email,
			)
		);
	$results = $smcFunc['db_fetch_assoc']($request);
	$smcFunc['db_free_result']($request);

    if (empty($results)) {
        return false;
	} else {
	    // return all the results.
	    return $results;
    }
}

/**
 * Gets the user's info from their member id
 *
 * Will take the users member id and return an array containing all the
 * user's information in the db. Will return false on failure
 *
 * @param  int $id the user's member id
 * @return array $results containing the user info || bool false
 * @since  0.1.0
 */
function smfapi_getUserById($id='')
{
    global $smcFunc;

    if ('' == $id || !is_numeric($id)) {
        return false;
    } else{
        $id += 0;
        if (!is_int($id)) {
            return false;
        }
    }

    $request = $smcFunc['db_query']('', '
			SELECT *
			FROM {db_prefix}members
			WHERE id_member = {int:id_member}
			LIMIT 1',
			array(
				'id_member' => $id,
			)
		);
	$results = $smcFunc['db_fetch_assoc']($request);
	$smcFunc['db_free_result']($request);

    if (empty($results)) {
        return false;
	} else {
	    // return all the results.
	    return $results;
    }
}

/**
 * Gets the user's info from their member name (username)
 *
 * Will take the users member name and return an array containing all the
 * user's information in the db. Will return false on failure
 *
 * @param  string $username the user's member name
 * @return array $results containing the user info || bool false
 * @since  0.1.0
 */
function smfapi_getUserByUsername($username='')
{
    global $smcFunc;

    if ('' == $username || !is_string($username)) {
        return false;
    }

    $request = $smcFunc['db_query']('', '
			SELECT *
			FROM {db_prefix}members
			WHERE member_name = {string:member_name}
			LIMIT 1',
			array(
				'member_name' => $username,
			)
		);
	$results = $smcFunc['db_fetch_assoc']($request);
	$smcFunc['db_free_result']($request);

    if (empty($results)) {
        return false;
	} else {
	    // return all the results.
	    return $results;
    }
}

/**
 * Gets the user's info
 *
 * Will take the users email, username or member id and return their data
 *
 * @param  int || string $username the user's email address username or member id
 * @return array $results containing the user info || bool false
 * @since  0.1.0
 */
function smfapi_getUserData($username='')
{
    if ('' == $username) {
        return false;
    }

    $user_data = array();

    // we'll try id || email, then username
    if (is_numeric($username)) {
        // number is most likely a member id
        $user_data = smfapi_getUserById($username);
    } else {
        // the email can't be an int
        $user_data = smfapi_getUserByEmail($username);
    }

    if (!$user_data) {
        $user_data = smfapi_getUserByUsername($username);
    }

    if (empty($user_data)) {
        return false;
    } else {
        return $user_data;
    }
}

/**
 * Logs the user in by setting the session cookie
 *
 * Be sure you've already authenticated the username/password
 * using smfapi_authenticate() or some other means because
 * this function WILL set the correct session cookie for the
 * user you specify and they WILL be logged in
 *
 * @param  string $username (or int member id or string email. We're not picky)
 * @param  int $cookieLength length to set the cookie for (in minutes)
 * @return bool whether the login cookie was set or not
 * @since  0.1.0
 */
function smfapi_login($username='', $cookieLength=525600)
{
    global $scripturl, $user_info, $user_settings, $smcFunc;
	global $cookiename, $maintenance, $modSettings, $sc, $sourcedir;

    if (1 == $maintenance || '' == $username) {
	    return false;
    }

    $user_data = smfapi_getUserData($username);

    if (!$user_data) {
        return false;
    }

	// cookie set, session too
	smfapi_setLoginCookie(60 * $cookieLength, $user_data['id_member'], sha1($user_data['passwd']
                   . $user_data['password_salt']));

	// you've logged in, haven't you?
	smfapi_updateMemberData($user_data['id_member'], array('last_login' => time(), 'member_ip' => $user_info['ip']));

	// get rid of the online entry for that old guest....
	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_online
		WHERE session = {string:session}',
		array(
			'session' => 'ip' . $user_info['ip'],
		)
	);

    smfapi_loadUserSettings();

	return true;
}

/**
 * Will authenticate the username/password combo
 *
 * Use this before setting the cookie to check if the username password are correct.
 *
 * @param  mixed $username the user's member name, email or member id
 * @param  string $password the password plaintext or encrypted in any of several
 *         methods including smf's method: sha1(strtolower($username) . $password)
 * @param  bool $encrypted whether the password is encrypted or not. If you get
           this wrong we'll figure it out anyways, just saves some work if it's right
 * @return bool whether the user is authenticated or not
 * @since  0.1.0
 */
function smfapi_authenticate($username='', $password='', $encrypted=true)
{

    global $scripturl, $user_info, $user_settings, $smcFunc;
	global $cookiename, $modSettings, $sc, $sourcedir;

    if ('' == $username || '' == $password) {
        return false;
    }

    // just in case they used the email or member id...
    $data = smfapi_getUserData($username);
    if (empty($data)) {
        return false;
    } else {
        $username = $data['member_name'];
    }

    // load the data up!
	$request = $smcFunc['db_query']('', '
		SELECT passwd, id_member, id_group, lngfile, is_activated, email_address, additional_groups, member_name, password_salt,
			openid_uri, passwd_flood
		FROM {db_prefix}members
		WHERE ' . ($smcFunc['db_case_sensitive'] ? 'LOWER(member_name) = LOWER({string:user_name})' : 'member_name = {string:user_name}') . '
		LIMIT 1',
		array(
			'user_name' => $smcFunc['db_case_sensitive'] ? strtolower($username) : $username,
		)
	);
    // no user data found... invalid username
	if ($smcFunc['db_num_rows']($request) == 0) {
        return false;
	}

    $user_settings = $smcFunc['db_fetch_assoc']($request);
	$smcFunc['db_free_result']($request);

	if (40 != strlen($user_settings['passwd'])) {
        // invalid hash in the db
        return false;
	}

    // if it's not encrypted, do it now
	if (!$encrypted) {
        $sha_passwd = sha1(strtolower($user_settings['member_name'])
                      . smfapi_unHtmlspecialchars($password));
    } else {
        $sha_passwd = $password;
	}

    // if they match the password/hash is correct
    if ($user_settings['passwd'] == $sha_passwd) {
        $user_info["id"] = $user_settings['id_member'];
        return true;
    } else {
        // try other hashing schemes
        $other_passwords = array();

        // in case they sent the encrypted password into this as unencrypted
        $other_passwords[] = $password;

		// none of the below cases will be used most of the time
        // (because the salt is normally set)
		if ('' == $user_settings['password_salt']) {
			// YaBB SE, Discus, MD5 (used a lot), SHA-1 (used some), SMF 1.0.x,
            // IkonBoard, and none at all
			$other_passwords[] = crypt($password, substr($password, 0, 2));
			$other_passwords[] = crypt($password, substr($user_settings['passwd'], 0, 2));
			$other_passwords[] = md5($password);
			$other_passwords[] = sha1($password);
			$other_passwords[] = md5_hmac($password, strtolower($user_settings['member_name']));
			$other_passwords[] = md5($password . strtolower($user_settings['member_name']));
			$other_passwords[] = md5(md5($password));
			$other_passwords[] = $password;

			// this one is a strange one... MyPHP, crypt() on the MD5 hash
			$other_passwords[] = crypt(md5($password), md5($password));

			// Snitz style - SHA-256.  Technically, this is a downgrade, but most PHP
            // configurations don't support sha256 anyway.
			if (strlen($user_settings['passwd']) == 64
                && function_exists('mhash') && defined('MHASH_SHA256')) {
				$other_passwords[] = bin2hex(mhash(MHASH_SHA256, $password));
            }

			// phpBB3 users new hashing.  We now support it as well ;)
			$other_passwords[] = phpBB3_password_check($password, $user_settings['passwd']);

			// APBoard 2 login method
			$other_passwords[] = md5(crypt($password, 'CRYPT_MD5'));
		}
		// the hash should be 40 if it's SHA-1, so we're safe with more here too
		elseif (strlen($user_settings['passwd']) == 32) {
			// vBulletin 3 style hashing?  Let's welcome them with open arms \o/
			$other_passwords[] = md5(md5($password) . $user_settings['password_salt']);

			// hmm.. p'raps it's Invision 2 style?
			$other_passwords[] = md5(md5($user_settings['password_salt'])
                                 . md5($password));

			// some common md5 ones
			$other_passwords[] = md5($user_settings['password_salt'] . $password);
			$other_passwords[] = md5($password . $user_settings['password_salt']);
		} elseif (strlen($user_settings['passwd']) == 40) {
			// maybe they are using a hash from before the password fix
			$other_passwords[] = sha1(strtolower($user_settings['member_name'])
                                 . smfapi_unHtmlspecialchars($password));

			// BurningBoard3 style of hashing
			$other_passwords[] = sha1($user_settings['password_salt']
                                 . sha1($user_settings['password_salt']
                                 . sha1($password)));

			// perhaps we converted to UTF-8 and have a valid password being
            // hashed differently
			if (!empty($modSettings['previousCharacterSet'])
                && $modSettings['previousCharacterSet'] != 'utf8') {

				// try iconv first, for no particular reason
				if (function_exists('iconv')) {
					$other_passwords['iconv'] = sha1(strtolower(iconv('UTF-8', $modSettings['previousCharacterSet'], $user_settings['member_name']))
                                                . un_htmlspecialchars(iconv('UTF-8', $modSettings['previousCharacterSet'], $password)));
                }

				// say it aint so, iconv failed
				if (empty($other_passwords['iconv']) && function_exists('mb_convert_encoding')) {
					$other_passwords[] = sha1(strtolower(mb_convert_encoding($user_settings['member_name'], 'UTF-8', $modSettings['previousCharacterSet']))
                                         . un_htmlspecialchars(mb_convert_encoding($password, 'UTF-8', $modSettings['previousCharacterSet'])));
                }
			}
		}

		// SMF's sha1 function can give a funny result on Linux (not our fault!)
        // if we've now got the real one let the old one be valid!
		if (strpos(strtolower(PHP_OS), 'win') !== 0) {
			require_once($sourcedir . '/Subs-Compat.php');
			$other_passwords[] = sha1_smf(strtolower($user_settings['member_name']) . smfapi_unHtmlspecialchars($password));
		}

        // if ANY of these other hashes match we'll accept it
		if (in_array($user_settings['passwd'], $other_passwords)) {
            // we're not going to update the password or the hash. whatever was
            // used worked, so it will work again through this api, or SMF will
            // update it if the user authenticates through there. No sense messing
            // with it if it's not broken imo. Authentication successful
			$user_info["id"] = $user_settings['id_member'];
			return true;
		}
    }

    //authentication failed
    return false;
}

/**
 * Will log out a user
 *
 * Takes a username, email or member id and logs that user out. If it can't find
 * a match it will look for the currently logged user if any.
 *
 * @param  string $username user's member name (or int member id or string email)
 * @return bool whether logout was successful or not
 * @since  0.1.0
 */
function smfapi_logout($username='')
{
    global $sourcedir, $user_info, $user_settings, $context, $modSettings, $smcFunc;

    if ('' == $username && $user_info['is_guest']) {
        return false;
    }

    $user_data = smfapi_getUserData($username);

    if (!$user_data) {
        if (isset($user_info['id_member']) && false !== smfapi_getUserById($user_info['id_member'])) {
            $user_data['id_member'] = $user_info['id_member'];
        } else {
            return false;
        }
    }

    // if you log out, you aren't online anymore :P.
    $smcFunc['db_query']('', '
        DELETE FROM {db_prefix}log_online
        WHERE id_member = {int:current_member}',
        array(
            'current_member' => $user_data['id_member'],
        )
    );

    if (isset($_SESSION['pack_ftp'])) {
		$_SESSION['pack_ftp'] = null;
    }

	// they cannot be open ID verified any longer.
	if (isset($_SESSION['openid'])) {
		unset($_SESSION['openid']);
    }

	// it won't be first login anymore.
	unset($_SESSION['first_login']);

	smfapi_setLoginCookie(-3600, 0);

    return true;
}

/**
 * Delete members
 *
 * Delete a member or an array of members by member id
 *
 * @param  int || int array $users the member id(s)
 * @return bool true when complete or false if user array empty
 * @since  0.1.0
 */
function smfapi_deleteMembers($users)
{
	global $sourcedir, $modSettings, $user_info, $smcFunc;

	// try give us a while to sort this out...
	@set_time_limit(600);
	// try to get some more memory
	if (@ini_get('memory_limit') < 128) {
		@ini_set('memory_limit', '128M');
    }

	// if it's not an array, make it so
	if (!is_array($users)) {
		$users = array($users);
    } else {
		$users = array_unique($users);
    }

    foreach ($users as &$user) {
        if (!is_int($user)) {
            $data = smfapi_getUserData($user);
            $user = $data['id_member'] + 0;
        }
    }

	// make sure there's no void user in here
	$users = array_diff($users, array(0));

	if (empty($users)) {
		return false;
    }

	// make these peoples' posts guest posts
	$smcFunc['db_query']('', '
		UPDATE {db_prefix}messages
		SET id_member = {int:guest_id}, poster_email = {string:blank_email}
		WHERE id_member IN ({array_int:users})',
		array(
			'guest_id' => 0,
			'blank_email' => '',
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		UPDATE {db_prefix}polls
		SET id_member = {int:guest_id}
		WHERE id_member IN ({array_int:users})',
		array(
			'guest_id' => 0,
			'users' => $users,
		)
	);

	// make these peoples' posts guest first posts and last posts
	$smcFunc['db_query']('', '
		UPDATE {db_prefix}topics
		SET id_member_started = {int:guest_id}
		WHERE id_member_started IN ({array_int:users})',
		array(
			'guest_id' => 0,
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		UPDATE {db_prefix}topics
		SET id_member_updated = {int:guest_id}
		WHERE id_member_updated IN ({array_int:users})',
		array(
			'guest_id' => 0,
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		UPDATE {db_prefix}log_actions
		SET id_member = {int:guest_id}
		WHERE id_member IN ({array_int:users})',
		array(
			'guest_id' => 0,
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		UPDATE {db_prefix}log_banned
		SET id_member = {int:guest_id}
		WHERE id_member IN ({array_int:users})',
		array(
			'guest_id' => 0,
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		UPDATE {db_prefix}log_errors
		SET id_member = {int:guest_id}
		WHERE id_member IN ({array_int:users})',
		array(
			'guest_id' => 0,
			'users' => $users,
		)
	);

	// delete the member
	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}members
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	// delete the logs...
	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_actions
		WHERE id_log = {int:log_type}
			AND id_member IN ({array_int:users})',
		array(
			'log_type' => 2,
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_boards
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_comments
		WHERE id_recipient IN ({array_int:users})
			AND comment_type = {string:warntpl}',
		array(
			'users' => $users,
			'warntpl' => 'warntpl',
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_group_requests
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_karma
		WHERE id_target IN ({array_int:users})
			OR id_executor IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_mark_read
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_notify
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_online
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_subscribed
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}log_topics
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}collapsed_categories
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	// make their votes appear as guest votes - at least it keeps the totals right
	$smcFunc['db_query']('', '
		UPDATE {db_prefix}log_polls
		SET id_member = {int:guest_id}
		WHERE id_member IN ({array_int:users})',
		array(
			'guest_id' => 0,
			'users' => $users,
		)
	);

	// delete personal messages
	smfapi_deleteMessages(null, null, $users);

	$smcFunc['db_query']('', '
		UPDATE {db_prefix}personal_messages
		SET id_member_from = {int:guest_id}
		WHERE id_member_from IN ({array_int:users})',
		array(
			'guest_id' => 0,
			'users' => $users,
		)
	);

	// they no longer exist, so we don't know who it was sent to
	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}pm_recipients
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	// it's over, no more moderation for you
	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}moderators
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}group_moderators
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	// if you don't exist we can't ban you
	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}ban_items
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	// remove individual theme settings
	$smcFunc['db_query']('', '
		DELETE FROM {db_prefix}themes
		WHERE id_member IN ({array_int:users})',
		array(
			'users' => $users,
		)
	);

	// I'm not your buddy, chief
	$request = $smcFunc['db_query']('', '
		SELECT id_member, pm_ignore_list, buddy_list
		FROM {db_prefix}members
		WHERE FIND_IN_SET({raw:pm_ignore_list}, pm_ignore_list) != 0 OR FIND_IN_SET({raw:buddy_list}, buddy_list) != 0',
		array(
			'pm_ignore_list' => implode(', pm_ignore_list) != 0 OR FIND_IN_SET(', $users),
			'buddy_list' => implode(', buddy_list) != 0 OR FIND_IN_SET(', $users),
		)
	);

	while ($row = $smcFunc['db_fetch_assoc']($request)) {
		$smcFunc['db_query']('', '
			UPDATE {db_prefix}members
			SET
				pm_ignore_list = {string:pm_ignore_list},
				buddy_list = {string:buddy_list}
			WHERE id_member = {int:id_member}',
			array(
				'id_member' => $row['id_member'],
				'pm_ignore_list' => implode(',', array_diff(explode(',', $row['pm_ignore_list']), $users)),
				'buddy_list' => implode(',', array_diff(explode(',', $row['buddy_list']), $users)),
			)
		);
    }

	$smcFunc['db_free_result']($request);

	// make sure no member's birthday is still sticking in the calendar...
	smfapi_updateSettings(array(
		'calendar_updated' => time(),
	));

	smfapi_updateStats('member');

	return true;
}

/**
 * Register a member
 *
 * Register a new member with SMF
 *
 * @param  array $regOptions the registration options
 * @return int $memberId the user's member id || bool false
 * @since  0.1.0
 */
function smfapi_registerMember($regOptions)
{
	global $scripturl, $modSettings, $sourcedir;
	global $user_info, $options, $settings, $smcFunc;

    $reg_errors = array();

	// check username
	if (empty($regOptions['member_name'])) {
		$reg_errors[] = 'username empty';
    }

    if (false !== smfapi_getUserbyUsername($regOptions['member_name'])) {
        $reg_errors[] = 'username taken';
    }

	// check email
	if (empty($regOptions['email'])
        || preg_match('~^[0-9A-Za-z=_+\-/][0-9A-Za-z=_\'+\-/\.]*@[\w\-]+(\.[\w\-]+)*(\.[\w]{2,6})$~', $regOptions['email']) === 0
        || strlen($regOptions['email']) > 255) {
		    $reg_errors[] = 'email invalid';
    }

    if (false !== smfapi_getUserbyEmail($regOptions['email'])) {
        $reg_errors[] = 'email already in use';
    }

	// generate a validation code if it's supposed to be emailed
	// unless there was one passed in for us to use
	$validation_code = '';
	if (!isset($regOptions['require']) || empty($regOptions['require'])) {
        //we need to set it to something...
        $regOptions['require'] = 'nothing';
	}
	if ($regOptions['require'] == 'activation') {
        if (isset($regOptions['validation_code'])) {
		    $validation_code = $regOptions['validation_code'];
		} else {
            $validation_code = smfapi_generateValidationCode();
		}
    }

    if (!isset($regOptions['password_check']) || empty($regOptions['password_check'])) {
        //make them match if the check wasn't set or it will fail the comparison below
        $regOptions['password_check'] = $regOptions['password'];
    }
	if ($regOptions['password'] != $regOptions['password_check']) {
        $reg_errors[] = 'password check failed';
    }

	// password empty is an error
	if ('' == $regOptions['password']) {
        $reg_errors[] = 'password empty';
	}

	// if there's any errors left return them at once
	if (!empty($reg_errors)) {
		return $reg_errors;
    }

	// some of these might be overwritten (the lower ones that are in the arrays below)
	$regOptions['register_vars'] = array(
		'member_name' => $regOptions['member_name'],
		'email_address' => $regOptions['email'],
		'passwd' => sha1(strtolower($regOptions['member_name']) . $regOptions['password']),
		'password_salt' => substr(md5(mt_rand()), 0, 4) ,
		'posts' => 0,
		'date_registered' => time(),
		'member_ip' => $user_info['ip'],
		'member_ip2' => isset($_SERVER['BAN_CHECK_IP'])?$_SERVER['BAN_CHECK_IP']:'',
		'validation_code' => $validation_code,
		'real_name' => isset($regOptions['real_name'])?$regOptions['real_name']:$regOptions['member_name'],
		'personal_text' => $modSettings['default_personal_text'],
		'pm_email_notify' => 1,
		'id_theme' => 0,
		'id_post_group' => 4,
		'lngfile' => isset($regOptions['lngfile'])?$regOptions['lngfile']:'',
		'buddy_list' => '',
		'pm_ignore_list' => '',
		'message_labels' => '',
		'website_title' => isset($regOptions['website_title'])?$regOptions['website_title']:'',
		'website_url' => isset($regOptions['website_url'])?$regOptions['website_url']:'',
		'location' => isset($regOptions['location'])?$regOptions['location']:'',
		'icq' => isset($regOptions['icq'])?$regOptions['icq']:'',
		'aim' => isset($regOptions['aim'])?$regOptions['aim']:'',
		'yim' => isset($regOptions['yim'])?$regOptions['yim']:'',
		'msn' => isset($regOptions['msn'])?$regOptions['msn']:'',
		'time_format' => isset($regOptions['time_format'])?$regOptions['time_format']:'',
		'signature' => isset($regOptions['signature'])?$regOptions['signature']:'',
		'avatar' => isset($regOptions['avatar'])?$regOptions['avatar']:'',
		'usertitle' => '',
		'secret_question' => isset($regOptions['secret_question'])?$regOptions['secret_question']:'',
		'secret_answer' => isset($regOptions['secret_answer'])?$regOptions['secret_answer']:'',
		'additional_groups' => '',
		'ignore_boards' => '',
		'smiley_set' => '',
		'openid_uri' => isset($regOptions['openid_uri'])?$regOptions['openid_uri']:'',
	);

	// maybe it can be activated right away?
	if ($regOptions['require'] == 'nothing')
		$regOptions['register_vars']['is_activated'] = 1;
	// maybe it must be activated by email?
	elseif ($regOptions['require'] == 'activation')
		$regOptions['register_vars']['is_activated'] = 0;
	// otherwise it must be awaiting approval!
	else
		$regOptions['register_vars']['is_activated'] = 3;

	if (isset($regOptions['memberGroup']))
	{
        // make sure the id_group will be valid, if this is an administator
		$regOptions['register_vars']['id_group'] = $regOptions['memberGroup'];

		// check if this group is assignable
		$unassignableGroups = array(-1, 3);
		$request = $smcFunc['db_query']('', '
			SELECT id_group
			FROM {db_prefix}membergroups
			WHERE min_posts != {int:min_posts}' . '
				OR group_type = {int:is_protected}',
			array(
				'min_posts' => -1,
				'is_protected' => 1,
			)
		);
		while ($row = $smcFunc['db_fetch_assoc']($request)) {
			$unassignableGroups[] = $row['id_group'];
        }

		$smcFunc['db_free_result']($request);

		if (in_array($regOptions['register_vars']['id_group'], $unassignableGroups)) {
			$regOptions['register_vars']['id_group'] = 0;
        }
	}

	// integrate optional user theme options to be set
	$theme_vars = array();

	if (!empty($regOptions['theme_vars'])) {
		foreach ($regOptions['theme_vars'] as $var => $value) {
			$theme_vars[$var] = $value;
        }
    }

	// right, now let's prepare for insertion
	$knownInts = array(
		'date_registered', 'posts', 'id_group', 'last_login', 'instant_messages', 'unread_messages',
		'new_pm', 'pm_prefs', 'gender', 'hide_email', 'show_online', 'pm_email_notify', 'karma_good', 'karma_bad',
		'notify_announcements', 'notify_send_body', 'notify_regularity', 'notify_types',
		'id_theme', 'is_activated', 'id_msg_last_visit', 'id_post_group', 'total_time_logged_in', 'warning',
	);
	$knownFloats = array(
		'time_offset',
	);

	$column_names = array();
	$values = array();

	foreach ($regOptions['register_vars'] as $var => $val) {
		$type = 'string';
		if (in_array($var, $knownInts)) {
			$type = 'int';
        } elseif (in_array($var, $knownFloats)) {
			$type = 'float';
        } elseif ($var == 'birthdate') {
			$type = 'date';
        }

		$column_names[$var] = $type;
		$values[$var] = $val;
	}

	// register them into the database
	$smcFunc['db_insert']('',
		'{db_prefix}members',
		$column_names,
		$values,
		array('id_member')
	);

	$memberID = $smcFunc['db_insert_id']('{db_prefix}members', 'id_member');

	// update the number of members and latest member's info - and pass the name, but remove the 's
	if ($regOptions['register_vars']['is_activated'] == 1) {
		smfapi_updateStats('member', $memberID, $regOptions['register_vars']['real_name']);
    } else {
		smfapi_updateStats('member');
    }

	// theme variables too?
	if (!empty($theme_vars)) {
		$inserts = array();
		foreach ($theme_vars as $var => $val) {
			$inserts[] = array($memberID, $var, $val);
        }
		$smcFunc['db_insert']('insert',
			'{db_prefix}themes',
			array('id_member' => 'int', 'variable' => 'string-255', 'value' => 'string-65534'),
			$inserts,
			array('id_member', 'variable')
		);
	}

	// okay, they're for sure registered... make sure the session is aware of this for security
	$_SESSION['just_registered'] = 1;

	return $memberID;
}

/**
 * Logs an error to the smf error log
 *
 * Logs errors of different types. Will refer to this file unless $file has a value.
 *
 * @param  string $error_message the error message to log
 * @param  string $error_type the type of error, see $known_error_types array below
 *         for valid values to use
 * @param  string $file the file to reference in the log, if any. Use __FILE__
 * @param  int $line the line number to reference in the log, if any. Use __LINE__
 * @return bool true if successful, false if error logging is disabled
 * @since  0.1.0
 */
function smfapi_logError($error_message, $error_type = 'general', $file = null, $line = null)
{
	global $modSettings, $sc, $user_info, $smcFunc, $scripturl, $last_error;

	// check if error logging is actually on.
	if (empty($modSettings['enableErrorLogging'])) {
		return false;
    }

	// basically, htmlspecialchars it minus &. (for entities!)
	$error_message = strtr($error_message, array('<' => '&lt;', '>' => '&gt;', '"' => '&quot;'));
	$error_message = strtr($error_message, array('&lt;br /&gt;' => '<br />', '&lt;b&gt;' => '<strong>', '&lt;/b&gt;' => '</strong>', "\n" => '<br />'));

	// add a file and line to the error message?
	// don't use the actual txt entries for file and line but instead use %1$s for file and %2$s for line
	if ($file == null) {
		$file = $scripturl;
    } else {
		// window style slashes don't play well, lets convert them to the unix style
		$file = str_replace('\\', '/', $file);
    }

	if ($line == null) {
		$line = 0;
    } else {
		$line = (int) $line;
    }

	// just in case there's no id_member or IP set yet
	if (empty($user_info['id'])) {
		$user_info['id'] = 0;
    }
	if (empty($user_info['ip'])) {
		$user_info['ip'] = '';
    }

	// find the best query string we can...
	$query_string = empty($_SERVER['QUERY_STRING']) ? (empty($_SERVER['REQUEST_URL']) ? '' : str_replace($scripturl, '', $_SERVER['REQUEST_URL'])) : $_SERVER['QUERY_STRING'];

	// don't log the session hash in the url twice, it's a waste.
	$query_string = htmlspecialchars((SMF == 'API' ? '' : '?') . preg_replace(array('~;sesc=[^&;]+~', '~'
                    . session_name() . '=' . session_id() . '[&;]~'), array(';sesc', ''), $query_string));


	// what types of categories do we have?
	$known_error_types = array(
		'general',
		'critical',
		'database',
		'undefined_vars',
		'user',
		'template',
		'debug',
	);

	// make sure the category that was specified is a valid one
	$error_type = in_array($error_type, $known_error_types) && $error_type !== true ? $error_type : 'general';

	// don't log the same error countless times, as we can get in a cycle of depression...
	$error_info = array($user_info['id'], time(), $user_info['ip'], $query_string, $error_message, (string) $sc, $error_type, $file, $line);
	if (empty($last_error) || $last_error != $error_info) {
		// insert the error into the database.
		$smcFunc['db_insert']('',
			'{db_prefix}log_errors',
			array('id_member' => 'int', 'log_time' => 'int', 'ip' => 'string-16', 'url' => 'string-65534', 'message' => 'string-65534', 'session' => 'string', 'error_type' => 'string', 'file' => 'string-255', 'line' => 'int'),
			$error_info,
			array('id_error')
		);
		$last_error = $error_info;
	}

	return true;
}

/**
 * Load the $modSettings array and adds to the $smcFunc array
 *
 * Loads the $modSettings array which all has the forum configuration data
 * that wasn't in Settings.php and adds the non-db related functions to the
 * $smcFunc array. Also sets the timezone so php time funcs won't throw errors
 *
 * @return bool true when complete, failure is not an option
 * @since  0.1.0
 */
function smfapi_reloadSettings()
{
	global $modSettings, $boarddir, $smcFunc, $txt, $db_character_set, $context, $sourcedir;

	// most database systems have not set UTF-8 as their default input charset.
	if (!empty($db_character_set)) {
		$smcFunc['db_query']('set_character_set', '
			SET NAMES ' . $db_character_set,
			array(
			)
		);
    }

	// try to load it from the cache first; it'll never get cached if the setting is off.
	if (($modSettings = smfapi_cacheGetData('modSettings', 90)) == null) {
		$request = $smcFunc['db_query']('', '
			SELECT variable, value
			FROM {db_prefix}settings',
			array(
			)
		);

		$modSettings = array();

		if (!$request) {
            return false;
        }

		while ($row = $smcFunc['db_fetch_row']($request)) {
			$modSettings[$row[0]] = $row[1];
        }

		$smcFunc['db_free_result']($request);

		if (!empty($modSettings['cache_enable'])) {
			smfapi_cachePutData('modSettings', $modSettings, 90);
        }
	}

	// UTF-8 in regular expressions is unsupported on PHP(win) versions < 4.2.3.
	$utf8 = (empty($modSettings['global_character_set']) ? $txt['lang_character_set'] : $modSettings['global_character_set']) === 'UTF-8' && (strpos(strtolower(PHP_OS), 'win') === false || @version_compare(PHP_VERSION, '4.2.3') != -1);

	// set a list of common functions.
	$ent_list = empty($modSettings['disableEntityCheck']) ? '&(#\d{1,7}|quot|amp|lt|gt|nbsp);' : '&(#021|quot|amp|lt|gt|nbsp);';
	$ent_check = empty($modSettings['disableEntityCheck']) ? array('preg_replace(\'~(&#(\d{1,7}|x[0-9a-fA-F]{1,6});)~e\', \'$smcFunc[\\\'entity_fix\\\'](\\\'\\2\\\')\', ', ')') : array('', '');

	// preg_replace can handle complex characters only for higher PHP versions.
	$space_chars = $utf8 ? (@version_compare(PHP_VERSION, '4.3.3') != -1 ? '\x{A0}\x{AD}\x{2000}-\x{200F}\x{201F}\x{202F}\x{3000}\x{FEFF}' : "\xC2\xA0\xC2\xAD\xE2\x80\x80-\xE2\x80\x8F\xE2\x80\x9F\xE2\x80\xAF\xE2\x80\x9F\xE3\x80\x80\xEF\xBB\xBF") : '\x00-\x08\x0B\x0C\x0E-\x19\xA0';

	$smcFunc += array(
		'entity_fix' => create_function('$string', '
			$num = substr($string, 0, 1) === \'x\' ? hexdec(substr($string, 1)) : (int) $string;
			return $num < 0x20 || $num > 0x10FFFF || ($num >= 0xD800 && $num <= 0xDFFF) || $num == 0x202E ? \'\' : \'&#\' . $num . \';\';'),
		'htmlspecialchars' => create_function('$string, $quote_style = ENT_COMPAT, $charset = \'ISO-8859-1\'', '
			global $smcFunc;
			return ' . strtr($ent_check[0], array('&' => '&amp;')) . 'htmlspecialchars($string, $quote_style, ' . ($utf8 ? '\'UTF-8\'' : '$charset') . ')' . $ent_check[1] . ';'),
		'htmltrim' => create_function('$string', '
			global $smcFunc;
			return preg_replace(\'~^(?:[ \t\n\r\x0B\x00' . $space_chars . ']|&nbsp;)+|(?:[ \t\n\r\x0B\x00' . $space_chars . ']|&nbsp;)+$~' . ($utf8 ? 'u' : '') . '\', \'\', ' . implode('$string', $ent_check) . ');'),
		'strlen' => create_function('$string', '
			global $smcFunc;
			return strlen(preg_replace(\'~' . $ent_list . ($utf8 ? '|.~u' : '~') . '\', \'_\', ' . implode('$string', $ent_check) . '));'),
		'strpos' => create_function('$haystack, $needle, $offset = 0', '
			global $smcFunc;
			$haystack_arr = preg_split(\'~(&#' . (empty($modSettings['disableEntityCheck']) ? '\d{1,7}' : '021') . ';|&quot;|&amp;|&lt;|&gt;|&nbsp;|.)~' . ($utf8 ? 'u' : '') . '\', ' . implode('$haystack', $ent_check) . ', -1, PREG_SPLIT_DELIM_CAPTURE | PREG_SPLIT_NO_EMPTY);
			$haystack_size = count($haystack_arr);
			if (strlen($needle) === 1)
			{
				$result = array_search($needle, array_slice($haystack_arr, $offset));
				return is_int($result) ? $result + $offset : false;
			}
			else
			{
				$needle_arr = preg_split(\'~(&#' . (empty($modSettings['disableEntityCheck']) ? '\d{1,7}' : '021') . ';|&quot;|&amp;|&lt;|&gt;|&nbsp;|.)~' . ($utf8 ? 'u' : '') . '\',  ' . implode('$needle', $ent_check) . ', -1, PREG_SPLIT_DELIM_CAPTURE | PREG_SPLIT_NO_EMPTY);
				$needle_size = count($needle_arr);

				$result = array_search($needle_arr[0], array_slice($haystack_arr, $offset));
				while (is_int($result))
				{
					$offset += $result;
					if (array_slice($haystack_arr, $offset, $needle_size) === $needle_arr)
						return $offset;
					$result = array_search($needle_arr[0], array_slice($haystack_arr, ++$offset));
				}
				return false;
			}'),
		'substr' => create_function('$string, $start, $length = null', '
			global $smcFunc;
			$ent_arr = preg_split(\'~(&#' . (empty($modSettings['disableEntityCheck']) ? '\d{1,7}' : '021') . ';|&quot;|&amp;|&lt;|&gt;|&nbsp;|.)~' . ($utf8 ? 'u' : '') . '\', ' . implode('$string', $ent_check) . ', -1, PREG_SPLIT_DELIM_CAPTURE | PREG_SPLIT_NO_EMPTY);
			return $length === null ? implode(\'\', array_slice($ent_arr, $start)) : implode(\'\', array_slice($ent_arr, $start, $length));'),
		'strtolower' => $utf8 ? (function_exists('mb_strtolower') ? create_function('$string', '
			return mb_strtolower($string, \'UTF-8\');') : create_function('$string', '
			global $sourcedir;
			require_once($sourcedir . \'/Subs-Charset.php\');
			return utf8_strtolower($string);')) : 'strtolower',
		'strtoupper' => $utf8 ? (function_exists('mb_strtoupper') ? create_function('$string', '
			return mb_strtoupper($string, \'UTF-8\');') : create_function('$string', '
			global $sourcedir;
			require_once($sourcedir . \'/Subs-Charset.php\');
			return utf8_strtoupper($string);')) : 'strtoupper',
		'truncate' => create_function('$string, $length', (empty($modSettings['disableEntityCheck']) ? '
			global $smcFunc;
			$string = ' . implode('$string', $ent_check) . ';' : '') . '
			preg_match(\'~^(' . $ent_list . '|.){\' . $smcFunc[\'strlen\'](substr($string, 0, $length)) . \'}~'.  ($utf8 ? 'u' : '') . '\', $string, $matches);
			$string = $matches[0];
			while (strlen($string) > $length)
				$string = preg_replace(\'~(?:' . $ent_list . '|.)$~'.  ($utf8 ? 'u' : '') . '\', \'\', $string);
			return $string;'),
		'ucfirst' => $utf8 ? create_function('$string', '
			global $smcFunc;
			return $smcFunc[\'strtoupper\']($smcFunc[\'substr\']($string, 0, 1)) . $smcFunc[\'substr\']($string, 1);') : 'ucfirst',
		'ucwords' => $utf8 ? create_function('$string', '
			global $smcFunc;
			$words = preg_split(\'~([\s\r\n\t]+)~\', $string, -1, PREG_SPLIT_DELIM_CAPTURE);
			for ($i = 0, $n = count($words); $i < $n; $i += 2)
				$words[$i] = $smcFunc[\'ucfirst\']($words[$i]);
			return implode(\'\', $words);') : 'ucwords',
	);

	// setting the timezone is a requirement for some functions in PHP >= 5.1.
	if (isset($modSettings['default_timezone'])
        && function_exists('date_default_timezone_set')) {
		    date_default_timezone_set($modSettings['default_timezone']);
    }

    return true;
}

/**
 * Loads the $user_info array
 *
 * Will take the user's member id and load the handy $user_info array. If no
 * user id is supplied, it will try the cookie, then the session to get a user id.
 * If that fails, the $user_info array will contain the fallback data for guests.
 *
 * @param  int $id_member the member id
 * @return bool true when complete, failure is not an option
 * @since  0.1.0
 */
function smfapi_loadUserSettings($id_member=0)
{
	global $modSettings, $user_settings, $sourcedir, $smcFunc;
	global $cookiename, $user_info, $language;

    if (0 == $id_member && isset($_COOKIE[$cookiename])) {
		// fix a security hole in PHP 4.3.9 and below...
		if (preg_match('~^a:[34]:\{i:0;(i:\d{1,6}|s:[1-8]:"\d{1,8}");i:1;s:(0|40):"([a-fA-F0-9]{40})?";i:2;[id]:\d{1,14};(i:3;i:\d;)?\}$~i', $_COOKIE[$cookiename]) == 1) {
			list ($id_member, $password) = @unserialize($_COOKIE[$cookiename]);
			$id_member = !empty($id_member) && strlen($password) > 0 ? (int) $id_member : 0;
		} else {
			$id_member = 0;
        }
	} elseif (empty($id_member) && isset($_SESSION['login_' . $cookiename]) && ($_SESSION['USER_AGENT'] == $_SERVER['HTTP_USER_AGENT'] || !empty($modSettings['disableCheckUA']))) {
		// !!! perhaps we can do some more checking on this, such as on the first octet of the IP?
		list ($id_member, $password, $login_span) = @unserialize($_SESSION['login_' . $cookiename]);
		$id_member = !empty($id_member) && strlen($password) == 40 && $login_span > time() ? (int) $id_member : 0;
	}

	// only load this stuff if the user isn't a guest.
	if ($id_member != 0) {
		// is the member data cached?
		if (empty($modSettings['cache_enable']) || $modSettings['cache_enable'] < 2
            || ($user_settings = smfapi_cacheGetData('user_settings-' . $id_member, 60)) == null) {
			    $request = $smcFunc['db_query']('', '
				    SELECT mem.*, IFNULL(a.id_attach, 0) AS id_attach, a.filename, a.attachment_type
				    FROM {db_prefix}members AS mem
					    LEFT JOIN {db_prefix}attachments AS a ON (a.id_member = {int:id_member})
				    WHERE mem.id_member = {int:id_member}
				    LIMIT 1',
				    array(
					    'id_member' => $id_member,
				    )
			    );

			    $user_settings = $smcFunc['db_fetch_assoc']($request);
			    $smcFunc['db_free_result']($request);

			    if (!empty($modSettings['cache_enable']) && $modSettings['cache_enable'] >= 2) {
				    smfapi_cachePutData('user_settings-' . $id_member, $user_settings, 60);
                }
		}
	}

	// found 'im, let's set up the variables.
	if (0 != $id_member) {
		$username = $user_settings['member_name'];

		if (empty($user_settings['additional_groups'])) {
			$user_info = array(
				'groups' => array($user_settings['id_group'], $user_settings['id_post_group'])
			);
        } else {
			$user_info = array(
				'groups' => array_merge(
					array($user_settings['id_group'], $user_settings['id_post_group']),
					explode(',', $user_settings['additional_groups'])
				)
			);
        }

		// because history has proven that it is possible for groups to go bad - clean up in case.
		foreach ($user_info['groups'] as $k => $v) {
			$user_info['groups'][$k] = (int) $v;
        }

		// this is a logged in user, so definitely not a spider.
		$user_info['possibly_robot'] = false;
	} else {
		// this is what a guest's variables should be.
		$username = '';
		$user_info = array('groups' => array(-1));
		$user_settings = array();
        $user_info['possibly_robot'] = false;
	}

	// set up the $user_info array.
	$user_info += array(
		'id' => $id_member,
		'username' => $username,
		'name' => isset($user_settings['real_name']) ? $user_settings['real_name'] : '',
		'email' => isset($user_settings['email_address']) ? $user_settings['email_address'] : '',
		'passwd' => isset($user_settings['passwd']) ? $user_settings['passwd'] : '',
		'language' => empty($user_settings['lngfile']) || empty($modSettings['userLanguage']) ? $language : $user_settings['lngfile'],
		'is_guest' => $id_member == 0,
		'is_admin' => in_array(1, $user_info['groups']),
		'theme' => empty($user_settings['id_theme']) ? 0 : $user_settings['id_theme'],
		'last_login' => empty($user_settings['last_login']) ? 0 : $user_settings['last_login'],
		'ip' => $_SERVER['REMOTE_ADDR'],
		'ip2' => isset($_SERVER['BAN_CHECK_IP']) ? $_SERVER['BAN_CHECK_IP']: '',
		'posts' => empty($user_settings['posts']) ? 0 : $user_settings['posts'],
		'time_format' => empty($user_settings['time_format']) ? $modSettings['time_format'] : $user_settings['time_format'],
		'time_offset' => empty($user_settings['time_offset']) ? 0 : $user_settings['time_offset'],
		'avatar' => array(
			'url' => isset($user_settings['avatar']) ? $user_settings['avatar'] : '',
			'filename' => empty($user_settings['filename']) ? '' : $user_settings['filename'],
			'custom_dir' => !empty($user_settings['attachment_type']) && $user_settings['attachment_type'] == 1,
			'id_attach' => isset($user_settings['id_attach']) ? $user_settings['id_attach'] : 0
		),
		'smiley_set' => isset($user_settings['smiley_set']) ? $user_settings['smiley_set'] : '',
		'messages' => empty($user_settings['instant_messages']) ? 0 : $user_settings['instant_messages'],
		'unread_messages' => empty($user_settings['unread_messages']) ? 0 : $user_settings['unread_messages'],
		'total_time_logged_in' => empty($user_settings['total_time_logged_in']) ? 0 : $user_settings['total_time_logged_in'],
		'buddies' => !empty($modSettings['enable_buddylist']) && !empty($user_settings['buddy_list']) ? explode(',', $user_settings['buddy_list']) : array(),
		'ignoreboards' => !empty($user_settings['ignore_boards']) && !empty($modSettings['allow_ignore_boards']) ? explode(',', $user_settings['ignore_boards']) : array(),
		'ignoreusers' => !empty($user_settings['pm_ignore_list']) ? explode(',', $user_settings['pm_ignore_list']) : array(),
		'warning' => isset($user_settings['warning']) ? $user_settings['warning'] : 0,
		'permissions' => array(),
	);
	$user_info['groups'] = array_unique($user_info['groups']);

	return true;
}

/**
 * Attempt to start the session, unless it already has been
 *
 * Modifies some ini settings, starts the session if there isn't one already and
 * sets the session variables.
 *
 * @return bool true when complete, failure is not an option
 * @since  0.1.0
 */
function smfapi_loadSession()
{
	global $HTTP_SESSION_VARS, $modSettings, $boardurl, $sc;

	// attempt to change a few PHP settings.
	@ini_set('session.use_cookies', true);
	@ini_set('session.use_only_cookies', false);
	@ini_set('url_rewriter.tags', '');
	@ini_set('session.use_trans_sid', false);
	@ini_set('arg_separator.output', '&amp;');

	if (!empty($modSettings['globalCookies'])) {
		$parsed_url = parse_url($boardurl);

		if (preg_match('~^\d{1,3}(\.\d{1,3}){3}$~', $parsed_url['host']) == 0
            && preg_match('~(?:[^\.]+\.)?([^\.]{2,}\..+)\z~i', $parsed_url['host'], $parts) == 1) {
			    @ini_set('session.cookie_domain', '.' . $parts[1]);
        }
	}

	// if it's already been started... probably best to skip this.
	if ('' == session_id()) {

		// this is here to stop people from using bad junky PHPSESSIDs.
		if (isset($_REQUEST[session_name()])
            && preg_match('~^[A-Za-z0-9]{16,32}$~', $_REQUEST[session_name()]) == 0
            && !isset($_COOKIE[session_name()])) {

			$session_id = md5(md5('smf_sess_' . time()) . mt_rand());
			$_REQUEST[session_name()] = $session_id;
			$_GET[session_name()] = $session_id;
			$_POST[session_name()] = $session_id;
		}

		// use database sessions? (they don't work in 4.1.x!)
		if (!empty($modSettings['databaseSession_enable'])
            && @version_compare(PHP_VERSION, '4.2.0') != -1) {

			session_set_save_handler('smfapi_sessionOpen', 'smfapi_sessionClose',
                                     'smfapi_sessionRead', 'smfapi_sessionWrite',
                                     'smfapi_sessionDestroy', 'smfapi_sessionGC');
			@ini_set('session.gc_probability', '1');
		} elseif (@ini_get('session.gc_maxlifetime') <= 1440
                  && !empty($modSettings['databaseSession_lifetime'])) {

		    @ini_set('session.gc_maxlifetime', max($modSettings['databaseSession_lifetime'], 60));
        }

		// use cache setting sessions?
		if (empty($modSettings['databaseSession_enable'])
            && !empty($modSettings['cache_enable']) && php_sapi_name() != 'cli') {

			if (function_exists('mmcache_set_session_handlers')) {
				mmcache_set_session_handlers();
            } elseif (function_exists('eaccelerator_set_session_handlers')) {
				eaccelerator_set_session_handlers();
            }
		}

		session_start();

		// change it so the cache settings are a little looser than default.
		if (!empty($modSettings['databaseSession_loose'])) {
			header('Cache-Control: private');
        }
	}

	// while PHP 4.1.x should use $_SESSION, it seems to need this to do it right.
	if (@version_compare(PHP_VERSION, '4.2.0') == -1) {
		$HTTP_SESSION_VARS['php_412_bugfix'] = true;
    }

	// set the randomly generated code.
	if (!isset($_SESSION['session_value'])) {
        $_SESSION['session_value'] = md5(session_id() . mt_rand());
    }

    if (!isset($_SESSION['session_var'])) {
        $_SESSION['session_var'] = substr(preg_replace('~^\d+~', '', sha1(mt_rand()
                                   . session_id() . mt_rand())), 0, rand(7, 12));
    }

	$sc = $_SESSION['session_value'];

    return true;
}

/**
 * Session open
 *
 * It doesn't do much :p
 *
 * @param  string $save_path
 * @param  string $session_name
 * @return true
 * @since  0.1.0
 */
function smfapi_sessionOpen($save_path, $session_name)
{
	return true;
}

/**
 * Session close
 *
 * It doesn't do much :p
 *
 * @return true
 * @since  0.1.0
 */
function smfapi_sessionClose()
{
	return true;
}

/**
 * Session read
 *
 * Loads the session data from the session code
 *
 * @param  string $session_id the session code
 * @return array $sess_data the session data || bool false if the session code
 *         isn't valid or there is no session data to return
 * @since  0.1.0
 */
function smfapi_sessionRead($session_id)
{
	global $smcFunc;

	if (preg_match('~^[A-Za-z0-9]{16,32}$~', $session_id) == 0) {
		return false;
    }

	// look for it in the database.
	$result = $smcFunc['db_query']('', '
		SELECT data
		FROM {db_prefix}sessions
		WHERE session_id = {string:session_id}
		LIMIT 1',
		array(
			'session_id' => $session_id,
		)
	);
	list ($sess_data) = $smcFunc['db_fetch_row']($result);
	$smcFunc['db_free_result']($result);

    if (!empty($sess_data)) {
	    return $sess_data;
    }

    return false;
}

/**
 * Session write
 *
 * Writes data into the session
 *
 * @param  string $session_id the session code
 * @param  string $data the data to write into the session
 * @return bool true when complete, false if the session code isn't valid
 * @since  0.1.0
 */
function smfapi_sessionWrite($session_id, $data)
{
	global $smcFunc;

	if (preg_match('~^[A-Za-z0-9]{16,32}$~', $session_id) == 0) {
		return false;
    }

	// first try to update an existing row...
	$result = $smcFunc['db_query']('', '
		UPDATE {db_prefix}sessions
		SET data = {string:data}, last_update = {int:last_update}
		WHERE session_id = {string:session_id}',
		array(
			'last_update' => time(),
			'data' => $data,
			'session_id' => $session_id,
		)
	);

	// if that didn't work, try inserting a new one.
	if ($smcFunc['db_affected_rows']() == 0) {
		$result = $smcFunc['db_insert']('ignore',
			'{db_prefix}sessions',
			array('session_id' => 'string', 'data' => 'string', 'last_update' => 'int'),
			array($session_id, $data, time()),
			array('session_id')
		);
    }

	return true;
}

/**
 * Session destroy
 *
 * Destroy a session
 *
 * @param  string $session_id the session code
 * @return bool true || string db error || false if the session code isn't valid
 * @since  0.1.0
 */
function smfapi_sessionDestroy($session_id)
{
	global $smcFunc;

	if (preg_match('~^[A-Za-z0-9]{16,32}$~', $session_id) == 0) {
		return false;
    }

	// just delete the row...
	return $smcFunc['db_query']('', '
		DELETE FROM {db_prefix}sessions
		WHERE session_id = {string:session_id}',
		array(
			'session_id' => $session_id,
		)
	);
}

/**
 * Session garbage collection
 *
 * How long should the unupdated session remain in the db before we delete it?
 *
 * @param  int $max_lifetime
 * @return bool true || string db error
 * @since  0.1.0
 */
function smfapi_sessionGC($max_lifetime)
{
	global $modSettings, $smcFunc;

	// just set to the default or lower?  ignore it for a higher value. (hopefully)
	if (!empty($modSettings['databaseSession_lifetime'])
        && ($max_lifetime <= 1440
        || $modSettings['databaseSession_lifetime'] > $max_lifetime)) {

		$max_lifetime = max($modSettings['databaseSession_lifetime'], 60);
    }

	// clean up ;).
	return $smcFunc['db_query']('', '
		DELETE FROM {db_prefix}sessions
		WHERE last_update < {int:last_update}',
		array(
			'last_update' => time() - $max_lifetime,
		)
	);
}

/**
 * Load the db connection
 *
 * Will add the db functions to $smcFunc array and set up and test the db connection
 *
 * @return bool if the db connection exists or not
 * @since  0.1.0
 */
function smfapi_loadDatabase()
{
	global $db_persist, $db_connection, $db_server, $db_user, $db_passwd;
	global $db_type, $db_name, $sourcedir, $db_prefix;

	// figure out what type of database we are using.
	if (empty($db_type) || !file_exists($sourcedir . '/Subs-Db-' . $db_type . '.php')) {
		$db_type = 'mysql';
    }

	// load the file for the database (safe to load)
	require_once($sourcedir . '/Subs-Db-' . $db_type . '.php');

	// make connection
	if (empty($db_connection)) {
		$db_connection = smf_db_initiate($db_server, $db_name, $db_user, $db_passwd, $db_prefix, array('persist' => $db_persist, 'dont_select_db' => SMF == 'API'));
    }

	// safe guard here, if there isn't a valid connection lets put a stop to it.
	if (!$db_connection) {
		return false;
    }

    // defined in Subs-Db-*.php
    db_fix_prefix($db_prefix, $db_name);

    return true;
}

/**
 * Put data in the cache
 *
 * Adds data to whatever cache method we're using
 *
 * @param  string $key the cache data identifier
 * @param  mixed $value the value to be stored
 * @param  int $ttl how long are we going to cache this data (in seconds)
 * @return void
 * @since  0.1.0
 */
function smfapi_cachePutData($key, $value, $ttl = 120)
{
	global $boardurl, $sourcedir, $modSettings, $memcached;
	global $cache_hits, $cache_count, $db_show_debug, $cachedir;

	if (empty($modSettings['cache_enable']) && !empty($modSettings)) {
		return;
    }

	$cache_count = isset($cache_count) ? $cache_count + 1 : 1;

	if (isset($db_show_debug) && $db_show_debug === true) {
		$cache_hits[$cache_count] = array('k' => $key,
                                          'd' => 'put',
                                          's' => $value === null ? 0 : strlen(serialize($value)));
		$st = microtime();
	}

	$key = md5($boardurl . filemtime($sourcedir . '/Load.php'))
           . '-SMF-' . strtr($key, ':', '-');
	$value = $value === null ? null : serialize($value);

	// eAccelerator...
	if (function_exists('eaccelerator_put')) {
		if (mt_rand(0, 10) == 1) {
			eaccelerator_gc();
        }

		if ($value === null) {
			@eaccelerator_rm($key);
        } else {
			eaccelerator_put($key, $value, $ttl);
        }
	}
	// turck MMCache?
	elseif (function_exists('mmcache_put')) {
		if (mt_rand(0, 10) == 1) {
			mmcache_gc();
        }

		if ($value === null) {
			@mmcache_rm($key);
        } else {
			mmcache_put($key, $value, $ttl);
        }
	}
	// alternative PHP Cache, ahoy!
	elseif (function_exists('apc_store')) {
		// An extended key is needed to counteract a bug in APC.
		if ($value === null) {
			apc_delete($key . 'smf');
        } else {
			apc_store($key . 'smf', $value, $ttl);
        }
	}
	// zend Platform/ZPS/etc.
	elseif (function_exists('output_cache_put')) {
		output_cache_put($key, $value);
    } elseif (function_exists('xcache_set') && ini_get('xcache.var_size') > 0) {
		if ($value === null) {
			xcache_unset($key);
        } else {
			xcache_set($key, $value, $ttl);
        }
	}
	// otherwise custom cache?
	else {
		if ($value === null) {
			@unlink($cachedir . '/data_' . $key . '.php');
        } else {
			$cache_data = '<' . '?' . 'php if (!defined(\'SMF\')) die; if ('
                          . (time() + $ttl)
                          . ' < time()) $expired = true; else{$expired = false; $value = \''
                          . addcslashes($value, '\\\'') . '\';}' . '?' . '>';
			$fh = @fopen($cachedir . '/data_' . $key . '.php', 'w');

			if ($fh) {
				// write the file.
				set_file_buffer($fh, 0);
				flock($fh, LOCK_EX);
				$cache_bytes = fwrite($fh, $cache_data);
				flock($fh, LOCK_UN);
				fclose($fh);

				// check that the cache write was successful; all the data should be written
				// if it fails due to low diskspace, remove the cache file
				if ($cache_bytes != strlen($cache_data)) {
					@unlink($cachedir . '/data_' . $key . '.php');
                }
			}
		}
	}

	if (isset($db_show_debug) && $db_show_debug === true) {
		$cache_hits[$cache_count]['t'] = array_sum(explode(' ', microtime())) - array_sum(explode(' ', $st));
    }

    return;
}

/**
 * Get data from the cache
 *
 * Get data from the cache that matches this key, if it exists
 *
 * @param  string $key the cache data identifier
 * @param  int $ttl how long will the data be considered fresh (in seconds)
 * @return mixed $value the cache data or null
 * @since  0.1.0
 */
function smfapi_cacheGetData($key, $ttl = 120)
{
	global $boardurl, $sourcedir, $modSettings, $memcached;
	global $cache_hits, $cache_count, $db_show_debug, $cachedir;

	if (empty($modSettings['cache_enable']) && !empty($modSettings)) {
		return;
    }

	$cache_count = isset($cache_count) ? $cache_count + 1 : 1;

	if (isset($db_show_debug) && $db_show_debug === true) {
		$cache_hits[$cache_count] = array('k' => $key, 'd' => 'get');
		$st = microtime();
	}

	$key = md5($boardurl . filemtime($sourcedir . '/Load.php'))
           . '-SMF-' . strtr($key, ':', '-');

	// again, eAccelerator.
	if (function_exists('eaccelerator_get')) {
		$value = eaccelerator_get($key);
    }
	// the older, but ever-stable, Turck MMCache...
	elseif (function_exists('mmcache_get')) {
		$value = mmcache_get($key);
    }
	// this is the free APC from PECL.
	elseif (function_exists('apc_fetch')) {
		$value = apc_fetch($key . 'smf');
    }
	// zend's pricey stuff.
	elseif (function_exists('output_cache_get')) {
		$value = output_cache_get($key, $ttl);
    } elseif (function_exists('xcache_get') && ini_get('xcache.var_size') > 0) {
		$value = xcache_get($key);
    }
	// otherwise it's SMF data!
	elseif (file_exists($cachedir . '/data_' . $key . '.php')
            && filesize($cachedir . '/data_' . $key . '.php') > 10) {

		require($cachedir . '/data_' . $key . '.php');

		if (!empty($expired) && isset($value)) {
			@unlink($cachedir . '/data_' . $key . '.php');
			unset($value);
		}
	}

	if (isset($db_show_debug) && $db_show_debug === true) {
		$cache_hits[$cache_count]['t'] = array_sum(explode(' ', microtime())) - array_sum(explode(' ', $st));
		$cache_hits[$cache_count]['s'] = isset($value) ? strlen($value) : 0;
	}

	if (empty($value)) {
		return null;
    }
	// if it's broke, it's broke... so give up on it.
	else {
		return @unserialize($value);
    }
}

/**
 * Update member data
 *
 * Update member data such as 'passwd' (hash), 'email_address', 'is_activated'
 * 'password_salt', 'member_name' or any other user info in the db
 *
 * @param  int $member member id (will also work with string username or email)
 * @param  associative array $data the data to be updated (htmlspecialchar'd)
 * @return bool whether update was successful or not
 * @since  0.1.0
 */
function smfapi_updateMemberData($member='', $data='')
{
	global $modSettings, $user_info, $smcFunc;

    if ('' == $member || '' == $data) {
        return false;
    }

    $user_data = smfapi_getUserData($member);

    if (!$user_data) {
        $member = $user_info['id'];
    } elseif (isset($user_data['id_member'])) {
        $member = $user_data['id_member'];
    } else {
        return false;
    }

	$parameters = array();
    $condition = 'id_member = {int:member}';
    $parameters['member'] = $member;

	// everything is assumed to be a string unless it's in the below.
    $knownInts = array(
		'date_registered', 'posts', 'id_group', 'last_login', 'instant_messages', 'unread_messages',
		'new_pm', 'pm_prefs', 'gender', 'hide_email', 'show_online', 'pm_email_notify', 'pm_receive_from', 'karma_good', 'karma_bad',
		'notify_announcements', 'notify_send_body', 'notify_regularity', 'notify_types',
		'id_theme', 'is_activated', 'id_msg_last_visit', 'id_post_group', 'total_time_logged_in', 'warning',
	);
	$knownFloats = array(
		'time_offset',
	);

	$setString = '';
	foreach ($data as $var => $val) {
		$type = 'string';
		if (in_array($var, $knownInts)) {
			$type = 'int';
        } elseif (in_array($var, $knownFloats)) {
			$type = 'float';
        }

		$setString .= ' ' . $var . ' = {' . $type . ':p_' . $var . '},';
		$parameters['p_' . $var] = $val;
	}

	$smcFunc['db_query']('', '
		UPDATE {db_prefix}members
		SET' . substr($setString, 0, -1) . '
		WHERE ' . $condition,
		$parameters
	);

	// clear any caching?
	if (!empty($modSettings['cache_enable']) && $modSettings['cache_enable'] >= 2
        && !empty($members)) {

        if ($modSettings['cache_enable'] >= 3) {
            smfapi_cachePutData('member_data-profile-' . $member, null, 120);
            smfapi_cachePutData('member_data-normal-' . $member, null, 120);
            smfapi_cachePutData('member_data-minimal-' . $member, null, 120);
        }
        smfapi_cachePutData('user_settings-' . $member, null, 60);
	}

    return true;
}

/**
 * Create random seed
 *
 * Generate a random seed and ensure it's stored in settings
 *
 * @return bool true when complete
 * @since  0.1.0
 */
function smfapi_smfSeedGenerator()
{
	global $modSettings;

	// never existed?
	if (empty($modSettings['rand_seed'])) {
		$modSettings['rand_seed'] = microtime() * 1000000;
		smfapi_updateSettings(array('rand_seed' => $modSettings['rand_seed']));
	}

	if (@version_compare(PHP_VERSION, '4.2.0') == -1) {
		$seed = ($modSettings['rand_seed'] + ((double) microtime() * 1000003)) & 0x7fffffff;
		mt_srand($seed);
	}

	// update the settings with the new seed
	smfapi_updateSettings(array('rand_seed' => mt_rand()));

    return true;
}

/**
 * Update SMF settings
 *
 * Updates settings in the $modSettings array and stores them in the db. Also
 * clears the modSettings cache
 *
 * @param  associative array $changeArray the settings to update
 * @param  bool $update whether to update or replace in db
 * @param  bool $debug deprecated
 * @return bool whether settings were changed or not
 * @since  0.1.0
 */
function smfapi_updateSettings($changeArray, $update = false, $debug = false)
{
	global $modSettings, $smcFunc;

	if (empty($changeArray) || !is_array($changeArray)) {
		return false;
    }

	// in some cases, this may be better and faster, but for large sets we don't want so many UPDATEs.
	if ($update) {
		foreach ($changeArray as $variable => $value) {
			$smcFunc['db_query']('', '
				UPDATE {db_prefix}settings
				SET value = {' . ($value === false || $value === true ? 'raw' : 'string') . ':value}
				WHERE variable = {string:variable}',
				array(
					'value' => $value === true ? 'value + 1' : ($value === false ? 'value - 1' : $value),
					'variable' => $variable,
				)
			);
			$modSettings[$variable] = $value === true ? $modSettings[$variable] + 1 : ($value === false ? $modSettings[$variable] - 1 : $value);
		}

		// clean out the cache and make sure the cobwebs are gone too
		smfapi_cachePutData('modSettings', null, 90);

		return true;
	}

	$replaceArray = array();
	foreach ($changeArray as $variable => $value) {
		// don't bother if it's already like that ;).
		if (isset($modSettings[$variable]) && $modSettings[$variable] == $value) {
			continue;
        }
		// if the variable isn't set, but would only be set to nothing'ness, then don't bother setting it.
		elseif (!isset($modSettings[$variable]) && empty($value)) {
			continue;
        }

		$replaceArray[] = array($variable, $value);

		$modSettings[$variable] = $value;
	}

	if (empty($replaceArray)) {
		return false;
    }

	$smcFunc['db_insert']('replace',
		'{db_prefix}settings',
		array('variable' => 'string-255', 'value' => 'string-65534'),
		$replaceArray,
		array('variable')
	);

	// clear the cache of modsettings data
	smfapi_cachePutData('modSettings', null, 90);

    return true;
}

/**
 * Sets the login cookie
 *
 * Refreshes old cookie and session or creates new ones
 *
 * @param  int $cookie_length cookie length in seconds
 * @param  int $id the user's member id
 * @param  string $password the password already hashed with SMF's encryption
 * @return true on completion
 * @since  0.1.0
 */
function smfapi_setLoginCookie($cookie_length, $id, $password = '')
{
	global $cookiename, $boardurl, $modSettings;

	// if changing state force them to re-address some permission caching
	$_SESSION['mc']['time'] = 0;

	// the cookie may already exist, and have been set with different options
	$cookie_state = (empty($modSettings['localCookies']) ? 0 : 1) | (empty($modSettings['globalCookies']) ? 0 : 2);

	if (isset($_COOKIE[$cookiename])
        && preg_match('~^a:[34]:\{i:0;(i:\d{1,6}|s:[1-8]:"\d{1,8}");i:1;s:(0|40):"([a-fA-F0-9]{40})?";i:2;[id]:\d{1,14};(i:3;i:\d;)?\}$~', $_COOKIE[$cookiename]) === 1) {
		$array = @unserialize($_COOKIE[$cookiename]);

		// out with the old, in with the new
		if (isset($array[3]) && $array[3] != $cookie_state) {
			$cookie_url = smfapi_urlParts($array[3] & 1 > 0, $array[3] & 2 > 0);
			setcookie($cookiename, serialize(array(0, '', 0)), time() - 3600, $cookie_url[1], $cookie_url[0], !empty($modSettings['secureCookies']));
		}
	}

	// get the data and path to set it on
	$data = serialize(empty($id) ? array(0, '', 0) : array($id, $password, time() + $cookie_length, $cookie_state));
	$cookie_url = smfapi_urlParts(!empty($modSettings['localCookies']), !empty($modSettings['globalCookies']));

	// set the cookie, $_COOKIE, and session variable
	setcookie($cookiename, $data, time() + $cookie_length, $cookie_url[1], $cookie_url[0], !empty($modSettings['secureCookies']));

	// if subdomain-independent cookies are on, unset the subdomain-dependent cookie too
	if (empty($id) && !empty($modSettings['globalCookies'])) {
		setcookie($cookiename, $data, time() + $cookie_length, $cookie_url[1], '', !empty($modSettings['secureCookies']));
    }

	// any alias URLs?  this is mainly for use with frames, etc
	if (!empty($modSettings['forum_alias_urls'])) {
		$aliases = explode(',', $modSettings['forum_alias_urls']);

		$temp = $boardurl;
		foreach ($aliases as $alias) {
			// fake the $boardurl so we can set a different cookie
			$alias = strtr(trim($alias), array('http://' => '', 'https://' => ''));
			$boardurl = 'http://' . $alias;

			$cookie_url = smfapi_urlParts(!empty($modSettings['localCookies']), !empty($modSettings['globalCookies']));

			if ($cookie_url[0] == '') {
				$cookie_url[0] = strtok($alias, '/');
            }

			setcookie($cookiename, $data, time() + $cookie_length, $cookie_url[1], $cookie_url[0], !empty($modSettings['secureCookies']));
		}

		$boardurl = $temp;
	}

	$_COOKIE[$cookiename] = $data;

	// make sure the user logs in with a new session ID
	if (!isset($_SESSION['login_' . $cookiename])
        || $_SESSION['login_' . $cookiename] !== $data) {

		// version 4.3.2 didn't store the cookie of the new session
		if (version_compare(PHP_VERSION, '4.3.2') === 0) {
			$sessionCookieLifetime = @ini_get('session.cookie_lifetime');
			setcookie(session_name(), session_id(), time() + (empty($sessionCookieLifetime) ? $cookie_length : $sessionCookieLifetime), $cookie_url[1], $cookie_url[0], !empty($modSettings['secureCookies']));
		}

		$_SESSION['login_' . $cookiename] = $data;
	}

    return true;
}

/**
 * Session regenerate id
 *
 * Regenerate the session id. In case PHP version < 4.3.2
 *
 * @return bool whether session id was regenerated or not
 * @since  0.1.0
 */
if (!function_exists('session_regenerate_id')) {

	function session_regenerate_id()
	{
		// too late to change the session now
		if (headers_sent()) {
			return false;
        } else {
            session_id(strtolower(md5(uniqid(mt_rand(), true))));
        }

		return true;
	}
}

/**
 * Break the url into pieces
 *
 * Gets the domain and path for the cookie
 *
 * @param  bool $local whether local cookies are on
 * @param  bool $global whether global cookies are on
 * @return array with domain and path for the cookie to set using
 * @since  0.1.0
 */
function smfapi_urlParts($local, $global)
{
	global $boardurl;

	// parse the URL with PHP to make life easier
	$parsed_url = parse_url($boardurl);

	// are local cookies off?
	if (empty($parsed_url['path']) || !$local) {
		$parsed_url['path'] = '';
    }

	// globalize cookies across domains (filter out IP-addresses)?
	if ($global && preg_match('~^\d{1,3}(\.\d{1,3}){3}$~', $parsed_url['host']) == 0
        && preg_match('~(?:[^\.]+\.)?([^\.]{2,}\..+)\z~i', $parsed_url['host'], $parts) == 1) {

	    $parsed_url['host'] = '.' . $parts[1];
    }
    // we shouldn't use a host at all if both options are off
	elseif (!$local && !$global) {
		$parsed_url['host'] = '';
    }
	// the host also shouldn't be set if there aren't any dots in it
	elseif (!isset($parsed_url['host']) || strpos($parsed_url['host'], '.') === false) {
		$parsed_url['host'] = '';
    }

	return array($parsed_url['host'], $parsed_url['path'] . '/');
}

/**
 * Update forum statistics for new member registration
 *
 * Updates latest member || updates member counts
 *
 * @param  string $type the type of update (we're only doing member)
 * @param  int $parameter1 the user's member id || null
 * @param  string $parameter2 the user's real name || null
 * @return bool whether stats were updated or not
 * @since  0.1.0
 */
function smfapi_updateStats($type='member', $parameter1 = null, $parameter2 = null)
{
	global $sourcedir, $modSettings, $smcFunc;

	switch ($type) {

	    case 'member':
		    $changes = array(
			    'memberlist_updated' => time(),
		    );

		    // #1 latest member ID, #2 the real name for a new registration
		    if (is_numeric($parameter1)) {
			    $changes['latestMember'] = $parameter1;
			    $changes['latestRealName'] = $parameter2;

			    smfapi_updateSettings(array('totalMembers' => true), true);
		    }
		    // we need to calculate the totals.
		    else {
			    // Update the latest activated member (highest id_member) and count.
			    $result = $smcFunc['db_query']('', '
				    SELECT COUNT(*), MAX(id_member)
				    FROM {db_prefix}members
				    WHERE is_activated = {int:is_activated}',
				    array(
					    'is_activated' => 1,
				    )
			    );
			    list ($changes['totalMembers'], $changes['latestMember']) = $smcFunc['db_fetch_row']($result);
			    $smcFunc['db_free_result']($result);

			    // Get the latest activated member's display name.
			    $result = $smcFunc['db_query']('', '
				    SELECT real_name
				    FROM {db_prefix}members
				    WHERE id_member = {int:id_member}
				    LIMIT 1',
				    array(
					    'id_member' => (int) $changes['latestMember'],
				    )
			    );
			    list ($changes['latestRealName']) = $smcFunc['db_fetch_row']($result);
			    $smcFunc['db_free_result']($result);

			    // Are we using registration approval?
			    if ((!empty($modSettings['registration_method'])
                    && $modSettings['registration_method'] == 2)
                    || !empty($modSettings['approveAccountDeletion'])) {

				    // Update the amount of members awaiting approval - ignoring COPPA accounts, as you can't approve them until you get permission.
				    $result = $smcFunc['db_query']('', '
					    SELECT COUNT(*)
					    FROM {db_prefix}members
					    WHERE is_activated IN ({array_int:activation_status})',
					    array(
						    'activation_status' => array(3, 4),
					    )
				    );
				    list ($changes['unapprovedMembers']) = $smcFunc['db_fetch_row']($result);
				    $smcFunc['db_free_result']($result);
			    }
		    }

		smfapi_updateSettings($changes);
		break;

		default:
            return false;
	}

	return true;
}

/**
 * Removes special entities from strings
 *
 * Fixes strings with special characters for compatibility with db
 *
 * @param  string $string the input string to be cleaned
 * @return string the string with special entities removed
 * @since  0.1.0
 */
function smfapi_unHtmlspecialchars($string)
{
	static $translation;

	if (!isset($translation)) {
		$translation = array_flip(get_html_translation_table(HTML_SPECIALCHARS, ENT_QUOTES)) + array('&#039;' => '\'', '&nbsp;' => ' ');
    }

	return strtr($string, $translation);
}

/**
 * Delete personal messages
 *
 * Deletes the personal messages for a member or an array of members.
 * Called by the delete member function
 *
 * @param  array $personal_messages the messages to delete
 * @param  string $folder the folder to delete from
 * @param  int || array $owner the member id(s) that need message deletion
 * @return bool whether deletion occurred or not
 * @since  0.1.0
 */
function smfapi_deleteMessages($personal_messages, $folder = null, $owner = null)
{
	global $user_info, $smcFunc;

	if ($owner === null) {
		$owner = array($user_info['id']);
    } elseif (empty($owner)) {
		return false;
    } elseif (!is_array($owner)) {
		$owner = array($owner);
    }

	if (null !== $personal_messages) {
		if (empty($personal_messages) || !is_array($personal_messages)) {
			return false;
        }

		foreach ($personal_messages as $index => $delete_id) {
			$personal_messages[$index] = (int) $delete_id;
        }

		$where = 'AND id_pm IN ({array_int:pm_list})';
	} else {
		$where = '';
    }

	if ('sent' == $folder || null === $folder) {
		$smcFunc['db_query']('', '
			UPDATE {db_prefix}personal_messages
			SET deleted_by_sender = {int:is_deleted}
			WHERE id_member_from IN ({array_int:member_list})
				AND deleted_by_sender = {int:not_deleted}' . $where,
			array(
				'member_list' => $owner,
				'is_deleted' => 1,
				'not_deleted' => 0,
				'pm_list' => $personal_messages !== null ? array_unique($personal_messages) : array(),
			)
		);
	}

	if ('sent' != $folder || null === $folder) {
		// calculate the number of messages each member's gonna lose...
		$request = $smcFunc['db_query']('', '
			SELECT id_member, COUNT(*) AS num_deleted_messages, CASE WHEN is_read & 1 >= 1 THEN 1 ELSE 0 END AS is_read
			FROM {db_prefix}pm_recipients
			WHERE id_member IN ({array_int:member_list})
				AND deleted = {int:not_deleted}' . $where . '
			GROUP BY id_member, is_read',
			array(
				'member_list' => $owner,
				'not_deleted' => 0,
				'pm_list' => $personal_messages !== null ? array_unique($personal_messages) : array(),
			)
		);
		// ...and update the statistics accordingly - now including unread messages
		while ($row = $smcFunc['db_fetch_assoc']($request)) {
			if ($row['is_read']) {
				smfapi_updateMemberData($row['id_member'], array('instant_messages' => $where == '' ? 0 : 'instant_messages - '
                                 . $row['num_deleted_messages']));
            } else {
				smfapi_updateMemberData($row['id_member'], array('instant_messages' => $where == '' ? 0 : 'instant_messages - '
                                 . $row['num_deleted_messages'], 'unread_messages' => $where == '' ? 0 : 'unread_messages - '
                                 . $row['num_deleted_messages']));
            }

			// if this is the current member we need to make their message count correct
			if ($user_info['id'] == $row['id_member']) {
				$user_info['messages'] -= $row['num_deleted_messages'];
				if (!($row['is_read']))
					$user_info['unread_messages'] -= $row['num_deleted_messages'];
			}
		}

		$smcFunc['db_free_result']($request);

		// do the actual deletion
		$smcFunc['db_query']('', '
			UPDATE {db_prefix}pm_recipients
			SET deleted = {int:is_deleted}
			WHERE id_member IN ({array_int:member_list})
				AND deleted = {int:not_deleted}' . $where,
			array(
				'member_list' => $owner,
				'is_deleted' => 1,
				'not_deleted' => 0,
				'pm_list' => $personal_messages !== null ? array_unique($personal_messages) : array(),
			)
		);
	}

	// if sender and recipients all have deleted their message, it can be removed
	$request = $smcFunc['db_query']('', '
		SELECT pm.id_pm AS sender, pmr.id_pm
		FROM {db_prefix}personal_messages AS pm
			LEFT JOIN {db_prefix}pm_recipients AS pmr ON (pmr.id_pm = pm.id_pm AND pmr.deleted = {int:not_deleted})
		WHERE pm.deleted_by_sender = {int:is_deleted}
			' . str_replace('id_pm', 'pm.id_pm', $where) . '
		GROUP BY sender, pmr.id_pm
		HAVING pmr.id_pm IS null',
		array(
			'not_deleted' => 0,
			'is_deleted' => 1,
			'pm_list' => $personal_messages !== null ? array_unique($personal_messages) : array(),
		)
	);

	$remove_pms = array();

	while ($row = $smcFunc['db_fetch_assoc']($request)) {
		$remove_pms[] = $row['sender'];
    }

	$smcFunc['db_free_result']($request);

	if (!empty($remove_pms)) {
		$smcFunc['db_query']('', '
			DELETE FROM {db_prefix}personal_messages
			WHERE id_pm IN ({array_int:pm_list})',
			array(
				'pm_list' => $remove_pms,
			)
		);

		$smcFunc['db_query']('', '
			DELETE FROM {db_prefix}pm_recipients
			WHERE id_pm IN ({array_int:pm_list})',
			array(
				'pm_list' => $remove_pms,
			)
		);
	}

	// any cached numbers may be wrong now
	smfapi_cachePutData('labelCounts:' . $user_info['id'], null, 720);

	return true;
}

/**
 * Generate validation code
 *
 * Generate a random validation code for registration purposes
 *
 * @return string random validation code (10 char)
 * @since  0.1.0
 */
function smfapi_generateValidationCode()
{
	global $smcFunc, $modSettings;

	$request = $smcFunc['db_query']('get_random_number', '
		SELECT RAND()',
		array(
		)
	);

	list ($dbRand) = $smcFunc['db_fetch_row']($request);
	$smcFunc['db_free_result']($request);

	return substr(preg_replace('/\W/', '', sha1(microtime()
           . mt_rand() . $dbRand . $modSettings['rand_seed'])), 0, 10);
}

/**
 * Check if user is online
 *
 * Checks whether the specified user is online or not
 *
 * @param string || int $username the username, member id or email of the user
 * @return bool whether the user is online or not
 * @since  0.1.0
 */
function smfapi_isOnline($username='')
{
	global $smcFunc;

    $user_data = smfapi_getUserData($username);

    if (!$user_data) {
        return false;
    }

    $request = $smcFunc['db_query']('', '
		SELECT lo.id_member
		FROM {db_prefix}log_online AS lo
		WHERE lo.id_member = {int:id_member}',
		array(
			'id_member' => $user_data['id_member'],
		)
	);

    if ($smcFunc['db_num_rows']($request) == 0) {
        return false;
	} else {
        $smcFunc['db_free_result']($request);
        return true;
	}
}

/**
 * Log user online
 *
 * Logs a user online, if their settings allow it
 *
 * @param string || int $username the username, member id or email of the user
 * @return bool whether they were logged online or not
 * @since  0.1.0
 */
function smfapi_logOnline($username='')
{
    global $smcFunc, $modSettings;

    $user_data = smfapi_getUserData($username);

    if (!$user_data) {
        return false;
    }

    if (!$user_data['show_online']) {
        return false;
    }

    if (smfapi_isOnline($username)) {
        $do_delete = true;
    } else {
        $do_delete = false;
    }

    $smcFunc['db_query']('', '
        DELETE FROM {db_prefix}log_online
        WHERE ' . ($do_delete ? 'log_time < {int:log_time}' : '')
                . ($do_delete && !empty($user_data['id_member']) ? ' OR ' : '')
                . (empty($user_data['id_member']) ? '' : 'id_member = {int:current_member}'),
        array(
            'current_member' => $user_data['id_member'],
            'log_time' => time() - $modSettings['lastActive'] * 60,
        )
    );

    $smcFunc['db_insert']($do_delete ? 'ignore' : 'replace',
        '{db_prefix}log_online',
        array('session'   => 'string',
              'id_member' => 'int',
              'id_spider' => 'int',
              'log_time'  => 'int',
              'ip'        => 'raw',
              'url'       => 'string'),
        array(session_id(),
              $user_data['id_member'],
              0,
              time(),
              'IFNULL(INET_ATON(\'' . (isset($user_data['ip']) ? $user_data['ip'] : '') . '\'), 0)',
              ''),
        array('session')
    );

	// Mark the session as being logged.
	$_SESSION['log_time'] = time();

	// Well, they are online now.
	if (empty($_SESSION['timeOnlineUpdated'])) {
		$_SESSION['timeOnlineUpdated'] = time();
    }

    return true;
}

/**
 * Find a file
 *
 * Locates a file given directories to search
 *
 * @param  array $files the files to search
 * @param  string $search the file or filetype we're searching for
 * @return array $matches the matching files found
 * @since  0.1.0
 */
function smfapi_getMatchingFile($files, $search)
{
    $matches = array();

    // split to name and filetype
    if (strpos($search, ".")) {
        $baseexp = substr($search, 0, strpos($search, "."));
        $typeexp = substr($search, strpos($search, ".") +1, strlen($search));
    } else {
        $baseexp = $search;
        $typeexp = "";
    }

    // escape all regexp characters
    $baseexp = preg_quote($baseexp);
    $typeexp = preg_quote($typeexp);

    // allow ? and *
    $baseexp = str_replace(array("\*", "\?"), array(".*", "."), $baseexp);
    $typeexp = str_replace(array("\*", "\?"), array(".*", "."), $typeexp);

    // search for matches
    $i=0;
    foreach ($files as $file) {
        $filename = basename($file);

        if (strpos($filename, '.')) {
            $base = substr($filename, 0, strpos($filename, '.'));
            $type = substr($filename, strpos($filename, '.') +1, strlen($filename));
        } else {
            $base = $filename;
            $type = '';
        }

        if (preg_match("/^" . $baseexp . "$/i", $base)
            && preg_match("/^" . $typeexp . "$/i", $type)) {

            $matches[$i] = $file;
            $i++;
        }
    }

    return $matches;
}

/**
 * Returns all the files of a directory and subdirectories
 *
 * @param  string $directory the absolute path of directory to begin searching
 * @param  array $exempt files to exclude
 * @param  array $files the files found passed by reference
 * @return array $files the files found
 * @since  0.1.0
 */
function smfapi_getDirectoryContents($directory, $exempt = array('.', '..'), &$files = array()) {
    $handle = opendir($directory);
    while (false !== ($resource = readdir($handle))) {
        if (!in_array(strtolower($resource), $exempt)) {
            if (is_dir($directory . $resource . '/')) {
                array_merge($files,
                    smfapi_getDirectoryContents($directory . $resource . '/', $exempt, $files));
            } else {
                $files[] = $directory . $resource;
            }
        }
    }

    closedir($handle);

    return $files;
}
?>
