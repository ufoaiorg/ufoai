MODELDIR ?= base/models

UFOMODEL = ./ufomodel

ifeq ($(USEWINDOWSCMD),1)
	# TODO fix this to handle other model types on Windows
    MODELS = $(shell dir /S/B $(MODELDIR)\*.md2)
else
    MODELS = $(shell find $(MODELDIR) -regexptype posix-egrep -iregex '.*(md2|md3|dpm|obj)')
endif

clean-mdx:
	@echo "Deleting cached normals and tangents (*.mdx)..."
ifeq ($(USEWINDOWSCMD),1)
	@del /S $(MODELDIR)\*.mdx
else
	@find $(MODELDIR) -name '*.mdx' -delete
endif
	@echo "done"
