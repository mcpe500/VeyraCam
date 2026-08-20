# VeyraCam — High-Performance Zero-Copy Virtual Webcam

> **Your phone. Your camera. Anywhere.**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/mcpe500/VeyraCam?label=release)](https://github.com/mcpe500/VeyraCam/releases)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-brightgreen.svg)](https://isocpp.org/)
[![Flutter](https://img.shields.io/badge/Flutter-3.47%20(stable)-02569B.svg?logo=flutter)](https://flutter.dev)
[![Android](https://img.shields.io/badge/Android-7.0%2B%20(API%2024%2B)-3DDC84.svg?logo=android)](https://developer.android.com/)
[![Windows](https://img.shields.io/badge/Windows-10%20%2F%2011%20(64--bit)-0078D6.svg?logo=windows)](https://microsoft.com/windows)
[![Security](https://img.shields.io/badge/Security-mbedTLS%203.6.2%20LTS-informational.svg)](#-security-hardening-v101)

Veyra turns your Android phone into a low-latency, **end-to-end encrypted** PC virtual webcam for **Zoom, Teams, OBS Studio, Google Meet, and Discord**.

Works over **USB (ADB/WinUSB)**, **Wi-Fi LAN**, **Wi-Fi Direct (P2P)**, and **Bluetooth Classic (RFCOMM)** with automatic link-quality scoring and sub-500 ms handoff. Media is encrypted per-packet (X25519 + ChaCha20-Poly1305 via [mbedTLS 3.6.2 LTS](https://github.com/Mbed-TLS/mbedtls)) and gated by PIN pairing.

---

## Table of Contents

- [Core Architecture](#-core-architecture--invariants)
- [Features](#-features)
- [Security Hardening (v1.0.1)](#-security-hardening-v101)
- [Repository Structure](#-repository-structure)
- [Prerequisites](#-prerequisites)
- [Quick Start (from Releases)](#-quick-start-from-releases)
- [Build & Release](#-how-to-build--release)
- [Pairing & Connect](#-pairing--connect)
- [Remote 3A Controls](#-remote-3a-camera-controls)
- [Verification & Tests](#-verification--test-suites)
- [CI/CD](#-cicd)
- [Troubleshooting](#-troubleshooting)
- [License](#-license)

---

## 🌟 Core Architecture & Invariants

```
                    VEYRACAM (ANDROID)
             [Android 7.0+ (API 24+) / ARM]
                          │
                   Camera2 Surface
                          ▼
            MediaCodec Hardware H.264 (CBR)
                          ▼
              Native Buffer (Zero-Copy)
                          ▼
         Veyra Link Protocol Packetizer
           + AEAD Encrypt (ChaCha20-Poly1305)  ← SessionCrypto (mbedTLS) [native/core/src/crypto.cpp:1]
           + Replay Filter (window 64)          ← Packetizer [native/core/src/packetizer.cpp:1]
                          │
       ┌──────────────────┼──────────────────┐
       ▼                  ▼                  ▼
   USB Tunnel         Wi-Fi UDP          RFCOMM BT
  (5150/5151)        (5150/5151)        (a888c728-6623-4217-9160-b6f2048995a9)
       │                  │                  │
       └──────────────────┼──────────────────┘
                          │  Pairing gate: PIN → token → peer filter
                          ▼
               VEYRA CORE C++ (WINDOWS)       ← [native/windows/]
               [Win10 / Win11 Native Service]
                          │
                Adaptive Jitter Buffer
                          ▼
            Media Foundation H.264 Decoder
                          ▼
             Direct3D 11 NV12 Shared Texture
                          │
            ┌─────────────┴─────────────┐
            ▼                           ▼
    VeyraLink (Desktop UI)        Veyra Camera
 (Zero-Copy GPU Texture)     (Zoom / Teams / OBS / Meet)
```

1. **Zero-Copy Native Path:** Flutter handles only UI/control. Frames stay in native memory (`Camera2 → MediaCodec → Native Sockets → Media Foundation → D3D11 NV12`).
2. **Single Canonical Identity:** Downstream apps see only **`Veyra Camera`**. Transport switches (USB ↔ Wi-Fi ↔ BT) without re-registering the device.
3. **Screen-Off Mode:** Preview surface off, encoder + network stay alive (~40% thermal/display saving).
4. **Adaptive Thermal Ladder:** Real-time thermal monitoring scales resolution/fps/bitrate before throttling.
5. **E2E Media Encryption:** Every video/audio packet carries `FLAG_ENCRYPTED (0x04)` + 16 B Poly1305 tag; header is AAD; nonce = 8 B direction salt + 4 B BE sequence. Decrypt-verified before reassembly, anti-replay, session-locked.

---

## ✨ Features

- **Transports:** USB, Wi-Fi LAN, Wi-Fi Direct, Bluetooth Classic with seamless handoff
- **Video:** Hardware H.264 CBR, adaptive jitter buffer, IDR recovery, 240p–1080p
- **Security:** X25519 key exchange, HKDF-SHA256, ChaCha20-Poly1305 AEAD, PIN pairing, per-session token, replay window 64, peer-IP filter, bonded-BT only
- **Controls:** Zoom 1–8×, exposure ±4 EV, AF/MF, torch, camera flip, force IDR
- **Service:** Android foreground service + Windows tray `VeyraCore.exe` + `veyra_core_api.dll`

---

## 🔒 Security Hardening (v1.0.1)

Full audit and remediation report: [`spec/specs/001-security-review.md`](spec/specs/001-security-review.md)

| Finding | Fix |
|---|---|
| **C-1** Broken crypto | Rewritten on **mbedTLS 3.6.2 LTS** (`native/core/src/crypto.cpp:1`, `native/core/include/veyra/crypto.h:1`): X25519 (`mbedtls_ecdh`), HKDF-SHA256 with transcript binding (`VeyraV1|s2c|c2s|…`), ChaCha20-Poly1305, CTR-DRBG CSPRNG |
| **C-2** No authentication | **PIN pairing** (`apps/veyracam/android/app/src/main/kotlin/com/veyra/cam/service/PairingManager.kt:1`, `native/windows/core/service_manager.cpp:1`): `pairing_challenge` → `pairing_response{pin,client_pubkey}` → `pairing_ok{token}`; constant-time `MessageDigest.isEqual`, 5 fails → 30 s lockout, 32 B token required on every control message, ephemeral X25519 per session |
| **C-3** Plaintext media | AEAD per packet (`native/core/src/packetizer.cpp:1`): nonce = salt+seq, AAD = header, tag 16 B, `FLAG_ENCRYPTED`, replay checked **after** auth |
| **H-1** Cleartext flag | `usesCleartextTraffic` removed (`apps/veyracam/android/app/src/main/AndroidManifest.xml:1`) |
| **H-2** Debug signing | Release keystore from CI secrets (`ANDROID_KEYSTORE_BASE64/PASSWORD/ALIAS/KEY_PASSWORD`) decoded at configure (`apps/veyracam/android/app/build.gradle:8`), `*.jks` gitignored |
| **H-3** UDP source spoof | `SetExpectedPeer(ip)` peer filter (`native/windows/network/iocp_udp_server.cpp:1`) |
| **H-4** BT no allowlist | Bonded-only + optional allowlist (`apps/veyracam/android/app/src/main/kotlin/com/veyra/cam/transport/BluetoothTransport.kt:1`) |
| **M-2** Unpinned CI actions | All actions pinned by SHA, per-job `permissions`, `persist-credentials: false`, NDK `r28c` (`.github/workflows/release.yml:1`) |
| **M-3** Wrapper checksum | `distributionSha256Sum` in `gradle-wrapper.properties:1` |
| **M-4** Predictable sessionId | `RandomId()` via CTR-DRBG (`native/core/src/crypto.cpp:1`) |
| **L-2..L-4** Caps/IOCP/telemetry | 64 KB control-line cap, dead IOCP removed, thread-local 4 KB telemetry buffer |

> **Residual risk (documented):** The 6-digit PIN is sent once over LAN TCP (5150) in plaintext. Mitigated by fresh ephemeral X25519 per connection (sniffed PIN cannot be replayed), single-use token, and H-3 IP filter. Full PAKE (SPAKE2/SRP) is planned.

---

## 📁 Repository Structure

```text
VeyraCam/
├── apps/
│   ├── veyracam/                 # Android app (Camera2 + MediaCodec + JNI + FGS)
│   │   ├── android/
│   │   │   ├── app/src/main/cpp/ # veyra_jni_bridge.cpp, CMakeLists (mbedTLS)
│   │   │   ├── app/src/main/kotlin/com/veyra/cam/
│   │   │   │   ├── service/      # VeyraStreamingService, VeyraNativeBridge, PairingManager
│   │   │   │   └── transport/    # UdpTransport (pairing+token), BluetoothTransport
│   │   │   └── gradle/wrapper/   # wrapper checksum + AGP 8.11.1 / Kotlin 2.2.20
│   │   └── lib/                  # camera_controller, camera_overlay (PIN badge)
│   └── veyralink/                # Windows desktop (3-pane workstation)
│       └── lib/                  # desktop_session_controller, device_discovery_dialog (PIN field)
├── packages/
│   ├── veyra_models/             # Shared immutable domain models
│   ├── veyra_protocol_dart/      # Binary command & handshake encoders
│   ├── veyra_native/             # dart:ffi C-ABI (pin + status callback)
│   └── veyra_ui/                 # Cyberpunk dark theme, HUD & sliders
├── native/
│   ├── core/                     # C++20 shared engine (crypto/packetizer/session/telemetry)
│   │   ├── include/veyra/        # crypto.h, packetizer.h, session.h, telemetry.h
│   │   ├── src/                  # crypto.cpp (mbedTLS), packetizer.cpp (AEAD), session.cpp, telemetry.cpp
│   │   └── tests/                # test_main.cpp (Protocol/Ring/Jitter/Crypto/SecurePath/Telemetry)
│   └── windows/                  # MF decoder, D3D11, IOCP UDP, TCP control, VeyraCore.exe
│       ├── core/service_manager* # pairing client state machine
│       ├── network/              # iocp_udp_server, tcp_control_server (64KB cap)
│       └── ipc/veyra_ffi_export* # connect_device(pin, statusCb), L-4 TLS buffer
├── test/                         # e2e_streaming_test.cpp (60-frame AEAD pipeline)
├── spec/specs/
│   ├── 000-veyra-system-spec.md
│   └── 001-security-review.md    # full audit + remediation status (v1.0.1)
└── .github/workflows/
    ├── release.yml               # pinned SHAs, per-job perms, signed APK + Windows bundle
    └── security.yml              # OSV scan
```

---

## 🛠 Prerequisites

| Component | Version |
|---|---|
| Flutter SDK | **≥3.22**, tested on **3.47** stable (`flutter --version`) |
| Dart SDK | `>=3.3.0` (required by `veyra_native`) |
| Android | SDK 34, **NDK `r28c` (28.2.13676358)**, JDK 17 (Temurin) |
| Gradle | 8.14, AGP **8.11.1**, Kotlin **2.2.20** (`apps/veyracam/android/settings.gradle:21`) |
| Windows | Win 10 1809+ / Win 11, **VS 2022** (Desktop C++), **CMake ≥3.22.1** |

> Local NDK must match CI (`r28c`). The Android `gradle-wrapper.properties:1` includes `distributionSha256Sum` for tamper protection.

---

## 📥 Quick Start (from Releases)

1. Download **latest release** from [Releases](https://github.com/mcpe500/VeyraCam/releases): `VeyraCam-1.0.1.apk` + `VeyraLink-1.0.1-windows-x64.zip`
2. Install APK on Android 7.0+ (allow unknown sources or via `adb install -r VeyraCam-*.apk`)
3. On Windows 10/11 64-bit, unzip and run `VeyraLink.exe` (bundle already contains `VeyraCore.exe` + `veyra_core_api.dll`)
4. Follow [Pairing & Connect](#-pairing--connect) below — you will be prompted for a **6-digit PIN** shown on the phone.

---

## 📦 How to Build & Release

### 0. Fetch all Dart dependencies

```bash
for d in packages/veyra_models packages/veyra_protocol_dart packages/veyra_ui packages/veyra_native apps/veyracam apps/veyralink; do
  (cd $d && flutter pub get)
done
```

### 1. Build Android APK (signed)

`apps/veyracam/android/app/build.gradle:8` reads signing secrets from env:

```bash
# Provide signing secrets (CI sets these via gh secrets; locally you can generate a debug keystore)
export ANDROID_KEYSTORE_BASE64=$(base64 -w0 /path/to/release-keystore.jks)
export ANDROID_KEYSTORE_PASSWORD=...
export ANDROID_KEY_ALIAS=veyra
export ANDROID_KEY_PASSWORD=...

cd apps/veyracam
flutter build apk --release --android-skip-build-dependency-validation
# or split per ABI:
flutter build apk --release --split-per-abi --android-skip-build-dependency-validation

# Output:
# apps/veyracam/build/app/outputs/flutter-apk/app-release.apk
# CI renames to VeyraCam-<tag>.apk

# Install:
adb install -r build/app/outputs/flutter-apk/app-release.apk
```

Without secrets the build falls back to `signingConfigs.debug`. Never commit `*.jks` (` .gitignore:1`).

### 2. Build Windows Suite (`VeyraLink.exe` + `VeyraCore.exe`)

```bash
# Step 2A: Native C++ engine (generates VeyraCore.exe + veyra_core_api.dll)
cd native/windows
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# -> native/windows/build/Release/VeyraCore.exe
# -> native/windows/build/Release/veyra_core_api.dll  (mbedTLS statically linked via mbedcrypto)

# Step 2B: Flutter desktop UI
cd ../../apps/veyralink
flutter pub get
flutter build windows --release
# Copy native artifacts into Flutter bundle (CI does this via pwsh)
cp ../../native/windows/build/Release/veyra_core_api.dll build/windows/x64/runner/Release/
cp ../../native/windows/build/Release/VeyraCore.exe    build/windows/x64/runner/Release/
# -> apps/veyralink/build/windows/x64/runner/Release/veyralink.exe
# CI bundles: VeyraLink-<tag>-windows-x64.zip
```

### 3. Tag a Release (maintainers)

```bash
# Ensure versionCode/versionName and pubspec 1.0.1+1 are bumped
git tag v1.0.1
git push origin v1.0.1
# .github/workflows/release.yml:1 triggers: Android signed APK + Windows bundle + GitHub Release
```

---

## 🔗 Pairing & Connect

Every new Wi-Fi/USB session requires **one-time PIN pairing**. The phone shows a 6-digit PIN in the overlay (`apps/veyracam/lib/widgets/camera_overlay.dart:1`); the PC prompts for it (`apps/veyralink/lib/views/device_discovery_dialog.dart:1`).

**Protocol (`PairingManager.kt:1`, `service_manager.cpp:1`):**
```
PC  --TCP 5150-->  PHONE : pairing_challenge{server_pubkey, session_id}
PHONE --TCP 5150--> PC   : pairing_response{pin, client_pubkey}
PHONE verifies PIN (constant-time, 5 fails → 30 s lockout)
PHONE --TCP 5150--> PC   : pairing_ok{token}  or  pairing_error
PC stores token → all further control JSON must include {auth_token: token}
Media UDP 5151 only allowed after auth, filtered by SetExpectedPeer(ip) [iocp_udp_server.cpp:1]
```

### Mode A: Wi-Fi LAN / Wi-Fi Direct

1. Phone: launch **VeyraCam** → **START STREAMING** → PIN badge appears + IP shown (e.g. `192.168.1.105`)
2. PC: launch **VeyraLink** → **CONNECT PHONE** → enter IP + **6-digit PIN** → Connect
3. On success the service shows `pairingStatus` and media starts (AEAD encrypted). Open Zoom/Teams/OBS → **`Veyra Camera`**.

### Mode B: USB Tunnel (ultra-low latency)

1. Connect phone via USB, enable USB Debugging.
2. `adb forward tcp:5150 tcp:5150` (and `adb forward tcp:5151 tcp:5151` if using Wi-Fi UDP over forward)
3. In VeyraLink connect to `127.0.0.1:5150` + PIN from phone.

### Mode C: Bluetooth Classic (emergency / low-bandwidth)

1. Pair phone + PC in Windows Bluetooth settings (bonded required — `BluetoothTransport.kt:1` rejects unpaired).
2. VeyraLink auto-discovers RFCOMM UUID `a888c728-6623-4217-9160-b6f2048995a9`, pairs with PIN, then streams adaptive 240p/360p.

---

## 🎛 Remote 3A Camera Controls

From **VeyraLink** desktop (`desktop_session_controller.dart:1`):

- **Digital Zoom:** 1.0×–8.0× (presets 1×/2×/5×)
- **Exposure:** −4 … +4 EV
- **Focus:** MF dial 0.0 (macro) → 1.0 (infinity) / AF toggle
- **Torch / Flash:** remote toggle
- **Camera Flip:** rear ↔ front on the fly
- **Force Keyframe (IDR):** recover from glitch

---

## 🧪 Verification & Test Suites

```bash
# 1. Native Core C++ unit tests (Release — NDEBUG-sensitive)
cmake -S native/core -B native/core/build -DCMAKE_BUILD_TYPE=Release
cmake --build native/core/build --config Release
./native/core/build/test_veyra_core
# -> TestProtocol, TestPacketizerAndReassembler, TestRingBuffer, TestJitterBuffer,
#    TestCrypto (AAD-tamper, wrong-seq, corrupted tag),
#    TestSecurePacketPath (AEAD roundtrip, replay, forged-drop, session-lock, downgrade),
#    TestTelemetry, TestTransportManager — all PASSED

# 2. End-to-end synthetic streaming (60-frame 720p + jitter + AEAD)
cmake -S test -B build-test -DCMAKE_BUILD_TYPE=Release
cmake --build build-test
./build-test/e2e_streaming_test

# 3. Dart packages
(cd packages/veyra_models && dart test)
(cd packages/veyra_protocol_dart && dart test)
# veyra_native / veyra_ui are flutter-analyzed (no dart test harness)

# 4. Flutter static analysis (must be clean)
for d in packages/veyra_models packages/veyra_protocol_dart packages/veyra_ui packages/veyra_native apps/veyracam apps/veyralink; do
  flutter analyze --no-pub $d
done

# 5. Android (Kotlin + JNI + mbedTLS via NDK) — heavy, ~4 min
cd apps/veyracam/android
./gradlew :app:assembleDebug            # quick (no signing)
ANDROID_KEYSTORE_BASE64=... ./gradlew :app:assembleRelease  # signed, verifies H-2
```

---

## 🔄 CI/CD

- **Release** (`.github/workflows/release.yml:1`): triggered on `v*` tags. Jobs: `build-android` (pinned `actions/checkout@11d5960a`, `setup-java@cf277c60`, `setup-ndk@aacaf74`, `flutter-action@1a44944`, JDK 17, NDK `r28c`, `flutter build apk --release`) → `VeyraCam-<tag>.apk` signed; `build-windows` (MSVC + `flutter build windows` + `cmake` for `VeyraCore.exe`/`veyra_core_api.dll`) → `VeyraLink-<tag>-windows-x64.zip`; `publish-release` (`softprops/action-gh-release@3bb12739`) creates GitHub Release.
- **Security Scan** (`.github/workflows/security.yml:1`): OSV scanning on pushes.

Artifacts are named dynamically from `${{ github.ref_name }}` (M-2).

---

## ❓ Troubleshooting

| Symptom | Fix |
|---|---|
| `usesCleartextTraffic` or APK unsigned | Ensure `ANDROID_KEYSTORE_BASE64` secrets set (`gh secret list`); check `apksigner verify --print-certs` shows `CN=Veyra` |
| `AGP 8.7.0 < 8.11.1` or `Kotlin 2.0.0 < 2.2.20` | Bump `settings.gradle:21` to `8.11.1` / `2.2.20` (Flutter 3.47 requirement) |
| NDK 404 `r28.2.13676358` | Use `r28c` in `release.yml:30` (not `r28.2.13676358`) |
| `mbedcrypto` not found | CMake FetchContent pulls `mbedtls-3.6.2.tar.bz2` (`8b54fb9b…ccdca`); ensure internet + `ENABLE_TESTING OFF` |
| `ReplayFilter` drops valid packets | Only after successful AEAD auth; check `HasKeys()` / `FLAG_ENCRYPTED` |
| PIN rejected / lockout | 5 wrong PINs → 30 s lockout (`PairingManager.kt:1`); wait or restart service to reset |
| Bluetooth not connecting | Device must be **bonded** in Windows settings; allowlist enforced (`BluetoothTransport.kt:1`) |
| `flutter analyze` SDK warnings | `veyra_native/pubspec.yaml:7` requires `sdk: '>=3.3.0'` for `NativeCallable` |

---

## 📚 Project Links

- System spec: [`spec/specs/000-veyra-system-spec.md`](spec/specs/000-veyra-system-spec.md)
- Security audit & remediation: [`spec/specs/001-security-review.md`](spec/specs/001-security-review.md)
- Releases: [github.com/mcpe500/VeyraCam/releases](https://github.com/mcpe500/VeyraCam/releases)

---

## 📄 License

MIT License — free for personal, commercial, and open-source use.
