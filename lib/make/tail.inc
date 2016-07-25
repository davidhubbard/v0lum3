# Copyright (c) David Hubbard 2016. Licensed under the GPLv3.

#
# Tell GNU Make to not automatically define suffix rules.
# (For example, this prevents a .cpp.o: rule.)
#
.SUFFIXES:

#
# Set up dependencies and compiler flags.
#
CXXFLAGS+=-ggdb -std=c++11 -fno-exceptions -Wall -Wextra
LDFLAGS+=-Og -ggdb
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
OBJS+=$(strip $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SRCS))))

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

ifeq ($(GLSLANG),)
GLSLANG:=$(TOP)vendor/bin/glslangValidator
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

CLEAN_FILES_TO_RM=$(strip $(TARGET_BIN) $(TARGET_LIB) $(OBJS) $(INTFILES))
clean:
ifneq ($(CLEAN_FILES_TO_RM),)
ifeq ($(V),)
	@printf "cd %-20s $(RM) $(CLEAN_FILES_TO_RM)\n" "$(RELATIVE_PATH)"; \
	$(RM) $(CLEAN_FILES_TO_RM)
else
	$(RM) $(CLEAN_FILES_TO_RM)
endif
endif

define SUBDIR_TARGET
.phony: $(2)/$(1)
$(1): $(2)/$(1)

$(2)/$(1):
	@$$(MAKE) $$(MAKE_SUBDIR_FLAGS) -C $(2) $(1) TOP=../$(TOP)

endef
$(foreach s,$(SUBDIRS),$(eval $(call SUBDIR_TARGET,all,$(s))))
$(foreach s,$(SUBDIRS),$(eval $(call SUBDIR_TARGET,clean,$(s))))

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

$(TARGET_BIN): $(OBJS) $(LIBS)
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

define CPP_TARGET
$(1).o : $(1).cpp $(HDRS)
ifeq ($(V),)
	@echo "CXX -c $(RELATIVE_PATH)$$<"; \
	$$(CXX) $$(CXXFLAGS) -c -o $$@ $$<
else
	$$(CXX) $$(CXXFLAGS) -c -o $$@ $$<
endif
endef
$(foreach f,$(filter %.cpp,$(SRCS)),$(eval $(call CPP_TARGET,$(patsubst %.cpp,%,$(f)))))

define COMPILE_GLSLANG
$(1).spv: $(1)
ifeq ($(V),)
	@( echo -n "GLSL   $(RELATIVE_PATH)"; \
	$$(GLSLANG) $$(GLSLANGFLAGS) -V -o $$@ $$< ) | sed -e '/^\(Linked.*stage:\|\)$$$$/d'
else
	$$(GLSLANG) $$(GLSLANGFLAGS) -V -o $$@ $$<
endif
endef
$(foreach f,$(filter %.spv,$(HDRS)),$(eval $(call COMPILE_GLSLANG,$(patsubst %.spv,%,$(f)))))
