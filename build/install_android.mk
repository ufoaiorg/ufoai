ANDROID_INST_DIR=contrib/installer/android
APK = ufoai.apk
APK_TEMPLATE = ufoai-template.apk
KEYSTORE = ufoai-team
PASSWORD = AliensWillEatYou

STRIP = $(dir $(shell which ndk-build))/build/prebuilt/linux-x86/arm-eabi-4.4.0/bin/arm-eabi-strip --strip-debug
AAPT = $(ANDROID_SDK)/platform-tools/aapt
ADB = $(ANDROID_SDK)/platform-tools/adb
DX = $(ANDROID_SDK)/platform-tools/dx
APKBUILDER = $(ANDROID_SDK)/tools/apkbuilder
ZIPALIGN = $(ANDROID_SDK)/tools/zipalign
JAVAC ?= javac
JAVACFLAGS = -source 1.5 -target 1.5

# This is a bit silly. I want to compile against the 1.6 android.jar,
# to make the compiler check that I don't use something that requires
# a newer Android. However, in order to use android:installLocation,
# we need to give aapt a version >=8 android.jar - even though the
# result will work ok on 1.5+.
ANDROID_JAR = $(ANDROID_SDK)/platforms/android-4/android.jar
ANDROID_JAR8 = $(ANDROID_SDK)/platforms/android-8/android.jar

androidinstaller: $(APK)

$(APK): $(ANDROID_INST_DIR)/$(APK_TEMPLATE) ufo
	$(STRIP) ufo
	cp -f $< $@
	rm -f lib/armeabi/libapplication.so
	mkdir -p lib/armeabi
	mv -f ufo lib/armeabi/libapplication.so
	cp -f $(ANDROID_INST_DIR)/*.so lib/armeabi/
	zip $@ lib/armeabi/*.so
	rm -rf lib
	jarsigner -verbose -keystore $(ANDROID_INST_DIR)/$(KEYSTORE).keystore -storepass $(PASSWORD) $@ $(KEYSTORE)
	$(ZIPALIGN) 4 $@ $@-1
	mv -f $@-1 $@
