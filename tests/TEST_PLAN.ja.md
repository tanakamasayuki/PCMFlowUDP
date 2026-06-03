# PCMFlowUDP 実機テスト計画

> English: [TEST_PLAN.md](TEST_PLAN.md)

この文書は、M5Stack Core2 / CoreS3 を使って PCMFlowUDP を実機検証するためのテスト計画です。

既存の `tests/*_python_loopback/` は ESP32 DUT と Python peer の UDP 相互運用を自動検証する。Core2/CoreS3 で追加する価値は、M5Stack 固有の入出力、つまり I2S スピーカー、マイク、LCD、ボタン、バッテリー動作、Wi-Fi 実環境での連続動作を含めた確認にある。そのため実機テストは `tests/manual/` に配置し、pytest の自動収集対象にはしない。

## 方針

- `tests/manual/<name>/<name>.py` と `<name>.ino` と `sketch.yaml` の 3 ファイル構成にする。
- Python ファイル名には `test_` プレフィックスを付けない。必要なハードウェアを準備して明示実行する。
- 実行時は `-s` を付け、シリアルログ、LCD 表示、音声、ボタン操作をオペレーターが確認できるようにする。
- ソフトウェアで判定できるものは `dut.expect()` で自動判定する。音の聴感、画面表示、物理ボタンなどは最小限の `y/n` 入力で確認する。
- Wi-Fi 設定は既存テストと同じく `.env` から `WIFI_SSID` / `WIFI_PASSWORD` を build define として渡す。
- M5Stack 依存テストは `sketch.yaml` の profile を `m5stack_core2` / `m5stack_cores3` に分け、通常の `esp32` profile と混ぜない。

## 必要な機材

- M5Stack Core2 または CoreS3
- USB-C ケーブル
- 2.4 GHz Wi-Fi AP
- pytest 実行用 PC
- PC 側 VBAN/RTP/RAW UDP 送受信用ツール
- 任意: 録音または音量確認用の外部マイク

## 推奨ツール

Core2 manual テストでは、まず pytest + Python helper で packet level の期待値を固定し、その後に実ソフトとの相互運用を確認する。実ソフトだけで確認すると合否が聴感や UI 表示に寄りやすいため、失敗時の切り分けには Python helper と packet capture を併用する。

| 用途 | 推奨ツール | 役割 |
|---|---|---|
| 基準判定 | `uv run pytest` + `tests/net_helpers/` | 既知 packet の送受信、RMS、sequence、timestamp、format を自動判定 |
| VBAN 受信 | VB-Audio VBAN Receptor | Core2 mic → PC playback の実ソフト確認 |
| VBAN 送信 | VB-Audio Voicemeeter | PC audio → Core2 speaker の実ソフト確認 |
| RTP 送信/受信 | GStreamer `gst-launch-1.0` | L16/PCMU/PCMA/G.722 など RTP payload を明示して流す |
| RTP 簡易送信 | `ffmpeg` | sine wave や音声ファイルを RTP/UDP に流す補助確認 |
| RTP 簡易受信 | `ffplay` / VLC | Core2 からの RTP audio を人間が聴く補助確認 |
| packet 確認 | Wireshark / `tshark` | VBAN は UDP 6980、RTP は UDP 5004 などを capture して header と payload を確認 |

Linux 開発機では次のパッケージを入れておく。

```sh
sudo apt install ffmpeg gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav wireshark tshark
```

Windows では VBAN Receptor / Voicemeeter を入れ、必要なら Wireshark と ffmpeg を追加する。VBAN の実ソフト確認は Windows を推奨環境にする。Linux/macOS では pytest helper と packet capture を主経路にし、RTP は GStreamer/ffmpeg で確認する。

## ネットワーク前提

- Core2 と PC は同じ L2 セグメントに置く。
- AP の client isolation は無効にする。
- PC firewall で UDP 6980 (VBAN) と UDP 5004 (RTP 例) を許可する。
- multicast はこのライブラリの対象外なので、VBAN fan-out が必要な場合は broadcast または明示 IP を使う。
- packet capture は最初に `udp port 6980 or udp port 5004` で開始し、問題が出たら capture を fixture 化して `tests/interop/captures/` に残す。

## 実行方法

```sh
cd tests
uv run --env-file .env pytest manual/core2_smoke/core2_smoke.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_raw_udp_ping/core2_raw_udp_ping.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_vban_speaker/core2_vban_speaker.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_vban_mic/core2_vban_mic.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_vban_receptor/core2_vban_receptor.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_voicemeeter_to_speaker/core2_voicemeeter_to_speaker.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_speaker/core2_rtp_speaker.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_mic/core2_rtp_mic.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_gstreamer/core2_rtp_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_g711_gstreamer/core2_rtp_g711_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_g722_gstreamer/core2_rtp_g722_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_opus_gstreamer/core2_rtp_opus_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

CoreS3 で実行する場合は `--profile m5stack_cores3` を指定する。各 `sketch.yaml` は Core2/CoreS3 の profile を持つ。

```yaml
profiles:
  m5stack_core2:
    fqbn: esp32:esp32:m5stack_core2
    platforms:
      - platform: esp32:esp32 (3.3.8)
        platform_index_url: https://espressif.github.io/arduino-esp32/package_esp32_index.json
    libraries:
      - dir: ../../../
      - PCMFlow (0.2.1)
      - M5Unified (0.2.16)
      - M5GFX (0.2.22)

  m5stack_cores3:
    fqbn: esp32:esp32:m5stack_cores3
    platforms:
      - platform: esp32:esp32 (3.3.8)
        platform_index_url: https://espressif.github.io/arduino-esp32/package_esp32_index.json
    libraries:
      - dir: ../../../
      - PCMFlow (0.2.1)
      - M5Unified (0.2.16)
      - M5GFX (0.2.22)

default_profile: m5stack_core2
```

Core2 の FQBN は `esp32:esp32:m5stack_core2`、CoreS3 の FQBN は `esp32:esp32:m5stack_cores3` に固定する。デバイス制御は `M5Unified` を使い、表示系依存として `M5GFX` も pin する。

## 確定した音声パラメータ

| 経路 | 形式 | sample rate | channels | chunk / packet | 理由 |
|---|---|---:|---:|---:|---|
| VBAN PCM 基準 | PCM16 LE | 16000 Hz | 1 | 256 frames | 既存 example / test と同じ。VBAN table 対応、Core2 mic/speaker と Wi-Fi 負荷のバランスが良い |
| RTP L16 基準 | PCM16 BE on wire, PT 11 | 16000 Hz | 1 | 320 frames (20 ms) | `RtpSender` の既定 packetization と一致。GStreamer でも扱いやすい |
| RTP PCMU 基準 | G.711 μ-law, PT 0 | 8000 Hz | 1 | 160 samples (20 ms) | RFC 3551 の static payload type と既存 `RtpVoipG711` example に合わせる |
| RTP G.722 基準 | G.722, PT 9 | 16000 Hz audio / 8000 Hz RTP clock | 1 | 160 bytes (20 ms) | RFC 3551 の G.722 timestamp 慣行と `PCMFlowG722` の 16 kHz PCM API に合わせる |
| RTP Opus 基準 | Opus, dynamic PT 96 | 48000 Hz RTP clock | 1 | 20 ms packet | GStreamer 標準の RTP/Opus と `PCMFlowOpus` の decode path を確認 |
| VBAN / RTP stereo 補助 | PCM16 | 48000 Hz | 2 | 64-256 frames | 実ソフト互換の追加確認用。最初の Core2 実装対象にはしない |

manual テストの codec 相互接続は G711、G722、Opus を対象にする。codec の詳細な正しさは各 codec ライブラリ側の自動テストに任せ、この repo では RTP payload として外部ソフトから受けて decode/playback できることを確認する。

スピーカー系テストの合否は当面、人間による確認を正式な判定方法にする。外部オーディオループバック機器がある環境では RMS / peak / 周波数検出で自動判定を追加できるが、必須条件にはしない。

## テスト一覧

| テスト | 目的 | 判定方法 | 状態 |
|---|---|---|---|
| `core2_smoke/` | Core2 でビルド、フラッシュ、Serial、LCD、ボタン、Wi-Fi 接続が動くことを確認する | `dut.expect()` + ボタン操作確認 | 追加済み |
| `core2_raw_udp_ping/` | Python から DUT へ RAW UDP packet を送り、DUT から ACK が返ることを確認する | 完全自動 | 追加済み |
| `core2_vban_speaker/` | Python が生成した VBAN PCM16 sine wave を Core2 が受信し、スピーカーから再生できることを確認する | packet 受信は自動、音声は人間確認 | 追加済み |
| `core2_vban_mic/` | Core2 のマイク入力を VBAN PCM16 として Python が受信し、無音でないサンプルを観測できることを確認する | Python で RMS/peak 判定 | 追加済み |
| `core2_vban_receptor/` | Core2 のマイク入力を VBAN Receptor / Voicemeeter に送り、実ソフトで受信・再生できることを確認する | DUT 統計 + 実ソフト UI/音声確認 | 追加済み |
| `core2_voicemeeter_to_speaker/` | Voicemeeter から送った VBAN stream を Core2 が受信し、スピーカーで再生できることを確認する | DUT 統計 + 音声確認 | 追加済み |
| `core2_rtp_speaker/` | RTP payload を Core2 が受信し、スピーカーから再生できることを確認する | packet 受信は自動、音声は人間確認 | 追加済み |
| `core2_rtp_mic/` | Core2 のマイク入力を RTP payload として Python が受信できることを確認する | Python で sequence/timestamp/RMS 判定 | 追加済み |
| `core2_rtp_gstreamer/` | GStreamer から送った RTP/L16 を Core2 が再生できることを確認する | tool log + DUT 統計 + 音声確認 | 追加済み |
| `core2_rtp_g711_gstreamer/` | GStreamer から送った RTP/PCMU を Core2 が G711 decode して再生できることを確認する | tool log + DUT 統計 + 音声確認 | 追加済み |
| `core2_rtp_g722_gstreamer/` | GStreamer から送った RTP/G.722 を Core2 が G722 decode して再生できることを確認する | tool log + DUT 統計 + 音声確認 | 追加済み |
| `core2_rtp_opus_gstreamer/` | GStreamer から送った RTP/Opus dynamic PT を DUT が Opus decode して再生できることを確認する | build size + runtime heap + 音声確認 | 追加済み |
| `core2_stability/` | Wi-Fi と UDP 送受信を 30 分継続し、drop、heap 低下、再接続不能がないことを確認する | シリアル統計を自動判定 | 追加済み |

## 各テストの詳細

### core2_smoke

目的:
Core2 固有の manual テスト基盤が動くことを確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_smoke/core2_smoke.py -v -s --profile m5stack_core2
```

手順:
1. Core2 を USB-C で PC に接続する。
2. `.env` にシリアルポートと Wi-Fi 認証情報を設定する。
3. pytest を `-s` 付きで実行する。
4. スケッチは LCD に IP アドレス、ボタン状態、Wi-Fi RSSI を表示する。
5. オペレーターが A/B/C ボタンを順に押す。

合格条件:
- Serial に `CORE2-READY ip=<addr>` が出る。
- LCD に IP アドレスが表示される。
- A/B/C ボタン押下ごとに Serial に `BUTTON A` / `BUTTON B` / `BUTTON C` が出る。

### core2_raw_udp_ping

目的:
Core2 の Wi-Fi 実ネットワーク上で `RawUdpStream` / `RawUdpSink` の最小送受信経路を確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_raw_udp_ping/core2_raw_udp_ping.py -v -s --profile m5stack_core2
```

手順:
1. DUT が `DUT-READY ip=<addr> port=<port>` を出す。
2. Python が 64 byte の既知 payload を DUT に送る。
3. DUT は payload の CRC32 と長さを Serial に出し、同じ payload を Python に返す。

合格条件:
- Python が送信 payload と同一の ACK payload を受信する。
- DUT の Serial に `RAW-RX len=64 crc=<expected>` が出る。

### core2_vban_speaker

目的:
VBAN 受信、PCM16 デコード、Core2 スピーカー出力の一連の経路を確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_vban_speaker/core2_vban_speaker.py -v -s --profile m5stack_core2
```

手順:
1. Python が 1 kHz sine wave の VBAN packet を 5 秒間送信する。
2. DUT は受信 packet 数、sample rate、channel 数、drop 数を Serial と LCD に表示する。
3. オペレーターがスピーカーから一定音が聞こえるか確認する。

合格条件:
- DUT が `VBAN-RX rate=16000 channels=1 packets=<n> drops=0` を出す。
- オペレーターが 5 秒間の一定音を確認し、pytest の確認プロンプトに `y` を入力する。

### core2_vban_mic

目的:
Core2 マイク入力を VBAN sender 経由で PC 側 Python が受信できることを確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_vban_mic/core2_vban_mic.py -v -s --profile m5stack_core2
```

手順:
1. DUT は Python が指定した IP/port へ VBAN PCM16 mono を送る。
2. Python は 5 秒間 packet を受信し、sequence、stream name、format、RMS、peak を集計する。
3. オペレーターは Core2 に向けて短く発声または拍手する。

合格条件:
- Python が 5 秒間に 100 packet 以上受信する。
- frame counter が単調増加する。
- RMS が無音しきい値を超える区間が 1 回以上ある。
- DUT が `VBAN-TX packets=<n>` を出す。

### core2_vban_receptor

目的:
Core2 マイク入力を実ソフトの VBAN Receptor または Voicemeeter で受信・再生できることを確認する。

推奨環境:
- Windows PC
- VB-Audio VBAN Receptor または Voicemeeter
- Core2 と Windows PC が同一 LAN

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_vban_receptor/core2_vban_receptor.py -v -s --profile m5stack_core2
```

手順:
1. Windows PC の IP アドレスを確認する。
2. VBAN Receptor または Voicemeeter の VBAN inbound を有効にし、UDP port 6980 を待ち受ける。
3. DUT の送信先を Windows PC の IP、port 6980、stream name `Core2Mic` にする。
4. pytest を実行し、DUT が `VBAN-TX stream=Core2Mic rate=16000 channels=1 dest=<pc-ip>:6980` を出すことを確認する。
5. オペレーターは Core2 に向けて発声し、実ソフト側の meter と音声出力を確認する。

合格条件:
- DUT が `VBAN-TX packets=<n> drops=0` を継続して出す。
- VBAN Receptor / Voicemeeter 側に `Core2Mic` stream が表示される。
- meter が入力音に反応し、PC から Core2 マイク音が聞こえる。

補助コマンド:

```sh
tshark -i any -f "udp port 6980" -Y "udp.port == 6980" -T fields -e ip.src -e ip.dst -e udp.length -e data.data
```

### core2_voicemeeter_to_speaker

目的:
Voicemeeter から送信した VBAN PCM stream を Core2 が受信し、Core2 スピーカーで再生できることを確認する。

推奨環境:
- Windows PC
- VB-Audio Voicemeeter
- Core2 と Windows PC が同一 LAN

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_voicemeeter_to_speaker/core2_voicemeeter_to_speaker.py -v -s --profile m5stack_core2
```

手順:
1. DUT が `DUT-READY ip=<core2-ip> port=6980 stream=PcToCore2` を出す。
2. Voicemeeter の VBAN outbound stream を有効にし、送信先を `<core2-ip>:6980` にする。
3. stream name を `PcToCore2`、format を PCM16、mono または stereo、sample rate を 16000 Hz または 48000 Hz にする。
4. PC 側で tone または音声ファイルを再生する。
5. オペレーターは Core2 スピーカーから音が出ることを確認する。

合格条件:
- DUT が `VBAN-RX stream=PcToCore2 rate=<rate> channels=<n> packets=<n> drops=0` を出す。
- Core2 スピーカーから PC 側音声が聞こえる。

補助コマンド:

```sh
tshark -i any -f "udp port 6980" -Y "udp.port == 6980" -T fields -e ip.src -e ip.dst -e udp.length
```

### core2_rtp_speaker

目的:
RTP 受信、payload decode、Core2 スピーカー出力の経路を確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_speaker/core2_rtp_speaker.py -v -s --profile m5stack_core2
```

手順:
1. Python が RTP packet として 1 kHz sine wave を 5 秒間送信する。
2. DUT は sequence、timestamp、packet 数、drop 数を Serial と LCD に表示する。
3. オペレーターがスピーカー音を確認する。

合格条件:
- DUT が `RTP-RX packets=<n> drops=0` を出す。
- オペレーターが 5 秒間の一定音を確認し、pytest の確認プロンプトに `y` を入力する。

### core2_rtp_mic

目的:
Core2 マイク入力を RTP sender 経由で PC 側 Python が受信できることを確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_mic/core2_rtp_mic.py -v -s --profile m5stack_core2
```

手順:
1. DUT は Python が指定した IP/port へ RTP payload を送る。
2. Python は sequence と timestamp の連続性、payload サイズ、RMS を検査する。
3. オペレーターは Core2 に向けて短く発声または拍手する。

合格条件:
- Python が 5 秒間に 100 packet 以上受信する。
- sequence が単調増加する。
- timestamp の増分が payload sample 数と一致する。
- RMS が無音しきい値を超える区間が 1 回以上ある。

### core2_rtp_gstreamer

目的:
GStreamer から送った RTP/L16 mono を DUT が受信し、PCMFlowUDP の L16 path で再生できることを確認する。codec payload は `core2_rtp_g711_gstreamer/`、`core2_rtp_g722_gstreamer/`、`core2_rtp_opus_gstreamer/` で分けて確認する。

推奨環境:
- Linux PC: GStreamer、ffmpeg、ffplay、Wireshark/tshark
- 任意: VLC または SIP softphone
- Core2 と PC が同一 LAN

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_gstreamer/core2_rtp_gstreamer.py -v -s --profile m5stack_core2
```

PC から Core2 speaker へ L16 mono RTP を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16BE,rate=16000,channels=1 \
  ! rtpL16pay pt=11 \
  ! udpsink host=<core2-ip> port=5004
```

### core2_rtp_g711_gstreamer

目的:
GStreamer から送った RTP/PCMU を Core2 が受信し、`PCMFlowG711` で decode して再生できることを確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_g711_gstreamer/core2_rtp_g711_gstreamer.py -v -s --profile m5stack_core2
```

PC から Core2 speaker へ PCMU RTP を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,rate=8000,channels=1 \
  ! audioconvert \
  ! mulawenc \
  ! rtppcmupay pt=0 \
  ! udpsink host=<core2-ip> port=5004
```

音声ファイルを RTP/PCMU で送る ffmpeg 例:

```sh
ffmpeg -re -i input.wav -ac 1 -ar 8000 -c:a pcm_mulaw -f rtp rtp://<core2-ip>:5004
```

### core2_rtp_g722_gstreamer

目的:
GStreamer から送った RTP/G.722 を Core2 が受信し、`PCMFlowG722` で decode して再生できることを確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_g722_gstreamer/core2_rtp_g722_gstreamer.py -v -s --profile m5stack_core2
```

PC から Core2 speaker へ G.722 RTP を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16LE,rate=16000,channels=1 \
  ! audioconvert \
  ! avenc_g722 \
  ! rtpg722pay pt=9 \
  ! udpsink host=<core2-ip> port=5004
```

### core2_rtp_opus_gstreamer

目的:
GStreamer から送った RTP/Opus dynamic PT 96 を DUT が受信し、`PCMFlowOpus` で decode して再生できることを確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_opus_gstreamer/core2_rtp_opus_gstreamer.py -v -s --profile m5stack_core2
```

PC から DUT speaker へ Opus RTP を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16LE,rate=48000,channels=1 \
  ! audioconvert \
  ! opusenc bitrate=24000 frame-size=20 audio-type=voice \
  ! rtpopuspay pt=96 \
  ! udpsink host=<dut-ip> port=5004
```

Core2 mic から PC へ RTP/L16 を送り、GStreamer で再生する例:

```sh
gst-launch-1.0 -v udpsrc port=5004 \
  caps="application/x-rtp,media=audio,clock-rate=16000,encoding-name=L16,channels=1,payload=11" \
  ! rtpL16depay \
  ! audioconvert \
  ! autoaudiosink
```

Core2 mic から PC へ RTP/PCMU を送り、GStreamer で再生する例:

```sh
gst-launch-1.0 -v udpsrc port=5004 \
  caps="application/x-rtp,media=audio,clock-rate=8000,encoding-name=PCMU,payload=0" \
  ! rtppcmudepay \
  ! mulawdec \
  ! audioconvert \
  ! autoaudiosink
```

packet capture:

```sh
tshark -i any -f "udp port 5004" -Y "rtp || udp.port == 5004"
```

合格条件:
- PC → Core2 では DUT が `RTP-RX pt=<pt> packets=<n> drops=0` を出し、Core2 スピーカーから音が聞こえる。
- Core2 → PC では GStreamer/ffplay/VLC が音を再生できる。
- tshark または Wireshark で RTP header の payload type、sequence、timestamp が継続している。

### core2_stability

目的:
Core2 の Wi-Fi 実環境で UDP audio stream を継続したとき、heap 漏れ、packet drop、Wi-Fi 切断後の復帰不能がないことを確認する。

実行コマンド:

```sh
cd tests
uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

短時間で動作確認する場合:

```sh
cd tests
CORE2_STABILITY_SECONDS=60 uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

手順:
1. Python が VBAN または RTP の sine wave を 30 分送信する。
2. DUT は 10 秒ごとに packet 数、drop 数、free heap、RSSI を Serial に出す。
3. Python は統計行を収集し、しきい値を超えた異常を検出する。

合格条件:
- 30 分間 `WIFI_ERROR` が出ない。
- drop 率が 0.1% 未満。
- free heap の最小値が開始直後から 10% 以上減少し続けない。
- DUT が最後に `STABILITY done` を出す。

## manual に分類する理由

| カテゴリ | 理由 |
|---|---|
| Core2 スピーカー出力 | 実際に音が聞こえるかは PC から直接観測できない。オーディオループバック機器がない限り人間確認が必要 |
| Core2 マイク入力 | 実マイク、筐体、音量、周辺ノイズの影響を受ける。完全な CI 再現が難しい |
| LCD / ボタン | 物理画面と物理ボタンを含むため、通常の CI では観測できない |
| Wi-Fi 長時間動作 | AP、RSSI、混雑、電源状態の影響を受ける。CI の短時間 UDP ループバックとは別に実環境確認が必要 |

## 実装順序

1. `tests/manual/README.ja.md` を追加し、実行方法とテスト一覧を置く。
2. `core2_smoke/` を追加し、フラッシュ、Serial、LCD、ボタン、Wi-Fi を確認する。
3. `core2_raw_udp_ping/` を追加し、音声なしの UDP 実機経路を安定させる。
4. `core2_vban_speaker/` と `core2_vban_mic/` を追加する。
5. `core2_vban_receptor/` と `core2_voicemeeter_to_speaker/` を追加し、VB-Audio 実ソフトとの相互運用を確認する。
6. `core2_rtp_speaker/` と `core2_rtp_mic/` を追加する。
7. `core2_rtp_gstreamer/` と codec 別 GStreamer テストを追加し、GStreamer/ffmpeg/VLC との相互運用を確認する。
8. `core2_stability/` を追加し、短時間の機能確認とは別に長時間統計を確認する。

## 決定事項

- Core2 の Arduino FQBN は `esp32:esp32:m5stack_core2`、CoreS3 は `esp32:esp32:m5stack_cores3` とする。
- Core2/CoreS3 の推奨デバイスライブラリは `M5Unified` とする。
- Core2/CoreS3 profile では `M5Unified (0.2.16)` と `M5GFX (0.2.22)` を pin する。
- manual テストの基準 PCM 音声形式は `16000 Hz / mono / PCM16` とする。
- VBAN の基準 chunk は `256 frames` とする。
- RTP/L16 の基準 packet は `16000 Hz / 20 ms / 320 frames / PT 11` とする。
- RTP/PCMU の基準 packet は `8000 Hz / 20 ms / 160 samples / PT 0` とする。
- RTP/G.722 の基準 packet は `PT 9 / 20 ms / 160 bytes` とする。
- RTP/Opus の基準 packet は `PT 96 / 48000 Hz RTP clock / 20 ms` とする。
- スピーカー系テストは人間確認を正式判定とし、外部オーディオループバックによる自動判定は任意の拡張扱いにする。
