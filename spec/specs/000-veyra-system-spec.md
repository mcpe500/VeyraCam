# Veyra System — Technical Specification

**Document Version:** 1.0
**Status:** Draft — Acuan Implementasi
**Target Platform:** Android 7.0+ & Windows 10/11
**Primary Goal:** Mengubah smartphone menjadi webcam PC melalui USB, Wi-Fi LAN, Wi-Fi Direct, atau Bluetooth dengan konsumsi resource rendah dan latency rendah — termasuk pada perangkat lama.

---

## 1. Overview & Brand Family

| Komponen | Nama | Fungsi |
|---|---|---|
| Aplikasi HP | **VeyraCam** | Mengubah HP menjadi kamera (capture, encode, transport) |
| Aplikasi Desktop | **VeyraLink** | UI desktop: discovery, pairing, preview, remote control |
| Native Engine Desktop | **Veyra Core** | Service native C++: receive, decode, virtual camera |
| Virtual Camera | **Veyra Camera** | Kamera virtual yang muncul di Zoom/Discord/Meet/OBS/Teams |
| Protokol | **Veyra Link Protocol** | Protokol komunikasi internal |

Tagline:

> **VeyraCam — Your phone. Your camera. Anywhere.**

Aturan penamaan (canonical identity):

- Aplikasi PC hanya melihat **satu** kamera: `Veyra Camera`.
- Dilarang membuat beberapa device seperti `Veyra USB Camera`, `Veyra WiFi Camera`, `Veyra Bluetooth Camera`.
- Transport adalah *implementation detail*, bukan identitas perangkat.

---

## 2. Goals & Non-Goals

### Goals v1
- Streaming kamera HP ke PC melalui:
  - Wi-Fi LAN
  - Wi-Fi Direct
  - USB
  - Bluetooth
- Low latency, high performance di HP lama (Android 7, 2 GB RAM) dan PC lama (Win10, 4 GB RAM).
- Virtual camera yang bisa dipakai di Zoom, Discord, Teams, OBS, Google Meet.
- Auto connection & seamless fallback antar transport (<500 ms handoff).
- Remote control kamera dari PC: zoom, focus, exposure, flash, switch camera.
- Foreground service agar tetap streaming saat layar HP mati.
- Adaptive quality: otomatis turun-naik resolusi/FPS/bitrate berdasarkan kondisi device & jaringan.

### Non-Goals v1
- Streaming lewat internet/cloud.
- iOS app.
- Multi-camera/studio (direncanakan untuk Veyra Studio).
- AI background blur, auto framing, dsb.
- Virtual microphone (roadmap V1.1/V1.2, lihat §10).
- Windows 7/8, 32-bit x86 Windows.

---

## 3. Target Platform & Minimum Requirements

### 3.1 VeyraCam Mobile

| Item | Minimum |
|---|---|
| OS | Android 7.0 (API 24) — batas bawah realistis Flutter + jangkauan HP lama |
| ABI | ARM32 & ARM64 |
| RAM | 2 GB |
| CPU | 4-core ARM |
| Kamera | Camera2 API support |
| Encoder | Hardware H.264 encoder (MediaCodec) — sangat disarankan |
| Bluetooth | Bluetooth Classic (RFCOMM), minimal 4.x |
| Wi-Fi | 2.4 GHz (5 GHz opsional) |
| USB | USB debugging untuk mode USB v1 |

Perangkat **tanpa** hardware H.264 tetap diberi Compatibility Mode (software encode), tetapi bukan target performance utama.

### 3.2 VeyraLink Desktop

| Item | Minimum |
|---|---|
| OS | Windows 10 1809+ (Build 17763) untuk VeyraLink UI & Veyra Core |
| RAM | 4 GB |
| CPU | 2-core x64 |
| GPU | DX11 disarankan untuk hardware decode; software decode sebagai fallback |
| Virtual Camera | Windows 11 22000+ → `MFCreateVirtualCamera` |
| Fallback Windows 10 | DirectShow source filter (compatibility-only) |

> **Catatan penting:** API `MFCreateVirtualCamera()` hanya tersedia di Windows 11 (Build 22000+). Untuk PC Windows 10, VeyraLink v1 memakai DirectShow virtual source (compatibility backend), bukan arsitektur masa depan.

---

## 4. Fundamental Architecture Rule

Aturan arsitektur paling penting:

> **Flutter hanya untuk UI/control.**
> **Seluruh jalur video/audio/frame TIDAK boleh lewat Dart.**

- Dart **tidak pernah** menerima raw video frame maupun encoded frame secara rutin.
- Tidak boleh ada pipeline seperti:

```
Camera
↓
Flutter plugin
↓
Uint8List
↓
Dart
↓
H264 encoder
↓
Dart socket
```

- `cameraImage.planes` dilarang sebagai jalur streaming production.
- Tidak ada raw frame dalam Dart, tidak ada encoded video packet lewat MethodChannel.

### 4.1 Arsitektur Sistem (End-to-End)

```
                    VEYRACAM
                 ANDROID PHONE
                       │
                       │
                ┌──────▼──────┐
                │ Flutter UI  │   presentation & control only
                └──────┬──────┘
                       │
                 control only
                       │
                ┌──────▼──────┐
                │ Native Core │   Kotlin/C++
                └──────┬──────┘
                       │
                  Camera2 API
                       │
                       ▼
                Camera Surface
                       │
                       ▼
                  MediaCodec
                Hardware H.264
                       │
                       ▼
                Encoded H.264
                  (native memory)
                       │
                       ▼
                Veyra Link Protocol
                       │
       ┌───────────────┼────────────────┐
       │               │                │
       ▼               ▼                ▼
      USB             Wi-Fi         Bluetooth
       │               │                │
       └───────────────┼────────────────┘
                       │
                       ▼
                  VEYRA CORE (C++)
                       │
              ┌────────┴────────┐
              │                 │
           Decoder          Flutter UI
              │             control only
              ▼
       D3D11 / NV12 frame
              │
       ┌──────┴──────────┐
       │                 │
       ▼                 ▼
   Preview (VeyraLink)   Veyra Camera
                         │
                         ▼
                 Zoom / Meet / OBS
```

### 4.2 Kenapa Flutter Tetap Dipakai

Flutter sangat baik untuk:

```
UI, settings, device selector, pairing, camera controls,
stats, onboarding, update UI, cross-platform desktop UI
```

Flutter **tidak** digunakan untuk:

```
camera frame processing, video encoding, video decoding,
RTP packetization, network packet processing, Bluetooth video
transfer, USB stream, color conversion, virtual camera
```

Dengan demikian Flutter bukan bottleneck. **Media path: native → native → native → native.**

### 4.3 Keputusan WebRTC

- Untuk prototype: `flutter_webrtc` boleh.
- Untuk arsitektur production: **tidak** menjadikan flutter_webrtc sebagai core transport Veyra.
- Alasan: Veyra hanya beroperasi pada local link phone↔PC, harus mendukung USB/Bluetooth/Wi-Fi sekaligus, dan mengejar perangkat lama. Native unified media protocol menghasilkan pipeline yang jauh lebih kecil, terkendali, dan terukur.

### 4.4 Performance Critical Path

```
CAMERA SENSOR
      │
      ▼
CAMERA2 SURFACE
      │
      ▼
MEDIACODEC H264
      │
      ▼
NATIVE BUFFER
      │
      ▼
VEYRA PACKETIZER
      │
      ▼
USB / UDP / RFCOMM
      │
      ▼
VEYRA CORE C++
      │
      ▼
JITTER BUFFER
      │
      ▼
MEDIA FOUNDATION H264 DECODER
      │
      ▼
D3D11 NV12 SURFACE
      │
      ├───────────────┐
      ▼               ▼
VEYRA CAMERA      PREVIEW (VeyraLink)
      │
      ▼
ZOOM / MEET / OBS
```

**DART TIDAK ADA** di sepanjang jalur media tersebut.

---

## 5. VeyraCam Mobile — Detailed Spec

### 5.1 Flutter UI (Mobile)

Hanya mengurus:
- Preview kamera (via native Texture, bukan frame Dart).
- Status koneksi, device discovery/pairing.
- Kontrol manual (zoom, flash, focus, switch camera).
- Settings, quality profile, performance mode.

### 5.2 Native Camera Engine

- Menggunakan **Camera2 API** (bukan CameraX) untuk kontrol penuh dan jangkauan HP lama.
- Pipeline:

```
Camera Sensor
      │
      ▼
Camera2
      │
      ▼
Surface
      │
      ▼
MediaCodec (H.264, hardware, surface input)
```

- **Wajib Surface input** ke MediaCodec — menghindari copy YUV/bitmap ke memory Dart.
- Preview merupakan output tambahan dari `CameraCaptureSession` (dua output: Encoder Surface + Preview Surface).
- Jika device lemah: stream = 720p30, preview = 360p15 (tidak perlu render preview full-resolution).

### 5.3 Video Encoder Configuration

| Parameter | Value |
|---|---|
| Codec | H.264 / AVC |
| Profile | Constrained Baseline / Baseline (default, compatibility); Main (performance); High hanya jika didukung hardware |
| Level | 3.1 untuk 720p, 4.0 untuk 1080p |
| Bitrate mode | CBR (constrained VBR hanya jika stabil) |
| Latency | low latency mode (`KEY_LATENCY` = 0 bila tersedia) |
| B-frames | **Nonaktif** (menambah frame-reordering delay) |
| SPS/PPS | Diulang setiap IDR |
| Keyframe interval | 1–2 detik |
| Resolution ladder | 320x240, 640x360, 640x480, 960x540, 1280x720, 1920x1080 |
| FPS | 15, 24, 30, 60 (60 hanya jika camera + encoder + transport + thermal mendukung) |
| Bitrate | lihat Quality Profiles (§5.4) |

HEVC/H.265 dan AV1 opsional di masa depan, **bukan default** (terutama device lama).

### 5.4 Quality Profiles

| Profile | Resolution | FPS | Bitrate |
|---|---|---|---|
| Ultra Low | 320×240 | 10–15 | 150–300 kbps |
| Bluetooth | 640×360 | 15 | 300–600 kbps |
| Low | 640×480 | 24 | 500–900 kbps |
| Balanced | 1280×720 | 30 | 1.5–3 Mbps |
| High | 1920×1080 | 30 | 3–6 Mbps |
| Performance | 1920×1080 | 60 | 6–10 Mbps |

60 FPS hanya muncul jika: camera mendukung **AND** encoder mendukung **AND** transport mendukung **AND** thermal state mengizinkan.

### 5.5 Capability Detection & Calibration

- Saat pertama dijalankan, VeyraCam membuat `DeviceCapabilityProfile`:

```
Device: Redmi Note X
H264 encoder: hardware
Maximum tested stable: 1920×1080@30
720p60: supported
1080p60: unsupported
Bluetooth: Classic RFCOMM
Wi-Fi: 2.4 / 5 GHz
Camera: front / back
```

- Veyra **tidak percaya hanya pada advertised capability** — lakukan benchmark pendek saat setup pertama:

```
Testing camera...
Testing encoder...
Testing transport...
```

- Hasil disimpan ke `device_profile.dat`:

```
Safe Profile: 720p30, H264 Baseline, 2 Mbps
```

### 5.6 Audio Capture

- `AudioRecord` → Opus encoder.
- Sample rate: 16 kHz atau 48 kHz (recommended 48 kHz).
- Mono.
- Bitrate: 16–64 kbps (recommended 24–64 kbps).
- Noise suppression sederhana (opsional).
- Audio tidak dikirim sebagai PCM melalui jaringan, kecuali USB debug mode.

### 5.7 Foreground Service (VeyraStreamingService)

- Service type: `camera | microphone` (wajib sejak Android 14).
- Streaming tidak bergantung pada lifecycle widget Flutter — native service yang mengelola:

```
Flutter Activity
       │
       ▼
Start session
       │
       ▼
VeyraStreamingService
       ├── camera
       ├── microphone
       ├── encoder
       └── transport
```

- Notifikasi:

```
VeyraCam
Streaming to EDELINE-PC
[ STOP ]
```

- Layar boleh mati, kamera tetap streaming.
- Wake lock hanya saat streaming aktif.

### 5.8 Screen-Off Mode (Preview Off)

Saat layar HP mati:

```
Preview Surface: OFF
Encoder Surface: ON
```

Mengurangi GPU load, memory bandwidth, display power, dan thermal load.

### 5.9 Battery & Thermal Management (Thermal Controller)

Input:

```
device temperature
encoder dropped frames
camera dropped frames
battery state
CPU load
network congestion
```

Output: bitrate, FPS, resolution.

Contoh adaptasi thermal:

```
Normal:      1080p30 @ 5 Mbps
HP panas:    1080p30 → 720p30
Masih panas: 720p30 → 720p24
Extreme:     480p24
```

Aturan dasar lama:

- Suhu CPU > 45°C → turunkan otomatis: 1080p30 → 720p30 → 480p24.
- Battery saver: matikan preview saat layar mati, kurangi bitrate.
- RAM < 2 GB → batasi 720p24.
- Core < 4 → batasi 480p30.

Streaming **tidak langsung mati** karena thermal — selalu turun bertahap.

### 5.10 Performance Modes (User Selectable)

| Mode | Behavior |
|---|---|
| Auto | Sistem memilih sendiri |
| Battery Saver | 480/720p, 24 fps, low bitrate, preview reduced |
| Balanced | 720p30 |
| Quality | 1080p30 |
| Low Latency | Buffer lebih kecil, USB preferred |

### 5.11 Old Phone Optimization (Low-End Mode)

```
720p30 maksimum (atau 480p30)
H264 hardware
preview 15fps
no blur, no filters, no AI, no software stabilization, no RGB conversion
Reduced Motion Mode pada UI
```

---

## 6. VeyraLink Desktop — Detailed Spec

### 6.1 Arsitektur Dua Executable

Desktop dibagi menjadi **dua executable**:

```text
VeyraLink.exe   — Flutter UI (presentation layer)
VeyraCore.exe   — Native C++ media service (mesin utama)
```

```
VeyraLink.exe (Flutter UI)
      │
      │ IPC
      ▼
VeyraCore.exe
      ├── networking
      ├── Bluetooth
      ├── USB
      ├── decoder
      ├── virtual camera
      └── device discovery
```

Keuntungan:

- Jika user menutup window VeyraLink → **stream tidak mati**; yang tetap berjalan hanya `VeyraCore.exe` yang ringan.
- Crash isolation: Flutter UI crash tidak merusak stream; native core crash bisa dideteksi & direstart oleh UI.

### 6.2 VeyraCore Idle Mode (Tray)

- Saat UI ditutup: System Tray → VeyraCore tetap berjalan.
- Target engineering (harus dibuktikan lewat profiling):

```text
VeyraCore idle:   ~20–50 MB working memory
CPU:              mendekati 0% ketika tidak streaming
```

- Auto-start saat Windows login (VeyraCore saja, bukan Flutter UI).
- Klik tray → launch VeyraLink UI.

### 6.3 Flutter Desktop UI

- Device list & pairing.
- Preview kamera (native GPU texture, bukan `Image.memory()`).
- Kamera control (zoom, focus, exposure, torch, switch camera).
- Settings transport & quality.
- Status latency/bitrate/FPS.
- System tray mode.
- Advanced diagnostics tab (lihat §9.7).

### 6.4 Flutter ↔ Native Communication

- Gunakan **`dart:ffi`** (bukan MethodChannel) untuk jalur high-frequency.
- Flutter hanya mengirim control command. Interface contoh:

```c
VeyraSession* veyra_session_create();

int veyra_connect(VeyraSession* session, const char* device_id);

int veyra_start_stream(VeyraSession* session, VeyraStreamConfig* config);

void veyra_set_zoom(VeyraSession* session, float zoom);

void veyra_disconnect(VeyraSession* session);
```

### 6.5 Event Frequency (Telemetry)

- Native **tidak boleh** mengirim event ke Flutter setiap frame (30 callbacks/detik = salah).
- UI telemetry cukup **2–5 update/detik**:

```text
FPS, bitrate, latency, battery, temperature,
signal strength, dropped frames
```

### 6.6 Decoder (Windows)

- Prioritas: **Media Foundation + hardware acceleration + D3D11**.

```
H264
 ↓
Media Foundation Decoder
 ↓
DXVA / GPU
 ↓
NV12
 ↓
D3D11 texture
```

- Software decode (OpenH264/FFmpeg) hanya fallback, dengan automatic quality reduction.
- **Decode only once**: satu decoder → satu NV12 frame → dibagi ke Preview **dan** Veyra Camera. Dilarang decode dua kali.

### 6.7 Video Scaling

Jika camera stream 1920×1080 tetapi Zoom meminta 1280×720 → scaling dilakukan di **GPU/D3D11**, bukan Dart dan bukan CPU biasa.

### 6.8 Virtual Camera Backend

| Windows Version | Backend |
|---|---|
| Windows 11 22000+ | `MFCreateVirtualCamera` (Media Foundation, `IMFVirtualCamera`) |
| Windows 10 1809+ | DirectShow virtual source (compatibility-only) |
| Optional future | Signed AVStream driver (bukan MVP — butuh driver dev, signing, installer elevation) |

- Device name: **Veyra Camera**.
- Output format utama: **NV12**; fallback: YUY2; hindari RGB jika tidak perlu.
- Transport switching **tidak boleh** menciptakan ulang device kamera Windows.

### 6.9 Desktop Startup & Tray Flow

```text
Windows boot
↓
VeyraCore auto-start (headless, tray)
↓
Flutter UI tidak perlu auto-launch (hemat RAM saat idle)
↓
User klik tray → launch VeyraLink UI
```

---

## 7. Veyra Link Protocol

### 7.1 Dua Lapisan Pesan

| Lapisan | Format | Digunakan untuk |
|---|---|---|
| Handshake & setup | JSON | hello, capabilities, startStream, pairing, debug |
| Session hot path | Binary | video packet, control, stats |

JSON **tidak** digunakan pada hot path.

### 7.2 Handshake (JSON)

Phone → PC:

```json
{
  "type": "hello",
  "protocol": 1,
  "device": {
    "name": "Galaxy S24",
    "id": "abc123"
  },
  "capabilities": {
    "video": true,
    "audio": true,
    "torch": true,
    "focus": true,
    "zoom": 10.0,
    "resolutions": ["1280x720", "1920x1080"],
    "fps": [24, 30]
  }
}
```

PC → Phone:

```json
{
  "type": "startStream",
  "video": {
    "width": 1280,
    "height": 720,
    "fps": 30,
    "bitrate": 2000000,
    "codec": "h264",
    "profile": "baseline"
  },
  "audio": true,
  "transport": "auto"
}
```

### 7.3 Binary Control Protocol (Hot Path)

Jangan gunakan JSON ketika session berjalan — gunakan compact binary messages:

```text
0x01 HELLO
0x02 CAPABILITIES
0x03 START_STREAM
0x04 STOP_STREAM
0x05 SET_ZOOM
0x06 SET_EXPOSURE
0x07 SET_FOCUS
0x08 REQUEST_IDR
0x09 SET_BITRATE
0x0A TRANSPORT_SWITCH
0x0B PING
0x0C PONG
0x0D STATS
```

JSON hanya untuk debug/logs/development.

### 7.4 Video Packet Format (Binary)

```text
VeyraMediaPacket
magic | version | flags | sessionId | streamId | frameId
sequence | fragmentIndex | fragmentCount | timestamp | payloadLength | payload
```

- Target header: **~24–40 bytes** (bukan ratusan byte metadata).
- MTU: payload ~1200 bytes agar aman dari fragmentation di berbagai network.
- NAL H.264 besar dipecah:

```text
Frame
 │
 ├── packet 1
 ├── packet 2
 ├── packet 3
 └── packet N
```

### 7.5 Packet Loss Handling

- Packet non-keyframe hilang → **drop affected frame** (jangan tunggu retransmission lama).
- Keyframe rusak → PC mengirim `REQUEST_IDR` (0x08) → HP mengirim keyframe baru.

### 7.6 Jitter Buffer

```text
Stable LAN:      20 ms
Unstable Wi-Fi:  40–80 ms (adaptive)
```

- Default: 20–60 ms.
- Jangan gunakan buffer ratusan milidetik seperti video streaming biasa — **ini webcam**.

### 7.7 Stats Messages (JSON, 1 detik, untuk debug/UI)

```json
{
  "type": "stats",
  "fps": 30,
  "bitrate": 2100000,
  "latency_ms": 42,
  "packet_loss": 0.2,
  "battery": 87,
  "temperature_c": 39
}
```

### 7.8 Remote Camera Controls (Capability-Gated)

PC dapat mengontrol: front/rear camera, zoom, torch, focus, exposure, white balance, resolution, FPS, bitrate, orientation, audio.

Hanya capability yang benar-benar tersedia pada device yang ditampilkan.

---

## 8. Transport Layer

### 8.1 Mode Transport

| Mode | Teknologi | Target | Desktop |
|---|---|---|---|
| Veyra Wi-Fi | LAN, UDP video + TCP control | 1080p30/60 | VeyraLink |
| Veyra Direct | Wi-Fi Direct (WifiP2pManager) | 1080p30/60 | VeyraLink |
| Veyra USB | USB data tunnel | 1080p30/60, latency rendah | VeyraLink |
| Veyra Bluetooth | Bluetooth Classic RFCOMM | 240p/360p/480p15 adaptive | VeyraLink |
| Veyra UVC | Android native UVC (device-dependent) | plug & play | Tidak perlu VeyraLink |

### 8.2 Transport Abstraction

Codec/session tidak boleh tahu transport mana yang dipakai:

```cpp
class Transport {
public:
    virtual bool connect() = 0;
    virtual ssize_t send(const uint8_t* data, size_t len) = 0;
    virtual void close() = 0;
    virtual TransportStats stats() = 0;
};
```

Implementations: `UdpTransport`, `TcpTransport`, `UsbTransport`, `BluetoothTransport`.

### 8.3 Wi-Fi LAN

```text
Phone
 │
 │ H264
 ▼
UDP        (video channel)
 │
 ▼
PC
```

- Video: **UDP** — tidak menunggu retransmission lama, packet lama dibuang, latency lebih penting daripada perfect delivery.
- Control: **TCP**.
- Wi-Fi Direct berbeda hanya pada *connection establishment*; media protocol tetap sama.

### 8.4 USB Detail

- **V1 — ADB tunnel** (untuk early release/developer):

```text
Phone → USB → ADB forward (adb forward tcp:5150 tcp:5150) → TCP → VeyraCore
```

Kekurangan: USB debugging required.

- **V2 — Native USB bulk transport** (consumer mode, target production):

```text
plug cable → detect → connect
```

tanpa konfigurasi jaringan, tidak mengandalkan Flutter.

- **Native UVC mode** (optional fast path, device-dependent, bukan requirement):

```text
Phone → USB UVC → Windows
```

VeyraLink dilewati untuk basic webcam mode. Tidak boleh menjadi requirement karena tergantung perangkat/OEM.

### 8.5 Bluetooth Detail

- **Bluetooth Classic RFCOMM** (`BluetoothSocket`), **bukan BLE** untuk video (BLE terlalu lambat).
- Jangan dipasarkan sebagai mode 1080p.
- Dua mode:

| Mode | Fungsi |
|---|---|
| Control | camera commands, discovery, pairing, status, fallback coordination |
| Video | emergency / low-bandwidth webcam: 240p, 360p, 480p jika throughput mencukupi |

- **Adaptive video** — Veyra mengukur bandwidth aktual:

```text
Available 420 kbps → 360p15 @ 350 kbps
Turun ke 250 kbps → 320×240 @ 12fps
```

- Jangan biarkan packet queue menumpuk: lebih baik **drop resolution** daripada latency menjadi 3 detik.
- Jika hanya BLE tersedia: tetap connect, tapi hanya untuk control channel.

### 8.6 Auto Transport Manager

Default UI:

```text
Connection
● Auto
○ USB
○ Wi-Fi
○ Direct
○ Bluetooth
```

Auto ranking:

```text
1. USB
2. Wi-Fi LAN good
3. Wi-Fi Direct
4. Bluetooth
```

Ranking dapat berubah berdasarkan measured performance.

### 8.7 Seamless Transport Handoff

Mechanism:

```text
Current Stream
      │
      ▼
Open candidate transport
      │
      ▼
Handshake
      │
      ▼
Request IDR frame
      │
      ▼
Receive valid keyframe
      │
      ▼
Switch decoder source
      │
      ▼
Close old transport
```

Bukan: `disconnect → reconnect`.

- Engineering target: **handoff <500 ms**, ideal <250 ms.
- Virtual camera instance tidak berubah.
- Reconnection: 0 detik connection lost → 0–2 detik coba transport yang sama → cari alternatif.

```text
Wi-Fi lost
   ↓
USB available?  → YES → handoff
   ↓ NO
Bluetooth paired → Bluetooth fallback (degraded, 360p15)
```

Veyra menunjukkan:

```text
Connection degraded
Bluetooth fallback
360p15
```

daripada memutus kamera sepenuhnya.

### 8.8 Connection Discovery

```text
Wi-Fi:      mDNS + UDP discovery fallback; manual IP tetap tersedia
Bluetooth:  service discovery, RFCOMM UUID
USB:        device enumeration
```

---

## 9. Performance Engineering

### 9.1 Zero-Copy Philosophy

- Android: `Surface` sebagai input encoder — native video buffer tidak dipetakan/disalin ke ByteBuffer.
- Target:

```text
0 raw-frame copy ke Dart
0 bitmap conversion
0 JPEG conversion
0 RGB conversion
```

### 9.2 Memory Pooling

- Saat startup: alokasikan packet pool, frame descriptors, receive buffers.
- Saat streaming: reuse, reuse, reuse.
- Dilarang `new/delete` per frame.
- Gunakan: ring buffer, object pool, fixed packet buffer pool.

### 9.3 Ring Buffers & Backpressure

- Setiap pipeline menggunakan bounded queue:

```text
Encoder → [0][1][2][3] → Network
```

- Queue penuh → **DROP OLD FRAME** (frame lama tidak berharga), bukan `WAIT WAIT WAIT`.
- Jika consumer terlambat (30 FPS input, 20 FPS capacity) → drop frame sampai kembali realtime, jangan biarkan latency menumpuk 1–3 detik.

### 9.4 Thread Architecture — Android

```text
Main Thread      Flutter/UI only
Camera Thread    Camera2 callbacks
Codec Thread     MediaCodec callbacks
Transport Thread network/native core
Audio Thread     AudioRecord
Telemetry Thread low frequency
```

Tidak ada blocking network operation di UI thread.

### 9.5 Thread Architecture — Windows

```text
Flutter UI Thread
VeyraCore:
  IO thread(s)        (IOCP — asynchronous I/O)
  Decoder thread
  Frame scheduler
  Virtual camera thread
  Audio thread
  Telemetry thread
```

- Gunakan thread count minimum; jangan spawn thread per connection event.
- Dilarang: 1 thread per packet, 1 allocation per packet.
- Process priority: streaming engine normal/high multimedia; UI normal. Jangan realtime.

### 9.6 Performance Budgets

**Android (reference: Android 7–9, 2 GB RAM, hardware H264):**

| Profile | Target |
|---|---|
| 480p24 | App memory <120 MB PSS; dropped frames <1%; thermal sustainable 60 min |
| 720p30 | App memory <160 MB PSS; dropped frames <1–2% |

**Desktop (reference: Win10, 4 GB RAM, old dual/quad-core, integrated GPU):**

| Item | Target |
|---|---|
| VeyraCore idle | <50 MB |
| VeyraCore 720p30 | <150 MB |
| Flutter UI terbuka | additional memory acceptable |
| UI ditutup | media stream tidak membutuhkan Flutter rendering |
| CPU (hardware decode) | low double-digit utilization |

- Jangan memaksakan 1080p60 software decode pada PC lama.
- Target CPU universal tidak ditetapkan karena SoC sangat beragam — diukur pada device test matrix.

### 9.7 Latency Goals

| Transport | Target |
|---|---|
| USB | p50 <80 ms, p95 <120 ms |
| Wi-Fi LAN good | p50 <100 ms, p95 <150 ms |
| Wi-Fi Direct | similar to LAN target |
| Bluetooth | quality > latency; <250–400 ms (compatibility transport) |

### 9.8 Latency Measurement

Jangan hitung latency hanya dengan stopwatch. Tambahkan timestamp per stage:

```text
capture timestamp
encoder timestamp
network send
network receive
decode timestamp
presentation timestamp
```

Diagnostic breakdown:

```text
Capture        8 ms
Encode        12 ms
Network       14 ms
Jitter        22 ms
Decode         7 ms
Output        11 ms
-----------------
Total         74 ms
```

### 9.9 Diagnostic Overlay (Developer Mode)

```text
FPS        29.97
Bitrate    2.3 Mbps
Encode     8.4 ms
Network    11 ms
Jitter     22 ms
Decode     5.7 ms
Dropped    0.3%
Transport  USB
Codec      H264 HW
```

Advanced diagnostics tab:

```text
Encoder:   OMX.qcom.video.encoder.avc (Hardware)
Decoder:   Media Foundation H264 (Hardware)
Transport: USB
Capture:   30.0 FPS
Output:    29.98 FPS
Packet loss: 0%
Jitter:    3.2 ms
End-to-end: 61 ms
```

Wajib ada untuk debugging device lama.

### 9.10 Flutter UI Performance Rules

- Preview via **native `Texture` widget** (mobile & desktop), bukan `Image.memory()`.
- Telemetry via EventChannel, bukan polling MethodChannel.
- UI di-throttle: status max 4–5 Hz; preview hanya saat window visible.
- Gunakan: `const` widgets, small widget trees, `RepaintBoundary` pada preview overlay, lazy list, cached icons, native texture.
- Hindari: backdrop blur, massive shadows, continuous animations, animated gradients, full-screen shader effects, large animated SVG, real-time charts (default mode).
- Stats chart: update 2 Hz, history 60 samples (cukup untuk grafik 30 detik).

### 9.11 Flutter Rebuild Rules

Dilarang `setState(() {})` 30 kali/detik. Telemetry: 2 Hz normal, 5 Hz diagnostics. Gunakan state granular — setiap data (connection status, quality, battery, latency) hanya me-rebuild widget yang membutuhkannya.

### 9.12 Preview Frame Rate

```text
stream = 30 FPS
tidak berarti UI harus render preview 30 FPS

Low-end:
  stream          = 30
  phone preview   = 15
  desktop preview = 15
  virtual camera  = 30
```

### 9.13 Startup Performance

```text
app open → UI visible <2 detik (low-end target)
```

Lazy initialization: Flutter UI → device selected → initialize camera. Native camera tidak perlu langsung startup.

### 9.14 Adaptive Network Quality

```text
packet loss 1%  → normal
packet loss 4%  → turunkan bitrate
packet loss 8%  → 720p → 480p
packet loss severe → transport failover
```

---

## 10. Audio

### 10.1 Audio Pipeline

```text
Android AudioRecord → Opus → Veyra transport → Windows decoder
```

Recommended: **48 kHz, mono, 24–64 kbps, Opus**.

### 10.2 Roadmap Virtual Microphone

| Release | Scope |
|---|---|
| V1.0 | Video webcam |
| V1.1 | Audio transport |
| V1.2 | Virtual microphone |

Virtual microphone jauh lebih kompleks daripada virtual camera — jangan biarkan menahan release V1.

---

## 11. Security & Pairing

### 11.1 Pairing Flow

- PC menampilkan PIN 6 digit (atau QR).
- HP memasukkan PIN.
- Kedua perangkat exchange public key (X25519/Ed25519).
- Setelah paired, device disimpan sebagai trusted identity.
- Session wajib: **authenticated, encrypted, replay protected**.
- Jangan menciptakan cipher sendiri — gunakan library crypto teruji.

### 11.2 Transport Security

| Transport | Security |
|---|---|
| Wi-Fi / Wi-Fi Direct | DTLS-SRTP (atau session encryption layer) |
| USB | data tunnel + session token |
| Bluetooth | Bluetooth secure pairing + application layer encryption |

### 11.3 No Open HTTP Server

- Tidak ada endpoint terbuka tanpa pairing.
- Discovery via mDNS hanya berisi device name + IP, bukan stream.

### 11.4 Pairing UX

```text
VeyraCam                        VeyraLink
      │ discovery
      ▼
"Pair with ED-PC?"
[ Cancel ] [ Pair ]
```

Subsequent: trusted device → automatic reconnect.

---

## 12. UI/UX

### 12.1 VeyraCam Mobile

```
┌──────────────────────────────────┐
│ VeyraCam                    ● LIVE│
│                                  │
│          CAMERA PREVIEW          │
│                                  │
├──────────────────────────────────┤
│ Connected to                     │
│  EDELINE-PC                      │
│                                  │
│ Connection                       │
│  [ Auto ]  Wi-Fi  USB  BT        │
│                                  │
│  720p • 30 FPS • 24 ms           │
│                                  │
│ [ Flip ] [ Flash ] [ Focus ]     │
│                                  │
│           [ STOP ]               │
└──────────────────────────────────┘
```

Automatic connection UX — user normal hanya melakukan:

```text
Open VeyraCam → PC detected → Tap Connect (atau Auto Connect)
```

Teknologi transport tidak perlu dipahami user.

### 12.2 VeyraLink Desktop

```
┌───────────────────────────────────────────────┐
│ VeyraLink                                     │
├───────────────────┬───────────────────────────┤
│ DEVICES           │                           │
│                   │        CAMERA             │
│ ● Galaxy S24      │        PREVIEW            │
│   192.168.1.64    │                           │
│                   │                           │
├───────────────────┼───────────────────────────┤
│ Camera            │ Video                     │
│ Rear Camera       │ 1280×720                  │
│                   │ 30 FPS                    │
│ Zoom       1.0x   │ H264                       │
│ Exposure     0    │ 2.1 Mbps                  │
│ Focus      Auto   │ Latency 42ms              │
│                   │                           │
│ [Disconnect]      │ Veyra Camera: ON          │
└───────────────────┴───────────────────────────┘
```

### 12.3 Desktop Device Page

```text
VeyraCam — Galaxy A52
CONNECTED
Transport   USB • 38 ms
Camera      Rear
Quality     720p • 30 FPS
Bitrate     2.1 Mbps
[ Camera Controls ]
```

### 12.4 Flutter State Model

```text
AppState
DeviceState
SessionState
CameraState
TransportState
TelemetryState
```

Jangan taruh seluruh aplikasi pada satu global object yang menyebabkan rebuild besar.

Flutter architecture:

```text
Presentation → Application → Native API
CameraPage → CameraController → VeyraNativeApi → FFI
```

Bukan: `UI → Socket → Encoder`.

API example:

```dart
await Veyra.session.start(
  profile: StreamProfile.balanced,
  transport: TransportMode.auto,
);
```

Di baliknya: `Dart → FFI → native session manager → camera/network/media`.

---

## 13. Repository Structure

```
veyra/
│
├── apps/
│   ├── veyracam/                  # Flutter Android app (VeyraCam)
│   │   ├── android/
│   │   └── lib/
│   │
│   └── veyralink/                 # Flutter Windows app (VeyraLink UI)
│       ├── windows/
│       └── lib/
│
├── packages/
│   ├── veyra_ui/                  # Shared Dart UI widgets
│   ├── veyra_models/              # Dart models
│   ├── veyra_protocol_dart/       # JSON protocol models / client helpers
│   └── veyra_native/              # FFI bindings ke native core
│
├── native/
│   ├── core/                      # VeyraCore — shared native media engine
│   │   ├── protocol/
│   │   ├── session/
│   │   ├── transport/
│   │   │   ├── udp/
│   │   │   ├── tcp/
│   │   │   ├── usb/
│   │   │   └── bluetooth/
│   │   ├── media/
│   │   │   ├── h264/
│   │   │   ├── opus/
│   │   │   └── jitter/
│   │   ├── crypto/
│   │   ├── telemetry/
│   │   └── platform/
│   │
│   ├── android/                   # Android native engine (Kotlin/C++)
│   │   ├── camera/
│   │   │   ├── CameraController
│   │   │   ├── CameraSession
│   │   │   └── CameraCapabilities
│   │   ├── codec/
│   │   │   └── H264Encoder
│   │   ├── audio/
│   │   ├── service/
│   │   │   └── VeyraStreamingService
│   │   ├── wifi/
│   │   ├── wifidirect/
│   │   ├── bluetooth/
│   │   └── native_bridge/
│   │
│   └── windows/                   # Windows native (C++)
│       ├── core/
│       ├── network/
│       ├── bluetooth/
│       ├── usb/
│       ├── decoder/
│       ├── renderer/
│       ├── virtual_camera/
│       │   ├── mf/                # Media Foundation (Win11)
│       │   └── dshow/             # DirectShow compatibility (Win10)
│       ├── ipc/
│       └── service/
│
├── tool/                          # build & codegen tools
│   ├── generate_protocol.dart
│   └── build_android_engine.gradle
│
└── docs/
    ├── protocol.md
    ├── performance.md
    └── testing.md
```

---

## 14. Development Order & Phases

### 14.1 Development Order (Wajib Urut)

```text
Stage 1:  Native Android: Camera2 → MediaCodec → H264 elementary stream
Stage 2:  Native desktop: H264 → Media Foundation → display
Stage 3:  Wi-Fi: phone → UDP → PC
Stage 4:  Flutter UI: VeyraCam + VeyraLink
Stage 5:  Windows virtual camera (Veyra Camera)
Stage 6:  USB
Stage 7:  Bluetooth
Stage 8:  Wi-Fi Direct
Stage 9:  Automatic transport switching
Stage 10: Audio
```

Urutan ini penting — **jangan mulai dari UI kemudian baru mencoba memasukkan media engine**.

### 14.2 Phases

| Phase | Scope |
|---|---|
| 1 — MVP Streaming | Flutter Android sender + Flutter Windows receiver; Wi-Fi LAN; preview dua sisi; target latency <100 ms |
| 2 — Virtual Camera | Native C++ `MFCreateVirtualCamera` (Win11); fallback Win10 DirectShow; Zoom/Discord mendeteksi Veyra Camera |
| 3 — Transport Tambahan | USB via ADB forward; Wi-Fi Direct; Bluetooth Classic |
| 4 — Seamless Switching | Transport Manager; auto fallback; keep virtual camera alive |
| 5 — Remote Control | Control messages; zoom, focus, exposure, torch, camera switch |
| 6 — Polish | Pairing security; background service; battery optimization; low-performance mode |

---

## 15. Testing & Acceptance

### 15.1 Android Test Matrix

| Tier | Device Class | RAM | Target |
|---|---|---|---|
| Tier A — Very Low End | Android 7/8, 4-core low-end ARM, 2.4 GHz Wi-Fi | 2 GB | 480p24, 720p24 optional |
| Tier B — Old Midrange | Android 9–11 | 3–4 GB | 720p30, 1080p30 optional |
| Tier C — Modern | Android 11+ | 4 GB+ | 1080p30/60 |

### 15.2 PC Test Matrix

| PC Class | Windows | RAM | Target |
|---|---|---|---|
| PC-Low | Win10 1809, old dual-core/4-thread, iGPU | 4 GB | 720p30 (software decode ok) |
| PC-Mid | Win10/11, older quad-core | 8 GB | 1080p30, hardware decode |
| PC-New | Win11 | 16 GB+ | 1080p60+ |

### 15.3 Testing Metrics

Setiap kombinasi wajib mencatat:

```text
startup time, connection time, capture FPS, output FPS,
encode latency, network latency, decode latency, end-to-end latency,
packet loss, CPU, RAM, GPU, battery drain, thermal state,
dropped frames, reconnect time, handoff time
```

### 15.4 Long-Run Test

Wajib: **30 menit, 1 jam, 3 jam, 8 jam** — masalah webcam biasanya tidak muncul pada 30 detik pertama.

Cari: memory leak, thermal throttling, timestamp drift, audio/video drift, packet buffer growth, decoder stall, camera freeze.

### 15.5 Performance Acceptance Rule

Feature dianggap **gagal** meskipun "berfungsi" jika:

```text
latency terus meningkat
memory terus naik
CPU terlalu tinggi
thermal menyebabkan FPS collapse
```

Real-time software harus: **bounded memory, bounded queues, bounded latency**.

### 15.6 Production Performance Rules

```text
Rule 1:  No raw frame in Dart.
Rule 2:  No encoded video packet through Flutter MethodChannel.
Rule 3:  Hardware encode first.
Rule 4:  Hardware decode first.
Rule 5:  NV12 preferred end-to-end.
Rule 6:  No unnecessary encode → decode → encode.
Rule 7:  No unbounded queues.
Rule 8:  Drop stale frames.
Rule 9:  UI telemetry ≤5 Hz.
Rule 10: Preview is optional.
Rule 11: Flutter desktop UI may terminate while native core continues.
Rule 12: Transport switching must not recreate the Windows camera.
```

### 15.7 Logging

- Rolling log: `veyra.log`, `veyra.1.log`, `veyra.2.log`; max size dibatasi.
- Default level: INFO.
- Packet-level log hanya pada DEBUG mode.

---

## 16. Risiko & Mitigasi

| Risiko | Mitigasi |
|---|---|
| Virtual camera tidak berjalan di Windows 10 | Fallback DirectShow + OBS bridge (compatibility-only) |
| Bluetooth lambat | Batasi kualitas, adaptive bitrate, drop resolution daripada latency |
| HP lama thermal throttling | Thermal controller: auto turunkan resolusi/fps bertahap |
| WebRTC overhead untuk HP low-end | Custom native UDP/TCP + H264 untuk semua transport; WebRTC hanya prototype |
| Flutter overhead di PC lama | UI minimal, native core, tray mode, VeyraCore independent dari UI |
| Windows virtual camera API terbatas | Uji format NV12/YUY2, fallback MJPEG jika perlu |
| Perangkat tanpa hardware H.264 | Compatibility Mode (software encode), bukan target performance |
| Driver virtual camera (AVStream) | Tidak masuk MVP; hanya optional future |

---

## 17. V1 Scope, Tech Stack & Final Decision

### 17.1 V1 Product Scope

```text
VEYRA 1.0

Android 7+ / Windows 10/11

Transport:
✓ USB
✓ Wi-Fi LAN
✓ Wi-Fi Direct
✓ Bluetooth
✓ Auto transport
✓ Seamless handoff

Video:
✓ H264 hardware
✓ 240p, 360p, 480p, 720p, 1080p
✓ 15/24/30 FPS
✓ 60 FPS capable devices

Camera:
✓ front/rear
✓ focus, exposure, zoom, torch

PC:
✓ Veyra Camera (single identity)
✓ Windows 10 compatibility (DirectShow)
✓ Windows 11 native virtual camera (MF)
✓ Tray mode
✓ Auto discovery
✓ Native hardware decoding
✓ Remote controls
✓ Diagnostics

Performance:
✓ Bounded queues
✓ Adaptive quality
✓ Hardware encode/decode
✓ Native zero/low-copy pipeline
✓ Thermal adaptation
✓ Transport fallback
```

### 17.2 Recommended Technology Stack

**VeyraCam:**

```text
UI                Flutter / Dart
Camera            Android Camera2
Encoder           MediaCodec H264 (hardware)
Audio             AudioRecord
Background        Foreground Service
Native transport  C++
Flutter bridge    dart:ffi / JNI control layer
Bluetooth         Android RFCOMM (BluetoothSocket)
Wi-Fi Direct      Android WifiP2pManager
```

**VeyraLink:**

```text
UI                Flutter Windows
Core              C++20
Networking        WinSock / IOCP
Bluetooth         Windows native RFCOMM
USB               native bridge
Decoder           Media Foundation
GPU               D3D11
Virtual Camera    Win11: IMFVirtualCamera / MFCreateVirtualCamera
                  Win10: DirectShow compatibility backend
Flutter/native    dart:ffi
UI preview        Flutter Texture / native texture bridge
```

### 17.3 Yang TIDAK Dipakai (Production Hot Path)

```text
flutter camera image stream
Dart JPEG encoding
Dart H264 encoding
Dart UDP packet loops
Image.memory()
WebSocket for video
JSON video metadata per frame
WebRTC DataChannel for raw video
software FFmpeg by default
OBS as runtime dependency
```

OBS boleh dipakai untuk development verification, tetapi Veyra final harus berdiri sendiri.

### 17.4 Final Architecture Decision

Veyra **bukan** Flutter application dengan beberapa native plugin. Secara arsitektural ia adalah:

> **Native realtime media system + Flutter user interface**

```text
Mobile:
Flutter → control → Native Android Media Engine

Desktop:
Flutter (VeyraLink.exe) → control → Native VeyraCore.exe
```

Media path:

```text
native → native → native → native
```

Dengan desain ini, memakai Flutter tidak berarti mengorbankan performa. Kita mendapatkan:

```text
satu UI framework
+ native multimedia performance
+ low latency
+ low memory
+ old-device compatibility
+ clean codebase
```

---

## Lampiran: Referensi Teknis

- Flutter supported deployment platforms: https://docs.flutter.dev/reference/supported-platforms
- Dart FFI (C interop): https://dart.dev/interop/c-interop
- Android MediaCodec (Surface input, zero-copy): https://developer.android.com/reference/android/media/MediaCodec
- Android 14 foreground service types: https://developer.android.com/about/versions/14/changes/fgs-types-required
- Android media codecs (hardware acceleration): https://developer.android.com/media/optimize/performance/codec
- Android Wi-Fi Direct (WifiP2pManager): https://developer.android.com/develop/connectivity/wifi/wifip2p
- Android BluetoothSocket (RFCOMM): https://developer.android.com/reference/android/bluetooth/BluetoothSocket
- MFCreateVirtualCamera (Win11 22000+): https://learn.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/nf-mfvirtualcamera-mfcreatevirtualcamera
- OBS DirectShow virtual camera (compatibility reference): https://github.com/obsproject/obs-studio/blob/master/plugins/win-dshow/dshow-plugin.cpp
- DirectShow samples: https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/DirectShow/directshow-samples.md
- AVStream camera driver sample (optional future): https://github.com/microsoft/Windows-driver-samples/tree/main/avstream/avscamera

---

## Kesimpulan

- **Desktop tetap Flutter**, tetapi VeyraLink hanyalah presentation/control layer; `VeyraCore.exe` native C++ adalah mesin utamanya.
- **Veyra Camera** adalah komponen native (Media Foundation/DirectShow), bukan Flutter.
- **Frame path native penuh** → HP lama dan PC lama tetap high-performance.
- **Multi-transport** Wi-Fi LAN, Wi-Fi Direct, USB, Bluetooth dengan seamless handoff.
- **Auto switching** menjaga Zoom/Discord/Meet tetap melihat kamera yang sama.

Dengan spesifikasi ini, VeyraCam bukan sekadar clone DroidCam, melainkan sistem kamera lintas-koneksi yang dirancang untuk perangkat tua sekalipun.