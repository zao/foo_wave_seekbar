use strict;
use Data::Dumper;
use File::Path qw(make_path remove_tree);
use File::Spec qw(rel2abs);
use File::chdir;
use File::Copy;
use File::Glob qw(:glob);

my $temp_dir = "build";

sub pub {
	if (@_) {
		system("pscp -i D:/Work/uni.ppk @_ zao\@suiko.acc.umu.se:public_html/");
	}
}

sub archive {
	my $pack = 'D:/utilities/7za.exe';
	my $target = File::Spec->rel2abs($_[0]);
	my $rwut = $_[1];
	while (my ($k, $v) = each %$rwut) {
		my $dst = "$temp_dir/$k";
		my @files = bsd_glob($v);
		make_path($dst);
		foreach my $file (@files) {
			copy($file, $dst);
		}
	}
	{
		local $CWD = $temp_dir;
		system("$pack a -tzip -mx9 -mmt \"$target\" *");
	}
	remove_tree($temp_dir);
}

my $comp_dir = '../user-components/foo_wave_seekbar';
my $fx_dir = '../effects';

my $release;
if (-1 != $#ARGV)
{
	$release = $ARGV[0];
} else {
	print 'Release version: ';
	chomp($release = <>);
}

my $rel_file = "foo_wave_seekbar-$release.fb2k-component";
my $arch_file = "foo_wave_seekbar-$release-archive.fb2k-component";

if (!-e $rel_file) {
	&archive($rel_file, {
				"./." => "$comp_dir/SciLexer.dll",
				"." => "$comp_dir/frontend_*.dll",
				"" => "$comp_dir/foo_wave_seekbar.dll"});
	&pub($rel_file);
}
if (!-e $arch_file) {
	&archive($arch_file, {  
				"./." => "$comp_dir/SciLexer.*",
				"." => "$comp_dir/frontend_*.*",
				"" => "$comp_dir/foo_wave_seekbar.*"});
	&pub($arch_file);
}
