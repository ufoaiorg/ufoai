#!/usr/bin/perl
#######################################
# Copyright (C) 2008 Werner Hoehrer
#
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License
#as published by the Free Software Foundation; either version 2
#of the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#See the GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#######################################

#######################################
# ufo2dot.pl
#	Script to parse (part) of the *.ufo files for UFO:Alien Invasion.
#	It could also partly be used for similar Quake-bases scripts
#	The main logic for parsing is in the parseUfoToken function.
#
#######################################
# Changelog
#	2010-07-29 jerikojerk
#		Add new requierement type "ufo" for rs_alien_ufo_theory
#	2008-09-30 Hoehrer
#		Added "redirect" entry.
#	2008-09-26 Hoehrer
#		Quick fix to make this work with "mdl" and "image" tech settings.
#		(previously mdl_top and image_top)
#	2008-04-03 Hoehrer
#		Added first working parseUfoToken function.
#		Multi-line support for text inside ""s
#		Hopefully fixed comment parsing - still needs some testing. Also added a repeating cleanup function.
#		First code to parse a technology and the requirements inside it.
#		Requirements are parsed now.
#		research.ufo file is parsed now .. but see TODO.
#		Writing basic .dot files (graphviz) works now
#		Items are now drawn as well.
#	2008-04-04 Hoehrer
#		Some formatting of the nodes ("ranksep")
#		More functions to avoid code duplication.
#		Skipping some unneeded topics (like skills and damages)
#	2008-04-05 Hoehrer
#		Parsing "researched_list" files
#		Display techs in different color depending on their research status on game-start.
#		Parsing (but not linking) "provided" items as well.
#		Skip some multiplayer-only techs.
#		Fixed a bug where (the boxes) for dead aliens where wrongly listed/printed.
#	2008-04-06 Hoehrer
#		Legend
#		Red border for "logic" techs.
#	2008-04-07 Hoehrer
#		Parsing tech chapters in ufopedia
#		Handling gettext strings (read: simply remove the "_")
#	2008-04-08 Hoehrer
#		Add note to techs that have researchtime==0 (e.g most ammo)
#		Fixed a bug where no real names where used for the techs.
#	2008-04-09 Hoehrer
#		Parsing "description" and "pre_description" an display them if there is more than the default one.
#		Added support for "tech_not" nodes.
#		Display number of required items/aliens next to the connecting edge.
#	2008-04-10 Hoehrer
#		Improved parsing/skipping of multiline comments.
#		Show research time in techs.
#		Skip future "2 autopsies" techs.
#
#######################################
# TODO
#	* Prettify this even more.
#	* MAJOR Parse all of the items/crafts/buildings/etc... files.
#	* Check why the svg export has too dark colors (I believe transparency is skipped here).
#	* Check if we can print "real" research times in some way (e.g. for an average of 10 scientists working on it) - not just the value from the .ufo file.
#######################################

use strict;
use warnings;

my $filenames = [
	'../../base/ufos/research.ufo',
	'../../base/ufos/research_logic.ufo'
];
my $filenameResearched = '../../base/ufos/researched_list.ufo';

my $filenameDot = "tree.dot";
my $useProvidesInfo = 0;

# This is regex syntax, so mind you that without the "^" and "$" characters it will not match _exactly_.
my $ignoredTechs = [
	'^rs_skill.*',
	'^rs_damage.*',
	# Multiplayer
	'^rs_weapon_shotgun$',
	'^rs_weapon_shotgun_ammo$',
	'^rs_power_armour$',	# may get into SP soon
	'^rs_weapon_chaingun$',
	'^rs_weapon_chaingun_ammo$',
	# This just makes the tree unreadable ... the "Any 2 Autopsies" tech is still in there anyway.
	'^rs_autopsy_O_T$',
	'^rs_autopsy_O_S$',
	'^rs_autopsy_O_Br$',
	'^rs_autopsy_T_S$',
	'^rs_autopsy_T_Br$',
	'^rs_autopsy_S_Br$'
];

# Settings inside "tech" we'll just skip for now.
my $technologySkipSettings = [
	"mail",			# { from xxx to xxx ...}
	"mail_pre"		# { from xxx to xxx ...}
];

# Settings of "tech" with just one parameter that we want to parse.
my $technologySimpleSettings = [
	"type",		# weapon
	"name",		# "_Continuous Wave Laser Operation"
	"image",	# techs/laser
	"mdl",		# weap_machine_pistol
	"up_chapter", 	# equipment
	"provides", 	# knifemono
	"time",		# 100
	"producetime",	# 100
	"delay",
	"event",
	"pushnews",
	"redirect"
];

# Settings of requirements that we want to parse
# The number tells us how many parameters they have.
my $technologyRequirementSettings = {
	"tech"		=> 1,
	"tech_not"	=> 1,
	"alienglobal"	=> 1,
	"item"		=> 2,
	"alien"		=> 2,
	"alien_dead"	=> 2,
	"event"		=> 1,
	"ufo"		=> 2
};


#######################################
# General functions
#######################################

# Remove leading "_" which is defined for gettext strings.
sub convertGettextString($) {
	my ($string) = @_;

	$string =~ s/^_//;
	return $string;
}

sub skipMultilineComment ($) {
	my ($d) = @_;
	my $text = $$d;

	while (not ($text =~ m/^\*\//)) {
		$text =~ s/^.|\n|\r//;	# Dunno why [.\n\r] doesn't work here ?
		#print substr($text, 0, 10). "\n";	# DEBUG
	}

	$text =~ s/^\*\///;

	$$d = $text;
}

#######################################
# Parsing (.ufo techtree syntax)
#######################################
sub parseUfoToken ($) {
	my ($d) = @_;
	my $text = $$d;

	my $token = '';

	# Clean the text up a bit
	my $modified = 1;
	while ($modified) {
		$modified = 0;

		$text =~ s/^\s*//;	# Skip whitespace

		# Skip single-line comment.
		if ($text =~ m/^\/\//) {
			#print "xx";	 # DEBUG
			$text =~ s/([^\n]*)([.\n?]*)/$2/;	# Skip everything until newline.
			$modified = 1;
			next;
		}

		# Skip multi-line comment.
		if ($text =~ m/^\/\*/) {
			# $text =~ s/\/\*.*?\*\/([.\n?]*)/$2/; # Skip everything until (and including) closing "*/" characters.
			skipMultilineComment(\$text);
			$modified = 1;
			next;
		}
	}

	$text =~ s/^\s*//;	# Skip whitespace

	# Garbage is gone at this point. Only the next token.

	# Parse (multi-line) text encloded by ""
	if ($text =~ m/^"/) {
		#print "\"\"\"\"\"\"\"\"\"\n";
		$text =~ s/"([^"]*?)"([.\n?]*)/$2/;
		$token = $1;
		$$d = $text;
		return $token;
	}

	$text =~ s/([^\s]*)([.\n?]*)/$2/;
	$token = $1;
	$$d = $text;
	return $token;
}

sub parseTechChapters ($) {
	my ($d) = @_;
	my $text = $$d;

	my $token = parseUfoToken(\$text);
	my $chaptersDef = { 'id' => $token};

	$token = parseUfoToken(\$text);
	if ($token ne '{') {
		die "parseTechChapters: Empty chapter definition found ('", $token, "').\n";
	}


	$token = parseUfoToken(\$text);
	while ($token ne '}') {
		my $chapter = { 'id' => $token };

		$token = parseUfoToken(\$text);
		$chapter->{'name'} = convertGettextString($token);

		$chaptersDef->{$chapter->{'id'}} = $chapter;

		$token = parseUfoToken(\$text);
	}

	$$d = $text;
	return $chaptersDef;
}

# Example
# pre_description {
#	default "_antimatter_pre_txt"
#	rs_antimatter_extra "_antimatter_extra_pre_txt"
# }
sub parseTechDescription ($$) {
	my ($d, $techID) = @_;
	my $text = $$d;

	my $token = parseUfoToken(\$text);

	if ($token ne '{') {
		die "parseTechDescription: Empty tech description in '", $techID, "' found ('", $token, "').\n";
	}
	my $descTemp = [];

	my $count = 0;
	$token = parseUfoToken(\$text);
	while ($token ne '}') {
		my $desc = {};
		if ($count == 0 && !$token eq 'default') {
			print "parseTechDescription: Expected description id 'default', got '", $token, "' instead. (tech '", $techID ,"')\n";
		}
		$desc->{'id'} = $token;

		$token = parseUfoToken(\$text);
		$desc->{'desc'} = $token;
		$count++;
		push(@{$descTemp}, $desc);

		$token = parseUfoToken(\$text);
	}
#print Dumper($descTemp); # DEBUG
	$$d = $text;
	return $descTemp;
}

# Example
# require_AND
# {
#	tech rs_weapon_kerrblade
#	tech rs_alien_bloodspider_autopsy
#	tech rs_skill_close
#	tech rs_damage_normal
# }
sub parseTechRequirements ($$) {
	my ($d, $techID) = @_;
	my $text = $$d;

	my $reqs = [];

	my $token = parseUfoToken(\$text);

	if ($token ne '{') {
		die "parseTechRequirements: Empty tech requirement in '", $techID, "' found ('", $token, "').\n";
	}

	$token = parseUfoToken(\$text);
	while ($token ne '}') {
		my $req = {};
		if (exists($technologyRequirementSettings->{$token})) {
			$req->{'type'} = $token;
			$token = parseUfoToken(\$text);
			$req->{'value1'} = $token;

			if ($technologyRequirementSettings->{$req->{'type'}} > 1) {
				$token = parseUfoToken(\$text);
				$req->{'value2'} = $token;
			}

			push(@{$reqs}, $req);
			$token = parseUfoToken(\$text);
		} else {
			die "parseTechRequirements: Unknown requirement type '", $token ,"' in tech '", $techID ,"'\n";
		}
	}

	$$d = $text;
	return $reqs;
}

sub parseSkip ($) {
	my ($d) = @_;
	my $text = $$d;

	#$text =~ s/\{([^\}]*?)\}([.\n?]*)/$2/;	# TODO how would this work?

	my $token = parseUfoToken(\$text);
	while ($token ne '}') {
		$token = parseUfoToken(\$text);
	}

	$$d = $text;
}

sub parseTech ($) {
	my ($d) = @_;
	my $text = $$d;

	my $tech = {};

	my $token = parseUfoToken(\$text);
	my $techID = $token;
	$tech->{'id'} = $techID;
	print "Tech: '", $techID, "'\n";

	$token = parseUfoToken(\$text);

	if ($token ne '{') {
		die "parseTech: Empty tech '", $techID, "' found ('", $token, "').\n";
	}

	$token = parseUfoToken(\$text);
	my $parsed;
	while ($token ne '}') {
		$parsed = 0;
		foreach my $setting (@{$technologySkipSettings}) {
			if ($token =~ m/^$setting$/) {
				parseSkip(\$text);
				$token = parseUfoToken(\$text);
				print "Skipped '", $setting, "' ... next token: '", $token, "'\n";
				$parsed = 1;
			}
		}
		if ($parsed) {
			next;
		}

		$parsed = 0;
		foreach my $setting (@{$technologySimpleSettings}) {
			if ($token =~ m/^$setting$/) {
				$tech->{$setting} = parseUfoToken(\$text);
				$token = parseUfoToken(\$text);
				print "Parsed setting '", $setting, "' ... next token: '", $token, "'\n";
				$parsed = 1;

				# Remove gettext character from string if any.
				if ($setting eq 'name') {
					$tech->{$setting} = convertGettextString($tech->{$setting});
				}
			}
		}
		if ($parsed) {
			next;
		}

		if ($token eq "description") {
			$tech->{'description'} = parseTechDescription(\$text, $techID);
			$token = parseUfoToken(\$text);
			#print Dumper($tech->{'description'});	# Debug
			#exit;
			print "Parsed 'description' - next token: $token\n";
		} elsif ($token eq "pre_description") {
			$tech->{'pre_description'} = parseTechDescription(\$text, $techID);
			$token = parseUfoToken(\$text);
			print "Parsed 'pre_description' - next token: $token\n";
		} elsif ($token eq "require_AND") {
			$tech->{'AND'} = parseTechRequirements(\$text, $techID);
			$token = parseUfoToken(\$text);
			print "Parsed AND - next token: $token\n";
		} elsif ($token eq "require_OR") {
			$tech->{'OR'} = parseTechRequirements(\$text, $techID);
			$token = parseUfoToken(\$text);
			print "Parsed OR - next token: $token\n";
		} elsif ($token eq "require_for_production") {
			$tech->{'require_for_production'} = parseTechRequirements(\$text, $techID);
			$token = parseUfoToken(\$text);
			print "Parsed require_for_production - next token: $token\n";
		} else {
			die "parseTech: Unknown tech setting: '", $token, "'\n";
		}

		#print "End of cycle\n";
	}

	$$d = $text;
	return $tech;
}

sub getTechItems ($) {
	my ($tech) = @_;

	my $items = {};

	foreach my $req (@{$tech->{'AND'}}) {
		if ($req->{'type'} eq "item") {
			$items->{$req->{'value1'}} = 1;
		}
	}
	foreach my $req (@{$tech->{'OR'}}) {
		if ($req->{'type'} eq "item") {
			$items->{$req->{'value1'}} = 1;
		}
	}

	# Store info about provided items.
	if (exists($tech->{'provides'})
	&& ($tech->{'type'} eq 'weapon' || $tech->{'type'} eq 'armour')) {
		$items->{$tech->{'provides'}} = 1;
	}

	return $items;
}

sub getTechAliens ($) {
	my ($tech) = @_;

	my $aliens = {};

	foreach my $req (@{$tech->{'AND'}}) {
		if ($req->{'type'} eq "alien") {
			$aliens->{$req->{'value1'}} = 1;
		}
	}
	foreach my $req (@{$tech->{'OR'}}) {
		if ($req->{'type'} eq "alien") {
			$aliens->{$req->{'value1'}} = 1;
		}
	}
	return $aliens;
}

sub getTechAliensDead ($) {
	my ($tech) = @_;

	my $aliensDead = {};

	foreach my $req (@{$tech->{'AND'}}) {
		if ($req->{'type'} eq "alien_dead") {
			$aliensDead->{$req->{'value1'}} = 1;
		}
	}
	foreach my $req (@{$tech->{'OR'}}) {
		if ($req->{'type'} eq "alien_dead") {
			$aliensDead->{$req->{'value1'}} = 1;
		}
	}
	return $aliensDead;
}

sub parseUfoTree (%$) {
	my ($self, $filename) = @_;
	die "\nUsage: parseUfoTree(techtree-HASHREF filename)"
	unless ((@_                == 2      ) &&
		(ref($self)        eq 'HASH' ) );

	print "======= Parsing UFO:AI research tree ('tech') =======\n";
	print "Read file '", $filename, "' ...\n";
	open(SOURCE, "< $filename") or die "\nCouldn not open file '$filename' for reading: $!\n";

	my $text2 = do { local( $/ ) ; <SOURCE> } ;
	my $text = $text2;
	my $techs = $self->{'techs'};
	my $items = $self->{'items'};
	my $aliens = $self->{'aliens'};
	my $aliens_dead = $self->{'aliens_dead'};
	my $chapters = $self->{'up_chapters'};

#my $debug; # DEBUG
	my $token = '';
	while ($text) {
		print "Parsing ...\n";
		$token = parseUfoToken(\$text);

		print "Parsing ... token '", $token ,"' ...\n";

		if ($token eq "up_chapters") {
			my $chaptersDef;
			$chaptersDef = parseTechChapters(\$text);
			$chapters->{$chaptersDef->{'id'}} = $chaptersDef;
#			print Dumper($chapters);	#DEBUG
#			exit;				#DEBUG
		} elsif ($token eq "tech") {
			my $tech = parseTech(\$text);

			if ($tech and exists($tech->{'id'})) {
				$techs->{$tech->{'id'}} =  $tech;
				print "... finished parsing tech '", $tech->{'id'}, "'.\n";
				my $newItems = getTechItems($tech);
				my $newAliens = getTechAliens($tech);
				my $newAliensDead = getTechAliensDead($tech);
				%{$items} = (%{$items},  %{$newItems});
				%{$aliens} = (%{$aliens},  %{$newAliens});
				%{$aliens_dead} = (%{$aliens_dead},  %{$newAliensDead});
#				$debug = $tech->{'id'};	#my $debug; # DEBUG
			}
		} else {
			print "parseUfoTree: Unknown keyword: '", $token, "'\n";
		}
		print "... next round ...\n";

#		if ($debug eq "rs_craft_ufo_harvester") {	 # DEBUG
#			print Dumper($text);
#		}
	}
	print "\n";

	$self->{'techs'} = $techs;
	$self->{'items'} = $items;
	$self->{'aliens'} = $aliens;
	$self->{'aliens_dead'} = $aliens_dead;
	$self->{'up_chapters'} = $chapters;

	close (SOURCE);
	return $self;
}
#######################################
# Parsing (.ufo "required_list  syntax)
#######################################
sub parseResearchedList (%$) {
	my ($self, $filename) = @_;
	die "\nUsage: parseResearchedList(techtree-HASHREF filename)"
	unless ((@_                == 2      ) &&
		(ref($self)        eq 'HASH' ) );

	print "======= Parsing researched-techs list ('researched' and 'researchable') =======\n";
	print "Read file '", $filename, "' ...\n";
	open(SOURCE, "< $filename") or die "\nCouldn not open file '$filename' for reading: $!\n";

	my $text2 = do { local( $/ ) ; <SOURCE> } ;
	my $text = $text2;
	my $researched = $self->{'researched'};
	my $researchable = $self->{'researchable'};

#my $debug; # DEBUG
	my $token = '';
	while ($text) {
		print "Parsing ...\n";
		$token = parseUfoToken(\$text);

		print "Parsing ... token '", $token ,"' ...\n";

		my $type = $token;
		if ($type eq "researched" || $type eq "researchable") {
			$token = parseUfoToken(\$text);
			my $researchedListID = $token;

			$token = parseUfoToken(\$text);

			if ($token ne '{') {
				print "parseResearchedList: Empty 'researched' list ('", $token, "'). Abort.\n";
				exit;
			}
			$token = parseUfoToken(\$text);

			if ($token eq '') {
				print "parseResearchedList: Empty 'researched' list ('", $token, "'). Abort.\n";
				exit;
			}

			if ($token ne "}") {
				my $researchedList = {};
				$token = parseUfoToken(\$text);

				while ($token ne "}") {
					$researchedList->{$token} = 1;
					$token = parseUfoToken(\$text);
				}

				if ($type eq "researched") {
					$researched->{$researchedListID} = $researchedList;
				} else {
					$researchable->{$researchedListID} = $researchedList;
				}
			}
		} else {
			print "parseResearchedList: Unknown keyword: '", $token, "'\n";
		}
		print "... next round ...\n";

	}
	print "\n";
	$self->{'researched'} = $researched;
	$self->{'researchable'} = $researchable;

	close (SOURCE);
	return $self;
}
#######################################
#######################################
# Writing (.dot syntax)
#######################################

sub skipTech ($) {
	my ($tech) = @_;

	foreach my $ignore (@{$ignoredTechs}) {
		if ($tech->{'id'} =~ m/$ignore/) {
			return 1;
		}
	}

	return 0;
}

sub techResearched ($$$) {
	my ($tech, $techs, $researchedList) = @_;

	if (exists($researchedList->{$tech->{'id'}})) {
		return 1;
	}

#	if ($tech->{'id'} eq 'rs_advanced_combat_armour') {	#DEBUG
#		print Dumper($tech);
#		print Dumper($researchedList);
#		exit;
#	}

	if (exists($tech->{'time'})
	&& $tech->{'time'} == 0) {
		# Initially researched tech.
		if (reqMet($tech, $techs)) {
			return 1;
		}

		# researchtime=0, there is at least one requirement (in the AND list) and they are researched as well. (e.g. ammo)
		if (exists($tech->{'AND'})
		&& $#{$tech->{'AND'}} >= 0) {
			my $reqOk = 1;
			foreach my $req (@{$tech->{'AND'}}) {
				if ($req->{'type'} eq 'tech'
				&& !techResearched($techs->{$req->{'value1'}}, $techs, $researchedList)
				&& !skipTech($techs->{$req->{'value1'}})) {
					$reqOk = 0;
				} elsif ($req->{'type'} ne 'tech') {
					$reqOk = 0;
				}
			}
			if ($reqOk) {
				return 1;
			}
		}
	}
	return 0;

}

sub printTechnologyStyle ($$) {
	my ($FH, $status) = @_;

	if ($status eq 'researchable') {
		printf $FH "\t".'node [shape=box, color="#00004e99", fillcolor="#00004e99",style=filled, fontcolor=black];'."\n";
	} elsif ($status eq 'researched') {
		printf $FH "\t".'node [shape=box, color="#4e000099", fillcolor="#4e000099", style=filled, fontcolor=black];'."\n";
	} else { # 'open'
		# To be researched later on after req. are met.
		printf $FH "\t".'node [shape=box, color="#004e0099", fillcolor="#004e0099", style=filled, fontcolor=black];'."\n";
	}

}

sub printTech ($$$) {
	my ($tech, $FH, $techs) = @_;

	# techID [label="Tech Name"];
	my $name;
	if (exists($tech->{'name'})) {
		$name = $tech->{'name'};
	} else {
		$name = $tech->{'id'};
	}

	# Add note for automatically researched techs such as most ammo.
	if (exists($tech->{'time'})) {
		if ($tech->{'time'} == 0) {
			$name .= '\n(Auto-researched)';
		} else {
			$name .= '\n(Researchtime: '. $tech->{'time'}. ')';
		}
	}

	my $additionalOptions = '';

	if ($tech->{'type'} eq 'logic') {
		$additionalOptions .= ', color="red"';
	}

	if ((exists($tech->{'description'}) && descNumber($tech, 'description', $techs) > 1)
	||  (exists($tech->{'pre_description'}) && descNumber($tech, 'pre_description', $techs) > 1)) {
		$name = '{ '. $name;
		$additionalOptions .= ', shape=record';

		if (descNumber($tech, 'pre_description', $techs) > 1) {
			$name .= "|Research Proposal";
#print Dumper($tech->{'pre_description'}); # DEBUG
			foreach my $desc (@{$tech->{'pre_description'}}) {
#print Dumper($desc); # DEBUG
				if ($desc->{'id'} ne 'default'
				&& exists($techs->{$desc->{'id'}})
				&& exists($techs->{$desc->{'id'}}->{'name'})) {
					$name .= "|{". $techs->{$desc->{'id'}}->{'name'}. "|". $desc->{'desc'}. "}";
				} else {
					$name .= "|{". $desc->{'id'}. "|". $desc->{'desc'}. "}";
				}
			}
		}
		if (descNumber($tech, 'description', $techs) > 1) {
			$name .= "|Research Result";
			foreach my $desc (@{$tech->{'description'}}) {
				if ($desc->{'id'} ne 'default'
				&& exists($techs->{$desc->{'id'}})
				&& exists($techs->{$desc->{'id'}}->{'name'})) {
					$name .= "|{". $techs->{$desc->{'id'}}->{'name'}. "|". $desc->{'desc'}. "}";
				} else {
					$name .= "|{". $desc->{'id'}. "|". $desc->{'desc'}. "}";
				}
			}
		}
		$name .= '}'
	}

	printf $FH "\t\t".$tech->{'id'}.' [label="'.$name.'"'. $additionalOptions .'];'."\n";
}

sub descNumber($$$) {
	my ($tech, $descType, $techs) = @_;

	if (exists($tech->{$descType})) {
		my $numDesc = $#{$tech->{$descType}};
		if ($numDesc >= 0) {
			foreach my $desc (@{$tech->{$descType}}) {
				if ($desc->{'id'} ne 'default' && skipTech($techs->{$desc->{'id'}})) {
					# Reduce number of requirements if there is an unwanted tech inside.
					$numDesc--;
				}
			}
		}
		if ($numDesc >= 0) {
			return $numDesc + 1;
		}
	}
	return 0;
}

sub reqExists($$$) {
	my ($tech, $reqType, $techs) = @_;

	if (exists($tech->{$reqType}) and $#{$tech->{$reqType}} >= 0) {
		my $numReq = $#{$tech->{$reqType}};
		foreach my $req (@{$tech->{$reqType}}) {
			# Reduce number of requirements if there is an unwanted tech inside.
			if ($req->{'type'} eq 'tech'
			and skipTech($techs->{$req->{'value1'}})) {
				$numReq--;
			}

		}
		if ($numReq >= 0) {
			return 1;
		}
	}
	return 0;
}

sub reqMet($$) {
	my ($tech, $techs) = @_;

	my $orHasReqs = (exists($tech->{'OR'}) and $#{$tech->{'OR'}} >= 0);
	my $andHasReqs = (exists($tech->{'AND'}) and $#{$tech->{'AND'}} >= 0);
	if (!$orHasReqs && !$andHasReqs) {
		return 1;
	}

	if (reqExists($tech, 'OR', $techs)
	 || reqExists($tech, 'AND', $techs)) {
		return 0;
	} else {
		return 1;
	}
}

sub getReqNotStyle () {
	my $color = "#ffb14199";	# TODO fix color
	return 'shape=ellipse, label="NOT", color="'.$color.'", fillcolor="'.$color.'",style=filled, fontcolor=black';
}

sub printReqStyle ($$) {
	my ($FH, $label) = @_;

	my $color;
	if ($label eq 'OR') {
		$color = '#71a4cf99';
	} else {
		$color = '#71cf9a99';
	}

	printf $FH "\t".'node [shape=ellipse, label="'.$label.'", color="'.$color.'", fillcolor="'.$color.'",style=filled, fontcolor=black];'."\n";
}

sub printReq ($$$$) {
	my ($tech, $FH, $label, $techs) = @_;

	if (reqExists($tech, $label, $techs)) {
		printf $FH "\t\t".$tech->{'id'}.'_'.$label.";\n";
	}
}

sub printItemStyle ($) {
	my ($FH) = @_;

	printf $FH "\t".'node [shape=box, color="#f7e30099", fillcolor="#f7e30099", style=filled, fontcolor=black];'."\n";
}

sub printItem ($$) {
	my ($item, $FH) = @_;

	printf $FH "\t\t".$item.' [label="'.$item.'"];'."\n";
}

sub printAlienStyle ($) {
	my ($FH) = @_;

	printf $FH "\t".'node [shape=box, color="#f7e30099", fillcolor="#f7e30099", style=filled, fontcolor=black];'."\n";
}

sub printAlien ($$$) {
	my ($alien, $type, $FH) = @_;

	if ($type eq 'alien') {
		printf $FH "\t\t".$alien.' [label="Live '.$alien.'"];'."\n";
	} else {
		printf $FH "\t\t".$alien.'_dead [label="Dead '.$alien.'"];'."\n";
	}
}

sub printTechGroup ($$$) {
	my ($tech, $FH, $techs) = @_;

	my $hasOR = reqExists($tech, 'OR', $techs);
	my $hasAND = reqExists($tech, 'AND', $techs);

	if (!$hasOR and !$hasAND) {
		return;
	}


if (0) {	# subgraphs just do not help anybody here
	# subgraph techID_C {
	printf $FH "\t".'subgraph cluster_'.$tech->{'id'}.' { ';
}
	if ($hasOR) {
		# techID_OR -> techID;
		printf $FH "\t".$tech->{'id'}.'_OR -> '.$tech->{'id'}.";\n";
	}

	if ($hasAND) {
		# techID_AND -> techID;
		printf $FH "\t".$tech->{'id'}.'_AND -> '.$tech->{'id'}.";\n";
	}
if (0) {
	printf $FH " }\n";
}
}

sub printTechLinks ($$$) {
	my ($tech, $FH, $techs) = @_;

	my $req;
	foreach $req (@{$tech->{'AND'}}) {
		if ($req->{'type'} eq "tech") {
			if (not skipTech($techs->{$req->{'value1'}})) {
				printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_AND'."\n";
			}
		} elsif ($req->{'type'} eq "tech_not") {
			if (not skipTech($techs->{$req->{'value1'}})) {
				printf $FH "\t".$req->{'value1'}.'_NOT ['. getReqNotStyle(). ']'."\n";
				printf $FH "\t".$req->{'value1'}.' -> '.$req->{'value1'}.'_NOT'."\n";
				printf $FH "\t".$req->{'value1'}.'_NOT -> '.$tech->{'id'}.'_AND'."\n";
			}
		} elsif ($req->{'type'} eq "item") {
			printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_AND [label="'. $req->{'value2'} .'"]'."\n";
		} elsif ($req->{'type'} eq "alien") {
			printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_AND [label="'. $req->{'value2'} .'"]'."\n";
		} elsif ($req->{'type'} eq "alien_dead") {
			printf $FH "\t".$req->{'value1'}.'_dead -> '.$tech->{'id'}.'_AND [label="'. $req->{'value2'} .'"]'."\n";
		} elsif ($req->{'type'} eq "alienglobal") {
			printf $FH "\t".'alienglobal -> '.$tech->{'id'}.'_AND [label="'. $req->{'value1'} .'"]'."\n";
		}
	}
	foreach $req (@{$tech->{'OR'}}) {
		if ($req->{'type'} eq "tech") {
			if (not skipTech($techs->{$req->{'value1'}})) {
				printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_OR'."\n";
			}
		} elsif ($req->{'type'} eq "tech_not") {
			if (not skipTech($techs->{$req->{'value1'}})) {
				printf $FH "\t".$req->{'value1'}.' -> '.$req->{'value1'}.'_NOT node [label="NOT"]'."\n";
				printf $FH "\t".$req->{'value1'}.'_NOT -> '.$tech->{'id'}.'_OR'."\n";
			}
		} elsif ($req->{'type'} eq "item") {
			printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_OR [label="'. $req->{'value2'} .'"]'."\n";
		} elsif ($req->{'type'} eq "alien") {
			printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_OR [label="'. $req->{'value2'} .'"]'."\n";
		} elsif ($req->{'type'} eq "alien_dead") {
			printf $FH "\t".$req->{'value1'}.'_dead -> '.$tech->{'id'}.'_OR [label="'. $req->{'value2'} .'"]'."\n";
		} elsif ($req->{'type'} eq "alienglobal") {
			printf $FH "\t".'alienglobal -> '.$tech->{'id'}.'_OR [label="'. $req->{'value1'} .'"]'."\n";
		}
	}

	# Back-link to provided items if the otpion was set.
	if ($useProvidesInfo) {
		if (exists($tech->{'provides'})
		&& ($tech->{'type'} eq 'weapon' || $tech->{'type'} eq 'armour')) {
			printf $FH "\t".$tech->{'id'}.' -> '.$tech->{'provides'}." [constraint=false]; /* Provided */\n";
		}
	}
}

sub writeDotFile(%$) {
	my ($self, $filename) = @_;
	die "\nUsage: writeDotFile(techtree-HASHREF filename)"
	unless ((@_                == 2      ) &&
		(ref($self)        eq 'HASH' ) );

	print "======= Writing dot (graphviz) file =======\n";
	my $techs = $self->{'techs'};
	my $items = $self->{'items'};
	my $aliens = $self->{'aliens'};
	my $aliens_dead = $self->{'aliens_dead'};
	my $researchedList = $self->{'researched'}->{'rslist_human'};

	print "Writing file '", $filename, "' ...\n";
	open(my $DOT, "> $filename") or die "\nCouldn not open file '$filename' for writing: $!\n";
	#my $DOT = <DOT>;

	printf $DOT 'digraph G {'."\n";
	printf $DOT '	ranksep="1.0 equally";'."\n";
	printf $DOT '	/* concentrate=true; */'."\n";
	printf $DOT '	edge [color="#0b75cf"];'."\n";
	printf $DOT '	label = "Research Tree\nUFO: Alien Invasion";'."\n";
	printf $DOT "\n";

	# Draw technology boxes
	printf $DOT "/* Technology boxes (researchable techs) */\n";
	printTechnologyStyle($DOT, 'researchable');
	my $techId;
	my $tech;
	foreach $techId (keys %{$techs}) {
		$tech = $techs->{$techId};
		if (!skipTech($tech)
		 && reqMet($tech, $techs)
		 && !techResearched($tech, $techs, $researchedList)) {
			printTech($tech, $DOT, $techs);
		}
	}
	printf $DOT "\n";

	printf $DOT "/* Technology boxes (researched techs) */\n";
	printTechnologyStyle($DOT, 'researched');
	foreach $techId (keys %{$techs}) {
		$tech = $techs->{$techId};
		if (!skipTech($tech)
		 && techResearched($tech, $techs, $researchedList)) {
			printTech($tech, $DOT, $techs);
		}
	}
	printf $DOT "\n";

	printf $DOT "/* Technology boxes (unresearched/open techs) */\n";
	printTechnologyStyle($DOT, 'open');
	foreach $techId (keys %{$techs}) {
		$tech = $techs->{$techId};
		if (!skipTech($tech)
		 && !techResearched($tech, $techs, $researchedList)
		 && !reqMet($tech, $techs)) {
			printTech($tech, $DOT, $techs);
		}
	}
	printf $DOT "\n";

	# Draw OR boxes for each technology that needs them.
	printf $DOT "/* Technology 'OR' boxes */\n";
	printReqStyle($DOT, "OR");
	foreach $techId (keys %{$techs}) {
		$tech = $techs->{$techId};
		if (not skipTech($tech)) {
			printReq($tech, $DOT, "OR", $techs);
		}
	}
	printf $DOT "\n";

	# Draw AND boxes for each technology that needs them.
	printf $DOT "/* Technology 'AND' boxes */\n";
	printReqStyle($DOT, "AND");
	foreach $techId (keys %{$techs}) {
		$tech = $techs->{$techId};
		if (not skipTech($tech)) {
			printReq($tech, $DOT, "AND", $techs);
		}
	}
	printf $DOT "\n";


	# Draw item boxes
	printf $DOT "/* Item boxes */\n";
	printItemStyle($DOT);
	foreach my $itemId (keys %{$items}) {
		printItem($itemId, $DOT);
	}
	printf $DOT "\n";

	my $alien;
	# Draw (live) alien boxes
	printf $DOT "/* Alien boxes */\n";
	printAlienStyle($DOT);
	printf $DOT "\t\t".'alienglobal [label="Global Aliens"];'."\n";
	foreach $alien (keys %{$aliens}) {
		printAlien($alien, 'alien', $DOT);
	}
	printf $DOT "\n";

	# Draw (dead) alien boxes
	printf $DOT "/* Dead alien boxes */\n";
	printAlienStyle($DOT);
	foreach $alien (keys %{$aliens_dead}) {
		printAlien($alien, 'alien_dead', $DOT);
	}
	printf $DOT "\n";

	# Draw tech subgraphs and OR/AND links
	printf $DOT "/* Technology groups (with tech + AND + OR boxes) */\n";
	foreach $techId (keys %{$techs}) {
		$tech = $techs->{$techId};
		if (not skipTech($tech)) {
			printTechGroup($tech, $DOT, $techs);
		}
	}
	printf $DOT "\n";


	# Create requirement links
	printf $DOT "/* Requirement edges */\n";
	foreach $techId (keys %{$techs}) {
		$tech = $techs->{$techId};
		if (not skipTech($tech)) {
			printTechLinks($tech, $DOT, $techs);
		}
	}
	printf $DOT "\n";

	print $DOT '
	subgraph clusterLegend {
		rank="sink";
		label="Legend";
		legendItems [label="Captured dead/live aliens,\nitems etc..." shape=box, color="#f7e30099", fillcolor="#f7e30099", style=filled, fontcolor=black];
		legendResearched [label="Initially\nresearched topics" shape=box, color="#4e000099", fillcolor="#4e000099", style=filled, fontcolor=black];
		legendResearchable [label="Topics that can be researched\nfrom the beginning or are\nintroduced by the game itself" shape=box, color="#00004e99", fillcolor="#00004e99", style=filled, fontcolor=black];
		legendLogic [label="Logic techs.\nUsed to build up complex\nrequirements or provide description\ntexts for dynamic topic." shape=box, color="red", fillcolor="white", style=filled, fontcolor=black];
		legendOpen[label="Research Topics that\nwill be researchable once the\nrequirements are met." shape=box, color="#004e0099", fillcolor="#004e0099", style=filled, fontcolor=black];
	}
';

	printf $DOT "\tfontsize=20;\n";
	printf $DOT "}\n";
	close $DOT;

	print "File '", $filenameDot, "' written.\n\n";
	print "Use\n";
	print " dot ", $filenameDot , " -Tpng >dia.png\n";
	print " dot ", $filenameDot , " -Tsvg >dia.svg\n";
	print "to convert it into a different format.\n";
	print "See also: 'graphviz'\n";
	print "\n";

}

####################
# MAIN
####################

use Data::Dumper;

my $tree = {
	'techs' => {},
	'items' => {},
	'aliens' => {},
	'aliens_dead' => {},
	'up_chapters' => {}
};

foreach my $filename (@{$filenames}) {
	$tree = parseUfoTree($tree, $filename);
#	print Dumper($tree);# DEBUG
}

$tree = parseResearchedList($tree, $filenameResearched);

#$useProvidesInfo = 1;	# Activate to draw "provides" links
writeDotFile($tree, $filenameDot);
