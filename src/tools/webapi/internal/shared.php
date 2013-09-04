<?php

const DEBUG = false;
if (DEBUG) {
	error_reporting ( E_ALL | E_STRICT | E_DEPRECATED );
} else {
	// do not disclose information
	error_reporting ( 0 );
}

function error($message) {
	http_response_code ( 403 );
	error_log ( $message );
	// stdout = fopen('php://stdout', 'w');
	// write($stdout, "$message\n");
	die ( $message );
}
