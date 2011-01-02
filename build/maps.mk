MAPSDIR           ?= base/maps
UFO2MAP            = ./ufo2map$(EXE_EXT)
MAPSRCS           := $(shell find $(MAPSDIR) -name '*.map' \! -name 'tutorial*' \! -name '*autosave*' \! -name 'test*' | xargs du | sort -bnr | sed -r -e 's/^[0-9]+[[:space:]]+//' )
BSPS              := $(MAPSRCS:.map=.bsp)
NICE              ?= 19
UFO2MAPFLAGS      ?= -v 4 -nice $(NICE) -quant 4 -extra
FAST_UFO2MAPFLAGS ?= -v 2 -quant 6 -nice $(NICE)
ENTS_UFO2MAPFLAGS ?= -v 2 -nice $(NICE) -onlyents

maps: ufo2map $(BSPS)

maps-fast:
	$(MAKE) maps UFO2MAPFLAGS="$(FAST_UFO2MAPFLAGS)"

maps-ents:
	$(MAKE) maps UFO2MAPFLAGS="$(ENTS_UFO2MAPFLAGS)"

# TODO only sync if there were updates on the map files
maps-sync:
	contrib/map-get/map-get.py upgrade

force-maps-sync:
	contrib/map-get/map-get.py upgrade --reply=yes

clean-maps:
	@echo "Deleting maps..."
	@find $(MAPSDIR) -name '*.bsp' -delete
	@echo "done"

.DELETE_ON_ERROR:

$(BSPS): %.bsp: %.map
	$(UFO2MAP) $(UFO2MAPFLAGS) $(<:base/%=%)
