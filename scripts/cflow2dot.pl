#!/usr/bin/perl
#
# $Id: cflow2dot.pl,v 1.4 2001-11-15 09:59:54 cmatsuoka Exp $

$driver   = "color=red";
$fmopl    = "color=orange";
$mixer    = "color=green";
$player   = "color=blue";
$loader   = "color=purple";

$api      = "shape=box";
$resource = "style=filled";

$def_edge  = "color=black style=solid";

print "digraph sarien {\n";
print "rankdir=LR; edge [$def_edge];\n";

while (<>) {
	s/^\d+//;

	# Assign node properties to interesting functions
	#
	$x = $_;
	if (/\t*(\w+) {.*\/(\w+)\.c/) {
		my ($func, $file) = ($1, $2);

		set_color ($func, "^fmopl",   $fmopl,   $file);
		set_color ($func, "^driver",  $driver,  $file);
		set_color ($func, "^mixer",   $mixer,   $file);
		set_color ($func, "^mix_",    $mixer,   $file);
		set_color ($func, "^player",  $player,  $file);
		set_color ($func, "^scan",    $player,  $file);
		set_color ($func, "^effects", $player,  $file);
		set_color ($func, "^period",  $player,  $file);
		set_color ($func, "^load",    $loader,  $file);
		set_color ($func, "^depack",  $loader,  $file);
		set_color ($func, "^unsqsh",  $loader,  $file);
		set_color ($func, "^mmcmp",   $loader,  $file);

		($func =~ /^xmp_/) &&
			push @{$attrlist{$func}}, "\t$func [$api];\n";
	}
	$_ = $x;

	($f) = /(\w+)/;
	$i = rindex $_, "\t";
	@m[$i] = $f;
	($i < 1) && next;

	$_ = "\t\"@m[$i - 1]\" -> \"@m[$i]\";\n";

	# Filter out functions we don't want charted
	#
	(/[^\w](fileno|[BL]_ENDIAN(16|32))|[ML]SN[^\w]|str_adj/
		|| /[^\w]((LOAD|PATTERN|INSTRUMENT)_INIT|report|V)[^\w]/
		|| /[^\w]((PATTERN|TRACK)_ALLOC|EVENT|MODULE_INFO)[^\w]/
		|| /[^\w](memset|feof|strtok|atoi|atol|abs|tolower)[^\w]/
		|| /[^\w](rand|srand|sin|cos|atan|log10|pow)[^\w]/
		|| /[^\w](strtoul|fseek|ftell|abort|fgetc|difftime)[^\w]/
		|| /[^\w](mkstemp|strstr)[^\w]/
		|| /[^\w](select|shm(get|at|dt|ctl))[^\w]/
		|| /[^\w](Log|CALC_FCSLOT)[^\w]/
		|| /[^\w](OPL_|set_ar_|set_ksl_|set_mul|set_sl_)/
		|| /[^\w](CSMKey|set_algor[iy]thm)/
		|| /[^\w](gtk_|gdk_|GTK_)/
		) && next;

	$listed{@m[$i - 1]} = 1;
	$listed{@m[$i]} = 1;

	$edge{@m[$i - 1]} &&
		($_ = "\tedge [$edge{@m[$i - 1]}];\n$_\tedge [$def_edge];\n");

	print;

	foreach $i (keys %attrlist) {
		print @{$attrlist{$i}} if $listed{$i};
	}
}

print "}\n";


sub set_color {
	my ($func, $re, $color, $file) = @_;

	if ($file =~ /$re/) {
		push @{$attrlist{$func}}, "\t$func [$color];\n";
		$edge{$func} = $color;
	}
}
