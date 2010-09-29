include build/pk3_def.mk

BASE_DIR = base
PAK_FILES_OUT = $(addprefix $(BASE_DIR)/,$(PAK_FILES))

pk3: $(PAK_FILES_OUT)

clean-pk3:
	$(Q)rm -f $(PAK_FILES_OUT)

define FIND
$(shell find $(BASE_DIR)/$(1) -type f -print)
endef

define ZIP
$(shell ([ -x "$$(which 7z 2> /dev/null)" ] && echo "7z a -tzip -mx=9") || ([ -x "$$(which zip 2> /dev/null)" ] && echo "zip -u9"))
endef

%.pk3 :
	$(Q)cd $(BASE_DIR); $(call ZIP) $(patsubst $(BASE_DIR)/%,%,$@) $(patsubst $(BASE_DIR)/%,%,$?)

$(BASE_DIR)/0pics.pk3 : $(filter %.jpg %.tga %.png, $(call FIND,pics))

$(BASE_DIR)/0textures.pk3 : $(filter %.jpg %.tga %.png, $(call FIND,textures))

$(BASE_DIR)/0models.pk3 : $(filter %.mdx %.md2 %.md3 %.dpm %.obj %.jpg %.png %.tga %.anm %.tag, $(call FIND,models))

$(BASE_DIR)/0snd.pk3 : $(filter %.ogg %.wav, $(call FIND,sound))

$(BASE_DIR)/0music.pk3 : $(wildcard $(BASE_DIR)/music/*.ogg)

$(BASE_DIR)/0maps.pk3 : $(filter %.bsp %.ump, $(call FIND,maps))

$(BASE_DIR)/0videos.pk3 : $(filter %.roq %.ogm, $(call FIND,videos))

$(BASE_DIR)/0media.pk3 : $(wildcard $(BASE_DIR)/media/*.ttf)

$(BASE_DIR)/0shaders.pk3 : $(wildcard $(BASE_DIR)/shaders/*.glsl)

$(BASE_DIR)/0ufos.pk3 : $(filter %.ufo, $(call FIND,ufos))

$(BASE_DIR)/0materials.pk3 : $(wildcard $(BASE_DIR)/materials/*.mat)

$(BASE_DIR)/0base.pk3 : $(wildcard $(BASE_DIR)/*.cfg) $(BASE_DIR)/mapcycle.txt $(BASE_DIR)/irc_motd.txt $(wildcard $(BASE_DIR)/ai/*.lua)
