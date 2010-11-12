#!/usr/bin/perl

package UFOAIBot;

use strict;
use warnings;
use base qw/Bot::BasicBot/;

# default connection options
my %options = (
	server 		=> shift || 'irc.freenode.org',
	port		=> shift || '6667',
	nick		=> 'UFOAIBot',
	name		=> 'UFOAIBot',
	username	=> 'UFOAIBot',
	channels	=> [shift || '#ufoai'],
);

sub said {
	my ($self, $message) = @_;

	if ($message->{'body'} =~ /^\!bug\s*#?(\d+)/i) {
		return "https://sourceforge.net/tracker/index.php?func=detail&aid=$1&group_id=157793&atid=805242";
	} elsif ($message->{'body'} =~ /^\!fr\s*#?(\d+)/i) {
		return "https://sourceforge.net/tracker/index.php?func=detail&aid=$1&group_id=157793&atid=805245";
	} elsif ($message->{'body'} =~ /^\!patch\s*#?(\d+)/i) {
		return "https://sourceforge.net/tracker/index.php?func=detail&aid=$1&group_id=157793&atid=805244";
	} elsif ($message->{'body'} =~ /^\!r(?:ev)?\s*#?([0-9a-f]{6,40})/i) {
		return "http://ufoai.git.sourceforge.net/git/gitweb.cgi?p=ufoai/ufoai;a=commitdiff;h=$1";
	} elsif ($message->{'body'} =~ /^\!r(?:ev)?\s*#?(\d{1,5})/i) {
		return "http://sourceforge.net/apps/trac/ufoai/changeset/$1";
	} elsif ($message->{'body'} =~ /^\!ticket\s*#?(\d+)/i) {
		return "https://sourceforge.net/apps/trac/ufoai/ticket/$1";
	} elsif ($message->{'body'} =~ /^\!faq\s*#?(\w*)?/i) {
		return "http://ufoai.ninex.info/wiki/index.php/FAQ#$1";
	} elsif ($message->{'body'} =~ /^\!todo/i) {
		return "http://ufoai.ninex.info/wiki/index.php/TODO";
	} elsif ($message->{'body'} =~ /^\!topic\s*#?(\d+)(?:\s*[\.#\s](\d+))?/i) {
		return "http://ufoai.ninex.info/forum/index.php?topic=$1".( (defined($2)) ? ".msg$2#msg$2" : "" ) ;
	} elsif ($message->{'body'} =~ /^\!/) {
		return "I'm just a bot - ask me for 'help' to get more information";
	}
}

sub help {
	return
		"UFOAI Channel Bot\n".
		"!bug #tracker-id\n".
		"!patch #tracker-id\n".
		"!fr #tracker-id\n".
		"!rev #svn-revision\n".
		"!rev #git-hash\n".
		"!ticket #trac-ticket-id\n".
		"!faq [#section_name]\n".
		"!todo\n".
		"!topic #forum-topic-id [.message-id]\n";
}

my $bot = UFOAIBot->new(%options)->run();
