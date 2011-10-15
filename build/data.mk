include build/pk3_def.mk

BASE_DIR = base
PAK_FILES_OUT = $(addprefix $(BASE_DIR)/,$(PAK_FILES))

pk3: $(PAK_FILES_OUT)

clean-pk3:
	$(Q)rm -f $(PAK_FILES_OUT)

define FIND
$(shell find $(BASE_DIR)/$(1) -type f -print)
endef

define ZIP #dont use 7zip it will ignore recursion on a recursive search for files on linux and Mingw
$(shell ([ -x "$$(which zip 2> /dev/null)" ] && echo "zip -u9"))
endef

%.pk3 : # zip return code 12: zip has nothing to do, bad for a "make pk3 -B"
	$(Q)cd $(BASE_DIR) ; $(call ZIP) $(filter -r,$(call $@)) $(notdir $@) . $(strip $(if $(findstring -r,$(firstword $(call $@))),$(subst *,\*,$(subst -r,-i,$(call $@))),$(call $@))) || [ $$? -eq 12 ] && exit 0

define $(BASE_DIR)/0pics.pk3
	-r pics/*.jpg pics/*.tga pics/*.png
endef

define $(BASE_DIR)/0textures.pk3
	-r textures/*.jpg textures/*.tga textures/*.png
endef

define $(BASE_DIR)/0models.pk3
	-r models/*.mdx models/*.md2 models/*.md3 models/*.dpm models/*.obj models/*.jpg models/*.png models/*.tga models/*.anm models/*.tag
endef

define $(BASE_DIR)/0snd.pk3
	-r sound/*.ogg sound/*.wav
endef

define $(BASE_DIR)/0music.pk3
	music/*.ogg
endef

define $(BASE_DIR)/0maps.pk3
	-r maps/*.bsp maps/*.ump
endef

define $(BASE_DIR)/0videos.pk3
	-r videos/*.roq videos/*.ogm
endef

define $(BASE_DIR)/0media.pk3
	media/*.ttf
endef

define $(BASE_DIR)/0shaders.pk3
	shaders/*.glsl
endef

define $(BASE_DIR)/0ufos.pk3
	-r ufos/*.ufo
endef

define $(BASE_DIR)/0materials.pk3
	materials/*.mat
endef

define $(BASE_DIR)/0base.pk3
	*.cfg mapcycle*.txt irc_motd.txt ai/*.lua
endef
