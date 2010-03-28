macinstaller: lang maps-sync pk3
	cd src/ports/macosx/installer; $(MAKE) create-dmg-ufoai TARGET_CPU=$(TARGET_CPU) UFOAI_VERSION=$(UFOAI_VERSION)
	cd src/ports/macosx/installer; $(MAKE) create-dmg-uforadiant TARGET_CPU=$(TARGET_CPU) UFORADIANT_VERSION=$(UFORADIANT_VERSION)
