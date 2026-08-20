# Veyra System — Security Review Report

**Document Version:** 1.0
**Tanggal Review:** 2026-08-20
**Scope:** Seluruh kodebase VeyraCam/VeyraLink (apps, packages, native, CI)
**Metode:** Manual code review terbimbing knowledge graph (graphify), verifikasi dependensi via osv-scanner v2.5.1 + OSV API
**Referensi Spec:** `spec/specs/000-veyra-system-spec.md` §10–11 (Security & Pairing)

---

## Ringkasan Eksekutif

| Severity | Jumlah |
|---|---|
| CRITICAL | 3 |
| HIGH | 4 |
| MEDIUM | 5 |
| LOW | 4 |

**Kesimpulan utama:** Risiko keamanan TIDAK berasal dari dependency pihak ketiga (semua bersih, lihat §Dependency Audit), melainkan dari **kode in-repo**: kriptografi yang sepenuhnya rusak, ketiadaan pairing/autentikasi pada seluruh jalur koneksi, dan streaming video/audio yang dikirim plaintext ke klien pertama yang tersambung. Status keamanan saat ini **tidak layak untuk penggunaan di luar jaringan pribadi yang terisolasi penuh**, dan melanggar spec sendiri (§11: *"Session wajib authenticated, encrypted, replay protected"*).

---

## CRITICAL

### C-1. Implementasi kriptografi sepenuhnya rusak (bukan X25519/ChaCha20-Poly1305)

**Lokasi:** `native/core/src/crypto.cpp:125-157`, `native/core/src/crypto.cpp:86-113`

Bukti kode:

```cpp
// crypto.cpp:138 — "public key derivation"
pair.publicKey[i] = pair.privateKey[i] ^ 0xA5;

// crypto.cpp:153 — "shared key computation"
sessionKey_[i] = peerPublicKey[i] ^ myPrivateKey[i] ^ (i < salt.size() ? salt[i] : 0x5C);
```

Dampak:
- **Private key dapat direkonstruksi dari public key**: `privateKey = publicKey ^ 0xA5`. Public key secara definisi diketahui publik → kunci privat bocor.
- **"Shared key" dapat dihitung oleh pengintip pasif**: karena `pubA = privA ^ 0xA5` dan `pubB = privB ^ 0xA5`, maka `pubA ^ pubB = privA ^ privB = sessionKey` (salt bersifat konstanta publik). Pengintip yang hanya melihat kedua public key di jaringan langsung memegang session key.
- **MAC bukan Poly1305** melainkan SipHash-like improvisasi (8-byte state, nonce hanya mempengaruhi `v0`), dengan `reinterpret_cast<const uint64_t*>(key)` (unaligned access + endian-dependence). Tag 128-bit yang dihasilkan tidak memiliki dasar keamanan formal.
- `std::random_device` untuk private key — tidak dijamin cryptographically secure pada semua platform.

Melanggar spec §11.1: *"Jangan menciptakan cipher sendiri — gunakan library crypto teruji."*

**Remediasi:** Ganti seluruh modul dengan library teruji. Rekomendasi: **libsodium** (X25519 + ChaCha20-Poly1305 AEAD + Ed25519 signature, API minimal, mudah diintegrasikan ke C++ native core dan di-bind ke Kotlin via JNI; alternatif: BoringSSL/mbedTLS). Lihat roadmap §Remediation Priority.

### C-2. Tidak ada pairing/autentikasi — klien pertama yang tersambung mengendalikan kamera dan menerima stream

**Lokasi:** `apps/veyracam/android/app/src/main/kotlin/com/veyra/cam/transport/UdpTransport.kt:33-76`, `VeyraStreamingService.kt:182-197`

Bukti alur:
1. `startStreaming()` membuka TCP control server di port **5150** pada semua interface — tanpa PIN, tanpa token, tanpa daftar trusted device.
2. Klien pertama yang `connect()` diterima langsung: `onClientConnected(clientIp, 5151)` → `nativeConfigureUdpDestination(handle, clientIp, 5151)` (`VeyraStreamingService.kt:190`).
3. Dari saat itu **seluruh video + audio HP dikirim ke IP penghubung** dalam plaintext H.264/Opus.
4. `handleControlCommand()` (`VeyraStreamingService.kt:212-252`) mengeksekusi perintah jaringan tanpa verifikasi: `setZoom`, `setFocus`, `setExposure`, `startStream`, `requestIdr`, `setBitrate`.

Dampak: siapa pun di jaringan Wi-Fi yang sama (kafe, kantor, apartemen) dapat:
- Menyambung lebih dulu dan **menerima livestream kamera + mikrofon korban**.
- Mengendalikan kamera korban (zoom/focus/exposure).
- Ini pelanggaran privasi kelas katastrofik untuk aplikasi kamera.

Grep seluruh repo mengonfirmasi **tidak ada implementasi pairing**: `isPaired` hanya field model Dart (`packages/veyra_models/lib/device.dart:84`) yang tidak pernah dipakai sebagai gate; UI "Pairing..." (`veyra_ui/lib/status_badge.dart:62`) hanya label status kosong. Spec §11 (PIN 6 digit, key exchange, trusted device) belum diimplementasikan sama sekali.

**Remediasi:** Implementasikan pairing sesuai spec §11 sebelum rilip publik apa pun: PIN 6 digit ditampilkan di PC → dikonfirmasi di HP → X25519 ephemeral key exchange (libsodium) → token trusted device tersimpan enkripsi keystore/DPAPI → semua koneksi ditolak tanpa token valid.

### C-3. Media path dikirim plaintext; enkripsi tidak pernah aktif meski flag nyala

**Lokasi:** `native/core/src/packetizer.cpp:10-80`, `native/core/src/session.cpp:42-46`

Bukti:
- `PacketizeFrame()`/`PacketizeAudio()` **tidak pernah memanggil** `SessionCrypto::Encrypt()` — seluruh frame H.264/Opus masuk `sendto()` plaintext.
- Saat `config.enableEncryption == true`, session melakukan:

```cpp
// session.cpp:44-45
auto keyPair = SessionCrypto::GenerateKeyPair();
crypto_->SetSessionKey(keyPair.publicKey.data());  // session key = PUBLIC KEY sendiri
```

  Tidak ada pertukaran kunci dengan peer sama sekali; "session key" adalah public key sendiri (yang by design harus diketahui pihak lain). Enkripsi justru memberikan kepalsuan rasa aman.
- Control command (`session.cpp:79-152`) juga dikirim tanpa enkripsi/MAC.
- `FrameReassembler::PushPacket()` (`packetizer.cpp:88+`) tidak memverifikasi MAC apa pun — **packet injection diterima**.

Dampak: pengintip jaringan dapat merekam video/audio; penyerang dapat meng-inject frame palsu (video palsu tampil di Zoom/OBS di sisi PC).

**Remediasi:** AEAD (ChaCha20-Poly1305) per-packet via libsodium: encrypt payload + header terautentikasi; sequence number sebagai nonce (64-bit monotonic per session); drop packet yang gagal verifikasi; wajib-on untuk semua transport kecuali USB tunnel yang terautentikasi fisik.

---

## HIGH

### H-1. `android:usesCleartextTraffic="true"` global

**Lokasi:** `apps/veyracam/android/app/src/main/AndroidManifest.xml` (tag `<application>`)

Melanggar spec §11.3 dan membuka jalan downgrade/intersepsi cleartext pada semua HTTP stack Android. Saat ini tidak ada traffic HTTP di kode, jadi flag ini tidak perlu.

**Remediasi:** Hapus flag; jika mode USB-ADB v1 butuh cleartext ke `127.0.0.1`, gunakan `networkSecurityConfig` terbatas `localhost` saja untuk build debug.

### H-2. Release build di-sign dengan debug key

**Lokasi:** `apps/veyracam/android/app/build.gradle` (buildType `release` → `signingConfig signingConfigs.debug`)

APK release yang di-sign debug key: tidak ada jaminan autentisitas, trivially repackagable, tidak bisa di-update ke Play, dan CI (`release.yml`) mempublikasikannya sebagai release publik.

**Remediasi:** Keystore release via secret CI (base64-encoded keystore + env `KEYSTORE_PASSWORD`), atau sign-with-play. Minimal: generate keystore khusus dan simpan aman.

### H-3. UDP receiver Windows bind `INADDR_ANY` tanpa filter sumber

**Lokasi:** `native/windows/network/iocp_udp_server.cpp:42` (`sin_addr.s_addr = INADDR_ANY`), callback `:86-88` tidak memeriksa `clientAddr`

Packet UDP dari **alamat IP mana pun** diproses dan diteruskan ke decoder → dikombinasikan C-3 (tanpa MAC), penyerang remote dapat meng-inject frame ke virtual camera yang sedang dipakai Zoom/Meet.

**Remediasi:** Setelah handshake, filter paket berdasarkan alamat peer tersambung + verifikasi MAC per-packet (C-3). Pertimbangkan bind ke alamat interface spesifik saat P2P direct.

### H-4. Bluetooth RFCOMM server menerima koneksi tanpa pairing aplikasi

**Lokasi:** `apps/veyracam/android/app/src/main/kotlin/com/veyra/cam/transport/BluetoothTransport.kt:44-54`

`listenUsingRfcommWithServiceRecord()` memakai secure RFCOMM (terbantu oleh pairing OS-level), tetapi `accept()` menerima perangkat bonded apa pun tanpa verifikasi app-level, dan `onDataReceived` saat ini kosong (`VeyraStreamingService.kt:200`) — jalur ini akan menjadi attack surface begitu diaktifkan.

**Remediasi:** Sebelum aktif: enforce bonded device + allowlist perangkat trusted (hasil pairing C-2) + challenge-response token sesi.

---

## MEDIUM

### M-1. Dependensi `androidx.camera:camera-core:1.3.1` dideklarasikan tapi tidak dipakai

**Lokasi:** `apps/veyracam/android/app/build.gradle` (dependencies)

Kode memakai Camera2 murni (`Camera2Controller.kt`); CameraX tidak pernah di-import. Ini menambah attack surface & ukuran APK tanpa manfaat, dan melanggar spec §5.2 ("bukan CameraX").

**Remediasi:** Hapus baris dependensi.

### M-2. CI: actions di-pin by mutable tag + `permissions: contents: write` luas

**Lokasi:** `.github/workflows/release.yml:1-13` dan seluruh `uses:` (`actions/checkout@v4`, `subosito/flutter-action@v2`, `nttld/setup-ndk@v1`, `softprops/action-gh-release@v2`, `actions/upload-artifact@v4`, `actions/setup-java@v4`)

Mutable tag dapat dipindahkan ke commit berbahaya (supply-chain). `contents: write` di level workflow memperluas blast radius.

**Remediasi:** Pin setiap action by full commit SHA; pindahkan `permissions` ke level job (`contents: write` hanya untuk job publish); tambahkan `persist-credentials: false` pada checkout job build.

### M-3. Gradle wrapper tanpa checksum verifikasi

**Lokasi:** `apps/veyracam/android/gradle/wrapper/gradle-wrapper.properties`

Tidak ada `distributionSha256Sum` — distribusi Gradle 8.14 diverifikasi hanya via TLS. Juga tidak ada `gradle.lockfile` (dependency locking off) dan versi NDK berbeda antara lokal (28.2) dan CI (r25c) → build tidak deterministik.

**Remediasi:** Tambahkan `distributionSha256Sum`; aktifkan `dependencyLocking`; samakan `ndkVersion` lokal/CI.

### M-4. `sessionId` dari `System.currentTimeMillis()` + `std::random_device`

**Lokasi:** `VeyraStreamingService.kt:140`, `native/core/src/session.cpp:8-9`

Session ID predictable (waktu) — dapat dipakai untuk spoof/predict terhadap logic yang menyaring berdasarkan sessionId (saat ini `DeserializeHeader` bahkan tidak memvalidasi sessionId, lihat M-5).

**Remediasi:** Generate 128-bit random via CSPRNG setelah pairing; validasi sessionId di sisi penerima.

### M-5. `DeserializeHeader` tidak memvalidasi `sessionId`/`streamId`

**Lokasi:** `native/core/src/protocol.cpp:14-23`

Hanya magic + version yang dicek. Paket dari sesi lain ( atau disusun attacker ) diterima begitu saja oleh `FrameReassembler`.

**Remediasi:** Validasi `sessionId == sessionId_` pada reassembler; tolak `streamId` tak dikenal.

### M-6. Informasi perangkat bocor pre-auth

**Lokasi:** `VeyraStreamingService.kt:215` — `Build.MODEL` + `Build.SERIAL` dikirim sebagai respons `HELLO` ke **siapa pun yang tersambung**.

`Build.SERIAL` deprecated & tergantung API level (biasanya `"unknown"`), tapi mengirim fingerprint perangkat sebelum autentikasi melanggar prinsip minimal disclosure.

**Remediasi:** Kirim capabilities hanya setelah pairing (C-2); hapus `Build.SERIAL`.

---

## LOW

### L-1. `VeyraStreamingService.instance` static mutable
Pola singleton static (`VeyraStreamingService.kt:36`) rawan leak antar-lifecycle; batasi referensi, null-kan di `onDestroy` (sudah ada) dan hindari memegang referensi dari proses lain.

### L-2. TCP control buffer tanpa batas akumulasi
`TcpControlClient::ReadLoop` (`tcp_control_server.cpp:88-114`) mengakumulasi string tanpa batas sampai `\n` ditemukan — peer malicious dapat membanjiri memori. Batasi max line length (mis. 64 KB).

### L-3. IOCP dibuat tapi tidak digunakan
`iocp_udp_server.cpp:52` membuat IOCP handle namun loop memakai `recvfrom` blocking — dead code yang menyesatkan (nama menjanjikan async). Gunakan `WSARecvFrom` overlapped atau hapus.

### L-4. `nativeGetTelemetryJson` mengembalikan heap string yang harus di-free manual
`veyra_ffi_export.cpp:51-58` — pola rawan leak/UMI dari sisi Dart; pertimbangkan buffer out-param dengan panjang yang dikembalikan.

---

## Dependency Audit (jawaban pertanyaan "virus / vulnerability?")

### Metode
- **osv-scanner v2.5.1** (Google) pada seluruh 6 `pubspec.lock` — hasil: **0 vulnerability** (235 package-instance ter-scan).
- **OSV API** untuk 5 artefak Gradle/Maven yang di-pin exact — hasil: **0 CVE**.

### Runtime dependencies (yang masuk binary release)

| Ecosystem | Package | Versi (lock) | Sumber | CVE |
|---|---|---|---|---|
| pub | `meta` | 1.18.3–1.19.0 | dart.dev (first-party) | 0 |
| pub | `ffi` | 2.2.0 | dart.dev (first-party) | 0 |
| pub | path deps (`veyra_*`) | 1.0.0 | in-repo | — |
| Maven | `androidx.appcompat` | 1.6.1 | Google Maven | 0 |
| Maven | `androidx.core:core-ktx` | 1.12.0 | Google Maven | 0 |
| Maven | `androidx.camera:camera-core` | 1.3.1 | Google Maven (tidak terpakai — M-1) | 0 |
| Maven | `kotlin-stdlib-jdk8` | 1.9.22 | JetBrains | 0 |
| Maven | `kotlinx-coroutines-android` | 1.7.3 | JetBrains | 0 |
| C++ | (tidak ada dependensi eksternal) | — | FetchContent/vcpkg/conan: **tidak ada** | — |

Dev-only (`test`, `lints`, `flutter_test`, dsb.) tidak masuk binary release dan hasil scan juga bersih.

### Apakah "pasti tidak ada virus"?

Jawaban jujur:
1. **Tidak ada jaminan absolut untuk software apa pun.** Yang bisa dijamin: semua dependency di-resolve **hanya** dari `pub.dev` + `google()` + `mavenCentral()` (tidak ada custom/mirror repository), semua versi ter-pin exact di lock, tidak ada dependency eksotis/unknown-maintainer, tidak ada post-install script, dan 0 vulnerability ditemukan di OSV hari ini.
2. **Risiko supply-chain yang tersisa ada di CI build** (M-2: mutable action tags) — ini vektor yang paling realistis untuk menyusupkan kode berbahaya ke artifact release, jauh lebih realistis daripada package pub.
3. **Risiko terbesar saat ini adalah kode first-party** (C-1..C-3), bukan pihak ketiga.
4. Verifikasi berkelanjutan ditambahkan via `.github/workflows/security.yml` (osv-scanner pada setiap push + dependency review).

---

## Remediation Priority (roadmap)

| Prioritas | Item | Effort est. |
|---|---|---|
| P0 | C-2: pairing + auth gate pada TCP 5150 (tolak koneksi tanpa token) | M |
| P0 | C-3: AEAD per-packet di packetizer/reassembler | M |
| P0 | C-1: replace crypto module → **libsodium** (X25519 kx, ChaCha20-Poly1305 AEAD, Ed25519 device identity) | M |
| P1 | H-1 cleartext flag; H-2 release signing; H-3 source filtering; H-4 BT allowlist | S |
| P1 | M-2 pin CI actions by SHA + per-job permissions | S |
| P2 | M-1 hapus CameraX; M-3 wrapper checksum + locking; M-4/M-5 session validation; M-6 info disclosure | S |
| P3 | seluruh LOW | S |

Rekomendasi library crypto (detail, sesuai keputusan "TBD di laporan"):

- **libsodium** (rekomendasi utama) — lisensi ISC, audit publik, API sulit salah-pakai, binding JNI Kotlin resmi tersedia, kompatibel Windows/Android/NDK via CMake `add_subdirectory`/vcpkg. Menyediakan semua primitif yang dibutuhkan spec: `crypto_kx` (X25519), `crypto_aead_xchacha20poly1305` , `crypto_sign_ed25519` (device identity), CSPRNG `randombytes_buf`.
- Alternatif: **BoringSSL** (lebih berat, build rumit), **mbedTLS** (BSD, polish bagus tapi AEAD & kx lebih verbose).

---

## Lampiran: Metode & Bukti

- Knowledge graph `graphify-out/graph.json` (1.454 node, 2.129 edge) dipakai untuk memetakan attack surface: query "pairing/session authentication", "encryption applied where", "network listeners & JNI exposed functions" mengarahkan ke file yang direview.
- Semua temuan diverifikasi dengan pembacaan penuh file terkait (crypto, session, packetizer, protocol, transport_manager, iocp/tcp server, JNI bridge, FFI export, dshow_registration, manifest, gradle, release.yml).
- `dshow_registration.cpp` adalah stub no-op (tidak ada risiko elevasi saat ini); `veyra_ffi_export` dievaluasi — shared texture handle bersifat lokal-machine, risiko rendah.
- osv-scanner dipanggil per lockfile: `--lockfile=<path>` untuk 6 pubspec.lock; OSV REST API untuk artefak Maven.

---

## Status Remediasi (v1.0.1)

Semua temuan di atas telah diremediasi pada rilis v1.0.1:

| Temuan | Status | Implementasi |
|---|---|---|
| C-1 (crypto rusak) | ✅ FIXED | Modul crypto ditulis ulang di atas **mbedTLS 3.6.2 LTS** (bukan libsodium): X25519 (`mbedtls_ecdh`), HKDF-SHA256 terikat transcript (canonical lexicographic pubkey, label `VeyraV1|s2c|c2s|ns2c|nc2s|`), ChaCha20-Poly1305 AEAD, CSPRNG CTR-DRBG. **Deviasi dari rekomendasi libsodium**: libsodium upstream tidak menyediakan CMakeLists (hanya autotools/build.zig) sehingga FetchContent gagal untuk Android NDK/MSVC/GCC; mbedTLS dipilih karena first-class CMake + audited. Semua test core & E2E PASSED. |
| C-2 (tanpa auth/pairing) | ✅ FIXED | Pairing berbasis PIN 6-digit out-of-band: `pairing_challenge` → `pairing_response(pin, client_pubkey)` → `pairing_ok(token)` / `pairing_error`; verifikasi PIN constant-time (`MessageDigest.isEqual`), 5 percobaan gagal → lockout 30 detik; token 32-byte CSPRNG wajib pada setiap perintah kontrol pasca-auth; keypair ephemeral baru per sesi; media UDP hanya dikirim setelah paired. Server: `PairingManager.kt` + `UdpTransport.kt` + JNI `nativeBeginPairing/nativeCompletePairing`. Client: state machine di `service_manager.cpp` + FFI `veyra_core_connect_device(host, port, pin, status_cb)` + UI PIN di VeyraLink & VeyraCam. |
| C-3 (media plaintext) | ✅ FIXED | AEAD per-paket: nonce = salt-arah 8B + sequence BE 4B, AAD = header paket, tag 16B; `FLAG_ENCRYPTED(0x04)`; decrypt-verify sebelum fragment assembly; ReplayFilter window 64 (commit sequence hanya setelah autentikasi sukses). |
| H-1 (cleartext flag) | ✅ FIXED | `android:usesCleartextTraffic="true"` dihapus dari manifest. |
| H-2 (signing debug) | ✅ FIXED | Keystore release dari secrets CI (`ANDROID_KEYSTORE_BASE64/PASSWORD/ALIAS/KEY_PASSWORD`), di-decode saat build, `*.jks` di-gitignore; fallback debug hanya jika secrets kosong. |
| H-3 (source UDP) | ✅ FIXED | `IocpUdpServer::SetExpectedPeer(ip)` — paket dari sumber selain peer ter-pairing di-drop. |
| H-4 (BT tanpa allowlist) | ✅ FIXED | Hanya device **bonded** diterima; opsional allowlist alamat (dimuat setelah pairing C-2). |
| M-1 (CameraX) | ✅ FIXED | Dependency `androidx.camera:camera-core` dihapus (pakai Camera2 langsung). |
| M-2 (CI actions) | ✅ FIXED | Semua actions di-pin by commit SHA (checkout, setup-java, setup-ndk, flutter-action, msvc-dev-cmd, upload/download-artifact, action-gh-release), `permissions` per-job (contents: write hanya di publish), `persist-credentials: false`, NDK diseragamkan ke r28.2.13676358, nama artefak dinamis dari `${{ github.ref_name }}`. |
| M-3 (wrapper checksum) | ✅ FIXED | `distributionSha256Sum` ditambahkan (gradle-8.14-all). |
| M-4 (sessionId predictable) | ✅ FIXED | `nativeCreateSession` mengabaikan input dan memakai `SessionCrypto::RandomId()` (CTR-DRBG); `session.cpp` juga memakai CSPRNG. |
| M-5 (validasi session/stream) | ✅ FIXED | `streamId` hanya 0/1; `FrameReassembler` adopt-and-lock session id; paket plaintext ditolak saat key armed (downgrade protection). |
| M-6 (info disclosure) | ✅ FIXED | HELLO/capabilities hanya post-auth; `Build.SERIAL` dihapus. |
| L-2 (line cap TCP) | ✅ FIXED | Cap 64 KB per line kontrol (server Kotlin & client C++). |
| L-3 (IOCP mati) | ✅ FIXED | IOCP completion port yang tidak terpakai dihapus dari `iocp_udp_server.cpp`. |
| L-4 (telemetry buffer) | ✅ FIXED | Buffer statis thread-local 4 KB, `veyra_core_free_string` menjadi no-op (ABI compatible). |

### Verifikasi

- `native/core` test suite (Release): TestProtocol, TestPacketizerAndReassembler, TestRingBuffer, TestJitterBuffer, TestCrypto (AAD-tamper, wrong-seq, corrupted tag), TestSecurePacketPath (AEAD roundtrip, replay, forged-packet drop, session lock, downgrade reject), TestTelemetry, TestTransportManager — **semua PASSED**.
- `test/e2e_streaming_test.cpp` (E2E full-system): 60 frame 720p reassembled, jitter buffer, telemetry, AEAD roundtrip — **PASSED**.
- `apps/veyracam/android`: `./gradlew :app:assembleDebug` — **BUILD SUCCESSFUL** (Kotlin + JNI C++ + mbedTLS via NDK r28).
- `flutter analyze` — **No issues** di veyra_models, veyra_protocol_dart, veyra_ui, veyra_native, veyracam, veyralink.
- Deadlock telemetry (nested lock `GetCurrentStatsPayload` → `GetAverageLatencyBreakdown`) ditemukan & diperbaiki (helper `ComputeLatencyBreakdownLocked`).

### Residual risk (didokumentasikan)

- PIN dikirim sekali per sesi di atas TCP kontrol plaintext LAN; mitigasi: keypair ephemeral baru per koneksi (sniffed PIN tidak berguna untuk sesi berikutnya), token sesi, lockout. PAKE penuh (SPAKE2/SRP) dicatat sebagai pekerjaan masa depan.
- TCP kontrol tidak dienkripsi (hanya media UDP); sesi AEAD media tetap aman karena key binding independen dari kontrol.
