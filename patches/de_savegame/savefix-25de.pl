#!/usr/bin/perl
#######################################
# Copyright (C) 2017 Tamás Fehérvári <geever@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#######################################

#######################################
# savefix-25de.pl
#		Script to fix savegame corruption caused by a broken German language file in v2.5
#
# Usage: savefix-25de.pl <in-file> <out-file>
#
#######################################

use strict;
use warnings;

use Compress::Zlib;

if (scalar(@ARGV) < 2) {
	printf("Required parameters: <input_file> <output_file>\n");
	exit 0;
}

my ($in, $out);
if (!open($in, '<:raw', $ARGV[0])) {
	printf("ERROR: Cannot open input file '%s'!: %s\n", $ARGV[0], $!);
	exit(1);
}
binmode($in);

# read header
my $binheader;
if (sysread($in, $binheader, 180) != 180) {
	printf("ERROR: Cannot read savegame header!\n");
	exit(2);
}

my $header = {};
(
	$header->{version},
	$header->{compressed},
	$header->{subsystems},
	$header->{reserved},
	$header->{game_version},
	$header->{name},
	$header->{game_date},
	$header->{real_date},
	$header->{xml_size}
) = unpack("(V)(V)(V)(Z52)(Z16)(Z32)(Z32)(Z32)(V)", $binheader);
undef($binheader);

# Save version check
if ($header->{version} != 4) {
	printf("Warning: Savegame master version differs, probably incompatible savegame: %u\n", $header->{version});
}

# Game version check
if ($header->{game_version} ne "2.5") {
	printf("Warning: Game version differs, probably incompatible savegame: %s\n", $header->{game_version});
}

# read data
my $content_size = (-s $ARGV[0]) - 180;
my ($content, $decoded_content);
if (sysread($in, $content, $content_size) != $content_size) {
	printf("ERROR: Cannot read savegame data!\n");
}
close($in);

# Uncompress if necessary
if ($header->{compressed}) {
	$decoded_content = uncompress($content);
	if (!defined($decoded_content)) {
		printf("ERROR: Cannot decompress savegame: corrupted stream\n");
		exit(3);
	}
	undef($content);
} else {
	$decoded_content = $content;
	undef($content);
}

# Fix content
$decoded_content =~ s/(?<=Au..erirdische Aktivit..ten wurden gemeldet) in: '[^']+?'//sg;
$header->{xml_size} = length($decoded_content);

# Open Output
if (!open($out, '>:raw', $ARGV[1])) {
	printf("ERROR: Cannot open output file '%s': %s!\n", $ARGV[1], $!);
	exit(2);
}
binmode($out);

$binheader = pack(
	"(V)(V)(V)(Z52)(Z16)(Z32)(Z32)(Z32)(V)",
	$header->{version},
	$header->{compressed},
	$header->{subsystems},
	$header->{reserved},
	$header->{game_version},
	$header->{name},
	$header->{game_date},
	$header->{real_date},
	$header->{xml_size}
);
syswrite($out, $binheader, 180);

# Compress if the save was compressed
if ($header->{compressed}) {
	$content = compress($decoded_content);
	if (!defined($content)) {
		printf("ERROR: Cannot compress savegame\n");
		exit(3);
	}
	undef($decoded_content);
} else {
	$content = $decoded_content;
	undef($decoded_content);
}
syswrite($out, $content);
if (!close($out)) {
	printf("ERROR: Write error on output file: %s\n", $!);
}
