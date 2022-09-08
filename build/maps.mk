MAPSDIR           ?= base/maps
UFO2MAP           := ufo2map
UFO2MAPPREFIX     := ./
UFO2MAP-SYS       := $(shell which ufo2map 2>/dev/null)
ifeq ($(ufo2map_DISABLE),yes) # Is the target exists?
  ifeq ("$(wildcard ./ufo2map$(EXE_EXT))","") # Is binary already builded?
    ifneq ("$(strip $(UFO2MAP-SYS))","") # Have an already system installed tool?
      UFO2MAP := $(UFO2MAP-SYS) # Use ufo2map from system
      UFO2MAPPREFIX :=
    endif
  endif
endif
MAPSRCS           := $(shell find $(MAPSDIR) -name '*.map' \! -name 'tutorial*' \! -name '*autosave*' \! -name 'test*' | xargs --no-run-if-empty du | sort -bnr | sed -e 's/^[0-9]*//' | tr -d "\t")
BSPS              := $(MAPSRCS:.map=.bsp)
NICE              ?= 19
UFO2MAPFLAGS      ?= -v 4 -nice $(NICE) -quant 4 -soft
FAST_UFO2MAPFLAGS ?= -v 2 -quant 6 -nice $(NICE)
ENTS_UFO2MAPFLAGS ?= -v 2 -nice $(NICE) -onlyents

# Check if Python3 and urllib3 module installed
urllib3:
	@$(PROGRAM_PYTHON3) -c 'import pkgutil; import sys; sys.exit(0) if pkgutil.find_loader("urllib3") else sys.exit(1)' || { echo Python3 or urllib3 module is not installed; exit 1; }

maps: $(BSPS)

maps-fast:
	$(MAKE) maps UFO2MAPFLAGS="$(FAST_UFO2MAPFLAGS)"

maps-ents:
	$(MAKE) maps UFO2MAPFLAGS="$(ENTS_UFO2MAPFLAGS)"

# TODO only sync if there were updates on the map files
maps-sync: Makefile.local urllib3
	$(PROGRAM_PYTHON3) contrib/map-get/update.py

force-maps-sync: Makefile.local urllib3
	$(PROGRAM_PYTHON3) contrib/map-get/update.py --reply=yes

clean-maps:
	@echo "Deleting maps..."
	@find $(MAPSDIR) -name '*.bsp' -delete
	@echo "done"

.DELETE_ON_ERROR:

$(BSPS): %.bsp: %.map $(UFO2MAP)
	$(UFO2MAPPREFIX)$(UFO2MAP)$(EXE_EXT) $(UFO2MAPFLAGS) $(<:base/%=%)
