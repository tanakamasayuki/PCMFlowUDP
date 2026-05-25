# PCMFlowUDP — Maintainer TODO (リリース前)

> このファイルはリリース前に消す前提のメンテナ作業メモ。コードベース側で潰せる作業は通常の commit で進め、人手・実機・PC ツールが要る作業だけここに残す。

## 1. 実機 (ESP32) でのスモーク確認

host 側のテストは全部緑だが、**実 WiFi スタック上で 1 回は通したい**。

- [ ] `examples/VbanMicToPc/` を M5Stack Core2 (または INMP441 接続の ESP32) に書き込み、自宅 LAN の PC に VBAN Receptor を立てて音が届くこと
- [ ] 同じ要領で `examples/RtpVoipG711/`(まだ未作成) を実機で確認
- [ ] フットプリント測定: `arduino-cli compile --build-property build.extra_flags=-Wl,-Map=out.map` で ESP32 ビルドのフラッシュ / RAM 使用量を計測し、SPEC §8 の目標(フル ≤ 20 KB)と突き合わせる

### スモーク用ハードウェアの候補

- マイク: M5Stack Core2 内蔵 / INMP441 (I2S MEMS) / ATOM Echo
- スピーカー: M5Stack Core2 内蔵 / MAX98357A I2S DAC
- ネットワーク: ESP32 WiFi (内蔵)、または Ethernet 必要なら W5500

## 2. 外部キャプチャの取得 (interop テストの fixture)

これが **本リリースの一番の信用基盤**。自前 encoder と自前 parser だけで往復させても「両方とも同じバグを持っているケース」は検出できない。**他人の実装が吐いた packet が我々の parser を通る** ことを実証したい。

### 2-1. 用意するもの

- **Wireshark** (Windows / macOS / Linux 任意) — pcap 取得用
- **Python** (要らないかも、後述)

### 2-2. VBAN キャプチャの取り方

**送信元 (PC 上で動かして PC が VBAN packet を出す):**

| ツール | 入手 | 備考 |
|---|---|---|
| **Voicemeeter Banana** (推奨) | https://vb-audio.com/Voicemeeter/banana.htm 無料 | A-Bus / B-Bus に VBAN 出力先 IP+port を設定。任意のオーディオを VBAN 化できる |
| **VBAN Receptor** | https://vb-audio.com/Voicemeeter/receptor.htm | 受信専用なので送信検証には使えない。「受信できるか」の試験には便利 |
| **vban-talkie / vban-cmd** | https://github.com/Onyx-und-Iris/vban-cmd-py | コマンドラインで生成。GPL 系なので fixture 元としては OK だが我々のコードには取り込まない |

**手順:**

- [ ] PC に Voicemeeter Banana をインストール
- [ ] Strip 1 の入力にテスト音源(後述のサイン波)を流す
- [ ] A-Bus → VBAN 出力 を設定: dest IP は同 PC の別 NIC か `127.0.0.1`、port `6980`、stream name `Capture`、48 kHz PCM16 mono など固定
- [ ] Wireshark で `udp port 6980` をキャプチャ、5〜20 packet 程度
- [ ] `.pcapng` ファイルを保存
- [ ] Wireshark / `tshark` で 1 パケットずつ UDP payload を抜き出し、`.bin` として保存
  - `tshark -r capture.pcapng -Y "udp.port == 6980" -T fields -e data | head -5` で hex 取得
  - または Python の `scapy` で `rdpcap("capture.pcapng")` してから `bytes(pkt[UDP].payload)` を `.bin` に出す

保存先案: `tests/interop/captures/vban_voicemeeter_48k_mono_pcm16_*.bin` (1 ファイル = 1 packet)

### 2-3. RTP キャプチャの取り方

**送信元:**

| ツール | 用途 |
|---|---|
| **gst-launch-1.0** (推奨) | 一行で正規 RTP を吐ける。Linux ならパッケージから一発 |
| **ffmpeg** | gst が入らない環境向け代替 |
| **Linphone / X-Lite / MicroSIP** | 実 SIP セッションで「現場の」RTP を取る場合 |

**コマンド例 (gst-launch-1.0):**

```bash
# L16 mono 8 kHz (PT 11)
gst-launch-1.0 -v audiotestsrc wave=sine freq=440 ! \
  audioconvert ! audio/x-raw,format=S16BE,rate=8000,channels=1 ! \
  rtpL16pay pt=11 ! udpsink host=127.0.0.1 port=5004

# PCMU (PT 0)
gst-launch-1.0 -v audiotestsrc wave=sine freq=440 ! \
  audioconvert ! audioresample ! audio/x-raw,rate=8000,channels=1 ! \
  mulawenc ! rtppcmupay ! udpsink host=127.0.0.1 port=5004

# Opus (dynamic PT 96)
gst-launch-1.0 -v audiotestsrc wave=sine freq=440 ! \
  audioconvert ! audio/x-raw,rate=48000,channels=2 ! \
  opusenc ! rtpopuspay pt=96 ! udpsink host=127.0.0.1 port=5004
```

**手順:**

- [ ] 上のコマンドを 5〜10 秒だけ走らせる(`Ctrl-C` で停止)
- [ ] Wireshark で `udp port 5004` をキャプチャ
- [ ] PT ごとに 5〜10 packet 抜き出して `.bin` で保存

保存先案: `tests/interop/captures/rtp_gst_pcmu_*.bin` / `rtp_gst_l16mono_8k_*.bin` / `rtp_gst_opus_*.bin`

### 2-4. fixture 化と interop テストの実装

これは私(コード作業側)が担当する。`tests/interop/` を以下の構造で作る:

```
tests/interop/
├── captures/
│   ├── vban_voicemeeter_48k_mono_pcm16_packet01.bin
│   ├── vban_voicemeeter_48k_mono_pcm16_packet02.bin
│   ├── rtp_gst_pcmu_packet01.bin
│   ├── rtp_gst_l16mono_8k_packet01.bin
│   └── rtp_gst_opus_packet01.bin
├── interop_vban.ino
├── interop_rtp.ino
├── sketch.yaml
└── test_interop.py
```

ino では `parseAudioHeader` / `parseRtpHeader` に capture バイト列を流し、ヘッダの各フィールドが期待値どおりかを assert。L16 / PCMU は payload も assert (サイン波の最初の数サンプルが理論値と一致するか)。

## 3. テスト音源について — 「サイン波でいいの?」

**結論: サイン波で十分**、というか **サイン波が最適**。

理由:

| 観点 | サイン波の利点 |
|---|---|
| **wire format 検証が主目的** | 何の音が入っているかは検証対象ではない。重要なのはヘッダ + payload バイトが仕様どおり並んでいること |
| **決定性** | 同じ周波数・サンプルレートで生成すれば毎回同じバイト列。テスト fixture として安定 |
| **理論値計算が簡単** | `s[n] = round(A * sin(2π f n / Fs))` で参照値を出せる。Python で 3 行 |
| **聴覚的にも確認可能** | キャプチャした音を再生すれば「だいたい合ってる感」が直感で分かる |

**推奨パラメータ:**

- 周波数: **440 Hz** (A4、標準音叉)。1 kHz でも可
- 振幅: ±10000(int16 のフルスケールの 30%)。クリップ余裕を取る
- 長さ: 5〜10 秒(数十パケット分)

サイン波で困らない:

- **G.711 μ/A-law** はサインを通すと量子化誤差が予測可能(理論上の SNR ~36 dB)。テスト assertion で許容差を設定しやすい
- **Opus** は psycho-acoustic なのでサイン波が一番苦手な信号(笑)。ただし interop 目的なら「Opus が吐いたバイト列を我々の parser が読めるか」だけなので問題なし

サイン波で困るかもしれない:

- **聴感品質評価** は別途要る (ホワイトノイズや声サンプルが望ましい)。だが v0.1 リリースの interop には不要

### Python での参照サイン波生成 (参考)

```python
import math, struct
Fs = 48000
f = 440
A = 10000
n = 256
samples = [int(round(A * math.sin(2 * math.pi * f * i / Fs))) for i in range(n)]
# big-endian for L16: pcm = b"".join(struct.pack(">h", s) for s in samples)
# little-endian for VBAN: pcm = b"".join(struct.pack("<h", s) for s in samples)
```

これを assertion の reference に使えば、capture から抜いた最初の N サンプルが理論値と一致するかを bit-exact (PCM) や ±差 (圧縮コーデック) で照合できる。

## 4. その他 (時間に余裕があれば)

- [ ] **`tests/net_helpers/` の Python TX/RX**(Python ↔ DUT 通信) — 私の方で実装。実機 CI への布石。レビューだけ依頼
- [ ] **GitHub Actions / CI** — 親 PCMFlow から `release.yml` を verbatim コピー済みなので、リポを GitHub に push したら自動で走るはず。最初の workflow_dispatch が通るかだけ確認してほしい
- [ ] **Arduino Library Manager 登録** — 公式 index に PR を出す段階で必要(リリース後)

## 5. リリースゲート(全部 ✅ になったら v1.0)

- [ ] このリポの自動テスト 6 件すべて緑(済)
- [ ] examples 2 件 (`VbanMicToPc` / `RtpVoipG711`) が実機 ESP32 で動くこと
- [ ] interop テストの fixture が揃い、テストが緑
- [ ] フットプリントが SPEC §8 の目標内
- [ ] README / SPEC の表記揺れチェック
- [ ] `tools/bump_version.py --preview` で意図したバージョン文字列になることを確認

完了したらこの TODO.ja.md を `rm` してリリースタグ。
