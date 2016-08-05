# Copyright (c) David Hubbard 2016. Licensed under the GPLv3.

#
# Tell GNU Make to not automatically define suffix rules.
# (For example, this prevents a .cpp.o: rule.)
#
.SUFFIXES:

#
# Set up dependencies and compiler flags.
#
CXXFLAGS+=-ggdb -std=c++11 -fno-exceptions -fno-omit-frame-pointer -Wall -Wextra -Wformat=2
CXXFLAGS+=-fstrict-aliasing -Wstrict-aliasing -fstrict-overflow -Wstrict-overflow=5
LDFLAGS+=-Og -ggdb
PKG_CONFIG_FLAGS+=--static
OBJCOPY ?= objcopy
OBJSYMS_HDR ?= objsyms.h
ifeq ($(PKG_CONFIG_PATH),)
$(error PKG_CONFIG_PATH is not set, please run build.sh and follow the directions to set PKG_CONFIG_PATH)
endif

ifneq ($(TOP),)
CXXFLAGS+=-I$(TOP)
endif

define IMPORT_PKG_CONFIG
CXXFLAGS+=$(strip $(shell pkg-config $(PKG_CONFIG_FLAGS) --cflags $(1)))
LDFLAGS+=$(strip $(shell pkg-config $(PKG_CONFIG_FLAGS) --libs $(1)))
endef
$(foreach pkg,$(PKG_CONFIG_PKGS),$(eval $(call IMPORT_PKG_CONFIG,$(pkg))))

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

define CPP_TARGET
$(1).o : $(1).cpp $(HDRS)
	$$(eval CMD := $$(CXX) $$(CXXFLAGS) -c -o $$@ $$<)
ifeq ($(V),)
	@echo "CXX    $(RELATIVE_PATH)$$<"; $$(CMD)
else
	$$(CMD)
endif
endef
$(foreach f,$(filter %.cpp,$(SRCS)),$(eval $(call CPP_TARGET,$(patsubst %.cpp,%,$(f)))))

define COMPILE_GLSLANG
$(1).spv: $(1)
	$$(eval CMD := $$(GLSLANG) $$(GLSLANGFLAGS) -V -o $$@ $$<)
ifeq ($(V),)
	@( echo -n "GLSL   $(RELATIVE_PATH)"; $$(CMD) ) | sed -e '/^\(Linked.*stage:\|\)$$$$/d'
else
	$$(CMD)
endif
endef
$(foreach f,$(GLSL),$(eval $(call COMPILE_GLSLANG,$(f))))
BINS+=$(patsubst %,%.spv,$(GLSL))

# This rule ignores the symbol $$(SYM)_size because end and start give that information.
define BIN_TO_OBJ_TARGET
$(1).o: $(1)
	$$(eval SYM := obj_$(subst .,_,$(subst /,_,$(1))))
	$$(eval OBJCPSYM := _binary_$(subst .,_,$(subst /,_,$(1))))
	$$(eval CMD := $(LD) -r -o $(1).o -z noexecstack --format=binary $(1) && \
		$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents \
		--redefine-sym $$(OBJCPSYM)_start=$$(SYM)_start \
		--redefine-sym $$(OBJCPSYM)_end=$$(SYM)_end $(1).o)
ifeq ($(V),)
	@echo "OBJCP  $(RELATIVE_PATH)$(1)"; $$(CMD)
else
	$$(CMD)
endif
	@if ! grep -q $$(SYM)_start $(OBJSYMS_HDR) /dev/null; then \
		echo "Warning: $(RELATIVE_PATH)$(OBJSYMS_HDR) is missing \"extern const char $$(SYM)_start[]\""; \
	fi
	@if ! grep -q $$(SYM)_end $(OBJSYMS_HDR) /dev/null; then \
		echo "Warning: $(RELATIVE_PATH)$(OBJSYMS_HDR) is missing \"extern const char $$(SYM)_end[]\""; \
	fi
endef
$(foreach f,$(BINS),$(eval $(call BIN_TO_OBJ_TARGET,$(f))))

#
# Convert $(SRCS) into $(OBJS)
#
OBJS+=$(strip $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SRCS))) $(patsubst %,%.o,$(BINS)))

#
# If TARGET_LIB was set, build a static library.
# Else if TARGET_BIN was set, build a binary.
#
ifneq ($(TARGET_LIB),)
all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJS)
	$(eval CMD := $(AR) $(ARFLAGS_RESPECTING_V)cs $@ $^)
ifeq ($(V),)
	@echo "AR $(ARFLAGS_RESPECTING_V)cs $(RELATIVE_PATH)$@"; $(CMD)
else
	$(CMD)
endif
else
ifneq ($(TARGET_BIN),)
all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS) $(LIBS)
	$(eval CMD := $(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS))
ifeq ($(V),)
	@echo "LINK   $(RELATIVE_PATH)$@"; $(CMD)
else
	$(CMD)
endif
else
ifeq ($(SUBDIRS),)
$(error Makefile did not set TARGET_BIN, TARGET_LIB, or SUBDIRS. One of them must be set.)
endif
endif
endif

#
# Define all: and clean: targets.
#
.phony: all clean

CLEAN_FILES_TO_RM=$(strip $(TARGET_BIN) $(TARGET_LIB) $(OBJS) $(patsubst %,%.spv,$(GLSL)))
clean:
	$(eval CMD := $(RM) $(CLEAN_FILES_TO_RM))
ifneq ($(CLEAN_FILES_TO_RM),)
ifeq ($(V),)
	@printf "cd %-20s $(CMD)\n" "$(RELATIVE_PATH)"; $(CMD)
else
	$(CMD)
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

.phony: check_build_sh_has_run

check_build_sh_has_run:
	@if [ ! -d $(TOP)vendor/etc/vulkan ]; then \
		echo "Missing $(TOP)vendor/etc/vulkan. Please try running:"; \
		echo "    build.sh"; \
		exit 1; \
	fi
