# Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
include $(TOP)lib/make/head.inc

SUBDIRS+=lib
SUBDIRS+=main

# Define subdir recursion order.
main/all: lib/all

include $(TOP)lib/make/tail.inc
