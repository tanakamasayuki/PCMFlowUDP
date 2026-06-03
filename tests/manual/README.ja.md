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
uv run --env-file .env pytest manual/core2_rtp_buffer_tuning/core2_rtp_buffer_tuning.py -v -s --profile m5stack_core2
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
| VBAN 実ソフト送信 | VB-Audio Voicemeeter、VBAN Talkie |
| RTP 実ソフト送受信 | GStreamer `gst-launch-1.0`、`ffmpeg`、`ffplay`、VLC |
| packet capture | Wireshark、`tshark` |

Linux では次を入れておくと RTP と packet capture の確認ができます。

```sh
sudo apt install ffmpeg gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav wireshark tshark
```

Windows では MSYS2 UCRT64 と winget を使う例:

```sh
pacman -Syu

pacman -S \
  mingw-w64-ucrt-x86_64-ffmpeg \
  mingw-w64-ucrt-x86_64-gstreamer \
  mingw-w64-ucrt-x86_64-gst-plugins-base \
  mingw-w64-ucrt-x86_64-gst-plugins-good \
  mingw-w64-ucrt-x86_64-gst-plugins-bad \
  mingw-w64-ucrt-x86_64-gst-libav
```

```powershell
winget install WiresharkFoundation.Wireshark
winget install VBurel.VBAN.Receptor
winget install VBurel.VBAN.Talkie
```

導入後の確認:

```sh
gst-launch-1.0 --version
gst-inspect-1.0 rtppcmupay
gst-inspect-1.0 rtpg722pay
gst-inspect-1.0 rtpopuspay
gst-inspect-1.0 opusenc
gst-inspect-1.0 avenc_g722
ffmpeg -version
```

```powershell
winget list VBAN
```

VBAN の実ソフト確認は Windows PC + VB-Audio VBAN Receptor / Voicemeeter / VBAN Talkie を推奨します。`core2_voicemeeter_to_speaker/` は Voicemeeter 前提の名前ですが、VBAN Talkie の送信機能でも代替確認できます。Core2/CoreS3 と PC は同じ LAN に置き、PC firewall で UDP 6980 (VBAN) と UDP 5004 (RTP 例) を許可します。

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
| `core2_rtp_buffer_tuning/` | Core2 ボタンで RTP/L16 のプリバッファと再生 chunk preset を切り替えて調整 | M5Stack Core2/CoreS3 + Linux/Windows PC + GStreamer | 追加済み |
| `core2_rtp_g711_gstreamer/` | GStreamer から送った RTP/PCMU を DUT が G711 decode して再生 | M5Stack Core2/CoreS3 + Linux PC | 追加済み |
| `core2_rtp_g722_gstreamer/` | GStreamer から送った RTP/G.722 を DUT が G722 decode して再生 | M5Stack Core2/CoreS3 + Linux PC | 追加済み |
| `core2_rtp_opus_gstreamer/` | GStreamer から送った RTP/Opus を DUT が Opus decode して再生 | M5Stack Core2/CoreS3 + Linux PC | 追加済み |
| `core2_stability/` | 30 分の UDP audio stream 継続、drop、heap、Wi-Fi 状態確認 | M5Stack Core2/CoreS3 + Wi-Fi AP | 追加済み |

## 実ソフトのコマンド例

PC から Core2 へ RTP/L16 mono を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! volume volume=0.5 \
  ! audioconvert \
  ! audio/x-raw,format=S16BE,rate=16000,channels=1 \
  ! rtpL16pay \
  ! udpsink host=<core2-ip> port=5004
```

GStreamer はこの 16 kHz L16 stream を dynamic PT 96 として送ることが多いため、`core2_rtp_gstreamer` は PT 96 を L16 として受けます。

`core2_rtp_buffer_tuning/` では 5 種類の preset を比較します。`p0-low` は初期 20 ms、`p1-voip` は初期 40 ms + 20 ms chunk、`p2-balanced` は初期 40 ms + 40 ms chunk、`p3-safe` は初期 60 ms、`p4-stable` は初期 80 ms です。GStreamer コマンドを動かしたまま Core2 のボタンを押します。Button A/B は `TUNE ... state=RUNNING` が出てから押してください。preset 変更時は意図的に再生を止めて `PREFILLING` を経由するため、その切替中の無音や途切れは評価対象外です。preset ごとに、`RUNNING` 後の聴感 gap、restart 後に安定するまでの秒数、`drop`、`empty`、`wait`、最後の `TUNE` 行、メモを残します。採用候補は、音が連続して counter が安定する最小遅延 preset を優先し、`late_delta` は参考値として扱います。preset 比較が終わってから GStreamer を止め、pytest 側で Enter を押して終了します。

PC から Core2 へ RTP/PCMU を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! volume volume=0.5 \
  ! audioconvert \
  ! audio/x-raw,rate=8000,channels=1 \
  ! audioconvert \
  ! mulawenc \
  ! rtppcmupay pt=0 \
  ! udpsink host=<core2-ip> port=5004
```

PC から Core2 へ RTP/G.722 を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! volume volume=0.5 \
  ! audioconvert \
  ! audio/x-raw,format=S16LE,rate=16000,channels=1 \
  ! audioconvert \
  ! avenc_g722 \
  ! rtpg722pay pt=9 \
  ! udpsink host=<core2-ip> port=5004
```

PC から DUT へ RTP/Opus を送る例:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! volume volume=0.5 \
  ! audioconvert \
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

packet capture は pcapng を保存し、必要なときにテキスト確認します。テスト別の保存ファイル名は `tests/TEST_PLAN.ja.md` を参照してください。

保存用:

```sh
tshark -i any -f "udp port 6980 or udp port 5004" -w core2_stability.pcapng
```

確認用:

```sh
tshark -r core2_stability.pcapng -Y "udp.port == 6980 || rtp || udp.port == 5004" \
  -T fields -e frame.time_relative -e ip.src -e ip.dst -e udp.srcport -e udp.dstport -e udp.length -e rtp.p_type -e rtp.seq -e rtp.timestamp
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
- Core2 speaker 系テストは 20 ms packet を受信し、初回 80 ms プリバッファ後に 40 ms chunk で `M5.Speaker.playRaw()` へ渡します。これは M5Unified speaker queue のアンダーランと buffer 寿命問題を避けるためです。
- packet jitter の吸収は PCMFlowUDP 側、`playRaw()` の非同期 queue 管理は app/helper 側の責務として扱います。詳細は `SPEC.ja.md` の「受信バッファと遅延の責務」を参照してください。
