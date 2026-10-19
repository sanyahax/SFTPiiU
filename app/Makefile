#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

#-------------------------------------------------------------------------------
# App metadata for the .wuhb bundle
#-------------------------------------------------------------------------------
APP_NAME       := SFTPiiU
APP_SHORTNAME  := SFTPiiU
APP_AUTHOR     := sanyahax - ajk.dev
APP_ICON       := $(TOPDIR)/icon.png
APP_TV_SPLASH  := $(TOPDIR)/splash_tv.png
APP_DRC_SPLASH := $(TOPDIR)/splash_drc.png

include $(DEVKITPRO)/wut/share/wut_rules

WUT_ROOT := $(DEVKITPRO)/wut
PKGCONF  := $(DEVKITPRO)/portlibs/wiiu/bin/powerpc-eabi-pkg-config

#-------------------------------------------------------------------------------
# The app shares the server core with the plugin (../source).
#-------------------------------------------------------------------------------
TARGET   := SFTPiiU
BUILD    := build
SOURCES  := source ../source ../source/crypto ../source/ssh ../source/util ../source/wiiu
DATA     :=
INCLUDES := source ../source

#-------------------------------------------------------------------------------
# options for code generation
#-------------------------------------------------------------------------------
CFLAGS   := -Wall -O2 -ffunction-sections $(MACHDEP)
CFLAGS   += $(INCLUDE) -D__WIIU__ -D__WUT__ $(shell $(PKGCONF) --cflags SDL2_image SDL2_ttf)
# Test build that can be remote-controlled through SD:/sftpiiu_tour.txt:
#   make UI_TOUR=1 TARGET=SFTPiiU_tour BUILD=build_tour
ifeq ($(UI_TOUR),1)
CFLAGS   += -DSFTPIIU_UI_TOUR
endif
CXXFLAGS := $(CFLAGS) -std=gnu++20
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map) $(EXTRA_LDFLAGS)

# pkg-config leaves out libjpeg and puts -lwut in the middle; add both at the end.
LIBS     := -lmocha $(shell $(PKGCONF) --libs --static SDL2_image SDL2_ttf) -ljpeg -lSDL2 -lwut

#-------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level
# containing include and lib
#-------------------------------------------------------------------------------
LIBDIRS  := $(PORTLIBS) $(WUT_ROOT) $(WUT_ROOT)/usr

#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)

export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                 $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD := $(CXX)

# The plugin (../SFTPiiU.wps) is linked in for "keep running in the
# background"; main.cpp declares its symbols itself, so no header is needed.
export PLUGIN_WPS := $(abspath $(CURDIR)/../SFTPiiU.wps)

export OFILES_BIN := $(addsuffix .o,$(BINFILES)) SFTPiiU.wps.o
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all plugin

#-------------------------------------------------------------------------------
all: $(BUILD)

# Builds the plugin first. MAKEFLAGS is dropped so that variables such as
# TARGET/BUILD of a test build do not leak into the plugin's Makefile.
plugin:
	@env -u MAKEFLAGS -u MAKEOVERRIDES $(MAKE) --no-print-directory -C $(CURDIR)/.. EXTRA_LDFLAGS="$(EXTRA_LDFLAGS)"

$(BUILD): plugin
	@$(shell [ ! -d $(BUILD) ] && mkdir -p $(BUILD))
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#-------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).wuhb $(TARGET).rpx $(TARGET).elf

#-------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

#-------------------------------------------------------------------------------
# main targets
#-------------------------------------------------------------------------------
all: $(OUTPUT).wuhb

$(OUTPUT).wuhb: $(OUTPUT).rpx
$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SRC): $(HFILES_BIN)

SFTPiiU.wps.o: $(PLUGIN_WPS)
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
