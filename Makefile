# Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
include $(TOP)lib/make/head.inc

SUBDIRS+=lib
SUBDIRS+=main

# Define subdir recursion order.
main/all: lib/all

# Add a check that build.sh has been run.
all: check_build_sh_has_run

include $(TOP)lib/make/tail.inc
