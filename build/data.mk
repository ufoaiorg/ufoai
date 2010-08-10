include build/pk3_def.mk

BASE_DIR = base
EMPTY =
SPACE = $(EMPTY) $(EMPTY)
BASE_DIR_REPLACE = $(EMPTY) $(BASE_DIR)

PAK_FILES_OUT = $(addprefix $(BASE_DIR)/,$(PAK_FILES))

pk3: $(PAK_FILES_OUT)

clean-pk3:
	rm $(PAK_FILES_OUT)

ifeq ($(TARGET_OS),mingw32)
FIND = dir \S \B $(1)
ZIP = 7za
ZIP_UP_OPTS = a -tzip -mx=9
ZIP_DEL_OPTS = d -tzip
# bonus points if you can get this to work using 7za
ZIP_LIST =
else
FIND = find $(addprefix $(BASE_DIR)/,$(1)) -type f -print
#ZIP = 7z
#ZIP_UP_OPTS = a -tzip -mx=9
#ZIP_DEL_OPTS = d -tzip
#ZIP_LIST = 7z l $(1) | grep -e :\.*: | tr -s " " | cut -d " " -f 6
ZIP = zip
ZIP_UP_OPTS = -u9
ZIP_DEL_OPTS = -d
ZIP_LIST = zipinfo -1 $(1)
endif

DEBASE = $(subst $(BASE_DIR_REPLACE)/,$(SPACE),$(1))

$(PAK_FILES_OUT) :
	cd base; $(ZIP) $(ZIP_UP_OPTS) $(call DEBASE, $@ $?)
	-$(ZIP) $(ZIP_DEL_OPTS) $@ $(filter-out $(call DEBASE,$^), $(shell $(call ZIP_LIST, $@)))

$(BASE_DIR)/0pics.pk3 : $(filter %.jpg %.tga %.png, $(shell $(call FIND, pics)))

$(BASE_DIR)/0textures.pk3 : $(filter %.jpg %.tga %.png, $(shell $(call FIND, textures)))

$(BASE_DIR)/0models.pk3 : $(filter %.mdx %.md2 %.md3 %.dpm %.obj %.jpg %.png %.tga %.anm %.txt %.tag, $(shell $(call FIND, models)))

$(BASE_DIR)/0snd.pk3 : $(filter %.txt %.ogg %.wav, $(shell $(call FIND, sound)))

$(BASE_DIR)/0music.pk3 : $(filter %.ogg %.txt, $(shell $(call FIND, music)))

$(BASE_DIR)/0maps.pk3 : $(filter %.bsp %.ump, $(shell $(call FIND, maps)))

$(BASE_DIR)/0videos.pk3 : $(filter %.roq %.ogm, $(shell $(call FIND, videos)))

$(BASE_DIR)/0media.pk3 : $(filter %.ttf, $(shell $(call FIND, media)))

$(BASE_DIR)/0shaders.pk3 : $(filter %.glsl, $(shell $(call FIND, shaders)))

$(BASE_DIR)/0ufos.pk3 : $(filter %.ufo, $(shell $(call FIND, ufos)))

$(BASE_DIR)/0materials.pk3 : $(filter %.mat, $(shell $(call FIND, materials)))

$(BASE_DIR)/0base.pk3 : $(wildcard base/*.cfg) base/mapcycle.txt base/irc_motd.txt $(wildcard base/ai/*.lua)
