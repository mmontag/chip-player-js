#!/usr/bin/python

import sys, os, xmp

argc = len(sys.argv)
argv = sys.argv

if argc < 2:
	print "Usage: %s <module> [channels]" % (os.path.basename(argv[0]))
	sys.exit(1)

info = xmp.struct_xmp_module_info()

x = xmp.Xmp()
try:
	x.loadModule(argv[1])
except IOError as (errno, strerror):
	sys.stderr.write("%s: %s\n" % (argv[1], strerror))
	sys.exit(1)

x.playerStart(44100, 0)
x.getInfo(info)

channels = []

if argc > 2:
	for i in range(argc - 2):
		channels.append(int(argv[i + 2]))
else:
	channels = range(info.mod[0].chn)

while x.playerFrame():
	x.getInfo(info)
	if info.loop_count > 0:
		break

	for i in channels:
		ci = info.channel_info[i]
		print "%d %d %d %d %d %d %d %d" % (info.time, info.row,
			info.frame, i, ci.period, ci.volume, ci.instrument,
			ci.pan)


x.playerEnd()
x.releaseModule()
