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

	if ($message->{'body'} =~ /^\!bug\s*#?(\d{1,5})/i) {
		return "http://sourceforge.net/p/ufoai/bugs/$1/";
	} elsif ($message->{'body'} =~ /^\!fr\s*#?(\d{1,5})/i) {
		return "http://sourceforge.net/p/ufoai/feature-requests/$1/";
	} elsif ($message->{'body'} =~ /^\!patch\s*#?(\d+)/i) {
		return "http://sourceforge.net/p/ufoai/patches/$1/";
	} elsif ($message->{'body'} =~ /^\!r(?:ev)?\s*#?([0-9a-f]{6,40})/i) {
		return "https://github.com/ufoai/ufoai/commit/$1/";
	} elsif ($message->{'body'} =~ /^\!faq\s*#?(\w*)?/i) {
		return "http://ufoai.org/wiki/index.php/Manual:FAQ#$1";
	} elsif ($message->{'body'} =~ /^\!todo/i) {
		return "http://ufoai.org/wiki/index.php/TODO";
	} elsif ($message->{'body'} =~ /^\!topic\s*#?(\d+)(?:\s*[\.#\s](\d+))?/i) {
		return "http://ufoai.org/forum/index.php?topic=$1".( (defined($2)) ? ".msg$2#msg$2" : "" ) ;
	}
}

sub help {
	return
		"UFOAI Channel Bot\n".
		"!bug #tracker-id\n".
		"!patch #tracker-id\n".
		"!fr #tracker-id\n".
		"!rev #git-hash\n".
		"!faq [#section_name]\n".
		"!todo\n".
		"!topic #forum-topic-id [.message-id]\n";
}

my $bot = UFOAIBot->new(%options)->run();
