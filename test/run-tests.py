#!/usr/bin/python

from subprocess import Popen, PIPE
import os
import sys
import shlex
import gzip

xmp = "../src/main/xmp -o/dev/null"
logname = "test.log"

class Log:
	def __init__(self, filename):
		self.f = open(filename, "w")

	def __del__(self):
		self.f.close()

	def i(self, string):
		self.f.write(string.rstrip() + "\n")

		
class Test:
	def __init__(self, filename, log):
		f = gzip.open(filename)
		self.dumptype = f.readline().rstrip()
		self.name, x = filename.split(".", 1)
		self.description = f.readline().rstrip()
		self.parameters = f.readline().rstrip()

		self.name = os.path.basename(self.name)
		self.log = log

		self.log.i("Test: %s" % (self.name));
		self.log.i("Type: %s" % (self.dumptype));
		self.log.i("Description: %s" % (self.description));

		# Skip comments
		firstline = True
		while (True):
			line = f.readline()
			if (line == "---\n"):
				break
			if (line == "\n"):
				if (firstline):
					firstline = False
					continue
				else:
					self.log.i(" .")
			else:
				self.log.i(" %s" % (line));

		# Testfile positioned at results
		self.testfile = f

	def __del__(self):
		self.testfile.close()

	def __compare(self, f1, f2):
		linecount = 1
		line1 = f1.readline()
		line2 = f2.readline()

		while (True):
			linecount += 1
			line1 = f1.readline()
			line2 = f2.readline()
			if (line1 != line2 or line1 == ''):
				break
		
		if (line1 != '' or line2 != ''):
			return linecount
		else:
			return 0

	def run(self):
		sys.stdout.write("%-4.4s %-10.10s %-50.50s... " %
				(self.dumptype, self.name, self.description))
		cmd = xmp + " --dump " + self.dumptype + " " + self.parameters

		self.log.i("Command: %s" % cmd)
		args = shlex.split(cmd)

		p = Popen(args, stdout=PIPE)

		if (self.__compare(p.stdout, self.testfile) == 0):
			print "pass"
			self.log.i("Result: pass")
			return 0
		else:
			print "**fail**"
			self.log.i("Result: **fail**")
			return -1
		
total = 0
fail = 0

log = Log(logname)

dirlist = sorted(os.listdir("t"))
for name in dirlist:
	test = Test("t/" + name, log)
	if (test.run() != 0):
		fail += 1
	total += 1
	log.i("")

print
if (fail > 0):
	print " %d tests failed. See %s for details" % (fail, logname)
else:
	print " All tests passed."
