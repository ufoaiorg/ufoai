MAPSDIR = .

MAPSRCS = $(shell find $(MAPSDIR) -name '*.map' -not -name 'tutorial*' -not -name 'prefab*')
BSPS = $(MAPSRCS:.map=.bsp)

QRAD3FLAGS = -bounce 0 -chop 32 -extra

all: $(BSPS)

$(BSPS): %.bsp: %.map
	ufo2map $(QRAD3FLAGS) $<
