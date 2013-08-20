<?php
include "internal/auth.php";

if (true === auth()) {
	echo "1";
	exit();
}

echo "0";
