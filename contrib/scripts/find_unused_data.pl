#!/usr/bin/perl

use strict;
use warnings;

my @maps = get_maps();
push(@maps, get_prefabs());

sub read_dir2 {
	my $DIR = shift(@_);
	my $EXT = shift(@_);
	opendir(FILES, $DIR) || die "Could not read dir ($DIR)";
	my @files = readdir(FILES);
	closedir(FILES);
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

sub get_prefabs {
	return read_dir2("radiant/prefabs", ".map");
}

sub get_materials {
	return read_dir2("base/materials", ".mat");
}

sub get_ufo_scripts {
	return read_dir2("base/ufos", ".ufo");
}

sub get_textures {
	my @textures = read_dir2("base/textures", ".png");
	push (@textures, read_dir2("base/textures", ".jpg"));
	push (@textures, read_dir2("base/textures", ".tga"));
	return @textures;
}

sub get_models {
	my @models = read_dir2("base/models", ".md2");
	push (@models, read_dir2("base/models", ".md3"));
	push (@models, read_dir2("base/models", ".dpm"));
	push (@models, read_dir2("base/models", ".obj"));
	return @models;
}

sub check_textures() {
	print "Checking textures\n";

	my @textures = get_textures();
	my @materials = get_materials();

	print "Found $#textures textures and $#materials materials\n";

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
				if (check_content($material, 'texture.*'.$anim.'.*0$')) {
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
}

sub check_models() {
	print "Checking models\n";
	my @models = get_models();
	my @ufos = get_ufo_scripts();

	print "Found $#models models\n";

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

		foreach my $ufo (@ufos) {
			if ($model =~ /^soldiers\// || $model =~ /^pilots\// || $model =~ /^civilians\// || $model =~ /^aliens\//) {
				# TODO:
				print "TODO $model\n";
				$used = 1;
				last;
			}
			elsif (check_content($ufo, $model)) {
				print "$model is used in an ufo script file\n";
				$used = 1;
				last;
			}
		}

		print STDERR "$model is not used in any map or script\n" unless $used;
	}
}

############### main

my $ACTION = "";
if ($#ARGV >= 0) {
	$ACTION = $ARGV[0];
}

print "Found $#maps maps (including prefabs)\n";

if ($ACTION eq "models") {
	check_models();
} elsif ($ACTION eq "textures") {
	check_textures();
} elsif ($ACTION eq "") {
	check_textures();
	check_models();
} else {
	die "Usage\t$0 [models|textures]\n";
}

__END__
