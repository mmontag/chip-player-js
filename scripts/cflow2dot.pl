#!/usr/bin/perl
#
# $Id: cflow2dot.pl,v 1.2 2001-11-09 22:15:32 cmatsuoka Exp $

$driver   = "color=red";
$fmopl    = "color=orange";
$mixer    = "color=green";
$player   = "color=blue";

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
		(my $x, $_) = ($1, $2);
		/_load/    && print "\t$x [$loader];\n";
		/^fmopl/   && print "\t$x [$fmopl];\n";
		/^driver/  && print "\t$x [$driver];\n";
		/^mixer/   && print "\t$x [$mixer];\n";
		/^mix_/    && print "\t$x [$mixer];\n";
		/^effects/ && print "\t$x [$player];\n";
		/^period/  && print "\t$x [$player];\n";
		/^player/  && print "\t$x [$player];\n";
		/^scan/    && print "\t$x [$player];\n";

		($x =~ /^xmp_/) && print "\t$x [$api];\n";
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
		|| /[^\w](gtk_|gdk_|GTK_)/
		|| /[^\w](strtoul|fseek|ftell|abort|fgetc)[^\w]/) && next;

	$edge{@m[$i - 1]} &&
		($_ = "\tedge [$edge{@m[$i - 1]}];\n$_\tedge [$def_edge];\n");

	print;
}

print "}\n";

