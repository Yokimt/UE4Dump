@echo off
call ndk-build



adb devices
adb push libs/arm64-v8a/mono_dump /data/local/tmp
adb shell su -c "chmod 777 /data/local/tmp/mono_dump"

adb shell su -c "cp /data/local/tmp/mono_dump /data/adb/mono_dump"


adb shell su -c "rm /data/local/tmp/mono_dump"