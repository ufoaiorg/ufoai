WINDOWS_INST_DIR=contrib/installer/windows

wininstaller: wininstaller-ufoai wininstaller-uforadiant

wininstaller-ufoai: ufo ufoded uforadiant strip-uforadiant installer-pre
	$(Q)$(CROSS)makensis -DPRODUCT_VERSION=$(UFOAI_VERSION)  $(WINDOWS_INST_DIR)/ufoai.nsi
	$(Q)md5sum $(WINDOWS_INST_DIR)/ufoai-$(UFOAI_VERSION)-win32.exe > $(WINDOWS_INST_DIR)/ufoai-$(UFOAI_VERSION)-win32.md5

wininstaller-uforadiant: uforadiant lang strip-uforadiant
	$(Q)$(CROSS)makensis -DPRODUCT_VERSION=$(UFORADIANT_VERSION) $(WINDOWS_INST_DIR)/uforadiant.nsi
	$(Q)md5sum $(WINDOWS_INST_DIR)/uforadiant-$(UFORADIANT_VERSION)-win32.exe > $(WINDOWS_INST_DIR)/uforadiant-$(UFORADIANT_VERSION)-win32.md5
