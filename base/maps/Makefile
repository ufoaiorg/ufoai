MAPSDIR = .

UFO2MAP = ../../ufo2map

MAPSRCS = $(shell find $(MAPSDIR) -name '*.map' -not -name 'tutorial*' -not -name '*autosave*' -not -name 'prefab*')
BSPS = $(MAPSRCS:.map=.bsp)

QRAD3FLAGS = -extra -bounce 0 -chop 32 -noradiosity

all: $(BSPS)

clean:
	find $(MAPSDIR) -name '*.bsp' | xargs --no-run-if-empty rm

$(BSPS): %.bsp: %.map
	$(UFO2MAP) $(QRAD3FLAGS) $<
