mkdir build
cd build

cmake \
  -DCMAKE_TOOLCHAIN_FILE=$NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_NATIVE_API_LEVEL=31 \
  -DCMAKE_MAKE_PROGRAM=make \
  -DANDROID_STL=c++_static \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
  ..

cmake --build .

$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip -s libmono.so

# adb push libmono.so /sdcard