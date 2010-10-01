MAPSDIR           ?= base/maps
UFO2MAP            = ./ufo2map
MAPSRCS           := $(shell find $(MAPSDIR) -name '*.map' \! -name 'tutorial*' \! -name '*autosave*' \! -name 'test*' )
BSPS              := $(MAPSRCS:.map=.bsp)
NICE              ?= 19
UFO2MAPFLAGS      ?= -v 4 -nice $(NICE) -quant 2 -extra
FAST_UFO2MAPFLAGS ?= -v 2 -quant 6 -nice $(NICE)
ENTS_UFO2MAPFLAGS ?= -v 2 -nice $(NICE) -onlyents

maps: ufo2map $(BSPS)

maps-fast:
	$(MAKE) maps UFO2MAPFLAGS="$(FAST_UFO2MAPFLAGS)"

maps-ents:
	$(MAKE) maps UFO2MAPFLAGS="$(ENTS_UFO2MAPFLAGS)"

# TODO only sync if there were updates on the map files
maps-sync:
	contrib/scripts/map-get.py upgrade

force-maps-sync:
	contrib/scripts/map-get.py upgrade --reply=yes

clean-maps:
	@echo "Deleting maps..."
	@find $(MAPSDIR) -name '*.bsp' -delete
	@echo "done"

$(BSPS): %.bsp: %.map
	$(UFO2MAP) $(UFO2MAPFLAGS) $(<:base/%=%)
