#!/usr/bin/env python
# Copyright (c) David Hubbard 2017. Licensed under GPLv3.
# glslangValidator compiles GLSL shader source into SPIR-V binary
import os
import re
import subprocess
import sys

args = []
for a in sys.argv[1:]:
	if a[0:4] == "spv_":
		a = a.replace(".", "_")
	args.append(a)

cmd = [ os.path.join(os.getcwd(), "glslangValidator") ] + args
coproc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

while True:
	line = coproc.stdout.readline()
	line = line.splitlines()
	if len(line) < 1 or line[0] == '':
		break
	line = line[0]
	# filter out annoying warning
	if "version 450 is not yet complete" in line:
		continue
	# do not print filename again (ninja already does it)
	if line == sys.argv[-1]:
		continue
	print(line)

