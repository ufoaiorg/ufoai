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

function readServerList ($listArray)
{
	$file = @fopen($GLOBALS["serverList"], 'r');
	if (!$file)
		return "";
	/* first entry/line in the file is the number of servers in the list */
	$num = fgets($file, 100);

	while (!feof($file)) {
		$i++;
		$server = fgets($file, 100);
		/* split ip, port, last heartbeat */
		$data = explode(" ", $server);
		/* TODO */
	}

	fclose($file);

	if ($i != $num) {
		echo "Error in serverlist";
		exit;
	}
}

###############################################################################
# Protocoll functions
###############################################################################

# Update the heartbeat entry for the given server
# Or add a new server to the list of there is no such entry
function serverHeartbeat ()
{
	if (isset($_GET["port"]))
		$port = $_GET["port"];
	else
		$port = $GLOBALS["standardPort"];

	# seconds since 1970/01/01
	$time = time();
}

# Remove the given server from the list
function serverHeartShutdown ()
{
	if (isset($_GET["port"]))
		$port = $_GET["port"];
	else
		$port = $GLOBALS["standardPort"];
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