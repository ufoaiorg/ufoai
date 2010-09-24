linuxinstaller: installer-pre linux_packdata linux_update_installer_data linux_makeself

LINUX_INST_DIR=$(INSTALLER_DIR)/linux
#everything inside this dir will be compressed to the self extracting archive
LINUX_INST_DATADIR=$(LINUX_INST_DIR)/data
#this is only for arranging the path names and tar the zip files
#this step is needed - otherwise loki_setup would automatically extract
#all zip files
LINUX_INST_TMPDIR=$(LINUX_INST_DIR)/tmp

#get the size of the zip data archives
LINUX_INST_SIZE=$(shell LANG=C; du -sch $(LINUX_INST_TMPDIR) $(BINARIES) $(BINARIES_BASE) | tail -1 | cut -f1;)

linux_packdata:
	$(Q)mkdir -p $(LINUX_INST_TMPDIR)/base
	$(Q)echo tar -cvjp -f $(LINUX_INST_DATADIR)/ufo-x86.tar.bz2 $(BINARIES) $(BINARIES_BASE)
	$(Q)tar -cvjp -f $(LINUX_INST_DATADIR)/i18n.tar.bz2 base/i18n/ --exclude updated*
	$(Q)cp base/*.pk3 $(LINUX_INST_TMPDIR)/base
	$(Q)cd $(LINUX_INST_TMPDIR) && tar -cvp -f ../data/data.tar base

linux_update_installer_data:
	$(Q)sed 's/@VERSION@/$(UFOAI_VERSION)/g' $(LINUX_INST_DIR)/setup.xml.in | sed 's/@LINUX_INST_SIZE@/$(LINUX_INST_SIZE)/g' > $(LINUX_INST_DATADIR)/setup.data/setup.xml
	$(Q)sed 's/@VERSION@/$(UFOAI_VERSION)/g' $(LINUX_INST_DIR)/README.in > $(LINUX_INST_DATADIR)/setup.data/README

linux_makeself:
	$(Q)cd $(LINUX_INST_DIR); ./makeself.sh --nocomp $(LINUX_INST_DATADIR)/ ufoai-$(UFOAI_VERSION)-linux32.run "UFO:Alien Invasion $(UFOAI_VERSION) Installer" sh setup.sh
