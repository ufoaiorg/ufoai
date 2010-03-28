WINDOWS_INST_DIR=contrib/installer/windows

wininstaller: installer-pre wininstaller-ufoai wininstaller-uforadiant

wininstaller-ufoai: installer-pre
	makensis $(WINDOWS_INST_DIR)/ufoai.nsi
	md5sum $(WINDOWS_INST_DIR)/ufoai-$(UFOAI_VERSION)-win32.exe > $(WINDOWS_INST_DIR)/ufoai-$(UFOAI_VERSION)-win32.md5

wininstaller-uforadiant: installer-pre
	makensis $(WINDOWS_INST_DIR)/uforadiant.nsi
	md5sum $(WINDOWS_INST_DIR)/uforadiant-$(UFORADIANT_VERSION)-win32.exe > $(WINDOWS_INST_DIR)/uforadiant-$(UFORADIANT_VERSION)-win32.md5
