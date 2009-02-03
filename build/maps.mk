MAPSDIR ?= base/maps

UFO2MAP = ./ufo2map

MAPSRCS = $(shell find $(MAPSDIR) -name '*.map' \! -name 'tutorial*' \! -name '*autosave*' \! -name 'prefab*' \! -name 'test*' )
BSPS = $(MAPSRCS:.map=.bsp)

# Set NUMTHREADS to enable multi-threading in ufo2map. This
# setting strongly depends on the OS and hardware, so need to
# be chosen appropriately.

ifeq ($(TARGET_OS),darwin)
	# Setting for Mac OS X and Darwin OS
	# \note This should also work for *BSD
	NUMTHREADS ?= $(shell sysctl -n hw.ncpu)
else
	ifeq ($(TARGET_OS),linux-gnu)
		NUMTHREADS ?= $(shell grep -c ^processor /proc/cpuinfo)
	else
		NUMTHREADS ?= 1
	endif
endif

NICE = 19
UFO2MAPFLAGS = -v 2 -nice $(NICE) -extra -t $(NUMTHREADS)
FAST_UFO2MAPFLAGS = -v 2 -quant 6 -nice $(NICE) -t $(NUMTHREADS)
ENTS_UFO2MAPFLAGS = -v 2 -nice $(NICE) -onlyents

maps: $(UFO2MAP_TARGET) $(BSPS)

maps-fast:
	$(MAKE) maps UFO2MAPFLAGS="$(FAST_UFO2MAPFLAGS)"

maps-ents:
	$(MAKE) maps UFO2MAPFLAGS="$(ENTS_UFO2MAPFLAGS)"

clean-maps:
	@echo "Deleting maps..."
	@find $(MAPSDIR) -name '*.bsp' -delete
	@echo "done"

$(BSPS): %.bsp: %.map
	$(UFO2MAP) $(UFO2MAPFLAGS) $<
