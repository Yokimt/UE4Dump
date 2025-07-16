APP_ABI := arm64-v8a
APP_PLATFORM := android-22
APP_STL := c++_static
APP_OPTIM := release
APP_STRIP_MODE := --strip-debug

# APP_CPPFLAGS :=-mllvm -hikari  -mllvm -enable-strcry -mllvm -strcry_prob=90 -mllvm -enable-constenc -mllvm -constenc_times=3  -mllvm -constenc_togv -mllvm -constenc_togv_prob=3  -mllvm -enable-func-variable-rename -mllvm -enable-bcfobf -mllvm -enable-cffobf