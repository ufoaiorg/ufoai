<?php
include "internal/webapi.php";

if (true === auth ()) {
	echo getUserId ();
	exit ();
}

echo "0";
