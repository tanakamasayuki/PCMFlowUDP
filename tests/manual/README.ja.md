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
uv run --env-file .env pytest manual/core2_vban_speaker/core2_vban_speaker.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_vban_mic/core2_vban_mic.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_vban_receptor/core2_vban_receptor.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_voicemeeter_to_speaker/core2_voicemeeter_to_speaker.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_speaker/core2_rtp_speaker.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_gstreamer/core2_rtp_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

`-s` は必須です。シリアルログ、オペレーターへの確認プロンプト、テスト中の手順を端末に表示します。

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
sudo apt install ffmpeg gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad wireshark tshark
```

VBAN の実ソフト確認は Windows PC + VB-Audio VBAN Receptor / Voicemeeter を推奨します。Core2 と PC は同じ LAN に置き、PC firewall で UDP 6980 (VBAN) と UDP 5004 (RTP 例) を許可します。

## 予定テスト

| テスト | 説明 | 必要なハードウェア | 状態 |
|---|---|---|---|
| `core2_smoke/` | Core2 のビルド、フラッシュ、Serial、LCD、ボタン、Wi-Fi 接続確認 | M5Stack Core2 | 追加済み |
| `core2_raw_udp_ping/` | Python と Core2 の RAW UDP 往復確認 | M5Stack Core2 + Wi-Fi AP | 計画 |
| `core2_vban_speaker/` | Python から送った VBAN PCM16 を Core2 スピーカーで再生 | M5Stack Core2 + Wi-Fi AP | 計画 |
| `core2_vban_mic/` | Core2 マイク入力を VBAN PCM16 として Python が受信 | M5Stack Core2 + Wi-Fi AP | 計画 |
| `core2_vban_receptor/` | Core2 マイク入力を VBAN Receptor / Voicemeeter で受信・再生 | M5Stack Core2 + Windows PC + VBAN Receptor / Voicemeeter | 計画 |
| `core2_voicemeeter_to_speaker/` | Voicemeeter から送った VBAN stream を Core2 スピーカーで再生 | M5Stack Core2 + Windows PC + Voicemeeter | 計画 |
| `core2_rtp_speaker/` | Python から送った RTP audio を Core2 スピーカーで再生 | M5Stack Core2 + Wi-Fi AP | 計画 |
| `core2_rtp_mic/` | Core2 マイク入力を RTP audio として Python が受信 | M5Stack Core2 + Wi-Fi AP | 計画 |
| `core2_rtp_gstreamer/` | GStreamer/ffmpeg/VLC など標準 RTP ツールと Core2 の相互運用 | M5Stack Core2 + Linux PC または RTP 対応ソフト | 計画 |
| `core2_stability/` | 30 分の UDP audio stream 継続、drop、heap、Wi-Fi 状態確認 | M5Stack Core2 + Wi-Fi AP | 計画 |

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
  ! mulawenc \
  ! rtppcmupay pt=0 \
  ! udpsink host=<core2-ip> port=5004
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

## 判定方針

- Packet、format、sequence、timestamp、RMS、heap などソフトウェアで観測できるものは pytest で自動判定します。
- スピーカー音、LCD 表示、物理ボタンなどはオペレーター確認にします。
- オペレーター確認は、具体的な期待結果を表示して `y` / `n` で判定します。
- 1 つの `.py` には原則 1 つのテスト関数だけを置き、フラッシュ後の状態共有による副作用を避けます。
