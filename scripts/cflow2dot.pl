#!/usr/bin/perl
#
# $Id: cflow2dot.pl,v 1.1 2001-11-09 21:45:08 cmatsuoka Exp $

$driver   = "color=red";
$loader   = "color=green";

$lowlevel = "shape=box style=bold";
$midlevel = "shape=trapezium";
$optional = "shape=hexagon";

$resource = "style=filled";

$def_edge  = "color=black style=solid";

print "digraph sarien {\n";
print "rankdir=LR; edge [$def_edge];\n";

while (<>) {
	s/^\d+//;

	# Assign node properties to interesting functions
	#
	$x = $_;
	if (/\t*(\w+) {(\w+)\.c/) {
		(my $x, $_) = ($1, $2);
		/_load/    && print "\t$x [$loader];\n";
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
		|| /[^\w](strtoul|fseek|ftell|abort)[^\w]/) && next;

	$edge{@m[$i - 1]} &&
		($_ = "\tedge [$edge{@m[$i - 1]}];\n$_\tedge [$def_edge];\n");

	print;
}

print "}\n";

