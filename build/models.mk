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

# smoothing normals and tangents
# * aircraft  0.7
# * aliens    0.3
# * animals   0.5
# * civilians 0.5
# * objects   0.2
# * soldiers  0.0
# * weapons   0.6

MDXS_MD2 := $(MODELS_MD2:.md2=.mdx)
MDXS_MD3 := $(MODELS_MD3:.md3=.mdx)
MDXS_OBJ := $(MODELS_OBJ:.obj=.mdx)
MDXS_DPM := $(MODELS_DPM:.dpm=.mdx)
MDXS     := $(MDXS_MD2) $(MDXS_MD3) $(MDXS_OBJ) $(MDXS_DPM)

models: $(UFOMODEL_TARGET) $(MDXS)

$(MDXS_MD2): %.mdx: %.md2
$(MDXS_MD3): %.mdx: %.md3
$(MDXS_OBJ): %.mdx: %.obj
$(MDXS_DPM): %.mdx: %.dpm

$(MDXS):
	$(UFOMODEL) $(UFOMODEL_PARAMS) -s 0.6 -f $(<:base/%=%)

clean-mdx:
	@echo "Deleting cached normals and tangents (*.mdx)..."
ifeq ($(USEWINDOWSCMD),1)
	@del /S $(MODELDIR)\*.mdx
else
	@find $(MODELDIR) -name '*.mdx' -delete
endif
	@echo "done"
