<?php
include "internal/auth.php";

if (true === auth()) {
	echo getUserId();
	exit();
}

echo "0";
