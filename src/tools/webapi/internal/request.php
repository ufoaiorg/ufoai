<?php

function getRequestCGame() {
	$r = $_REQUEST ['cgame'];
	if (! isset ( $r ))
		error ( "missing request parameter" );
	$valid = array('-', '_');
	if (!ctype_alnum(str_replace($valid, '', $r))) {
		error ( "invalid request parameter given" );
	}
	return $r;
}

function getUsername() {
	$r = $_REQUEST ['username'];
	if (! isset ( $r ))
		return "";
	return $r;
}

function getPassword() {
	$r = $_REQUEST ['password'];
	if (! isset ( $r ))
		return "";
	return $r;
}

function getRequestUserId() {
	if (false === hasRequestUserId ( $r ))
		error ( "missing request parameter" );
	$r = $_REQUEST ['userid'];
	return intval ( $r );
}

function hasRequestUserId() {
	$r = $_REQUEST ['userid'];
	return isset ( $r ) && intval ( $r ) > 0;
}

function getRequestCategory() {
	$r = $_REQUEST ['category'];
	if (! isset ( $r ))
		error ( "missing request parameter" );
	return intval ( $r );
}

function getRequestFile() {
	$r = $_REQUEST ['file'];
	if (! isset ( $r ))
		error ( "missing request parameter" );
	$valid = array('-', '_');
	if (!ctype_alnum(str_replace($valid, '', $r))) {
		error ( "invalid request parameter given" );
	}
	return $r;
}
