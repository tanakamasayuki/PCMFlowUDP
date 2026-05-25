# PCMFlowUDP 仕様書

> English: [SPEC.md](SPEC.md)

## 1. 目的と範囲

**PCMFlowUDP** は [PCMFlow](https://github.com/tanakamasayuki/PCMFlow) ファミリーの **UDP transport アダプタ** で、Arduino クラスのデバイスと PC の間で PCM 音声(あるいはコーデック圧縮済みバイト列)を **ローカルネットワーク上で UDP 送受信** することを目的とします。

PCMFlow ファミリー初の **非コーデックメンバー** です。コーデック兄弟([PCMFlowG711](https://github.com/tanakamasayuki/PCMFlowG711) / [PCMFlowG722](https://github.com/tanakamasayuki/PCMFlowG722) / [PCMFlowOpus](https://github.com/tanakamasayuki/PCMFlowOpus))が **サンプル値を変換** するのに対し、PCMFlowUDP は **音声を線で運ぶだけ** です。

サポートするキャリアモードは 2 つ:

- **RAW UDP** — 呼び出し側が渡したバイト列(コーデック後のバイトでも素の PCM でも可)を UDP datagram でそのまま送る。線上のフレーミングは UDP データグラム境界のみ。最もシンプルで最小オーバーヘッド。
- **VBAN 互換サブセット** — [VBAN プロトコル](https://vb-audio.com/Voicemeeter/vban.htm) の **サブセット** を実装し、VB-Audio 社の *VBAN Receptor* / *Voicemeeter* など、PC 上の VBAN 対応ツールと相互運用する。具体的には **Audio サブプロトコル** (PCM ペイロード) と **Service サブプロトコルの最小部分** (ping / identification) のみ。**フル VBAN 実装ではない** — MIDI / Serial / Service サブタイプの大半は対象外(完全な対応表は §6 参照)。本書中の「VBAN」は線上プロトコルを識別するための記述的な使用で、商標注記は §14 を参照。

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

PCMFlowG711 の `G711Encoder` 出力 → `RawUdpSink` で呼び出し側定義の VoIP 風パケットを送る。**PCMU / PCMA / G.722 / Opus の標準コーデック over UDP 相互運用は将来 `RtpSender` / `RtpReceiver` (§12) の役割** で、RTP が入るまでは RAW が一時的にその位置を埋める。

### 3.4 トランスポート選択 (codec × carrier)

各トランスポートは意図的にスコープを狭く切る。何と相互運用したいかで選ぶ:

| キャリア | 目的 | PCM | G.711 / G.722 / Opus |
|---|---|---|---|
| **RAW** | BYO プロトコルの逃げ道 — UDP datagram の上にユーザー定義の wire format を載せる | ✓ (バイト列) | ✓ (バイト列、コーデック兄弟と組み合わせ) |
| **VBAN** | VB-Audio Voicemeeter / VBAN Receptor との相互運用 | ✓ (PCM16、必要なら PCM8) | **対象外** — コーデック対応は RTP の役割 |
| **RTP** *(v0.2.0 予定)* | VoIP / WebRTC / 標準ストリーミングツールとの相互運用 | ✓ (L16, PT 10/11) | ✓ 標準 payload type 経由: PCMU(0)、PCMA(8)、G722(9)、Opus (dynamic, RFC 7587) |

RAW は意図的にコーデック非依存。標準ツールと相互運用したい場合は **VBAN (PCM 専用) か RTP (コーデック対応)** を選ぶ。RAW はデバイス間独自プロトコル、テレメトリ、低レイヤ試験向け。

## 4. ハードウェア対応

PCMFlowUDP は Arduino 標準の `UDP` 抽象基底に対してコーディングするので、Arduino 互換 `UDP` 実装を持つ任意のボードで動きます:

| 区分 | 例 |
|---|---|
| **メインターゲット** | ESP32 ファミリ (`WiFiUDP` / `EthernetUDP`)、host (`lang-ship:host` 1.0.6+ が `WiFiUDP` / `IPAddress` を提供、§10 参照) |
| **ベストエフォート** | Arduino UNO R4 WiFi (`WiFiS3::WiFiUDP`)、MKR WiFi 1010 / Nano 33 IoT (`WiFiNINA::WiFiUDP`)、Raspberry Pi Pico W (arduino-pico)、Teensy 4.1 (`NativeEthernet` / `QNEthernet`)、Portenta H7、任意のボード + W5500/ENC28J60 シールド (`EthernetUDP`) |
| **対象外** | 8bit AVR(VBAN フレームに RAM 不足)、nRF52840 BLE 専用ボード(IP スタック無し) |

「ベストエフォート」は、コンパイル・実行できる想定だがメンテナが動作確認はしない、という意味。PR 歓迎。

**マルチキャストは非対応**。host プラットフォームの `UDP::beginMulticast()` は Windows 互換性の都合で意図的に no-op となっているため、PCMFlowUDP もどのプラットフォームでもマルチキャストに依存しない。ディスカバリや VBAN 風 fan-out は **ブロードキャスト** (`IPAddress(255,255,255,255)`) で行う。

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
WiFiUDP udp;
VbanSender sender(udp);
sender.begin(IPAddress(192,168,1,100), 6980, "Stream1");
sender.setFormat({16000, 1, 16});  // 16 kHz mono 16-bit
```

受信側はローカル port を bind し、必要ならストリーム名でフィルタ:

```cpp
WiFiUDP udp;
VbanReceiver recv(udp);
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

PCMFlowUDP は **VBAN のサブセット** のみを実装する — VBAN 対応ツールとの音声送受信に必要な部分だけ。**フル VBAN スタックではない**。具体的には:

| VBAN サブプロトコル | ステータス |
|---|---|
| Audio (0x00) | **PCM のみ実装** — v0.1.x は 16-bit LE PCM。PCM 8-bit は要望があれば追加。**μ-law / A-law / Opus を VBAN 内で運ぶことは意図的に対象外**: 標準コーデック over UDP は将来 RTP の役割 (§3.4 / §12) で、VBAN サブコーデックと RTP payload type の両方で同じコーデックを抱えるとマトリクスが重複するため。他の PCM ビット深度 (24 / 32 / float) は先送り |
| Serial (0x20) | 非実装 |
| MIDI (0x40) | 非実装 |
| Service (0x60) | **ヘッダ検出 + ユーザコールバック** — `parseServiceHeader()` と `VbanReceiver::setServiceCallback()` で Service パケットを観測・応答可能。Service レスポンダの **payload フォーマットは v0.1.x では実装しない**: VB-Audio が normative な payload 仕様を公開しておらず、GPL 実装からの reverse-engineering は本ライブラリのクリーンルーム MIT ポリシーに反するため。「VBAN Receptor の discovery 一覧に出たい」ユーザはキャプチャを取って自前でレスポンダを実装する想定 |

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
│  └─ vban_interop/          # VBAN Receptor 実キャプチャ → デコーダ
├─ doc/
│  └─ sibling_library_brief.md
├─ tools/
│  └─ bump_version.py
└─ .github/workflows/
   └─ release.yml
```

## 9. 上流コード取り込み

**プロトコル本体は無し**。VBAN の線上フォーマットは VB-Audio が公開しており、二進ヘッダ配置のみが normative。PCMFlowUDP は公開仕様に基づいて VBAN パケット組立・解析を自前実装し、第三者コードは流用しない。

**host 側の shim も vendor しない**。`lang-ship:host` Arduino core (1.0.6+) が Berkeley socket ベースの `WiFiUDP` / `IPAddress` を提供しており、ESP32 ビルドと API 互換。PCMFlowUDP は両ターゲットで単に `#include <WiFiUdp.h>` するだけで済む。

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

**`lang-ship:host` Arduino core (1.0.6+) が Berkeley socket ベースの `WiFiUDP` / `IPAddress` を提供** し、ESP32 と API 互換になっている。host テストもデバイス向けスケッチと同じ `#include <WiFiUdp.h>` でそのまま動くので、本リポ内に shim / vendor は不要。テストの `sketch.yaml` は `platform: lang-ship:host (1.0.6)` 以降を指定する。

## 11. バージョニング

SemVer (`major.minor.patch`)、`library.properties` / `library.json` / `src/pcmflowudp_version.h` で管理。PCMFlow のバージョンとは独立。

## 12. 先送り機能

忘れないようここに記録。v0.1.x には含めない:

- **RTP-over-UDP** (RFC 3550) — **v0.2.0 で予定**。VBAN クラスと並列に、同じ `UDP&` injection パターンで `RtpSender` / `RtpReceiver` を提供。**標準 payload type でコーデック対応**: PCMU(0)、PCMA(8)、G722(9)、L16 モノ/ステレオ(11/10)、Opus は dynamic PT (RFC 7587)。送信側は bytes-in API (`writeEncoded`) を公開し、コーデック兄弟 (`G711Encoder`、将来の `G722Encoder` / `OpusEncoder`) と自然に合成可能。L16 用に `writeFrames(int16_t*)` ヘルパ (network byte order pack) も提供。実装着手は PCMFlowOpus の安定後 (Opus が最大のコーデック兄弟で、RTP の timing contract を決める要因になる)
- **VBAN MIDI / Serial / 他の Service サブタイプ** — 音楽コア部分ではない
- **ライブラリ内蔵の VBAN Service レスポンダ** (payload まで) — 現行 VB-Audio ツールの実キャプチャと、identification reply 構造のクリーンルーム解釈が必要。v0.1.x のヘッダ検出 + コールバック API で、ユーザが out-of-tree にレスポンダを実装できる
- **VBAN の Opus / AAC サブコーデック** — コーデック兄弟側の準備も必要。PCMFlowOpus が安定してから再検討
- **マルチキャスト** — 追加しない。host Arduino core が `beginMulticast()` を提供しない (Windows 互換性の都合) ため、プラットフォーム間で挙動を揃える方針として PCMFlowUDP もマルチキャストに依存しない。VBAN の用途はブロードキャストで足りる
- **受信側内蔵のジッタバッファ / PLC** — PCMFlow のリングバッファが数十 ms のジッタは吸収する。具体的な dropout 報告が出たら再検討

## 13. 未決事項

実装中に詰める論点。忘れないようにここに残す:

- `VbanSender` のバッファ所有 — 呼び出し側供給 vs クラス所有。RAM 制御 vs エルゴノミクスのトレードオフ
- `VbanReceiver` が VBAN フレームカウンタ / ストリーム名を呼び出し側に公開すべきか (重複排除 / 複数ストリーム対応のため)
- エラー通知スタイル — 戻り値 enum vs `lastError()` アクセサ
- 「想定外ストリーム名のパケット到着」の通知方法 (黙って捨てる / カウント / コールバック)

## 14. ライセンスと商標

PCMFlowUDP: **MIT** ([LICENSE](LICENSE))。本リポ内には vendoring した第三者コードを一切含まない (`src/` は VBAN 公開仕様をベースに手書き、host 側 UDP は `lang-ship:host` Arduino core が外部から提供。§9 / §10 参照)。

**商標注記**。VBAN は **VB-Audio Software** が開発・公開しているプロトコル名。本ライブラリ中の「VBAN」表記(`VbanSender` / `VbanReceiver` などのクラス名を含む)は、本ライブラリが部分実装している線上プロトコルを識別するための **記述的・指示的(nominative)用法** であり、PCMFlowUDP は **VB-Audio Software と提携・推薦・後援関係にない**。また、本ライブラリは VBAN 仕様の **サブセットのみ** を対応する(§6)。本書中に出てくる Voicemeeter / VBAN Receptor 等の名称は、各権利者の商標。
