#!/usr/bin/python

import sys
import os

ROOT_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(ROOT_PATH + "/../libxmp-python")

import pyxmp

argc = len(sys.argv)
argv = sys.argv

if argc < 2:
	print "Usage: %s <module> [channels]" % (os.path.basename(argv[0]))
	sys.exit(1)

mi = pyxmp.struct_xmp_module_info()
fi = pyxmp.struct_xmp_frame_info()

xmp = pyxmp.Xmp()
try:
	xmp.load_module(argv[1])
except IOError as (errno, strerror):
	sys.stderr.write("%s: %s\n" % (argv[1], strerror))
	sys.exit(1)

xmp.start_player(8000, 0)
xmp.get_module_info(mi)

channels = []

if argc > 2:
	for i in range(argc - 2):
		channels.append(int(argv[i + 2]))
else:
	channels = range(mi.mod[0].chn)

while xmp.play_frame():
	xmp.get_frame_info(fi)
	if fi.loop_count > 0:
		break

	for i in channels:
		ci = fi.channel_info[i]
		print "%d %d %d %d %d %d %d %d %d" % (fi.time, fi.row,
			fi.frame, i, ci.period, ci.volume, ci.instrument,
			ci.pan, ci.sample)


xmp.end_player()
xmp.release_module()
