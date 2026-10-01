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

# Usa la plataforma ESTABLE mas reciente (las preliminares declaran un
# CodeName en source.properties): una app que apunta a una version preliminar
# no se puede instalar en dispositivos normales.
PLATFORM_DIR=""
for d in $(ls -d "$SDK"/platforms/android-* | sort -V); do
  if ! grep -q "AndroidVersion.CodeName" "$d/source.properties" 2>/dev/null \
     && [[ "${d##*-}" =~ ^[0-9]+$ ]]; then
    PLATFORM_DIR="$d"
  fi
done
[ -n "$PLATFORM_DIR" ] || { echo "No hay una plataforma Android estable instalada." >&2; exit 1; }
BUILD_TOOLS="$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)"
TARGET_API="${PLATFORM_DIR##*-}"
echo "SDK: plataforma $TARGET_API, build-tools $(basename "$BUILD_TOOLS"), NDK $(basename "$NDK")"

# Version: el codigo (versionCode) debe crecer en cada build para que Android
# acepte instalar encima de la version previa. Minutos desde 1970: siempre crece.
VERSION_NAME="$(head -1 "$ROOT/VERSION")"
VERSION_CODE="${ESTEPA_VERSION_CODE:-$(( $(date +%s) / 60 ))}"
echo "Version $VERSION_NAME (versionCode $VERSION_CODE)"

BUILD="$ROOT/build-android"
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$MIN_API" \
  -DCMAKE_BUILD_TYPE=Release -DESTEPA_BUILD_TESTS=OFF \
  -DESTEPA_BUILD_CODE="$VERSION_CODE"
cmake --build "$BUILD"

STAGE="$BUILD/apk"
rm -rf "$STAGE" && mkdir -p "$STAGE/lib/$ABI"
cp "$BUILD/libestepa.so" "$STAGE/lib/$ABI/"
"$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strip --strip-unneeded "$STAGE/lib/$ABI/libestepa.so"

# 1) Manifest + assets.
"$BUILD_TOOLS/aapt2" link -o "$BUILD/estepa.unaligned.apk" \
  --manifest "$ROOT/android/AndroidManifest.xml" \
  -I "$PLATFORM_DIR/android.jar" -A "$ROOT/assets" \
  --min-sdk-version "$MIN_API" --target-sdk-version "$TARGET_API" \
  --version-code "$VERSION_CODE" --version-name "$VERSION_NAME" --replace-version

# 2) Biblioteca nativa, sin comprimir (alineada por zipalign -p).
(cd "$STAGE" && zip -q -0 -r "$BUILD/estepa.unaligned.apk" lib)

# 3) Alinear y firmar.
# Siempre la MISMA clave: Android solo acepta actualizar una app si la version
# nueva viene firmada con la misma clave que la instalada. Por defecto se usa
# la clave de desarrollo del repo (android/estepa-dev.keystore); para releases
# se pasa otra por variables de entorno (p. ej. desde los secrets de GitHub).
"$BUILD_TOOLS/zipalign" -f -p 4 "$BUILD/estepa.unaligned.apk" "$BUILD/estepa.aligned.apk"
KEYSTORE="${ESTEPA_KEYSTORE:-$ROOT/android/estepa-dev.keystore}"
KS_PASS="${ESTEPA_KEYSTORE_PASS:-estepa-dev}"
KEY_ALIAS="${ESTEPA_KEY_ALIAS:-estepa}"
mkdir -p "$(dirname "$OUT_APK")"
"$BUILD_TOOLS/apksigner" sign --ks "$KEYSTORE" --ks-pass "pass:$KS_PASS" \
  --ks-key-alias "$KEY_ALIAS" --out "$OUT_APK" "$BUILD/estepa.aligned.apk"
"$BUILD_TOOLS/apksigner" verify "$OUT_APK"

# 4) Con la clave de desarrollo, comprueba que el certificado es el esperado:
# si alguna vez cambiara, las actualizaciones dejarian de instalarse encima.
if [ -z "${ESTEPA_KEYSTORE:-}" ]; then
  EXPECTED="$(tr -d ' \n' < "$ROOT/android/dev-cert.sha256")"
  ACTUAL="$("$BUILD_TOOLS/apksigner" verify --print-certs "$OUT_APK" \
    | grep -m1 'SHA-256 digest' | sed 's/.*: //' | tr -d ' :' | tr 'A-F' 'a-f')"
  if [ "$EXPECTED" != "$ACTUAL" ]; then
    echo "ERROR: el certificado del APK ($ACTUAL) no es el de desarrollo ($EXPECTED)." >&2
    exit 1
  fi
  echo "Certificado de desarrollo verificado: $ACTUAL"
fi
echo "APK listo: $OUT_APK ($(du -h "$OUT_APK" | cut -f1)) — v$VERSION_NAME, versionCode $VERSION_CODE"
