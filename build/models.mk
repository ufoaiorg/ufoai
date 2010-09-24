MODELDIR ?= base/models

UFOMODEL = ./ufomodel
UFOMODEL_PARAMS = -mdx -overwrite -v

MODELS_MD2 := $(shell find $(MODELDIR) -name "*.md2")
MODELS_MD3 := $(shell find $(MODELDIR) -name "*.md3")
MODELS_OBJ := $(shell find $(MODELDIR) -name "*.obj")
MODELS_DPM := $(shell find $(MODELDIR) -name "*.dpm")

# smoothing normals and tangents
define get-smooth-value
    $(if $(filter $(dir $(1)), models/aircraft),  0.7, \
    $(if $(filter $(dir $(1)), models/aliens),    0.3, \
    $(if $(filter $(dir $(1)), models/animals),   0.0, \
    $(if $(filter $(dir $(1)), models/civilians), -0.5, \
    $(if $(filter $(dir $(1)), models/objects),   0.2, \
    $(if $(filter $(dir $(1)), models/soldiers),  -0.3, \
    $(if $(filter $(dir $(1)), models/weapons),   0.6, \
    0.5)))))))
endef

MDXS_MD2 := $(MODELS_MD2:.md2=.mdx)
MDXS_MD3 := $(MODELS_MD3:.md3=.mdx)
MDXS_OBJ := $(MODELS_OBJ:.obj=.mdx)
MDXS_DPM := $(MODELS_DPM:.dpm=.mdx)
# TODO see https://sourceforge.net/tracker/?func=detail&aid=2993773&group_id=157793&atid=805242
#MDXS     := $(MDXS_MD2) $(MDXS_MD3) $(MDXS_OBJ) $(MDXS_DPM)
MDXS     := $(MDXS_MD2) $(MDXS_MD3) $(MDXS_DPM)

models: ufomodel $(MDXS)

$(MDXS_MD2): %.mdx: %.md2
$(MDXS_MD3): %.mdx: %.md3
$(MDXS_OBJ): %.mdx: %.obj
$(MDXS_DPM): %.mdx: %.dpm

$(MDXS):
	$(UFOMODEL) $(UFOMODEL_PARAMS) -s $(strip $(call get-smooth-value,$<)) -f $(<:base/%=%)

clean-mdx:
	@echo "Deleting cached normals and tangents (*.mdx)..."
	$(Q)find $(MODELDIR) -name '*.mdx' -delete
