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

	# New Bug/Feature/Patch Tracker links
	if ($message->{'body'} =~ /^\!bug\s*#?(\d{1,5})/i) {
		return "http://ufoai.org/bugs/ufoalieninvasion/issues/$1/";
	} elsif ($message->{'body'} =~ /^\!fr\s*#?(\d{1,5})/i) {
		return "http://ufoai.org/bugs/ufoalieninvasion/issues/$1/";
	} elsif ($message->{'body'} =~ /^\!patch\s*#?(\d+)/i) {
		return "http://ufoai.org/bugs/ufoalieninvasion/issues/$1/";
	} elsif ($message->{'body'} =~ /^\!issue\s*#?(\d+)/i) {
		return "http://ufoai.org/bugs/ufoalieninvasion/issues/$1/";
	}

	# SourceForge Bug/Feature/Patch Tracker links
	elsif ($message->{'body'} =~ /^\!sfbug\s*#?(\d{1,5})/i) {
		return "http://sourceforge.net/p/ufoai/bugs/$1/";
	} elsif ($message->{'body'} =~ /^\!sffr\s*#?(\d{1,5})/i) {
		return "http://sourceforge.net/p/ufoai/feature-requests/$1/";
	} elsif ($message->{'body'} =~ /^\!sfpatch\s*#?(\d+)/i) {
		return "http://sourceforge.net/p/ufoai/patches/$1/";
	}

	# GitHub Commit Links
	elsif ($message->{'body'} =~ /^\!r(?:ev)?\s*#?([0-9a-f]{6,40})/i) {
		return "https://github.com/ufoai/ufoai/commit/$1/";
	}

	# Wiki Links
	elsif ($message->{'body'} =~ /^\!faq\s*#?(\w*)?/i) {
		return "http://ufoai.org/wiki/index.php/Manual:FAQ#$1";
	} elsif ($message->{'body'} =~ /^\!todo/i) {
		return "http://ufoai.org/wiki/index.php/TODO";
	}

	# Forum Links
	elsif ($message->{'body'} =~ /^\!topic\s*#?(\d+)(?:\s*[\.#\s](\d+))?/i) {
		return "http://ufoai.org/forum/index.php?topic=$1".( (defined($2)) ? ".msg$2#msg$2" : "" ) ;
	}
}

sub help {
	return
		"UFOAI Channel Bot\n".
		"!issue #tracker-id :: Link to the new issue tracker\n".
		"!bug #tracker-id :: Alias to the !issue command\n".
		"!patch #tracker-id :: Alias to the !issue command\n".
		"!fr #tracker-id :: Alias to the !issue command\n".
		"!sfbug #tracker-id :: Link to the SourceForge bug tracker\n".
		"!sfpatch #tracker-id :: Link to the SourceForge patch tracker\n".
		"!sffr #tracker-id :: Link to the SourceForge feature request tracker\n".
		"!rev #git-hash :: Link to the commit on GitHub\n".
		"!faq [#section_name] :: Link to FAQ on the Wiki\n".
		"!todo :: Link to the TODO Lists on the Wiki\n".
		"!topic #forum-topic-id [.message-id] :: Link to a topic on the Forum\n".
		"";
}

my $bot = UFOAIBot->new(%options)->run();
