# PCMFlowUDP

> English: [README.md](README.md)

[PCMFlow](https://github.com/tanakamasayuki/PCMFlow) 用の **UDP トランスポートアダプタ**。Arduino クラスのデバイスと PC 間で PCM 音声をローカルネットワーク経由でストリーミングします。**素の UDP datagram**、**[VBAN プロトコル](https://vb-audio.com/Voicemeeter/vban.htm) 互換** (PCM サブセット。VB-Audio Voicemeeter / VBAN Receptor と相互運用)、または **[RTP](https://datatracker.ietf.org/doc/html/rfc3550)** (標準コーデック payload type 経由で PCMU / PCMA / G.722 / L16 / Opus) のいずれかで送受信できます。

**クリーンルーム MIT 実装**。本リポジトリ内に第三者ソースコードを一切 vendoring しておらず、全体が単一著者の MIT。

詳細は [SPEC.ja.md](SPEC.ja.md) を参照。

> **商標について**。「VBAN」は VB-Audio Software のプロトコル名。PCMFlowUDP は **VB-Audio と提携関係にありません**。本書中の「VBAN」表記は、本ライブラリが部分実装している線上プロトコルを識別するための記述的用法です。対応範囲は VBAN の PCM サブセットのみ。詳細は [SPEC §13](SPEC.ja.md#13-ライセンスと商標) を参照。

---

## 構成要素

| クラス | 方向 | キャリア | PCMFlow インターフェース |
|---|---|---|---|
| `RawUdpSink` | bytes → UDP datagram | RAW (呼び出し側定義のペイロード) | `ByteSink` |
| `RawUdpStream` | UDP datagram → bytes | RAW | `ByteStream` |
| `VbanSender` | PCM → UDP datagram | VBAN audio (PCM のみ) | `PCMSink` |
| `VbanReceiver` | UDP datagram → PCM | VBAN audio (PCM のみ) | `PCMSource` |
| `RtpSender` | PCM またはエンコード済みバイト → UDP datagram | RTP (RFC 3550) | `PCMSink` (L16) + `writeEncoded()` |
| `RtpReceiver` | UDP datagram → PCM またはエンコード済みバイト | RTP | `PCMSource` (L16) + `readEncoded()` |

6 クラスとも、呼び出し側が用意した Arduino `UDP` インスタンス (通常 `WiFiUDP`) を受け取る形なので、PCMFlowUDP 自体は具体的な WiFi / Ethernet スタックを引き込みません。ESP32 でも `lang-ship:host` テストターゲットでも同じコードが動きます。

---

## キャリアの選び方

各トランスポートは意図的にスコープを狭く切ってあります。何と相互運用したいかで選びます:

| キャリア | こんなとき | PCM | G.711 / G.722 / Opus |
|---|---|---|---|
| **RAW** | UDP の上に独自 wire format を定義したい (デバイス間、テレメトリ、低レイヤ試験) | ✓ バイト列 | ✓ バイト列 (コーデック兄弟と組合せ) |
| **VBAN** | PC 上の VB-Audio Voicemeeter / VBAN Receptor に流したい | ✓ | 対象外 — コーデック対応は RTP の役割 |
| **RTP** | VoIP / WebRTC / 標準ストリーミングとコーデック対応で相互運用したい | ✓ (L16) | ✓ (PCMU / PCMA / G722 / Opus) |

RAW は意図的にコーデック非依存。標準ツールと相互運用したい場合は **VBAN (PCM)** か **RTP (コーデック対応)** を選びます。詳細は [SPEC §3.4](SPEC.ja.md#34-トランスポート選択-codec--carrier) を参照。

---

## PCMFlow ファミリー

PCMFlowUDP は PCMFlow ファミリーの **transport 専用メンバー**。

| | 役割 |
|---|---|
| [PCMFlow](https://github.com/tanakamasayuki/PCMFlow) | 親 (必須)。リングバッファ / フォーマット変換 / WAV / MP3 / FLAC |
| [PCMFlowG711](https://github.com/tanakamasayuki/PCMFlowG711) | 狭帯域 μ-law / A-law コーデック |
| [PCMFlowG722](https://github.com/tanakamasayuki/PCMFlowG722) | 広帯域 HD ボイスコーデック |
| [PCMFlowOpus](https://github.com/tanakamasayuki/PCMFlowOpus) | 低ビットレート / フルバンドコーデック |
| **PCMFlowUDP** (本ライブラリ) | UDP transport (RAW + VBAN + RTP) |

コーデック兄弟と組み合わせれば圧縮音声を UDP で送れます。例えば `G711Encoder` のバイト列を `RtpSender::writeEncoded()` (payload type PCMU) に渡せば、任意の SIP ソフトフォンが再生できる規格準拠の VoIP ストリームが出ます。

---

## ヘッドラインユースケース — ESP32 マイク → PC (VBAN Receptor)

ESP32 がマイクから収録し、Windows PC 上の VBAN Receptor に流す。PC はローカル音源と同じように再生する。

```cpp
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlow.h>
#include <PCMFlowUDP.h>

WiFiUDP udp;
VbanSender sender(udp);

void setup() {
    WiFi.begin("ssid", "passphrase");
    while (WiFi.status() != WL_CONNECTED) delay(100);

    // Arduino UDP の規約: 送受信前に udp.begin() を必ず一度呼ぶ。
    // EthernetUDP / WiFiNINA / WiFiS3 / lang-ship:host コア では必須。
    // ESP32 の WiFiUDP は寛容だが、明示呼び出しの方が portable。
    // 送信のみのときは 0 を渡してエフェメラルポートに bind する。
    udp.begin(0);

    sender.begin(IPAddress(192,168,1,100), 6980, "ESP32-Mic");
    sender.setFormat({16000, 1, 16});  // 16 kHz モノラル 16-bit
}

void loop() {
    int16_t mic[256];
    // ...I2S 等から `mic` を埋める...
    sender.writeFrames(mic, 256);
}
```

完全な配線サンプルは [examples/VbanMicToPc/](examples/VbanMicToPc/)、RTP/PCMU で SIP ソフトフォンに繋ぐサンプルは [examples/RtpVoipG711/](examples/RtpVoipG711/) を参照。

---

## ハードウェア対応

PCMFlowUDP は Arduino `UDP` 抽象基底クラスに対してコーディングするため、Arduino 互換の UDP 実装を持つ任意のボードで動きます:

- **メインターゲット**: ESP32 ファミリ (`WiFiUDP` / `EthernetUDP`)、Linux host ([lang-ship:host](https://github.com/tanakamasayuki/lang-ship-arduino-core))
- **ベストエフォート** (メンテナ未検証): Arduino UNO R4 WiFi、MKR WiFi 1010 / Nano 33 IoT、Raspberry Pi Pico W、Teensy 4.1 + Ethernet、Portenta H7、W5500/ENC28J60 シールド + 任意のボード
- **対象外**: 8bit AVR (VBAN / RTP フレームに RAM 不足)、BLE 専用ボード

**マルチキャストはどのプラットフォームでも非対応**。host Arduino core が `beginMulticast()` を提供しないため (Windows 互換性の都合)。VBAN 風 fan-out は **ブロードキャスト** で行います (もともと VBAN の典型運用)。

---

## ライセンス

MIT。[LICENSE](LICENSE) 参照。本リポジトリ内に vendoring した第三者コードはありません。VBAN / Voicemeeter / VBAN Receptor の商標注記は [SPEC §13](SPEC.ja.md#13-ライセンスと商標) を参照。
