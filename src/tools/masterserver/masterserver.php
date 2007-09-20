<?php

$standardPort = 27910;
$serverList = "serverlist.txt";
#every 300 seconds should be a heartbeat from the server
$serverTimeoutSeconds = "320";

###############################################################################
# NOTES
# Create a file names serverlist.txt in the same directory as the
# masterserver.php is. Make sure, that is writeable for the masterserver script
###############################################################################
# FORMAT OF serverlist.txt
###############################################################################
# First line is a number - the amount of servers in the list
# every other line is a server
# * ip
# * port
# * last heartbeat timestamp (used to clean the list of outdated servers)
# these fields are space seperated
###############################################################################


###############################################################################
# Helper functions
###############################################################################

function updateServerList ($remove, $ip, $port)
{
	# seconds since 1970/01/01
	$time = time();
	$ip = $HTTP_SERVER_VARS["REMOTE_ADDR"];
	if (isset($_GET["port"]))
		$port = $_GET["port"];
	else
		$port = $GLOBALS["standardPort"];

	$newListContent = "";

	$file = @fopen($GLOBALS["serverList"], 'r');
	if (!$file)
		return "";
	/* first entry/line in the file is the number of servers in the list */
	$num = fgets($file, 100);

	$updatedServer = 0;

	while (!feof($file)) {
		$skipThisServer = 0;
		/* get the hole server line */
		$server = fgets($file, 100);
		/* split ip, port, last heartbeat */
		$data = explode(" ", $server);
		if ($time > $data[2] + $GLOBALS["serverTimeoutSeconds"]) {
			# don't readd this server - time out
		} else if (!strcmp($data[0], $ip) && !strcmp($data[1], $port)) {
			if (!$remove) {
				# heartbeat
				$newListContent = "$data[0] $data[1] " . $time . "\n";
				$updatedServer = 1;
			}
		} else {
			$newListContent = "$data[0] $data[1] $data[2]\n";
		}
	}

	fclose($file);

	$file = @fopen($GLOBALS["serverList"], 'w');
	if (!$file) {
		echo "Error - could not write " . $GLOBALS["serverList"];
		exit;
	}
	fwrite($file, $newListContent);
	# new server
	if (!$updatedServer) {
		fwrite($file, "$ip $port $time");
	}
	fclose($file);
}

###############################################################################
# Protocoll functions
###############################################################################

# Update the heartbeat entry for the given server
# Or add a new server to the list of there is no such entry
function serverHeartbeat ()
{
	updateServerList(0);
}

# Remove the given server from the list
function serverHeartShutdown ()
{
	updateServerList(1);
}

# Send the serverlist to the client
function sendServerList ()
{
	$file = @fopen($GLOBALS["serverList"], 'r');
	if (!$file) {
		echo "Error - could not read " . $GLOBALS["serverList"];
		exit;
	}
	echo fread($file, filesize($GLOBALS["serverList"]));
	fclose($file);
}

# entry point
function main ()
{
	/* check which action the caller wants to do */
	if (isset($_GET["heartbeat"])) {
		serverHeartbeat();
	} else if (isset($_GET["shutdown"])) {
		serverShutdown();
	} else if (isset($_GET["query"])) {
		sendServerList();
	} else
		echo 'Invalid command';
}

###############################################################################

main();
exit;

?>