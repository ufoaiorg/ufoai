WINDOWS_INST_DIR=contrib/installer/windows

wininstaller: installer-pre
	makensis $(WINDOWS_INST_DIR)/ufoai.nsi
	makensis $(WINDOWS_INST_DIR)/uforadiant.nsi
	md5sum $(WINDOWS_INST_DIR)/ufoai-$(UFOAI_VERSION)-win32.exe > $(WINDOWS_INST_DIR)/ufoai-$(UFOAI_VERSION)-win32.md5
	md5sum $(WINDOWS_INST_DIR)/uforadiant-$(UFORADIANT_VERSION)-win32.exe > $(WINDOWS_INST_DIR)/uforadiant-$(UFORADIANT_VERSION)-win32.md5
