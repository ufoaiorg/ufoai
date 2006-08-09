export

.PHONY: src maps all help clean clean_src clean_maps

.DEFAULT:
	-@$(MAKE) -C src $(MAKECMDGOALS)
	-@$(MAKE) -C base/maps $(MAKECMDGOALS)

all: src maps

src:
	@$(MAKE) -C src all

maps:
	@$(MAKE) -C base/maps all

help:
	@echo "Try typing `make all' followed by `make install' or read the README."

clean: clean_src

clean_src:
	@$(MAKE) -C src clean 

clean_maps:
	@$(MAKE) -C base/maps clean

