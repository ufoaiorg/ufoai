#!/usr/bin/perl -w

use Net::IRC;
use strict;

# create the IRC object
my $irc = new Net::IRC;

# Create a connection object.  You can have more than one "connection" per
# IRC object, but we'll just be working with one.
my $conn = $irc->newconn(
	Server 		=> shift || 'irc.freenode.org',
	Port		=> shift || '6667',
	Nick		=> 'UFOAIBot',
	Ircname		=> 'UFOAIBot',
	Username	=> 'UFOAIBot',
	LocalAddr	=> 'ufo.ninex.info'
);

# We're going to add this to the conn hash so we know what channel we
# want to operate in.
$conn->{channel} = shift || '#ufoai';

sub handleCommand($$) {
	my ($receiver, $text) = @_;

	if ($text =~ /^\!bug\s*#?(\d+)/i) {
		$conn->privmsg($receiver, "https://sourceforge.net/tracker/index.php?func=detail&aid=$1&group_id=157793&atid=805242");
	} elsif ($text =~ /^\!fr\s*#?(\d+)/i) {
		$conn->privmsg($receiver, "https://sourceforge.net/tracker/index.php?func=detail&aid=$1&group_id=157793&atid=805245");
	} elsif ($text =~ /^\!patch\s*#?(\d+)/i) {
		$conn->privmsg($receiver, "https://sourceforge.net/tracker/index.php?func=detail&aid=$1&group_id=157793&atid=805244");
	} elsif ($text =~ /^\!r(?:ev)?\s*#?(\d+)/i) {
		$conn->privmsg($receiver, "http://ufoai.svn.sourceforge.net/viewvc/ufoai?view=rev&revision=$1");
		$conn->privmsg($receiver, "http://sourceforge.net/apps/trac/ufoai/changeset/$1");
	} elsif ($text =~ /^\S*\s+\*\s+r(\d+)\s*.*/) {
		$conn->privmsg($receiver, "http://ufoai.svn.sourceforge.net/viewvc/ufoai?view=rev&revision=$1");
		$conn->privmsg($receiver, "http://sourceforge.net/apps/trac/ufoai/changeset/$1");
	} elsif ($text =~ /^\!ticket\s*#?(\d+)/i) {
		$conn->privmsg($receiver, "https://sourceforge.net/apps/trac/ufoai/ticket/$1");
	} elsif ($text =~ /^\!faq\s*#?(\w*)?/i) {
		$conn->privmsg($receiver, "http://ufoai.ninex.info/wiki/index.php/FAQ#$1");
	} elsif ($text =~ /^\!todo/i) {
		$conn->privmsg($receiver, "http://ufoai.ninex.info/wiki/index.php/TODO");
	} elsif ($text =~ /^\!topic\s*#?(\d+)(?:\s*[\.#\s](\d+))?/i) {
		$conn->privmsg($receiver, "http://ufoai.ninex.info/forum/index.php?topic=$1".( (defined($2)) ? ".msg$2#msg$2" : "" ) );
	} else {
		# unknown command
		return 0;
	}

	return 1;
}

sub on_connect {
	# shift in our connection object that is passed automatically
	my $conn = shift;

	# when we connect, join our channel and greet it
	$conn->join($conn->{channel});
	$conn->{connected} = 1;
}

sub on_public {
	# shift in our connection object that is passed automatically
	my ($conn, $event) = @_;

	# this is what was said in the event
	my $text = $event->{args}[0];
	my $channel = $event->{to}[0];

	handleCommand($channel, $text);
}

sub on_msg {
	# shift in our connection object that is passed automatically
	my ($conn, $event) = @_;

	# this is what was said in the event
	my $text = $event->{args}[0];
	my $nick = $event->{nick};

	if ($text =~ /help/) {
		$conn->privmsg($nick, "UFOAI Channel Bot");
		$conn->privmsg($nick, "!bug #tracker-id");
		$conn->privmsg($nick, "!patch #tracker-id");
		$conn->privmsg($nick, "!fr #tracker-id");
		$conn->privmsg($nick, "!rev #svn-revision");
		$conn->privmsg($nick, "!ticket #trac-ticket-id");
		$conn->privmsg($nick, "!faq [#section_name]");
		$conn->privmsg($nick, "!todo");
		$conn->privmsg($nick, "!topic #forum-topic-id [.message-id]");
	} else {
		if (!handleCommand($nick, $text)) {
			$conn->privmsg($nick, "I'm just a bot - ask me for 'help' to get more information");
		}
	}
}

# add event handlers
$conn->add_handler('public', \&on_public);
$conn->add_handler('msg', \&on_msg);

# The end of MOTD (message of the day), numbered 376 signifies we've connect
$conn->add_handler('376', \&on_connect);

# start IRC
$irc->start();

