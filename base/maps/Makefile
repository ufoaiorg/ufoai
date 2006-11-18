MAPSDIR = .

UFO2MAP = ../../ufo2map

MAPSRCS = $(shell find $(MAPSDIR) -name '*.map' -not -name 'tutorial*' -not -name '*autosave*' -not -name 'prefab*' -not -name 'test*' )
BSPS = $(MAPSRCS:.map=.bsp)

MAPNICE = -nice 19
UFO2MAPFLAGS = -extra -bounce 0
FAST_UFO2MAPFLAGS = -bounce 0

all: $(BSPS)

fast:
	$(MAKE) all UFO2MAPFLAGS="$(FAST_UFO2MAPFLAGS)"

clean:
	find $(MAPSDIR) -name '*.bsp' | xargs --no-run-if-empty rm

$(BSPS): %.bsp: %.map
	$(UFO2MAP) $(MAPNICE) $(UFO2MAPFLAGS) $<
