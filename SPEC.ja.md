# PCMFlowUDP 仕様書

> English: [SPEC.md](SPEC.md)

## 1. 目的と範囲

**PCMFlowUDP** は [PCMFlow](https://github.com/tanakamasayuki/PCMFlow) の **UDP transport アダプタ** です。Arduino クラスのデバイスと PC の間で PCM(あるいはコーデック圧縮済みバイト列)をローカルネットワーク経由でストリーミングします。

PCMFlow ファミリーの **transport 専用メンバー** です。コーデック兄弟([PCMFlowG711](https://github.com/tanakamasayuki/PCMFlowG711) / [PCMFlowG722](https://github.com/tanakamasayuki/PCMFlowG722) / [PCMFlowOpus](https://github.com/tanakamasayuki/PCMFlowOpus))がサンプル値を変換するのに対し、PCMFlowUDP は音声を線で運ぶだけです。

3 つのキャリアをサポートします。それぞれ意図的にスコープを狭く切ってあります (§3.4 の選択マトリクス参照):

- **RAW UDP** — ペイロードは呼び出し側が渡したバイト列。UDP datagram 境界以外のフレーミングは無し。デバイス間独自プロトコル、テレメトリ、低レイヤ試験の **逃げ道**
- **VBAN (PCM サブセット)** — [VBAN プロトコル](https://vb-audio.com/Voicemeeter/vban.htm) の PCM 部分を実装し、VB-Audio 社の *VBAN Receptor* / *Voicemeeter* など、VBAN 対応ツールと相互運用する。具体的には **Audio サブプロトコル (PCM ペイロード)** と、**Service サブプロトコルのヘッダ検出 + ユーザコールバック**。**フル VBAN 実装ではない** — 対応表は §6 参照。本書中の「VBAN」は線上プロトコルを識別するための記述的な使用で、商標注記は §14 を参照
- **RTP (RFC 3550)** — VoIP / WebRTC / 標準ストリーミングツールとの相互運用に必要な RTP のサブセットを実装。**コーデック対応**(PCMU / PCMA / G722 / L16 / Opus の payload type)。PCMFlowUDP がパケット化し、コーデック兄弟がバイト列を供給する

責務:

- UDP 上に RAW datagram、整形済み VBAN パケット、または規格準拠 RTP パケットとして **送信**
- UDP datagram を **受信**、キャリアに応じてディスパッチ(RAW バイト、VBAN 検証済み PCM、RTP payload-type 別のバイト列または PCM)
- PCMFlow の `PCMSource` / `PCMSink` / `ByteStream` / `ByteSink` インターフェースと **結線**

PCMFlowUDP は **transport 専用**。エンコード / デコードは行わない。圧縮ペイロードを扱うときは PCMFlow 本体内蔵の WAV/MP3/FLAC かコーデック兄弟と組み合わせる。

### 1.1 受信バッファと遅延の責務

UDP audio では、packet 到着間隔の揺れと出力デバイス側の非同期再生を分けて扱う。

| 層 | 責務 | 方針 |
|---|---|---|
| PCMFlow | PCM の生成、変換、codec decode | transport jitter や実機 speaker queue には依存しない |
| PCMFlowUDP | RTP/VBAN/RAW UDP packet の受信、header 解析、PCMSource / ByteStream への安定供給 | 小さな packet jitter を吸収する設定可能な受信 ring buffer、`availableFrames()`、初期プリバッファ、読み出し chunk 設定を提供する。RAM 制約のため、バッファは小さく保ち、必要なら呼び出し側提供バッファを使う |
| アプリ / 出力 helper | M5Unified speaker など出力デバイス固有の非同期 queue 管理 | `playRaw()` に渡す buffer の寿命管理、三重 buffer、`playRaw()` が false のときの retry を隠す |

目標遅延は用途別に分ける。

| 用途 | 初期プリバッファ目安 | 通常読み出し chunk | 備考 |
|---|---:|---:|---|
| VoIP / 双方向会話 | 20-40 ms | 10-20 ms | 80 ms は会話用途では重い。必要なら adaptive jitter buffer で上限を抑える |
| LAN 内の実機 speaker 再生 / manual test | 40-80 ms | 20-40 ms | 音切れを避ける安定寄り設定。Core2 manual speaker テストは初回 80 ms、以後 40 ms |
| BGM / 監視 / 片方向ストリーミング | 80 ms 以上も可 | 40 ms 以上も可 | 低遅延より安定性を優先できる |

PCMFlowUDP は packet loss concealment は行わない。sequence 欠落や timestamp 不連続は観測可能にし、無音挿入、補間、codec PLC は呼び出し側または codec 側で扱う。

## 2. 対象外

- **コーデック機能** — PCMFlow 本体またはコーデック兄弟(G.711 / G.722 / Opus)が担当
- **TCP / WebSocket** — 対象外。UDP のみ
- **TLS / DTLS / SRTP** — 対象外
- **IPv6** — 対象外。Arduino の `IPAddress` が IPv4 のみのため
- **マルチキャスト** — host Arduino core が `beginMulticast()` を提供しないため (Windows 互換性の都合)、PCMFlowUDP もどのプラットフォームでも MC に依存しない。スコープ内のユースケースはブロードキャストで足りる
- **パケットロス補償** — 欠落した音声の補間や PLC はコーデック兄弟または呼び出し側の責務
- **VBAN Service レスポンダの payload 部** — Service サブプロトコルの *ヘッダ* はパースしてユーザコールバックに渡すが、*reply payload* (デバイス識別構造) はライブラリ内蔵しない。VB-Audio が normative な payload 仕様を公開しておらず、GPL 実装からの reverse-engineering は本ライブラリのクリーンルーム MIT ポリシーに反するため。ユーザは自前キャプチャに対して out-of-tree でレスポンダを実装する
- **VBAN MIDI / Serial / 他の Service サブタイプ** — 音楽コア部分ではない
- **VBAN PCM 以外のサブコーデック** (μ-law / A-law / Opus を VBAN 内で運ぶ) — 標準コーデック over UDP は RTP の役割 (§3.4)。VBAN サブコーデックと RTP payload type の両方で同じコーデックを抱えるとマトリクスが重複するため
- **音声デバイス I/O** — PCMFlow が担当
- **具体的な UDP スタック** — PCMFlowUDP は Arduino 標準の `UDP` 抽象基底クラスに対してコーディング。`WiFiUDP` / `EthernetUDP` 等は呼び出し側が注入し、本ライブラリでは `#include` しない

## 3. 主要ユースケース

### 3.1 ESP32 ↔ PC を VBAN で繋ぐ

開発動機。マイク付き ESP32 が 16 kHz モノラル PCM を Windows PC 上の VBAN Receptor に流し、PC ではローカル音源と同じように再生される。逆方向 (PC のマイク → ESP32 のスピーカー) も同じライブラリで動く。

### 3.2 ESP32 ↔ ESP32 を RAW UDP で繋ぐ

同一 LAN 上の ESP32 同士が、呼び出し側が定義した wire format で素の UDP を介して音声をやり取り。最小オーバーヘッド (プロトコルヘッダ無し)。デバイス間独自リンクやテレメトリ向け。

### 3.3 ESP32 ↔ VoIP / WebRTC を RTP で繋ぐ

ESP32 が G.711 μ-law (PCMU, RTP payload type 0) を SIP ソフトフォンや PC 上の `gst-launch-1.0` パイプラインへストリーミング。コーデック兄弟 (`G711Encoder`) がバイト列を生成し、`RtpSender` が正しい payload type / sequence number / timestamp / SSRC を付けてパケット化。Opus / G.722 / L16 PCM も同様。

### 3.4 トランスポート選択 (codec × carrier)

何と相互運用したいかで選ぶ:

| キャリア | 目的 | PCM | G.711 / G.722 / Opus |
|---|---|---|---|
| **RAW** | BYO プロトコルの逃げ道 — UDP datagram の上にユーザー定義の wire format | ✓ (バイト列) | ✓ (バイト列、コーデック兄弟と組み合わせ) |
| **VBAN** | VB-Audio Voicemeeter / VBAN Receptor との相互運用 | ✓ (PCM16) | 対象外 — コーデック対応は RTP の役割 |
| **RTP** | VoIP / WebRTC / 標準ストリーミングツールとの相互運用 | ✓ (L16, PT 10/11) | ✓ 標準 payload type 経由: PCMU(0)、PCMA(8)、G722(9)、Opus (dynamic, RFC 7587) |

RAW は意図的にコーデック非依存。標準ツールと相互運用したい場合は **VBAN (PCM) か RTP (コーデック対応)** を選ぶ。

## 4. ハードウェア対応

PCMFlowUDP は Arduino 標準の `UDP` 抽象基底に対してコーディングするので、Arduino 互換 `UDP` 実装を持つ任意のボードで動く:

| 区分 | 例 |
|---|---|
| **メインターゲット** | ESP32 ファミリ (`WiFiUDP` / `EthernetUDP`)、host (`lang-ship:host` が `WiFiUDP` / `IPAddress` を提供、§10 参照) |
| **ベストエフォート** | Arduino UNO R4 WiFi (`WiFiS3::WiFiUDP`)、MKR WiFi 1010 / Nano 33 IoT (`WiFiNINA::WiFiUDP`)、Raspberry Pi Pico W (arduino-pico)、Teensy 4.1 (`NativeEthernet` / `QNEthernet`)、Portenta H7、任意のボード + W5500/ENC28J60 シールド (`EthernetUDP`) |
| **対象外** | 8bit AVR (VBAN / RTP フレームに RAM 不足)、nRF52840 BLE 専用ボード (IP スタック無し) |

「ベストエフォート」は、コンパイル・実行できる想定だがメンテナが動作確認はしない。PR 歓迎。

## 5. 公開 API

6 クラス、キャリア種別 × 方向の 3×2 マトリクス:

| | RAW | VBAN | RTP |
|---|---|---|---|
| 送信 | `RawUdpSink` (`ByteSink` 実装) | `VbanSender` (`PCMSink` 実装) | `RtpSender` (L16 で `PCMSink`、コーデックは `writeEncoded()`) |
| 受信 | `RawUdpStream` (`ByteStream` 実装) | `VbanReceiver` (`PCMSource` 実装) | `RtpReceiver` (L16 で `PCMSource`、コーデックは `readEncoded()`) |

### 5.1 共通の構築パターン

6 クラスとも `UDP&` 参照 (利用者の `WiFiUDP` インスタンス) と設定を受け取る:

```cpp
WiFiUDP udp;
udp.begin(0);                              // エフェメラルローカル bind、§5.5 参照

VbanSender sender(udp);
sender.begin(IPAddress(192,168,1,100), 6980, "Stream1");
sender.setFormat({16000, 1, 16});          // 16 kHz モノラル 16-bit
```

受信側はローカル port を bind し、必要ならストリーム名でフィルタ:

```cpp
WiFiUDP udp;
VbanReceiver recv(udp);
recv.begin(6980, "Stream1");
```

### 5.2 RAW

- `RawUdpSink::write(const void *src, size_t count)` — バイトを蓄積。`flush()` で蓄積分を 1 つの UDP datagram として送信 (チャンク粒度は flush タイミングで呼び出し側が決める)
- `RawUdpStream::read(void *dst, size_t count)` — 直近受信した datagram からバイトを取り出す。使い切ったら次の datagram へ (datagram 未保持なら auto-poll)

RAW は PCM を理解しない。バイト透過。

### 5.3 VBAN

VBAN パケットは 28 byte ヘッダ + 最大 1408 byte ペイロード。PCMFlowUDP は **Audio サブプロトコルの PCM16 ペイロード** と **Service サブプロトコルのユーザコールバック** (ヘッダ検出のみ、reply payload はユーザ責務 — §6 / §2 参照) を扱う。

- `VbanSender::writeFrames(const int16_t *pcm, size_t frames)` — PCM16 サンプルを VBAN Audio パケットに詰めて送信。パケット粒度はチャンネル数から自動計算
- `VbanSender::flush()` — 蓄積中の部分パケットを即時送信
- `VbanReceiver::readFrames(int16_t *pcm, size_t maxFrames)` — 受信した PCM を `PCMSource` 経由で取り出す
- `VbanReceiver::setServiceCallback(cb, userData)` — Service サブプロトコルパケットのコールバックを登録。`poll()` 内で、パースされたヘッダ、生 payload ポインタ、送信元アドレス、receiver の `UDP*` (応答送信用) と共に呼び出される

### 5.4 RTP

RTP パケットは 12 byte ヘッダ + ペイロード。ヘッダはバージョン、payload type、sequence number、timestamp、SSRC を含む。PCMFlowUDP は static payload type ([RFC 3551](https://datatracker.ietf.org/doc/html/rfc3551)) と Opus (RFC 7587 dynamic PT) を扱う。

対応 payload type:

| PT | コーデック | クロック | 備考 |
|---|---|---|---|
| 0 | PCMU (G.711 μ-law) | 8000 Hz | `G711Encoder` / `G711Decoder` と組み合わせ |
| 8 | PCMA (G.711 A-law) | 8000 Hz | `G711Encoder` / `G711Decoder` と組み合わせ |
| 9 | G.722 | 8000 Hz (RTP timestamp の慣行) | `G722Encoder` / `G722Decoder` と組み合わせ |
| 10 | L16 ステレオ | 設定可能 | PCM16 BE、network byte order |
| 11 | L16 モノラル | 設定可能 | PCM16 BE、network byte order |
| 96..127 | dynamic | payload ごと | Opus は 48000 Hz が典型。GStreamer などが L16/16 kHz を dynamic PT 96 で送る場合は `RtpReceiver::setDynamicL16PayloadType()` で L16 として受ける |

API:

- `RtpSender::setPayloadType(uint8_t pt, uint32_t clockRate)` — コーデック枠を設定。SSRC は `begin()` 時に乱数で決定、`setSsrc()` で上書き可能
- `RtpSender::writeFrames(const int16_t *pcm, size_t frames)` — L16 経路: PCM16 を network byte order でパックして送信。PT 10 / 11 のみ
- `RtpSender::writeEncoded(const uint8_t *bytes, size_t count)` — コーデック経路: 1 呼び出し = 1 RTP パケット (渡したバイト列が payload)。PT 0 / 8 / 9 / dynamic
- `RtpReceiver::readFrames(int16_t *pcm, size_t maxFrames)` — L16 経路: host byte order の PCM16 を返す
- `RtpReceiver::setDynamicL16PayloadType(uint8_t pt, uint8_t channels)` — dynamic PT を L16 PCM として扱う。GStreamer の `rtpL16pay` が 16 kHz L16 を PT 96 として送る場合に使う
- `RtpReceiver::readEncoded(uint8_t *bytes, size_t maxBytes)` — コーデック経路: 1 パケットの payload バイトを返す
- `RtpReceiver::payloadType()` / `sequenceNumber()` / `timestamp()` / `ssrc()` — 最新パケットのメタデータ (アプリ側のデコード / ジッタ解析向け)

timestamp と sequence number は `RtpSender` が自動でインクリメントする。呼び出し側で管理する必要はない。

### 5.5 ローカル port bind 規約

6 クラスとも、いずれかの操作の前に呼び出し側で `udp.begin(port)` (任意ポート、送信専用は 0 でエフェメラル) を呼んでおく必要がある。これは Arduino `UDP` の文書化された規約 (`EthernetUDP` / `WiFiNINA` / `WiFiS3` / host コア すべて必須。ESP32 の `WiFiUDP` は寛容だが明示する方が portable)。サンプルスケッチと README にこのイディオムが書いてある。

### 5.6 PCMFlow パイプライン結線

- `VbanReceiver` / `RtpReceiver` (L16 モード) → `PCMFlow::setInputSource()` でそのまま再生
- `VbanSender` / `RtpSender` (L16 モード) → `PCMFlow::writeFrames()` やマイク収録タスクから流し込む
- `RawUdpStream` / `RawUdpSink` → コーデック兄弟と合成 (例: 受信した RAW datagram を `G711Decoder` に通す)
- `RtpSender::writeEncoded()` / `RtpReceiver::readEncoded()` → VoIP 風フロー用にコーデック兄弟と合成

## 6. VBAN プロトコルの対応範囲

PCMFlowUDP は VBAN のうち PCM 音声を VBAN 対応ツールと往復するのに必要な部分と、Service ヘッダレベル検出だけを実装する。

| VBAN サブプロトコル | 状態 |
|---|---|
| Audio (0x00) | **PCM のみ** — PCM16 LE が主経路。サンプルレートは TX 側で VBAN 21 エントリテーブル、RX 側はテーブル内任意レートを受理。μ-law / A-law / Opus / 非 16-bit PCM を VBAN サブコーデックで運ぶことは対象外 (§2 — コーデック対応は RTP の役割) |
| Serial (0x20) | 非実装 |
| MIDI (0x40) | 非実装 |
| Service (0x60) | **ヘッダ検出 + ユーザコールバック**。`parseServiceHeader()` と `VbanReceiver::setServiceCallback()` で Service パケットを観測し、receiver の UDP ソケットから応答を送れる。ライブラリ内蔵のレスポンダ payload は提供しない (§2) |

## 7. RTP プロトコルの対応範囲

PCMFlowUDP は RFC 3550 のうち、ワンショット音声ストリーミングに必要な部分のみ実装する:

- 12 byte 固定ヘッダ (CSRC なし、ヘッダ拡張なし、デフォルトで padding なし)
- sequence number は `begin()` 時の乱数開始値から 1 ずつインクリメント
- timestamp は設定クロックレートでパケットあたりサンプル数だけインクリメント
- SSRC は `begin()` 時の 32 bit 値 (省略時は乱数)
- marker ビットは `begin()` 後の最初のパケットと、ユーザ要求時 (`setMarker()`) に立つ。それ以外は 0

対象外:
- **RTCP** (sender / receiver report)。多くの RTP 音声用途は RTCP 無しで動く。必要ならば後日 `RtcpReporter` 等の別クラスとして RTP クラスを壊さずに追加可能
- **SRTP / DTLS-SRTP** (§2)
- **再順序化 / パケットロス補償** (§2)。小さな packet jitter の吸収は §1.1 の範囲で扱うが、欠落・大幅な遅延・順序入れ替わりの修復は行わない
- **Static PT で必要な範囲を超えた payload format 拡張** (例: Opus の FEC ネゴシエーションは SDP 側であって RTP パケットではない)

## 8. メモリ・フットプリント目標

| 項目 | 目標 |
|---|---|
| Flash (フル、RAW + VBAN + RTP、送受信両方) | ≤ 20 KB |
| Flash (RAW のみ、片方向) | ≤ 3 KB |
| RAM、`VbanSender` / `VbanReceiver` 1 インスタンスあたり | ≤ 2 KB (1500 byte パケットバッファ込み) |
| RAM、`RtpSender` / `RtpReceiver` 1 インスタンスあたり | ≤ 2 KB (1500 byte パケットバッファ込み) |
| RAM、`RawUdpSink` / `RawUdpStream` 1 インスタンスあたり | ≤ 256 B + 呼び出し側バッファ |
| 呼び出し毎のスクラッチ | 無し (動的確保なし) |

ヘッダはエンコード / デコードとも in-place 処理。パケットバッファは可能な限り呼び出し側所有。

## 9. リポジトリ構成

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
│  ├─ VbanProtocol.h/.cpp    # ヘッダ定数 + encode/parse
│  ├─ RtpSender.h/.cpp
│  ├─ RtpReceiver.h/.cpp
│  ├─ RtpProtocol.h/.cpp     # ヘッダ定数 + payload type テーブル + encode/parse
│  └─ pcmflowudp_version.h
├─ examples/
│  ├─ VbanMicToPc/           # ESP32 マイク → Windows の VBAN Receptor
│  ├─ RtpVoipG711/           # ESP32 ↔ SIP ソフトフォン (RTP PCMU)
│  └─ EspToEspRaw/           # ESP32 ↔ ESP32 の RAW UDP ループバック
├─ tests/
│  ├─ README.md / README.ja.md
│  ├─ conftest.py
│  ├─ pyproject.toml
│  ├─ smoke/
│  ├─ raw_loopback/          # RAW バイト往復
│  ├─ vban_header/           # byte-exact VBAN ヘッダ encode/parse
│  ├─ vban_loopback/         # PCM の VBAN 往復
│  ├─ rtp_header/            # byte-exact RTP ヘッダ encode/parse
│  ├─ rtp_loopback/          # L16 + コーデック payload の RTP 往復
│  └─ interop/               # 外部キャプチャ (VBAN Receptor / gst-rtp 等) → デコーダ
├─ doc/
│  └─ sibling_library_brief.md
├─ tools/
│  └─ bump_version.py
└─ .github/workflows/
   └─ release.yml
```

## 10. 上流コード取り込み

**無し**。VBAN の wire format は VB-Audio が公開、RTP は RFC 3550 / 3551 / 7587 で定義されている。PCMFlowUDP は両方を公開仕様から手書きしており、第三者コードは流用しない。

host 側テスト用には `lang-ship:host` Arduino core を使う。これは Berkeley socket ベースの `WiFiUDP` / `IPAddress` を提供し、ESP32 と API 互換。両ターゲットで同じ `#include <WiFiUdp.h>` が動くため、本リポ内に shim を vendor していない。

ライセンス整合性: 配布されるライブラリ本体 (`src/`) は **MIT、単一著者、第三者帰属表示不要**。

## 11. テスト

親 PCMFlow と同じ規約:

- pytest-embedded + Arduino CLI バックエンド
- 2 プロファイル: `lang-ship:host` (ロジック・host UDP loopback・大型 fixture・高速 CI) と `esp32:esp32:esp32` (実機検証・フットプリント測定)
- 機能ごとのテストディレクトリに `<feature>.ino` / `sketch.yaml` / `test_<feature>.py`
- `EXPECT_TRUE` / `EXPECT_EQ` / `EXPECT_NEAR` マクロと `TEST done N/M` Serial プロトコル

PCMFlowUDP 固有のテスト設計:

| テストディレクトリ | 対象 | 戦略 |
|---|---|---|
| `raw_loopback/` | RAW sink → stream の往復 | host: `127.0.0.1` の 2 つの `WiFiUDP` インスタンスでバイト一致を assert |
| `vban_header/` | byte-exact な VBAN ヘッダ encode/parse (Audio + Service) | 既知設定を渡し、ヘッダ各バイトを参照テーブルと突き合わせる |
| `vban_loopback/` | PCM → VBAN encode → VBAN decode → PCM、Service コールバック往復 | host: 2 つの `WiFiUDP`、PCM bit-exact 確認 |
| `rtp_header/` | 各対応 PT に対する byte-exact な RTP ヘッダ encode/parse | 既知設定 → 参照テーブルと突き合わせ |
| `rtp_loopback/` | L16 PCM 往復 + コーデック payload 往復 + sequence/timestamp 連続性 | host loopback、パケットごとのヘッダ値と payload 一致 |
| `interop/` | 外部キャプチャ (VBAN Receptor、`gst-rtp` 等) のデコード | 静的 `.bin` fixture でサンプル値とメタデータ確認 |

host プロファイルは `lang-ship:host` 1.0.6 以降を pin する (Berkeley socket `WiFiUDP` を同梱した最初のリリース)。

## 12. バージョニング

SemVer (`major.minor.patch`)、`library.properties` / `library.json` / `src/pcmflowudp_version.h` で管理。PCMFlow のバージョンとは独立。

## 13. ライセンスと商標

PCMFlowUDP: **MIT** ([LICENSE](LICENSE))。本リポ内に vendoring した第三者コードを一切含まない (`src/` は VBAN 公開仕様および RFC 3550 / 3551 / 7587 をベースに手書き、host 側 UDP は `lang-ship:host` Arduino core が外部から提供。§10 / §11 参照)。

**商標注記**。VBAN は **VB-Audio Software** が開発・公開しているプロトコル名。本ライブラリ中の「VBAN」表記 (`VbanSender` / `VbanReceiver` 等のクラス名を含む) は、本ライブラリが部分実装している線上プロトコルを識別するための **記述的・指示的 (nominative) 用法** であり、PCMFlowUDP は **VB-Audio Software と提携・推薦・後援関係にない**。また、本ライブラリは VBAN 仕様の **サブセット** のみ対応する (§6)。本書中に出てくる Voicemeeter / VBAN Receptor 等の名称は、各権利者の商標。
