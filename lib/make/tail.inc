# Copyright (c) David Hubbard 2016. Licensed under the GPLv3.

#
# Set up dependencies and compiler flags.
#
CXXFLAGS+=-g -std=c++11 -fno-exceptions -Wall -Wextra
PKG_CONFIG_FLAGS+=--static

ifneq ($(TOP),)
CXXFLAGS+=-I$(TOP)
endif

define IMPORT_PKG_CONFIG
CXXFLAGS+=$(strip $(shell pkg-config $(PKG_CONFIG_FLAGS) --cflags $(1)))
LDFLAGS+=$(strip $(shell pkg-config $(PKG_CONFIG_FLAGS) --libs $(1)))
endef
$(foreach pkg,$(PKGCONFIG_PKGS),$(eval $(call IMPORT_PKG_CONFIG,$(pkg))))

#
# Convert $(SRCS) into $(OBJS)
#
OBJS=$(strip $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SRCS))))

ifneq ($(VERBOSE),)
V=$(VERBOSE)
endif
ifneq ($(D),)
V=$(D)
endif

# ARFLAGS: remove 'v' unless V=1 was specified to make.
ifeq ($(V),)
MAKE_SUBDIR_FLAGS=--no-print-directory
ifneq ($(subst v,,$(ARFLAGS)),$(ARFLAGS))
ARFLAGS_RESPECTING_V=$(subst v,,$(ARFLAGS))
else
ARFLAGS_RESPECTING_V=$(ARFLAGS)
endif
else
MAKE_SUBDIR_FLAGS=
ARFLAGS_RESPECTING_V=$(ARFLAGS)
endif

ifeq ($(TOP),)
RELATIVE_PATH=
else
RELATIVE_PATH=$(shell a=$$PWD; echo $${a\#$$(realpath $(TOP))/})/
endif

#
# Define all: and clean: targets.
#
.phony: all clean

clean:
ifneq ($(strip $(TARGET_BIN) $(TARGET_LIB) $(OBJS)),)
ifeq ($(V),)
	@echo "clean $(RELATIVE_PATH): $(RM) $(strip $(TARGET_BIN) $(TARGET_LIB) $(OBJS))"; \
	$(RM) $(strip $(TARGET_BIN) $(TARGET_LIB) $(OBJS))
else
	$(RM) $(strip $(TARGET_BIN) $(TARGET_LIB) $(OBJS))
endif
endif

define SUBDIR_TARGET
.phony: subdirs_$(1)
$(1): subdirs_$(1)

subdirs_$(1):
	@$$(foreach d,$(2),$$(MAKE) $$(MAKE_SUBDIR_FLAGS) -C $$(d) $(1) TOP=../$(TOP) && )true

endef
ifneq ($(SUBDIRS),)
$(eval $(call SUBDIR_TARGET,all,$(SUBDIRS)))
$(eval $(call SUBDIR_TARGET,clean,$(SUBDIRS)))
endif

#
# If TARGET_LIB was set, build a static library.
# Else if TARGET_BIN was set, build a binary.
#
ifneq ($(TARGET_LIB),)
all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJS)
ifeq ($(V),)
	@echo "AR $(ARFLAGS_RESPECTING_V)cs $(RELATIVE_PATH)$@"; \
	$(AR) $(ARFLAGS_RESPECTING_V)cs $@ $^
else
	$(AR) $(ARFLAGS_RESPECTING_V)cs $@ $^
endif
else
ifneq ($(TARGET_BIN),)
all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
ifeq ($(V),)
	@echo "LINK   $(RELATIVE_PATH)$@"; \
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS)
else
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS)
endif
else
ifeq ($(SUBDIRS),)
$(error Makefile did not set TARGET_BIN, TARGET_LIB, or SUBDIRS. One of them must be set.)
endif
endif
endif

.cpp.o: $(HDRS)
ifeq ($(V),)
	@echo "CXX -c $(RELATIVE_PATH)$<"; \
	$(CXX) $(CXXFLAGS) -c -o $@ $<
else
	$(CXX) $(CXXFLAGS) -c -o $@ $<
endif
