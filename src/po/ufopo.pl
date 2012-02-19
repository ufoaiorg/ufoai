#!/usr/bin/perl

# This script parses the *.ufo files in base/ufos for translateable strings
# This is called from Makefile

# The script is taken from Wesnoth (wmlxgettext) and adopted for our needs

use POSIX qw(strftime);
use Getopt::Long;
use File::Basename;

sub raw2postring
{
	my $str = shift;

	$str =~ s/\\"$//g;
	$str =~ s/^(.*)$/"$1\\n"/mg;
	$str =~ s/\n$/\n"\\n"/mg;
	$str =~ s/\\n\"$/\"\n/g;

	return $str;
}

our ($str,$translatable,$line,%messages);

our $toplevel = '.';
GetOptions ('directory=s' => \$toplevel);

chdir $toplevel;

foreach my $file (@ARGV)
{
	open ( FILE, "<$file" ) or die "cannot read from $file";
	LINE: while (<FILE>)
	{
		# skip comments //
		next LINE if m/^\s*\/\//;

		# single-line quoted string description
		if (!defined $str and m/\"_(.*)\"/)
		{
			# ie. translatable
			push @{$messages{raw2postring($1)}}, "$file:$." if ($1 ne '');

			# process remaining of the line
			$_ = $3 . "\n";
			redo LINE;
		}
=for TODO: Add multiline support
		# start of multi-line
		elsif (!defined $str and m/^(?:[^\"]*?)((?:_\s*)?)\s*\"([^\"]*)/)
		{
			$translatable = ($1 ne '');
			$_ = $2;
			if (m/(.*)\r/) { $_ = "$1\n"; }
			$str = $_;
			$line = $.;
		}
		# end of multi-line
		elsif (m/(.*?)\"(.*)/)
		{
			die "end of string without a start in $file" if !defined $str;

			$str .= $1;

			push @{$messages{"\"\"\n" . raw2postring($str)}}, "$file:$."
				if $translatable;
			$str = undef;

			# process remaining of the line
			$_ = $2 . "\n";
			redo LINE;
		}
		# end of multi-line
		elsif (defined $str)
		{
			if (m/(.*)\r/) { $_ = "$1\n"; }
			$str .= $_;
		}
=cut
	}
	close ( FILE );
	print STDERR "Processed $file\n";
}

## index strings by their location in the source so we can sort them

our @revmessages;
foreach my $key (keys %messages)
{
	foreach my $line (@{$messages{$key}})
	{
		my ($file, $lineno) = split /:/, $line;
		push @revmessages, [ $file, $lineno, $key ];
	}
}

# sort them
@revmessages = sort { $a->[0] cmp $b->[0] or $a->[1] <=> $b->[1] } @revmessages;


my $date = strftime "%F %R%z", localtime();

foreach my $occurence (@revmessages)
{
	my $key = $occurence->[2];
	if ( defined $messages{$key} )
	{
		my @array = @{$messages{$key}};
		foreach my $line (@array)
		{
			print "#: $line\n";
		}
		print "msgid $key",
		"msgstr \"\"\n\n";

		# be sure we don't output the same message twice
		delete $messages{$key};
	}
}
