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
#	Scriot to parse (part) of the *.ufo files for UFO:Alien Invasion.
#	It could also partly be used for similar Quake-bases scripts
#	The main logic for parsing is in the parseUfoToken function.
#
#######################################
# Changelog
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
#######################################
# TODO
#	* Prettify this.
#######################################

use strict;
use warnings;

my $filenames = [
	#"research.ufo",
	#"research_logic.ufo"
	"../../base/ufos/research.ufo",
	"../../base/ufos/research_logic.ufo"
];

my $filenameDot = "tree.dot";

my $ignoredTechs = [
	'rs_skill.*',
	'rs_damage.*'
];

# Settings inside "tech" we'll just skip for now.
my $technologySkipSettings = [
	"description",		# { default "_monoknife_txt" }
	"pre_description",	# { default "_monoknife_pre_txt" }
	"mail",			# { from xxx to xxx ...}
	"mail_pre"		# { from xxx to xxx ...}
];

# Settings of "tech" with just one parameter that we want to parse.
my $technologySimpleSettings = [
	"type",		# weapon
	"name",		# "_Continuous Wave Laser Operation"
	"image_top",	# techs/laser
	"mdl_top",	# weap_machine_pistol
	"up_chapter", 	# equipment
	"provides", 	# knifemono
	"time",		# 100
	"producetime",	# 100
	"delay",
	"event",
	"pushnews"

];

# Settings of requirements that we want to parse
# The number tells us how many parameters they have.
my $technologyRequirementSettings = {
	"tech"		=> 1,
	"event"		=> 1,
	"alienglobal"	=> 1,
	"item"		=> 2,
	"alien"		=> 2,
	"alien_dead"	=> 2
};

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
			$text =~ s/\/\*([^\R]*?)\*\/([.\n?]*)/$2/; # Skip everything until (and including) closing "*/" characters.
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



#~ require_AND
#~ {
	#~ tech rs_weapon_kerrblade
	#~ tech rs_alien_bloodspider_autopsy
	#~ tech rs_skill_close
	#~ tech rs_damage_normal
#~ }
sub parseTechRequirements ($$) {
	my ($d, $techID) = @_;
	my $text = $$d;

	my $reqs = [];

	my $token = parseUfoToken(\$text);

	if ($token ne '{') {
		die "parseTechRequirements: Empty tech requirement in '", $techID, "' found.\n";
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
		die "parseTech: Empty tech '", $techID, "' found.\n";
	}

	$token = parseUfoToken(\$text);
	my $parsed;
	while ($token ne '}') {
		$parsed = 0;
		foreach my $setting (@{$technologySkipSettings}) {
			if ($token =~ m/^$setting/) {
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
			if ($token =~ m/^$setting/) {
				$tech->{$setting} = parseUfoToken(\$text);
				$token = parseUfoToken(\$text);
				print "Parsed setting '", $setting, "' ... next token: '", $token, "'\n";
				$parsed = 1;
			}
		}
		if ($parsed) {
			next;
		}

		if ($token eq "require_AND") {
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

	print "Read file '", $filename, "' ...\n";
	open(SOURCE, "< $filename") or die "\nCouldn not open file '$filename' for reading: $!\n";

	my $text2 = do { local( $/ ) ; <SOURCE> } ;
	my $text = $text2;
	my $techs = $self->{'techs'};
	my $items = $self->{'items'};
	my $aliens = $self->{'aliens'};
	my $aliens_dead = $self->{'aliens_dead'};

#my $debug; # DEBUG
	my $token = '';
	while ($text) {
		print "Parsing ...\n";
		$token = parseUfoToken(\$text);

		print "Parsing ... token '", $token ,"' ...\n";

		if ($token eq "up_chapters") {
			# skip for now
			$token = parseUfoToken(\$text);
			parseSkip(\$text);
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

	$self->{'techs'} = $techs;
	$self->{'items'} = $items;
	$self->{'aliens'} = $aliens;
	$self->{'aliens_dead'} = $aliens_dead ;

	close (SOURCE);
	return $self;
}

#######################################
# Writing (.dot syntax)
#######################################

sub skipTech ($$) {
	my ($tech, $ignoreList) = @_;
	
	foreach my $ignore (@{$ignoreList}) {
		if ($tech->{'id'} =~ m/$ignore/) {
			return 1;
		}
	}
	
	return 0;
}
sub printTechnologyStyle ($) {
	my ($FH) = @_;
	printf $FH "\t".'node  [shape=box, color="#004e0099", style=filled, fontcolor=black];'."\n";
}

sub printTech ($$) {
	my ($tech, $FH) = @_;

	# techID [label="Tech Name"];
	my $name;
	if (exists($tech->{'name'})) {
		$name = $tech->{'name'};
	} else {
		$name = $tech->{'id'};
	}
	printf $FH "\t\t".$tech->{'id'}.' [label="'.$tech->{'id'}.'"];'."\n";
}

sub reqExists($$$) {
	my ($tech, $reqType, $techs, $ignoredTechs) = @_;
	
	if (exists($tech->{$reqType}) and $#{$tech->{$reqType}} >= 0) {
		my $numReq = $#{$tech->{$reqType}};
		foreach my $req (@{$tech->{$reqType}}) {
			# Reduce number of requirements if there is an unwanted tech inside.
			if ($req->{'type'} eq 'tech'
			and skipTech($req->{'value1'}, $ignoredTechs)) {
				$numReq--;
			}
			
		}
		if ($numReq >= 0) {
			return 1;
		}
	}
	return 0;
}

sub printReqStyle ($$) {
	my ($FH, $label) = @_;

	my $color;
	if ($label eq 'OR') {
		$color = '#71a4cf99';
	} else {
		$color = '#71cf9a99';
	}

	printf $FH "\t".'node [shape=ellipse, label="'.$label.'", color="'.$color.'", style=filled, fontcolor=black];'."\n";
}

sub printReq ($$$$) {
	my ($tech, $FH, $label, $ignoredTechs) = @_;

	if (reqExists($tech, $label, $ignoredTechs)) {
		printf $FH "\t\t".$tech->{'id'}.'_'.$label.";\n";
	}
}

sub printItemStyle ($) {
	my ($FH) = @_;

	printf $FH "\t".'node [shape=box, color="#f7e30099", style=filled, fontcolor=black];'."\n";
}

sub printItem ($$) {
	my ($item, $FH) = @_;

	printf $FH "\t\t".$item.' [label="'.$item.'"];'."\n";
}

sub printAlienStyle ($) {
	my ($FH) = @_;

	printf $FH "\t".'node [shape=box, color="#f7e30099", style=filled, fontcolor=black];'."\n";
}

sub printAlien ($$$) {
	my ($alien, $type, $FH) = @_;

	if ($type eq 'alien') {
		printf $FH "\t\t".$alien.' [label="Live '.$alien.'"];'."\n";
	} else {
		printf $FH "\t\t".$alien.'_dead [label="'.$alien.' autopsy"];'."\n";
	}
}

sub printTechGroup ($$$) {
	my ($tech, $FH, $ignoredTechs) = @_;

	my $hasOR = reqExists($tech, 'OR', $ignoredTechs);
	my $hasAND = reqExists($tech, 'AND', $ignoredTechs);

	# subgraph techID_C { 
	printf $FH "\t".'subgraph '.$tech->{'id'}.'_C { ';
	if ($hasOR) {
		# techID_OR -> techID;
		printf $FH $tech->{'id'}.'_OR -> '.$tech->{'id'}.';';
	}

	if ($hasAND) {
		# techID_AND -> techID;
		printf $FH $tech->{'id'}.'_AND -> '.$tech->{'id'}.';';
	}
	printf $FH " }\n";
}

sub printTechLinks ($$$$) {
	my ($techs, $tech, $FH, $ignoredTechs) = @_;

	foreach my $req (@{$tech->{'AND'}}) {
		if ($req->{'type'} eq "tech") {
			if (not skipTech($techs->{$req->{'value1'}}, $ignoredTechs)) {
				printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_AND'."\n";
			}
		} elsif ($req->{'type'} eq "item") {
			printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_AND'."\n";
		} elsif ($req->{'type'} eq "alien") {
			printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_AND'."\n";
		} elsif ($req->{'type'} eq "alien_dead") {
			printf $FH "\t".$req->{'value1'}.'_dead -> '.$tech->{'id'}.'_AND'."\n";
		} elsif ($req->{'type'} eq "alien_global") {
			printf $FH "\t".'alien_global -> '.$tech->{'id'}.'_AND'."\n";
		}
	}
	foreach my $req (@{$tech->{'OR'}}) {
		if ($req->{'type'} eq "tech") {
			if (not skipTech($techs->{$req->{'value1'}}, $ignoredTechs)) {
				printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_OR'."\n";
			}
		} elsif ($req->{'type'} eq "item") {
			printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_OR'."\n";
		} elsif ($req->{'type'} eq "alien") {
			printf $FH "\t".$req->{'value1'}.' -> '.$tech->{'id'}.'_OR'."\n";
		} elsif ($req->{'type'} eq "alien_dead") {
			printf $FH "\t".$req->{'value1'}.'_dead -> '.$tech->{'id'}.'_OR'."\n";
		} elsif ($req->{'type'} eq "alien_global") {
			printf $FH "\t".'alien_global -> '.$tech->{'id'}.'_OR'."\n";
		}
	}
}

sub writeDotFile(%$) {
	my ($self, $filename) = @_;
	die "\nUsage: writeDotFile(techtree-HASHREF filename)"
	unless ((@_                == 2      ) &&
		(ref($self)        eq 'HASH' ) );

	my $techs = $self->{'techs'};
	my $items = $self->{'items'};
	my $aliens = $self->{'aliens'};
	my $aliens_dead = $self->{'aliens_dead'};

	print "Writing file '", $filename, "' ...\n";
	open(my $DOT, "> $filename") or die "\nCouldn not open file '$filename' for writing: $!\n";
	#my $DOT = <DOT>;

	printf $DOT 'digraph G {'."\n";
	printf $DOT '	ranksep="1.0 equally";'."\n";
	printf $DOT '	edge [color="#0b75cf"];'."\n";
	printf $DOT '	label = "Research Tree\nUFO: Alien Invasion";'."\n";
	printf $DOT "\n";

	# Draw technology boxes
	printf $DOT "/* Technology boxes */\n";
	printTechnologyStyle($DOT);
	foreach my $techId (keys %{$techs}) {
		my $tech = $techs->{$techId};
		if (not skipTech($tech, $ignoredTechs)) {
			printTech($tech, $DOT);
		}
	}
	printf $DOT "\n";

	# Draw OR boxes for each technology that needs them.
	printf $DOT "/* Technology 'OR' boxes */\n";
	printReqStyle($DOT, "OR");
	foreach my $techId (keys %{$techs}) {
		my $tech = $techs->{$techId};
		if (not skipTech($tech, $ignoredTechs)) {
			printReq($tech, $DOT, "OR", $ignoredTechs);
		}
	}
	printf $DOT "\n";

	# Draw AND boxes for each technology that needs them.
	printf $DOT "/* Technology 'AND' boxes */\n";
	printReqStyle($DOT, "AND");
	foreach my $techId (keys %{$techs}) {
		my $tech = $techs->{$techId};
		if (not skipTech($tech, $ignoredTechs)) {
			printReq($tech, $DOT, "AND", $ignoredTechs);
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

	# Draw (live) alien boxes
	printf $DOT "/* Alien boxes */\n";
	printAlienStyle($DOT);
	printf $DOT "\t\t".'alien_global [label="Global Aliens"];'."\n";
	foreach my $alien (keys %{$aliens}) {
		printAlien($alien, 'alien', $DOT);
	}
	printf $DOT "\n";
	
	# Draw (dead) alien boxes
	printf $DOT "/* Dead alien boxes */\n";
	printAlienStyle($DOT);
	foreach my $alien (keys %{$aliens}) {
		printAlien($alien, 'alien_dead', $DOT);
	}
	printf $DOT "\n";

	# Draw tech subgraphs and OR/AND links
	printf $DOT "/* Technology groups (with tech + AND + OR boxes) */\n";
	foreach my $techId (keys %{$techs}) {
		my $tech = $techs->{$techId};
		if (not skipTech($tech, $ignoredTechs)) {
			printTechGroup($tech, $DOT, $ignoredTechs);
		}
	}
	printf $DOT "\n";


	# Create requirement links
	printf $DOT "/* Requirement edges */\n";
	foreach my $techId (keys %{$techs}) {
		my $tech = $techs->{$techId};
		if (not skipTech($tech, $ignoredTechs)) {
			printTechLinks($techs, $tech, $DOT, $ignoredTechs);
		}
	}
	printf $DOT "\n";

	printf $DOT "\tfontsize=20;\n";
	printf $DOT "}\n";
	close $DOT;

	print "File '", $filenameDot, "' written.\n\n";
	print "Use\n";
	print " dot ", $filenameDot , " -Tpng >dia.png\n";
	print " dot ", $filenameDot , " -Tsvg >dia.svg\n";
	print "to convert it into a different format.\n";
	print "See also: 'graphviz'\n";

}

####################
# MAIN
####################

use Data::Dumper;

my $tree = {
	'techs' => {},
	'items' => {},
	'aliens' => {},
	'aliens_dead' => {}
};
foreach my $filename (@{$filenames}) {
	$tree = parseUfoTree($tree, $filename);
#	print Dumper($tree);
}


writeDotFile($tree, $filenameDot);

#############
# EOF