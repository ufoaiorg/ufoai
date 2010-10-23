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
$(shell ([ -x "$$(which 7z 2> /dev/null)" ] && echo "7z u -tzip -mx=9") || ([ -x "$$(which 7za 2> /dev/null)" ] && echo "7za u -tzip -mx=9") || ([ -x "$$(which zip 2> /dev/null)" ] && echo "zip -u9"))
endef

%.pk3:
ifeq (7z,$(findstring 7z,$(call ZIP)))
	$(Q)cd $(BASE_DIR); $(call ZIP) $(filter -r,$(call $@)) $(notdir $@) $(filter-out -r,$(call $@))
else
	$(Q)cd $(BASE_DIR); $(call ZIP) $(filter -r,$(call $@)) $(notdir $@) . -i $(subst *,\*,$(filter-out -r,$(call $@)))
endif

define $(BASE_DIR)/0pics.pk3
	-r pics/*.jpg pics/*.tga pics/*.png
endef

define $(BASE_DIR)/0textures.pk3
	-r textures/*.jpg textures/*.tga textures/*.png
endef

define $(BASE_DIR)/0models.pk3
	-r models/*.mdx models/*.md2 models/*.md3 models/*.dpm models/*.obj models/*.jpg models/*.png models/*.tga models/*.anm models/*.tag
endef

define $(BASE_DIR)/0models.pk3
	-r models/*.mdx models/*.md2 models/*.md3 models/*.dpm models/*.obj models/*.jpg models/*.png models/*.tga models/*.anm models/*.tag
endef

define $(BASE_DIR)/0snd.pk3
	-r sound/*.ogg sound/*.wav
endef

define $(BASE_DIR)/0music.pk3
	-r music/*.ogg
endef

define $(BASE_DIR)/0maps.pk3
	-r maps/*.bsp maps/*.ump
endef

define $(BASE_DIR)/0videos.pk3
	-r videos/*.roq videos/*.ogm
endef

define $(BASE_DIR)/0media.pk3
	-r media/*.ttf
endef

define $(BASE_DIR)/0shaders.pk3
	-r shaders/*.glsl
endef

define $(BASE_DIR)/0ufos.pk3
	-r ufos/*.ufo
endef

define $(BASE_DIR)/0materials.pk3
	-r materials/*.mat
endef

define $(BASE_DIR)/0base.pk3
	-r *.cfg mapcycle.txt irc_motd.txt ai/*.lua
endef
