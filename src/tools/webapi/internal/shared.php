<?php

const DEBUG = false;
if (DEBUG) {
	error_reporting ( E_ALL | E_STRICT | E_DEPRECATED );
} else {
	// do not disclose information
	error_reporting ( 0 );
}

function error($message, $error = 500) {
	http_response_code ( $error );
	error_log ( $message );
	die ( "Error processing the request" );
}
