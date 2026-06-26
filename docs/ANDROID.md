# SCAVENGER para Android (APK)

La app Android es un **cliente (WebView)** que empaqueta la interfaz de
SCAVENGER y se conecta al **backend** que corres en tu PC/servidor. El motor de
optimización (Python + solver CBC) no puede ejecutarse dentro del teléfono, por
eso el cálculo vive en el backend y la app es el cliente.

```
┌────────────┐        Wi-Fi / red         ┌──────────────────────┐
│  APK        │  ── HTTP /api/... ──▶       │  Backend SCAVENGER    │
│ (WebView)   │  ◀── JSON ──                │  (uvicorn :8000)      │
└────────────┘                             └──────────────────────┘
```

## 1. Descargar la APK

La APK se compila automáticamente en GitHub Actions y se publica como
**Release** descargable:

- **Releases → `android-latest` → `scavenger-debug.apk`**
  (`https://github.com/amaturanas-sys/Scavenger/releases/tag/android-latest`)

También puedes lanzarla manualmente: pestaña **Actions → Android APK → Run
workflow**, y descargar el artefacto `scavenger-debug-apk`.

> Es un APK **debug** sin firmar para pruebas. Android pedirá permitir
> "instalar apps de orígenes desconocidos".

## 2. Levantar el backend (en tu PC/servidor)

```bash
# en el repo, en tu computador
pip install -r requirements.txt
make run            # uvicorn en 0.0.0.0:8000  (accesible en la red local)
# o con Docker:
docker run --rm -p 8000:8000 scavenger
```

El servidor escucha en `0.0.0.0:8000`, así que es accesible desde otros
dispositivos de la misma red. Averigua la **IP local** del PC:

- Linux/macOS: `ip addr` o `ifconfig` (busca algo como `192.168.x.x`)
- Windows: `ipconfig`

Asegúrate de que el **firewall** permita conexiones entrantes al puerto 8000 y
que el teléfono esté en la **misma red Wi-Fi**.

## 3. Conectar la app

1. Abre la app SCAVENGER en el teléfono.
2. La primera vez te pedirá la **URL del servidor**. Ingresa
   `http://IP-DE-TU-PC:8000` (ej: `http://192.168.1.10:8000`).
3. Listo: la app carga el catálogo y el usuario demo. Puedes cambiar la URL
   cuando quieras con el botón **⚙️** del encabezado.

## Compilar la APK localmente (opcional)

Necesitas Android SDK (platform 34, build-tools 34). El proyecto está en
`android/` con su Gradle wrapper:

```bash
# copia la interfaz a los assets de la app
mkdir -p android/app/src/main/assets/www && cp -r frontend/* android/app/src/main/assets/www/
cd android && ./gradlew assembleDebug
# APK en: android/app/build/outputs/apk/debug/app-debug.apk
```

## APK de release firmado (keystore)

El build de CI genera además un **APK de release firmado** (`scavenger-release.apk`)
cuando existen los secrets del keystore. Pasos (una sola vez):

### 1. Crear el keystore

Con `keytool` (viene con el JDK / Android Studio):

```bash
keytool -genkeypair -v -keystore scavenger-release.jks \
  -alias scavenger -keyalg RSA -keysize 2048 -validity 10000
```

Te pedirá una **contraseña de keystore**, datos (nombre/organización) y una
**contraseña de clave** (puede ser la misma). **Guarda este archivo y las
contraseñas en un lugar seguro**: se necesita el mismo keystore para publicar
actualizaciones de la app.

### 2. Codificar el keystore en base64

```bash
# Linux
base64 -w0 scavenger-release.jks
# macOS
base64 -i scavenger-release.jks
# Windows (PowerShell)
[Convert]::ToBase64String([IO.File]::ReadAllBytes("scavenger-release.jks"))
```

### 3. Agregar los secrets en GitHub

Repo → **Settings → Secrets and variables → Actions → Secrets**:

| Secret | Valor |
|--------|-------|
| `ANDROID_KEYSTORE_BASE64` | el base64 del paso 2 |
| `ANDROID_KEYSTORE_PASSWORD` | la contraseña del keystore |
| `ANDROID_KEY_ALIAS` | `scavenger` |
| `ANDROID_KEY_PASSWORD` | la contraseña de la clave |

### 4. Construir

Lanza **Actions → Android APK → Run workflow** (o haz un push que toque
`android/`/`frontend/`). El workflow detecta los secrets y publica
`scavenger-release.apk` en el Release `android-latest`. Sin esos secrets, solo
se genera el `scavenger-debug.apk` (el flujo no falla).

> El keystore **nunca** se commitea al repo: vive solo como secret y como tu
> archivo local de respaldo.

## Notas

- **HTTP en claro**: la app permite `usesCleartextTraffic` para poder hablar
  con un backend local por `http://`. Para producción conviene HTTPS.
- **CORS**: el backend ya responde con `Access-Control-Allow-Origin: *`, así que
  el WebView (origen `file://`) puede consumir la API.
- **Datos**: la app no guarda datos propios; todo vive en el backend que
  configures (su base SQLite). Cambiar de servidor cambia los datos.
- **Autónoma (a futuro)**: una app 100% offline exigiría portar el optimizador
  (hoy en Python + CBC) a una implementación on-device; queda como evolución.
