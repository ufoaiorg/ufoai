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
	$(Q)mkdir -p $(MOJOSETUP_INST_TMP)
	$(Q)mkdir -p $(MOJOSETUP_INST_TMP)/guis
	$(Q)mkdir -p $(MOJOSETUP_INST_TMP)/meta
	$(Q)mkdir -p $(MOJOSETUP_INST_TMP)/scripts
	$(Q)mkdir -p $(MOJOSETUP_INST_DATADIR)
	$(Q)mkdir -p $(MOJOSETUP_INST_DATADIR)/base
	$(Q)mkdir -p $(MOJOSETUP_INST_DATADIR)/base/i18n
	$(Q)cp -f $(MOJOSETUP_INST_DIR)/guis/*.so $(MOJOSETUP_INST_TMP)/guis
	$(Q)cp -f $(INSTALLER_DIR)/ufoai.bmp $(MOJOSETUP_INST_TMP)/meta
	$(Q)cp base/*.pk3 $(MOJOSETUP_INST_DATADIR)/base
	$(Q)cp COPYING $(MOJOSETUP_INST_DATADIR)
	$(Q)cp README $(MOJOSETUP_INST_DATADIR)
	$(Q)cp -f $(INSTALLER_DIR)/ufoai.xpm $(MOJOSETUP_INST_DATADIR)
	$(Q)cp -f $(BINARIES) $(MOJOSETUP_INST_DATADIR)
	$(Q)cp -f $(BINARIES_BASE) $(MOJOSETUP_INST_DATADIR)/base
	$(Q)tar -cvpf $(MOJOSETUP_INST_TMP)/i18n.tar base/i18n/ --exclude updated*
	$(Q)tar -xf $(MOJOSETUP_INST_TMP)/i18n.tar -C $(MOJOSETUP_INST_DATADIR)

# Make a .zip archive of the Base Archive dirs and nuke the originals...
# Append the .zip archive to the mojosetup binary, so it's "self-extracting."
mojo_makeself:
	$(Q)cd $(MOJOSETUP_INST_TMP); zip -9r ../pdata.zip *; rm -rf $(MOJOSETUP_INST_TMP)
	$(Q)cat $(MOJOSETUP_INST_DIR)/mojosetup $(MOJOSETUP_INST_DIR)/pdata.zip >> $(MOJOSETUP_INSTALLER)
	$(Q)chmod +x $(MOJOSETUP_INSTALLER)
	$(Q)rm -f $(MOJOSETUP_INST_DIR)/pdata.zip
	$(Q)rm -f $(MOJOSETUP_INST_DIR)/scripts/config.lua

mojo_update_installer_data:
	$(Q)sed 's/@VERSION@/$(UFOAI_VERSION)/g' $(MOJOSETUP_INST_DIR)/scripts/config.lua.in | sed 's/@TOTAL_INSTALL_SIZE@/$(TOTAL_INSTALL_SIZE)/g' > $(MOJOSETUP_INST_DIR)/scripts/config.lua

mojo_compile_lua: $(LUA_OBJS)

$(MOJOSETUP_INST_TMP)/scripts/%.luac: $(MOJOSETUP_INST_DIR)/scripts/%.lua
	@echo " * [LUA] $<";
	$(Q)$(MOJOSETUP_INST_DIR)/mojoluac $(LUAC_OPTS) -o $@ $<
