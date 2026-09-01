#!/usr/bin/env bash
set -e

echo "=== [1/4] Verificando NDK e SDK Android ==="
if [ -z "$ANDROID_NDK_HOME" ] && [ -z "$ANDROID_NDK" ]; then
    if [ -d "$HOME/Android/Sdk/ndk" ]; then
        LATEST_NDK=$(ls -d $HOME/Android/Sdk/ndk/* 2>/dev/null | sort -V | tail -n1)
        export ANDROID_NDK_HOME="$LATEST_NDK"
    fi
fi

if [ -n "$ANDROID_NDK" ] && [ -z "$ANDROID_NDK_HOME" ]; then
    export ANDROID_NDK_HOME="$ANDROID_NDK"
fi

if [ -z "$ANDROID_NDK_HOME" ] || [ ! -d "$ANDROID_NDK_HOME" ]; then
    echo "ERRO: ANDROID_NDK_HOME não configurado!"
    echo "Defina com: export ANDROID_NDK_HOME=/caminho/para/seu/ndk"
    exit 1
fi

echo "Usando NDK: $ANDROID_NDK_HOME"

echo "=== [2/4] Compilando Native C++ com CMake e Ninja (arm64-v8a) ==="
mkdir -p build-android
cmake -B build-android -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=latest \
    -DANDROID_STL=c++_static \
    -DENABLE_VCPKG=ON \
    -DVCPKG_TARGET_TRIPLET=arm64-android \
    -DBUILD_MK64=ON \
    -DBUILD_NAUDIO=ON \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build-android --target libSpaghettify.so --parallel $(nproc)

echo "=== [3/4] Copiando libSpaghettify.so para o projeto Android ==="
mkdir -p android/app/libs/arm64-v8a
cp build-android/libSpaghettify.so android/app/libs/arm64-v8a/libSpaghettify.so
ls -lh android/app/libs/arm64-v8a/libSpaghettify.so

echo "=== [4/4] Gerando APK com Gradle ==="
cd android
chmod +x ./gradlew
./gradlew assembleDebug

echo "=== COMPILAÇÃO CONCLUÍDA COM SUCESSO! ==="
echo "APK gerado em:"
ls -lh app/build/outputs/apk/debug/*.apk
