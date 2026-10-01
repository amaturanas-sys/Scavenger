#!/usr/bin/env bash
# Compila y empaqueta ESTEPA como APK de Android (arm64-v8a), sin Gradle.
#
# Requiere: Android SDK (platforms + build-tools), NDK, CMake, Ninja, Java.
# Variables: ANDROID_SDK_ROOT (o ANDROID_HOME) y ANDROID_NDK_ROOT
# (o ANDROID_NDK_LATEST_HOME, como en los runners de GitHub Actions).
#
# Uso: tools/android/build_apk.sh [salida.apk]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_APK="${1:-$ROOT/build-android/estepa.apk}"
SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
NDK="${ANDROID_NDK_ROOT:-${ANDROID_NDK_LATEST_HOME:-${ANDROID_NDK_HOME:-}}}"
MIN_API=24
ABI=arm64-v8a

[ -d "$SDK" ] || { echo "Falta el Android SDK (ANDROID_SDK_ROOT)." >&2; exit 1; }
[ -d "$NDK" ] || { echo "Falta el Android NDK (ANDROID_NDK_ROOT)." >&2; exit 1; }

# Usa la plataforma y las build-tools mas recientes instaladas.
PLATFORM_DIR="$(ls -d "$SDK"/platforms/android-* | sort -V | tail -1)"
BUILD_TOOLS="$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)"
TARGET_API="${PLATFORM_DIR##*-}"
echo "SDK: plataforma $TARGET_API, build-tools $(basename "$BUILD_TOOLS"), NDK $(basename "$NDK")"

BUILD="$ROOT/build-android"
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$MIN_API" \
  -DCMAKE_BUILD_TYPE=Release -DESTEPA_BUILD_TESTS=OFF
cmake --build "$BUILD"

STAGE="$BUILD/apk"
rm -rf "$STAGE" && mkdir -p "$STAGE/lib/$ABI"
cp "$BUILD/libestepa.so" "$STAGE/lib/$ABI/"
"$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strip --strip-unneeded "$STAGE/lib/$ABI/libestepa.so"

# 1) Manifest + assets.
"$BUILD_TOOLS/aapt2" link -o "$BUILD/estepa.unaligned.apk" \
  --manifest "$ROOT/android/AndroidManifest.xml" \
  -I "$PLATFORM_DIR/android.jar" -A "$ROOT/assets" \
  --min-sdk-version "$MIN_API" --target-sdk-version "$TARGET_API"

# 2) Biblioteca nativa, sin comprimir (alineada por zipalign -p).
(cd "$STAGE" && zip -q -0 -r "$BUILD/estepa.unaligned.apk" lib)

# 3) Alinear y firmar (clave de depuracion si no se provee otra).
"$BUILD_TOOLS/zipalign" -f -p 4 "$BUILD/estepa.unaligned.apk" "$BUILD/estepa.aligned.apk"
KEYSTORE="${ESTEPA_KEYSTORE:-$BUILD/debug.keystore}"
KS_PASS="${ESTEPA_KEYSTORE_PASS:-android}"
KEY_ALIAS="${ESTEPA_KEY_ALIAS:-androiddebugkey}"
if [ ! -f "$KEYSTORE" ]; then
  keytool -genkeypair -keystore "$KEYSTORE" -storepass "$KS_PASS" -keypass "$KS_PASS" \
    -alias "$KEY_ALIAS" -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Estepa Debug,O=Estepa,C=CL" >/dev/null 2>&1
fi
mkdir -p "$(dirname "$OUT_APK")"
"$BUILD_TOOLS/apksigner" sign --ks "$KEYSTORE" --ks-pass "pass:$KS_PASS" \
  --ks-key-alias "$KEY_ALIAS" --out "$OUT_APK" "$BUILD/estepa.aligned.apk"
"$BUILD_TOOLS/apksigner" verify "$OUT_APK"
echo "APK listo: $OUT_APK ($(du -h "$OUT_APK" | cut -f1))"
