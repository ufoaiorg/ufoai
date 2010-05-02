MODELDIR ?= base/models

UFOMODEL = ./ufomodel
UFOMODEL_PARAMS = -mdx -overwrite -v 

ifeq ($(USEWINDOWSCMD),1)
    MODELS_MD2 := $(shell dir /S/B $(MODELDIR)\*.md2)
    MODELS_MD3 := $(shell dir /S/B $(MODELDIR)\*.md3)
    MODELS_OBJ := $(shell dir /S/B $(MODELDIR)\*.obj)
    MODELS_DPM := $(shell dir /S/B $(MODELDIR)\*.dpm)
else
    MODELS_MD2 := $(shell find $(MODELDIR) -name "*.md2")
    MODELS_MD3 := $(shell find $(MODELDIR) -name "*.md3")
    MODELS_OBJ := $(shell find $(MODELDIR) -name "*.obj")
    MODELS_DPM := $(shell find $(MODELDIR) -name "*.dpm")
endif

MDXS_MD2 := $(MODELS_MD2:.md2=.mdx)
MDXS_MD3 := $(MODELS_MD3:.md3=.mdx)
MDXS_OBJ := $(MODELS_OBJ:.obj=.mdx)
MDXS_DPM := $(MODELS_DPM:.dpm=.mdx)

models: $(UFOMODEL_TARGET) $(MDXS_MD2) $(MDXS_MD3) $(MDXS_OBJ) $(MDXS_DPM)

$(MDXS_MD2): %.mdx: %.md2
	$(UFOMODEL) $(UFOMODEL_PARAMS) -s 0.6 -f $(subst base/,,$<)

$(MDXS_MD3): %.mdx: %.md3
	$(UFOMODEL) $(UFOMODEL_PARAMS) -s 0.6 -f $(subst base/,,$<)

$(MDXS_OBJ): %.mdx: %.obj
	$(UFOMODEL) $(UFOMODEL_PARAMS) -s 0.6 -f $(subst base/,,$<)

$(MDXS_DPM): %.mdx: %.dpm
	$(UFOMODEL) $(UFOMODEL_PARAMS) -s 0.6 -f $(subst base/,,$<)

clean-mdx:
	@echo "Deleting cached normals and tangents (*.mdx)..."
ifeq ($(USEWINDOWSCMD),1)
	@del /S $(MODELDIR)\*.mdx
else
	@find $(MODELDIR) -name '*.mdx' -delete
endif
	@echo "done"
