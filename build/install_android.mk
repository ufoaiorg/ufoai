ANDROID_INST_DIR=contrib/installer/android
APK_MAIN = ufoai.apk

AAPT = $(ANDROID_SDK)/platform-tools/aapt
ADB = $(ANDROID_SDK)/platform-tools/adb
DX = $(ANDROID_SDK)/platform-tools/dx
APKBUILDER = $(ANDROID_SDK)/tools/apkbuilder
JAVAC ?= javac
JAVACFLAGS = -source 1.5 -target 1.5

# This is a bit silly. I want to compile against the 1.6 android.jar,
# to make the compiler check that I don't use something that requires
# a newer Android. However, in order to use android:installLocation,
# we need to give aapt a version >=8 android.jar - even though the
# result will work ok on 1.5+.
ANDROID_JAR = $(ANDROID_SDK)/platforms/android-4/android.jar
ANDROID_JAR8 = $(ANDROID_SDK)/platforms/android-8/android.jar

androidinstaller: installer-pre androidinstaller-ufoai

androidinstaller-ufoai: $(APK_MAIN) installer-pre
