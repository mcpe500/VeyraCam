# VeyraCam — High-Performance Zero-Copy Virtual Webcam

> **Your phone. Your camera. Anywhere.**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-brightgreen.svg)](https://isocpp.org/)
[![Flutter](https://img.shields.io/badge/Flutter-3.16+-02569B.svg?logo=flutter)](https://flutter.dev)
[![Android](https://img.shields.io/badge/Android-7.0%2B%20(API%2024%2B)-3DDC84.svg?logo=android)](https://developer.android.com/)
[![Windows](https://img.shields.io/badge/Windows-10%20%2F%2011%20(64--bit)-0078D6.svg?logo=windows)](https://microsoft.com/windows)

Veyra turns your Android smartphone into a low-latency, hardware-accelerated PC virtual webcam for **Zoom, Microsoft Teams, OBS Studio, Google Meet, and Discord**.

Operating seamlessly across **USB (ADB/WinUSB)**, **Wi-Fi LAN**, **Wi-Fi Direct (P2P)**, and **Bluetooth Classic (RFCOMM)** with automatic link quality scoring and sub-500ms seamless handoff.

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
         Veyra Link Protocol Binary Packetizer
                          │
       ┌──────────────────┼──────────────────┐
       ▼                  ▼                  ▼
   USB Tunnel         Wi-Fi UDP          RFCOMM BT
       │                  │                  │
       └──────────────────┼──────────────────┘
                          │
                          ▼
               VEYRA CORE C++ (WINDOWS)
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

1. **Zero-Copy Native Path:** Flutter is used exclusively for UI/control logic. Video frames and H.264 NAL packets travel purely through native memory (`Camera2 → MediaCodec → Native Sockets → Media Foundation Decoder → Direct3D 11 NV12`).
2. **Single Canonical Camera Identity:** Downstream apps only see **`Veyra Camera`**. Transport switching (USB $\leftrightarrow$ Wi-Fi $\leftrightarrow$ Bluetooth) happens under the hood without resetting device registration or freezing calls.
3. **Screen-Off Mode:** Turning off the phone screen shuts down the preview surface while keeping the hardware encoder and network active, saving up to ~40% display/GPU thermal budget.
4. **Adaptive Thermal Ladder:** Realtime thermal monitoring gracefully scales resolution, framerate, and bitrate before hardware throttling occurs.

---

## 📁 Repository Structure

```text
VeyraCam/
├── apps/
│   ├── veyracam/                # Android Flutter App (Viewfinder & Controls)
│   │   ├── android/             # Camera2, MediaCodec, JNI, Foreground Service
│   │   └── lib/                 # Flutter UI & HUD
│   └── veyralink/               # Windows Flutter App (Desktop Dashboard)
│       └── lib/                 # 3-pane control workstation
├── packages/
│   ├── veyra_models/            # Shared immutable domain models
│   ├── veyra_protocol_dart/     # Binary command & handshake encoders
│   ├── veyra_native/            # Low-level dart:ffi C-ABI bindings
│   └── veyra_ui/                # Cyberpunk dark theme, HUD & sliders
├── native/
│   ├── core/                    # Shared C++20 Core Media & Protocol Engine
│   └── windows/                 # Windows Media Foundation, D3D11, IOCP & VeyraCore.exe
└── test/                        # Master end-to-end integration test harness
```

---

## 🛠 Prerequisites

- **Flutter SDK:** $\ge$ 3.16.0
- **Android Development:** Android SDK 34, Android NDK 25.1+, JDK 17
- **Windows Development:** Windows 10 (1809+) or Windows 11 (22000+), Visual Studio 2022 (C++ Desktop Workload), CMake $\ge$ 3.20

---

## 📦 How to Build & Release

### 1. Build Android APK (`VeyraCam.apk`)

To generate an optimized release APK with all native C++ JNI libraries compiled:

```bash
# Navigate to Android App folder
cd apps/veyracam

# Fetch dependencies
flutter pub get

# Option A: Build Universal Release APK
flutter build apk --release

# Option B: Build Split-per-ABI APKs (Smaller file size for arm64-v8a / armeabi-v7a)
flutter build apk --release --split-per-abi

# Output location:
# apps/veyracam/build/app/outputs/flutter-apk/app-release.apk
```

#### Install on Android Phone:
```bash
# Via ADB:
adb install -r apps/veyracam/build/app/outputs/flutter-apk/app-release.apk
```

---

### 2. Build Windows Executable & Native Core (`VeyraLink.exe` + `VeyraCore.exe`)

Building the desktop suite produces two components:
1. `VeyraCore.exe` + `veyra_core_api.dll` (Lightweight C++ background tray service)
2. `VeyraLink.exe` (Flutter Desktop Control Dashboard)

#### Step 2A: Build Native C++ Engine (`VeyraCore.exe` & `veyra_core_api.dll`)
```bash
cd native/windows

# Generate CMake build files (Visual Studio 2022 on Windows)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Compile binaries
cmake --build build --config Release

# Output binaries:
# native/windows/build/Release/VeyraCore.exe
# native/windows/build/Release/veyra_core_api.dll
```

#### Step 2B: Build Flutter Desktop UI (`VeyraLink.exe`)
```bash
cd apps/veyralink

# Fetch dependencies
flutter pub get

# Build Windows Release
flutter build windows --release

# Copy the native DLL into the release directory
cp ../../native/windows/build/Release/veyra_core_api.dll build/windows/x64/runner/Release/

# Output location:
# apps/veyralink/build/windows/x64/runner/Release/veyralink.exe
```

---

## 🚀 How to Run & Connect

### Mode A: Wi-Fi LAN / Wi-Fi Direct (Wireless)
1. Launch **VeyraCam** on your Android phone.
2. Tap **START STREAMING**. The phone will display its local IP (e.g. `192.168.1.105`).
3. Launch **VeyraLink** on your PC.
4. Click **CONNECT PHONE**, enter the phone's IP, and click **Connect**.
5. Open Zoom, Microsoft Teams, Discord, or OBS $\rightarrow$ Select **`Veyra Camera`** as your video source.

### Mode B: USB Tunnel (Ultra-Low Latency, No Wi-Fi Needed)
1. Connect your phone to your PC via USB cable.
2. Enable USB Debugging on your phone.
3. Forward port 5150:
   ```bash
   adb forward tcp:5150 tcp:5150
   ```
4. In **VeyraLink**, connect to `127.0.0.1:5150`.

### Mode C: Bluetooth Classic (Emergency / Low-Bandwidth)
1. Pair your phone and PC via Windows Bluetooth settings.
2. VeyraLink will automatically detect the RFCOMM service channel (`a888c728-6623-4217-9160-b6f2048995a9`) and establish an adaptive 240p/360p video link.

---

## 🎛 Remote 3A Camera Controls

From the **VeyraLink** desktop panel, you can adjust:
- **Digital Zoom:** 1.0x to 8.0x (with quick 1x / 2x / 5x presets).
- **Exposure Compensation:** $-4$ to $+4$ EV steps.
- **Manual Focus Dial / Auto-Focus (AF):** Macro (0.0) to Infinity (1.0).
- **Torch / Flash Toggle:** Remote flashlight activation.
- **Camera Flip:** Switch between Rear and Front cameras on the fly.
- **Force Keyframe (IDR):** Instantly request an IDR frame to recover from network glitch.

---

## 🧪 Verification & Test Suites

You can execute the automated test suites to verify integrity:

```bash
# 1. Run Native Core C++ Unit Tests (Protocol, RingBuffer, JitterBuffer, Crypto, Telemetry)
./native/core/build/test_veyra_core

# 2. Run Master End-to-End Synthetic Streaming Test (60-frame pipeline simulation)
./test/build/e2e_streaming_test

# 3. Run Dart Packages Unit Tests
(cd packages/veyra_models && dart test)
(cd packages/veyra_protocol_dart && dart test)
(cd packages/veyra_native && dart test)

# 4. Run Flutter Static Analysis
flutter analyze packages/veyra_ui apps/veyracam apps/veyralink
```

---

## 📄 License

MIT License — free for personal, commercial, and open-source use.
