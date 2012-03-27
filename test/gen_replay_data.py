#!/usr/bin/python

import sys, os, xmp

if len(sys.argv) < 2:
	print "Usage: %s <module>" % (os.path.basename(sys.argv[0]))
	sys.exit(1)

info = xmp.struct_xmp_module_info()

x = xmp.Xmp()
try:
	x.loadModule(sys.argv[1])
except IOError as (errno, strerror):
	sys.stderr.write("%s: %s\n" % (sys.argv[1], strerror))
	sys.exit(1)

x.playerStart(44100, 0)

while x.playerFrame():
	x.getInfo(info)
	if info.loop_count > 0:
		break

	ci = info.channel_info[0]
	print "%d %d %d %d %d %d" % (info.time, info.row, info.frame, ci.period, ci.volume, ci.instrument)


x.playerEnd()
x.releaseModule()
