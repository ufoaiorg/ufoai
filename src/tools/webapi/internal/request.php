<?php

function getRequestCGame() {
	$r = $_REQUEST ['cgame'];
	if (! isset ( $r ))
		error ( "missing request parameter: cgame", 400 );
	$valid = array('-', '_');
	if (!ctype_alnum(str_replace($valid, '', $r))) {
		error ( "invalid request parameter given: " . $r, 400 );
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
	if (false === hasRequestUserId ())
		error ( "missing request parameter: userid", 400 );
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
		error ( "missing request parameter: category", 400 );
	return intval ( $r );
}

function getRequestFile() {
	$r = basename ( $_REQUEST ['file'] );
	if (! isset ( $r ))
		error ( "missing request parameter: file", 400 );
	$valid = array('-', '_', '.');
	if (!ctype_alnum(str_replace($valid, '', $r))) {
		error ( "invalid request parameter given: " . $r, 400 );
	}
	return $r;
}
