# PCMFlowUDP 手動テスト

> English: [README.md](README.md)

M5Stack Core2 など、実ハードウェアと人間による観測が必要なテストを置くディレクトリです。

自動テストで確認できる UDP packet の byte-level 互換性は `tests/*_python_loopback/` で扱います。この manual ディレクトリでは、Core2 のスピーカー、マイク、LCD、ボタン、Wi-Fi 実環境での連続動作を確認します。

詳細な計画は [../TEST_PLAN.ja.md](../TEST_PLAN.ja.md) を参照してください。

## 実行方法

手動テストの Python ファイルには `test_` プレフィックスを付けないため、pytest から自動収集されません。必要なハードウェアを準備してから明示的に実行します。

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

`-s` は必須です。シリアルログ、オペレーターへの確認プロンプト、テスト中の手順を端末に表示します。M5Stack CoreS3 で実行する場合は `--profile m5stack_cores3` を指定します。

## 推奨環境

基準判定は pytest + Python helper で行い、実ソフト相互運用は manual テストとして別に実行します。

| 用途 | 推奨ツール |
|---|---|
| Core2 flash / serial / 自動判定 | `uv run pytest`、`pytest-embedded`、Arduino CLI |
| VBAN 実ソフト受信 | VB-Audio VBAN Receptor |
| VBAN 実ソフト送信 | VB-Audio Voicemeeter |
| RTP 実ソフト送受信 | GStreamer `gst-launch-1.0`、`ffmpeg`、`ffplay`、VLC |
| packet capture | Wireshark、`tshark` |

Linux では次を入れておくと RTP と packet capture の確認ができます。

```sh
sudo apt install ffmpeg gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav wireshark tshark
```

VBAN の実ソフト確認は Windows PC + VB-Audio VBAN Receptor / Voicemeeter を推奨します。Core2 と PC は同じ LAN に置き、PC firewall で UDP 6980 (VBAN) と UDP 5004 (RTP 例) を許可します。

## 予定テスト

| テスト | 説明 | 必要なハードウェア | 状態 |
|---|---|---|---|
| `core2_smoke/` | Core2/CoreS3 のビルド、フラッシュ、Serial、LCD、ボタン、Wi-Fi 接続確認 | M5Stack Core2 または CoreS3 | 追加済み |
| `core2_raw_udp_ping/` | Python と DUT の RAW UDP 往復確認 | M5Stack Core2/CoreS3 + Wi-Fi AP | 追加済み |
| `core2_vban_speaker/` | Python から送った VBAN PCM16 を DUT スピーカーで再生 | M5Stack Core2/CoreS3 + Wi-Fi AP | 追加済み |
| `core2_vban_mic/` | DUT マイク入力を VBAN PCM16 として Python が受信 | M5Stack Core2/CoreS3 + Wi-Fi AP | 追加済み |
| `core2_vban_receptor/` | DUT マイク入力を VBAN Receptor / Voicemeeter で受信・再生 | M5Stack Core2/CoreS3 + Windows PC + VBAN Receptor / Voicemeeter | 追加済み |
| `core2_voicemeeter_to_speaker/` | Voicemeeter から送った VBAN stream を DUT スピーカーで再生 | M5Stack Core2/CoreS3 + Windows PC + Voicemeeter | 追加済み |
| `core2_rtp_speaker/` | Python から送った RTP audio を DUT スピーカーで再生 | M5Stack Core2/CoreS3 + Wi-Fi AP | 追加済み |
| `core2_rtp_mic/` | DUT マイク入力を RTP audio として Python が受信 | M5Stack Core2/CoreS3 + Wi-Fi AP | 追加済み |
| `core2_rtp_gstreamer/` | GStreamer から送った RTP/L16 を DUT が再生 | M5Stack Core2/CoreS3 + Linux PC | 追加済み |
| `core2_rtp_g711_gstreamer/` | GStreamer から送った RTP/PCMU を DUT が G711 decode して再生 | M5Stack Core2/CoreS3 + Linux PC | 追加済み |
| `core2_rtp_g722_gstreamer/` | GStreamer から送った RTP/G.722 を DUT が G722 decode して再生 | M5Stack Core2/CoreS3 + Linux PC | 追加済み |
| `core2_rtp_opus_gstreamer/` | GStreamer から送った RTP/Opus を DUT が Opus decode して再生 | M5Stack Core2/CoreS3 + Linux PC | 追加済み |
| `core2_stability/` | 30 分の UDP audio stream 継続、drop、heap、Wi-Fi 状態確認 | M5Stack Core2/CoreS3 + Wi-Fi AP | 追加済み |

## 実ソフトのコマンド例

PC から Core2 へ RTP/L16 mono を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16BE,rate=16000,channels=1 \
  ! rtpL16pay pt=11 \
  ! udpsink host=<core2-ip> port=5004
```

PC から Core2 へ RTP/PCMU を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,rate=8000,channels=1 \
  ! audioconvert \
  ! mulawenc \
  ! rtppcmupay pt=0 \
  ! udpsink host=<core2-ip> port=5004
```

PC から Core2 へ RTP/G.722 を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16LE,rate=16000,channels=1 \
  ! audioconvert \
  ! avenc_g722 \
  ! rtpg722pay pt=9 \
  ! udpsink host=<core2-ip> port=5004
```

PC から DUT へ RTP/Opus を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16LE,rate=48000,channels=1 \
  ! audioconvert \
  ! opusenc bitrate=24000 frame-size=20 audio-type=voice \
  ! rtpopuspay pt=96 \
  ! udpsink host=<dut-ip> port=5004
```

Core2 から PC へ RTP/L16 を送り、PC 側で再生する例:

```sh
gst-launch-1.0 -v udpsrc port=5004 \
  caps="application/x-rtp,media=audio,clock-rate=16000,encoding-name=L16,channels=1,payload=11" \
  ! rtpL16depay \
  ! audioconvert \
  ! autoaudiosink
```

packet capture:

```sh
tshark -i any -f "udp port 6980 or udp port 5004" -Y "udp.port == 6980 || rtp || udp.port == 5004"
```

長時間 stability を短縮して試す例:

```sh
CORE2_STABILITY_SECONDS=60 uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

## 判定方針

- Packet、format、sequence、timestamp、RMS、heap などソフトウェアで観測できるものは pytest で自動判定します。
- スピーカー音、LCD 表示、物理ボタンなどはオペレーター確認にします。
- オペレーター確認は、具体的な期待結果を表示して `y` / `n` で判定します。
- 1 つの `.py` には原則 1 つのテスト関数だけを置き、フラッシュ後の状態共有による副作用を避けます。
