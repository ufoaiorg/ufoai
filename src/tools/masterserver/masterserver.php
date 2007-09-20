<?php

$standardPort = 27910;
$serverList = "serverlist.txt";
#every 300 seconds should be a heartbeat from the server
$serverTimeoutSeconds = "320";

###############################################################################
# NOTES
# Create a file names serverlist.txt in the same directory as the
# masterserver.php is. Make sure, that is writeable for the masterserver script
#
# The server list is updated with every call, new servers are added with 'ping',
# updated (otherwise they would time out) with 'heartbeat' and removed with
# 'shutdown'. The clients query with 'query' to get the hole list.
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

# Update the server list
# Checks whether a server timed out, or is going to be removed ($remove == 1)
# Even new servers are added with this function ($add == 1)
# TODO FIXME Lock the file in case of more than one query at a time
function updateServerList ($remove, $add)
{
	# seconds since 1970/01/01
	$time = time();
	$ip = $_SERVER["REMOTE_ADDR"];
	if (!$ip)
		$ip = $HTTP_SERVER_VARS["REMOTE_ADDR"];

	if (isset($_GET["port"]))
		$port = $_GET["port"];
	else
		$port = $GLOBALS["standardPort"];

	# this string is stored in the serverlist.txt
	$newListContent = "";
	# this string is send to a client (if called from sendServerList)
	$serverListStr = "";

	$file = file($GLOBALS["serverList"]);

	$updatedServer = 0;
	$i = 0;

	for ($j = 0; $j < sizeof($file); $j++) {
		$skipThisServer = 0;
		/* split ip, port, last heartbeat */
		$data = explode(" ", $file[$j]);
		if ($time > $data[2] + $GLOBALS["serverTimeoutSeconds"]) {
			# don't readd this server - timed out
		} else if (!strcmp($data[0], $ip) && !strcmp($data[1], $port)) {
			if (!$remove) {
				# heartbeat
				$newListContent .= "$data[0] $data[1] $time\n";
				# the client is only interested in ip and port
				$serverListStr .= "$data[0] $data[1]\n";
				$updatedServer = 1;
				$i++;
			}
		} else {
			$i++;
			# now updates - so write it back
			$newListContent .= $file[$j];
			# the client is only interested in ip and port
			$serverListStr .= "$data[0] $data[1]\n";
		}
	}

	$file = @fopen($GLOBALS["serverList"], 'w');
	if (!$file) {
		echo "Error - could not write " . $GLOBALS["serverList"];
		return;
	}

	fwrite($file, "$i\n");
	# new server
	if (!$updatedServer && $add) {
		fwrite($file, "$ip $port $time\n");
		$serverListStr .= "$ip $port $time\n";
	}
	fwrite($file, $newListContent);
	fclose($file);
	return "$i\n$serverListStr";
}

###############################################################################
# Protocoll functions
###############################################################################

# Update the heartbeat entry for the given server
function serverHeartbeat ()
{
	updateServerList(0, 0);
#	echo "OK";
}

# add a new server to the list
function serverPing ()
{
	updateServerList(0, 1);
#	echo "OK";
}

# Remove the given server from the list
function serverShutdown ()
{
	updateServerList(1, 0);
#	echo "OK";
}

# Send the serverlist to the client
function sendServerList ()
{
	$time = time();

	# print the list
	echo file($GLOBALS["serverList"]);
#	updateServerList(0, 0);
}

# entry point
function main ()
{
	/* check which action the caller wants to do */
	if (isset($_GET["heartbeat"])) {
		serverHeartbeat();
	} else if (isset($_GET["ping"])) {
		serverPing();
	} else if (isset($_GET["shutdown"])) {
		serverShutdown();
	} else if (isset($_GET["query"])) {
		sendServerList();
	} else
		echo 'Masterserver query: Invalid command';
}

###############################################################################

main();
exit;

?>