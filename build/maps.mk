MAPSDIR = base/maps

UFO2MAP = ./ufo2map

MAPSRCS = $(shell find $(MAPSDIR) -name '*.map' \! -name 'tutorial*' \! -name '*autosave*' \! -name 'prefab*' \! -name 'test*' )
BSPS = $(MAPSRCS:.map=.bsp)

NICE = 19
UFO2MAPFLAGS = -nice $(NICE) -extra
FAST_UFO2MAPFLAGS = -nice $(NICE)
ENTS_UFO2MAPFLAGS = -nice $(NICE) -onlyents

maps: $(UFO2MAP_TARGET) $(BSPS)

maps-fast:
	$(MAKE) maps UFO2MAPFLAGS="$(FAST_UFO2MAPFLAGS)"

maps-ents:
	$(MAKE) maps UFO2MAPFLAGS="$(ENTS_UFO2MAPFLAGS)"

maps-clean:
	find $(MAPSDIR) -name '*.bsp' | xargs rm

$(BSPS): %.bsp: %.map
	$(UFO2MAP) $(UFO2MAPFLAGS) $<
