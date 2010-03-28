wininstaller: lang maps-sync pk3
	makensis contrib/installer/windows/ufoai.nsi
	makensis contrib/installer/windows/uforadiant.nsi
	md5sum contrib/installer/windows/ufoai-$(UFOAI_VERSION)-win32.exe > contrib/installer/windows/ufoai-$(UFOAI_VERSION)-win32.md5
	md5sum contrib/installer/windows/uforadiant-$(UFORADIANT_VERSION)-win32.exe > contrib/installer/windows/uforadiant-$(UFORADIANT_VERSION)-win32.md5
