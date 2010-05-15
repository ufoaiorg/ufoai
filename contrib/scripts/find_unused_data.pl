#!/usr/bin/perl

use strict;
use File::Basename;

my $SEARCH = $ARGV[1];

sub read_dir2 {
	my $DIR = shift(@_);
	my $EXT = shift(@_);
	opendir(FILES, $DIR) || die "Could not read dir ($DIR)";
	my @files = readdir(FILES);
	my @filter = ();
	foreach (@files) {
		next if ($_ =~ m/^\./);
		if (-d $DIR."/".$_) {
			push (@filter, read_dir2($DIR."/".$_, $EXT));
			next;
		}
		next unless $_ =~ /$EXT$/;
		push(@filter, $DIR."/".$_);
	}
	closedir(MAPS);

	return @filter;
}

sub check_content {
	my $file = shift(@_);
	my $content = shift(@_);
	my $found = 0;
	open(FILE, $file) || die "Could not read $file";
	foreach (<FILE>) {
		if ($_ =~ /$content/) {
			$found = 1;
			last;
		}
	}
	close(FILE);
	return $found;
}

sub get_maps {
	return read_dir2("base/maps", ".map");
}


sub get_materials {
	return read_dir2("base/materials", ".mat");
}

sub get_textures {
	my @textures = read_dir2("base/textures", ".png");
	push (@textures, read_dir2("base/textures", ".jpg"));
	push (@textures, read_dir2("base/textures", ".tga"));
	return @textures;
}

sub get_models {
	my @models = read_dir2("base/models/objects", ".md2");
	push (@models, read_dir2("base/models/objects", ".md3"));
	push (@models, read_dir2("base/models/objects", ".dpm"));
	push (@models, read_dir2("base/models/objects", ".obj"));
	return @models;
}

my @maps = get_maps();
my @textures = get_textures();
my @materials = get_materials();
my @models = get_models();

my @suffixlist = (".png", ".tga", ".jpg");

foreach my $texture (sort @textures) {
	next if ($texture =~ /tex_common/ || $texture =~ /tex_terrain/);
	my $used = 0;
	$texture =~ s/^base\/textures\///;
	$texture =~ s/\..*$//;

	if ($texture =~ /_gm$/ || $texture =~ /_nm$/) {
		my $base = $texture;
		$base =~ s/_.*$//;
		unless (grep(/$base/, @textures)) {
			print STDERR "$texture has no base texture\n";
		}
		next;
	}

	foreach my $material (@materials) {
		if (check_content($material, $texture)) {
			$used = 1;
			last;
		}
		if ($texture =~ /\d$/) {
			my $anim = $texture;
			$anim =~ s/\d+$//;
			if (check_content($material, $anim)) {
				print "$texture is used as animation $anim\n";
				$used = 1;
				last;
			}
		}
	}
	foreach my $map (@maps) {
		if (check_content($map, $texture)) {
			print "$texture is used in a map\n";
			$used = 1;
			last;
		}
	}
	print STDERR "$texture is not used in materials or maps\n" unless $used;
}

foreach my $model (sort @models) {
	my $used = 0;
	$model =~ s/^base\/models\///;
	$model =~ s/\..*$//;
	foreach my $map (@maps) {
		if (check_content($map, $model)) {
			print "$model is used in a map\n";
			$used = 1;
			last;
		}
	}
	print STDERR "$model is not used in any maps\n" unless $used;
}

