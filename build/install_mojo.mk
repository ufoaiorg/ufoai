mojoinstaller: installer-pre mojo_packdata mojo_update_installer_data mojo_compile_lua mojo_makeself

MOJOSETUP_INST_DIR=$(INSTALLER_DIR)/mojosetup
MOJOSETUP_INST_TMP=$(MOJOSETUP_INST_DIR)/image
MOJOSETUP_INST_DATADIR=$(MOJOSETUP_INST_TMP)/data
MOJOSETUP_INSTALLER=$(MOJOSETUP_INST_DIR)/ufoai-$(UFOAI_VERSION)-linux32.run
TOTAL_INSTALL_SIZE=$(shell LANG=C; du -sb $(MOJOSETUP_INST_DATADIR) | tail -1 | cut -f1)
LUA_SRCS=$(shell cd $(MOJOSETUP_INST_DIR)/scripts; ls *.lua)
LUA_OBJS=$(patsubst %.lua, $(MOJOSETUP_INST_TMP)/scripts/%.luac, $(filter %.lua, $(LUA_SRCS)))

#strip debug symbols
LUAC_OPTS=-s

mojo_packdata:
	@mkdir -p $(MOJOSETUP_INST_TMP)
	@mkdir -p $(MOJOSETUP_INST_TMP)/guis
	@mkdir -p $(MOJOSETUP_INST_TMP)/meta
	@mkdir -p $(MOJOSETUP_INST_TMP)/scripts
	@mkdir -p $(MOJOSETUP_INST_DATADIR)
	@mkdir -p $(MOJOSETUP_INST_DATADIR)/base
	@mkdir -p $(MOJOSETUP_INST_DATADIR)/base/i18n
	@cp -f $(MOJOSETUP_INST_DIR)/guis/*.so $(MOJOSETUP_INST_TMP)/guis
	@cp -f $(INSTALLER_DIR)/ufoai.bmp $(MOJOSETUP_INST_TMP)/meta
	@cp base/*.pk3 $(MOJOSETUP_INST_DATADIR)/base
	@cp COPYING $(MOJOSETUP_INST_DATADIR)
	@cp README $(MOJOSETUP_INST_DATADIR)
	@cp -f $(INSTALLER_DIR)/ufoai.xpm $(MOJOSETUP_INST_DATADIR)
	@cp -f $(BINARIES) $(MOJOSETUP_INST_DATADIR)
	@cp -f $(BINARIES_BASE) $(MOJOSETUP_INST_DATADIR)/base
	@tar -cvpf $(MOJOSETUP_INST_TMP)/i18n.tar base/i18n/ --exclude .svn --exclude updated*
	@tar -xf $(MOJOSETUP_INST_TMP)/i18n.tar -C $(MOJOSETUP_INST_DATADIR)

# Make a .zip archive of the Base Archive dirs and nuke the originals...
# Append the .zip archive to the mojosetup binary, so it's "self-extracting."
mojo_makeself:
	@cd $(MOJOSETUP_INST_TMP); zip -9r ../pdata.zip *; rm -rf $(MOJOSETUP_INST_TMP)
	@cat $(MOJOSETUP_INST_DIR)/mojosetup $(MOJOSETUP_INST_DIR)/pdata.zip >> $(MOJOSETUP_INSTALLER)
	@chmod +x $(MOJOSETUP_INSTALLER)
	@rm -f $(MOJOSETUP_INST_DIR)/pdata.zip
	@rm -f $(MOJOSETUP_INST_DIR)/scripts/config.lua

mojo_update_installer_data:
	@sed 's/@VERSION@/$(UFOAI_VERSION)/g' $(MOJOSETUP_INST_DIR)/scripts/config.lua.in | sed 's/@TOTAL_INSTALL_SIZE@/$(TOTAL_INSTALL_SIZE)/g' > $(MOJOSETUP_INST_DIR)/scripts/config.lua

mojo_compile_lua: $(LUA_OBJS)

$(MOJOSETUP_INST_TMP)/scripts/%.luac: $(MOJOSETUP_INST_DIR)/scripts/%.lua
	@echo " * [LUA] $<"; \
		$(MOJOSETUP_INST_DIR)/mojoluac $(LUAC_OPTS) -o $@ $<
