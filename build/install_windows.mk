WINDOWS_INST_DIR=contrib/installer/windows

wininstaller: installer-pre wininstaller-ufoai wininstaller-uforadiant

wininstaller-ufoai: installer-pre
	$(Q)$(CROSS)makensis -DPRODUCT_VERSION=$(UFOAI_VERSION)  $(WINDOWS_INST_DIR)/ufoai.nsi
	$(Q)md5sum $(WINDOWS_INST_DIR)/ufoai-$(UFOAI_VERSION)-win32.exe > $(WINDOWS_INST_DIR)/ufoai-$(UFOAI_VERSION)-win32.md5

wininstaller-uforadiant: installer-pre
	$(Q)$(CROSS)makensis -DPRODUCT_VERSION=$(UFORADIANT_VERSION) $(WINDOWS_INST_DIR)/uforadiant.nsi
	$(Q)md5sum $(WINDOWS_INST_DIR)/uforadiant-$(UFORADIANT_VERSION)-win32.exe > $(WINDOWS_INST_DIR)/uforadiant-$(UFORADIANT_VERSION)-win32.md5
