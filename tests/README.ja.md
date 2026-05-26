# テスト

> English: [README.md](README.md)

PCMFlowUDP の自動テストスイート。親 [PCMFlow テストスイート](https://github.com/tanakamasayuki/PCMFlow/tree/main/tests) の規約を踏襲:

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI バックエンド
- 2 プロファイル: `lang-ship:host` (ロジック検証、host 側 UDP ループバック、高速 CI) と `esp32:esp32:esp32` (実機検証、フットプリント測定)
- 機能ごとのサブディレクトリに `<feature>.ino` / `sketch.yaml` / `test_<feature>.py` (必要なら `input/` fixture も)
- アサーションは `EXPECT_TRUE` / `EXPECT_EQ` / `EXPECT_NEAR` マクロと `TEST done N/M` Serial プロトコル

host プロファイルは **`lang-ship:host` 1.0.6 以降** を pin する。これが Berkeley socket ベースの `WiFiUDP` / `IPAddress` を同梱した最初のリリースで、UDP ループバックテストはこれに依存する。

## ディレクトリ構成

- `smoke/` — テスト基盤自体と PCMFlowUDP のコンパイル可能性を確認する smoke (host プロファイル)
- *(予定)* `raw_loopback/` — `127.0.0.1` 上の 2 つの `WiFiUDP` インスタンスで `RawUdpSink` → `RawUdpStream` のバイト送受
- *(予定)* `vban_header/` — 既知設定に対する byte-exact な VBAN ヘッダエンコード
- *(予定)* `vban_loopback/` — `VbanSender` → `VbanReceiver` の PCM 往復
- *(予定)* `vban_interop/` — VB-Audio Voicemeeter / VBAN Receptor の実キャプチャパケットのデコード

## ESP32 と host の違い

両プロファイルは **同じ `.ino` ソースを `#ifdef ARDUINO_ARCH_ESP32` ガード無しで動かす** 想定で設計されている。`lang-ship:host` core が Arduino API の薄い stub レイヤ (`WiFi.h` / `WiFiUdp.h` / `IPAddress.h` / `Stream.h` …) を提供しているので、デバイス向けに書いたコードがそのまま Linux プロセスで動く。具体的には:

| API | ESP32 での挙動 | host stub での挙動 |
|---|---|---|
| `Serial.begin(115200)` | 実 USB-CDC ブリッジ。pytest に出力が見えるまで ~5 秒の安定待ちが必要 | ハーネス経由の仮想 stdout。即座に使えるが、対称性と保険のため 5 秒 `delay()` は残してある |
| `WiFi.mode(WIFI_STA)` / `WiFi.begin(ssid, pw)` | 実際の Wi-Fi join。`WL_CONNECTED` になるまで(タイムアウト付き)ブロック | no-op で即 `WL_CONNECTED` を返す |
| `WiFi.localIP()` | AP / DHCP から払い出された IP (例: `192.168.13.152`) | 常に `127.0.0.1` |
| `WiFiUDP::begin(port)` | lwIP の UDP socket を bind | BSD の `SOCK_DGRAM` を bind |
| `WiFiUDP::beginPacket(host, port)` + `endPacket()` | WiFi スタック経由で実 Ethernet フレーム送出 | BSD socket の `sendto()` |
| `random()` | ハードウェア RNG (ESP32) | プロセス単位 seed の libc `rand()` |

### コード規約

- **テストスケッチや examples で `#ifdef ARDUINO_ARCH_ESP32` ガードを書かない**。同じソースが両ターゲットで動くべき。ガードを書きたくなったら、たいていは「host stub に足りない API を追加する」が正解
- **送受信前に必ず `udp.begin(port)` を呼ぶ**。ESP32 の `WiFiUDP` は寛容(`beginPacket` 内で遅延 open)だが、`EthernetUDP` / `WiFiNINA` / `WiFiS3` / `lang-ship:host` 等は明示要求。送信のみのときは `udp.begin(0)` でエフェメラルポートに bind
- **`Serial.begin()` の直後に `delay(5000);`** を全テストスケッチに入れている。ESP32 の USB-Serial ブリッジが最初の数百 ms を取りこぼすことがあるため。host では無害

### ESP32 で動かせないテスト

一部のテストは構造上 host 専用。`sketch.yaml` に esp32 プロファイルを置かず、`.ino` 冒頭のコメントで理由を説明している。

| テスト | 理由 |
|---|---|
| `raw_loopback/` | 同一プロセス内の 2 つの `WiFiUDP` が 127.0.0.1 で UDP datagram をやり取りする。arduino-esp32 の lwIP ビルドは loopback I/F (`LWIP_HAVE_LOOPIF`) を有効化しておらず、127.0.0.1 が届かない。`WiFi.localIP()` に置き換えても自己宛 packet を AP に hairpin させる必要があり portable でない。実機での RAW on-wire パス検証は `raw_python_loopback/` が担当 |
| `vban_loopback/` | `raw_loopback/` と同じ理由。実機での VBAN on-wire 検証は `vban_python_loopback/` |
| `rtp_loopback/` | `raw_loopback/` と同じ理由。実機での RTP on-wire 検証は `rtp_python_loopback/` |

ESP32 側で等価テストが必要になった場合は **2 台のデバイス**(またはデバイス + host PC)で LAN 越しに会話させる形が正解。すでに `*_python_loopback/` 系がそのパターン(pytest が "もう一方の peer" 役)で動いている。

## 商標について

`vban_*` テストは PCMFlowUDP の VBAN サブセット実装を検証する。「VBAN」は VB-Audio Software のプロトコル名。詳細は [../SPEC.ja.md §13](../SPEC.ja.md#13-ライセンスと商標) を参照。
