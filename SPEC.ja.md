# PCMFlowUDP 仕様書

> English: [SPEC.md](SPEC.md)

## 1. 目的と範囲

**PCMFlowUDP** は [PCMFlow](https://github.com/tanakamasayuki/PCMFlow) ファミリーの **UDP transport アダプタ** で、Arduino クラスのデバイスと PC の間で PCM 音声(あるいはコーデック圧縮済みバイト列)を **ローカルネットワーク上で UDP 送受信** することを目的とします。

PCMFlow ファミリー初の **非コーデックメンバー** です。コーデック兄弟([PCMFlowG711](https://github.com/tanakamasayuki/PCMFlowG711) / [PCMFlowG722](https://github.com/tanakamasayuki/PCMFlowG722) / [PCMFlowOpus](https://github.com/tanakamasayuki/PCMFlowOpus))が **サンプル値を変換** するのに対し、PCMFlowUDP は **音声を線で運ぶだけ** です。

サポートするキャリアモードは 2 つ:

- **RAW UDP** — 呼び出し側が渡したバイト列(コーデック後のバイトでも素の PCM でも可)を UDP datagram でそのまま送る。線上のフレーミングは UDP データグラム境界のみ。最もシンプルで最小オーバーヘッド。
- **VBAN 互換** — [VBAN プロトコル](https://vb-audio.com/Voicemeeter/vban.htm)(Audio サブプロトコル + Service サブプロトコルの ping/identification)を実装。VB-Audio 社の *VBAN Receptor* / *Voicemeeter* など、PC 上の VBAN 対応ツールと相互運用できる。

担当範囲:

- 音声フレームを RAW datagram または整形済み VBAN パケットとして **送信**
- UDP から音声フレームを **受信**、VBAN モードではヘッダを検証
- PCMFlow の `PCMSource` / `PCMSink` / `ByteStream` / `ByteSink` インターフェースと **結線**

PCMFlowUDP は **transport 専用**。圧縮された音声を運ぶ場合は PCMFlow 本体内蔵の WAV/MP3/FLAC かコーデック兄弟と組み合わせます。

## 2. 対象外

- **コーデック機能** — PCMFlow 本体またはコーデック兄弟(G.711 / G.722 / Opus)が担当
- **TCP / WebSocket / RTP** — 対象外。UDP のみ。RTP-over-UDP は具体的な要望が出たら検討(§「先送り機能」)
- **TLS / DTLS** — 対象外
- **IPv6** — 対象外。Arduino の `IPAddress` が IPv4 のみのため
- **ジッタバッファ / パケットロス補償** — 呼び出し側の責務(または PCMFlow のリングバッファが吸収)
- **音声デバイス I/O** — PCMFlow が担当
- **具体的な UDP スタック** — PCMFlowUDP は Arduino 標準の `UDP` 抽象基底クラスに対してコーディングする。`WiFiUDP` / `EthernetUDP` 等は呼び出し側が注入する形にし、ライブラリ内では `#include` しない

## 3. 主要ユースケース

### 3.1 ESP32 ↔ PC を VBAN で繋ぐ

開発動機。マイク付き ESP32 が 16 kHz モノラル音声を Windows PC 上の VBAN Receptor に流し、PC ではローカル音源と同じように再生される。逆方向(PC のマイク → ESP32 のスピーカー)も同じライブラリで動く。

### 3.2 ESP32 ↔ ESP32 を RAW UDP で繋ぐ

同一 LAN 上の ESP32 同士が素の UDP で音声をやり取り。VBAN ヘッダ 28 byte 分のオーバーヘッドが無いので、多チャンネル / 高サンプルレート用途で有利。

### 3.3 コーデック + transport の合成

PCMFlowG711 の `G711Encoder` 出力 → `RawUdpSink` で圧縮 VoIP 風パケットを送る、あるいは VBAN 互換 `VbanSink` のサブコーデック欄に μ-law を指定して VBAN PCMU ストリームを流す。

## 4. ハードウェア対応

PCMFlowUDP は Arduino 標準の `UDP` 抽象基底に対してコーディングするので、Arduino 互換 `UDP` 実装を持つ任意のボードで動きます:

| 区分 | 例 |
|---|---|
| **メインターゲット** | ESP32 ファミリ (`WiFiUDP` / `EthernetUDP`)、host (Linux テスト shim、§10 参照) |
| **ベストエフォート** | Arduino UNO R4 WiFi (`WiFiS3::WiFiUDP`)、MKR WiFi 1010 / Nano 33 IoT (`WiFiNINA::WiFiUDP`)、Raspberry Pi Pico W (arduino-pico)、Teensy 4.1 (`NativeEthernet` / `QNEthernet`)、Portenta H7、任意のボード + W5500/ENC28J60 シールド (`EthernetUDP`) |
| **対象外** | 8bit AVR(VBAN フレームに RAM 不足)、nRF52840 BLE 専用ボード(IP スタック無し) |

「ベストエフォート」は、コンパイル・実行できる想定だがメンテナが動作確認はしない、という意味。PR 歓迎。

## 5. 公開 API

> **ステータス: ドラフト**。クラス名・シグネチャは暫定。実装開始時に [src/](src/) のヘッダで確定させます。

4 クラス、キャリア種別 × 方向の 2×2 マトリクス:

| | RAW モード | VBAN モード |
|---|---|---|
| 送信 (UDP へ) | `RawUdpSink` (`ByteSink` 実装) | `VbanSender` (`PCMSink` 実装) |
| 受信 (UDP から) | `RawUdpStream` (`ByteStream` 実装) | `VbanReceiver` (`PCMSource` 実装) |

### 5.1 共通の構築パターン

4 クラスとも `UDP&` 参照(利用者の `WiFiUDP` インスタンス)と設定を受け取る:

```cpp
WiFiUDP wifi;
VbanSender sender(wifi);
sender.begin(IPAddress(192,168,1,100), 6980, "Stream1");
sender.setFormat({16000, 1, 16});  // 16 kHz mono 16-bit
```

受信側はローカル port を bind し、必要ならストリーム名でフィルタ:

```cpp
WiFiUDP wifi;
VbanReceiver recv(wifi);
recv.begin(6980, "Stream1");
```

### 5.2 RAW モード

- `RawUdpSink::write(const void *src, size_t count)` — バイトを蓄積し、`flush()` または内部バッファが埋まったタイミングで UDP datagram としてフラッシュする(チャンク粒度は呼び出し側が決める)
- `RawUdpStream::read(void *dst, size_t count)` — 直近受信した datagram からバイトを取り出す。使い切ったら次の datagram へ

詳細シグネチャは実装で確定。

### 5.3 VBAN モード

VBAN パケットは 28 byte ヘッダ + 最大 1408 byte ペイロード。ヘッダにはサンプルレートインデックス、チャンネル数、サブプロトコル (Audio / Service)、サブコーデック (PCM / μ-law / A-law / Opus / ...)、フレームカウンタ、16 byte のストリーム名が含まれる。

- `VbanSender::writeFrames(const int16_t *pcm, size_t frames)` — サンプルを VBAN Audio パケットに詰めて送信
- `VbanReceiver::readFrames(int16_t *pcm, size_t maxFrames)` — 受信した音声を `PCMSource` 経由で公開

Service サブプロトコル (ping / identification) は内部処理: `VbanReceiver` は受信した ping に応答するので、VBAN 対応ピアからは自デバイスが discovery 可能。

詳細シグネチャは実装で確定。未確定論点は §「未決事項」に記録。

### 5.4 PCMFlow パイプライン結線

- `VbanReceiver` は `PCMSource` → `PCMFlow::setInputSource()` でそのまま再生
- `VbanSender` は `PCMSink` → `PCMFlow::writeFrames()` やマイク収録タスクから流し込む
- `RawUdpStream` / `RawUdpSink` は `ByteStream` / `ByteSink` → コーデック兄弟と合成 (例: 受信した RAW datagram を G.711 デコード)

## 6. VBAN プロトコルの対応範囲

PCMFlowUDP は **VBAN Receptor / Voicemeeter との相互運用に必要な部分** のみ実装し、フル標準は追わない:

| VBAN サブプロトコル | ステータス |
|---|---|
| Audio (0x00) | **実装** — 16-bit LE PCM が主、サブコーデックで μ-law / A-law も選択可能。他の PCM ビット深度 (8 / 24 / 32 / float) は先送り |
| Serial (0x20) | 非実装 |
| MIDI (0x40) | 非実装 |
| Service (0x60) | **実装 (ping + identification のみ)** — VBAN Receptor のソース一覧に出る + discovery に応答できるだけ。他の Service サブタイプは先送り |

サンプルレート: VBAN 標準の 21 種テーブルを TX 側で対応。RX 側は header が宣言する任意レートを受け付けるが、音声パスは `begin()` 時点で設定固定なので、不一致はエラーコードで通知(ランタイムリサンプリングはしない)。必要なら PCMFlow のリサンプラを上流に置く。

## 7. メモリ・フットプリント目標

> **ステータス: 目標値、実測ではない**。実装完了後に確定。

| 項目 | 目標 |
|---|---|
| Flash (フル、RAW + VBAN、送受信両方) | ≤ 12 KB |
| Flash (RAW のみ、片方向) | ≤ 3 KB |
| RAM、`VbanSender` / `VbanReceiver` 1 インスタンスあたり | ≤ 2 KB (1500 byte パケットバッファ込み) |
| RAM、`RawUdpSink` / `RawUdpStream` 1 インスタンスあたり | ≤ 256 B + 呼び出し側バッファ |
| 呼び出し毎のスクラッチ | 無し (動的確保なし) |

VBAN の 28 byte ヘッダはパース時に in-place 処理。パケットバッファは可能な限り呼び出し側所有。

## 8. リポジトリ構成

```
PCMFlowUDP/
├─ README.md / README.ja.md
├─ SPEC.md   / SPEC.ja.md
├─ CHANGELOG.md
├─ LICENSE
├─ library.properties        # depends=PCMFlow
├─ library.json
├─ keywords.txt
├─ src/
│  ├─ PCMFlowUDP.h           # umbrella header
│  ├─ RawUdpSink.h/.cpp
│  ├─ RawUdpStream.h/.cpp
│  ├─ VbanSender.h/.cpp
│  ├─ VbanReceiver.h/.cpp
│  ├─ VbanProtocol.h         # ヘッダ定数 / サブプロトコル / サブコーデック enum
│  └─ pcmflowudp_version.h
├─ examples/
│  ├─ VbanMicToPc/           # ESP32 マイク → Windows の VBAN Receptor
│  └─ EspToEspRaw/           # ESP32 ↔ ESP32 の RAW UDP ループバック
├─ tests/
│  ├─ README.md / README.ja.md
│  ├─ conftest.py
│  ├─ pyproject.toml
│  ├─ smoke/
│  ├─ raw_loopback/          # host のみ、同一プロセスの送信 → 受信
│  ├─ vban_header/           # 既知入力に対する byte-exact ヘッダエンコード
│  ├─ vban_loopback/         # PCM の VBAN エンコード/デコード往復
│  ├─ vban_interop/          # VBAN Receptor 実キャプチャ → デコーダ
│  └─ host_udp/              # host 側 UDP / IPAddress shim (vendoring)
├─ doc/
│  └─ sibling_library_brief.md
├─ tools/
│  └─ bump_version.py
└─ .github/workflows/
   └─ release.yml
```

## 9. 上流コード取り込み

**プロトコル本体は無し**。VBAN の線上フォーマットは VB-Audio が公開しており、二進ヘッダ配置のみが normative。PCMFlowUDP は公開仕様に基づいて VBAN パケット組立・解析を自前実装し、第三者コードは流用しない。

**テスト用に 1 つだけ vendor**: `lang-ship:host` テスト用の host 側 UDP / IPAddress shim (§10 参照)。Arduino ライブラリ本体には同梱しないよう `tests/host_udp/` に配置する。

ライセンス整合性: 配布されるライブラリ本体 (`src/`) は **MIT、単一著者、第三者帰属表示不要**。

## 10. テスト

親 PCMFlow と同じ規約:

- pytest-embedded + Arduino CLI バックエンド
- 2 プロファイル: `lang-ship:host` (ロジック・大型 fixture) と `esp32:esp32:esp32` (実機検証)
- 機能ごとのテストディレクトリに `<feature>.ino` / `sketch.yaml` / `test_<feature>.py`
- `EXPECT_TRUE / EXPECT_EQ / EXPECT_NEAR` マクロと `TEST done N/M` Serial プロトコル

PCMFlowUDP 固有のテスト設計:

| テストディレクトリ | 対象 | 戦略 |
|---|---|---|
| `vban_header/` | byte-exact な VBAN ヘッダエンコード | 既知設定を渡し、ヘッダ各バイトを参照テーブルと突き合わせる |
| `raw_loopback/` | RAW sink → stream の往復 | host: `127.0.0.1` の 2 つの `UDP` インスタンスでバイト一致を assert |
| `vban_loopback/` | PCM → VBAN エンコード → VBAN デコード → PCM | host: 上と同じ要領。RAW PCM ペイロードは bit-exact、μ-law / A-law ペイロードは ±量子化誤差 |
| `vban_interop/` | VBAN Receptor / Voicemeeter の実キャプチャを解析 | 静的 `.bin` fixture でサンプル値とメタデータを assert |

**`lang-ship:host` プロファイルは Arduino 風の `UDP` / `IPAddress` を必要とする** (実 WiFi スタックがないため)。これらは配布ライブラリには含めず、`tests/host_udp/` に置き、別途開発する。shim に求める API は [HOST_UDP_API_REQUEST.md](HOST_UDP_API_REQUEST.md) (納品後に削除する一時文書) に整理してある。

## 11. バージョニング

SemVer (`major.minor.patch`)、`library.properties` / `library.json` / `src/pcmflowudp_version.h` で管理。PCMFlow のバージョンとは独立。

## 12. 先送り機能

忘れないようここに記録。v0.1.x には含めない:

- **RTP-over-UDP** (RFC 3550) — VoIP / WebRTC との相互運用の標準。具体的要望があったら検討
- **VBAN MIDI / Serial / 他の Service サブタイプ** — 音楽コア部分ではない
- **VBAN の Opus / AAC サブコーデック** — コーデック兄弟側の準備も必要。PCMFlowOpus が安定してから再検討
- **`UDP` 基底経由の `beginMulticast` を超えるマルチキャストグループ join** — VBAN には不要
- **受信側内蔵のジッタバッファ / PLC** — PCMFlow のリングバッファが数十 ms のジッタは吸収する。具体的な dropout 報告が出たら再検討

## 13. 未決事項

実装中に詰める論点。忘れないようにここに残す:

- `VbanSender` のバッファ所有 — 呼び出し側供給 vs クラス所有。RAM 制御 vs エルゴノミクスのトレードオフ
- `VbanReceiver` が VBAN フレームカウンタ / ストリーム名を呼び出し側に公開すべきか (重複排除 / 複数ストリーム対応のため)
- エラー通知スタイル — 戻り値 enum vs `lastError()` アクセサ
- 「想定外ストリーム名のパケット到着」の通知方法 (黙って捨てる / カウント / コールバック)

## 14. ライセンス

PCMFlowUDP: **MIT** ([LICENSE](LICENSE))。`src/` に vendoring した第三者コード無し。`tests/host_udp/` 配下の host 側 shim も MIT (§9 / §10 参照)。
