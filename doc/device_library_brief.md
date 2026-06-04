# PCMFlowDevice 立ち上げ引き継ぎメモ

> このメモは、PCMFlow ファミリーにデバイス I/O 向けの兄弟ライブラリ
> `PCMFlowDevice` を新規に立ち上げるための一時的な引き継ぎ資料です。
>
> 既存の `doc/sibling_library_brief.md` はコーデック兄弟
> (`PCMFlowG711` / `PCMFlowG722` / `PCMFlowOpus` など)向けです。
> 本メモはコーデックではなく、M5Unified speaker/mic や I2S デバイスの
> 癖を PCMFlow の抽象に寄せるための device helper ライブラリを対象にします。
>
> 新リポを起こすと、この PCMFlowUDP で見えている実機文脈は持ち越されないため、
> 「なぜ必要か」「最初に何を作るか」「どこまでを責務にするか」をここに残します。

---

## 1. 立ち上げ理由

PCMFlowUDP の manual test では、RTP/VBAN/Raw UDP の transport 自体よりも、
M5Unified speaker/mic のデバイス固有挙動が失敗原因になりやすい。

代表例:

- `M5.Speaker.playRaw()` は非同期再生で、渡した PCM バッファを関数 return 後も読む。
  stack 配列やすぐ再利用されるバッファを渡すと、RTP 受信ログ上は
  `drops=0` / `frames>0` でも、実音が無音・破損・途切れになる可能性がある。
- `playRaw()` の内部 queue depth、現在消費中の buffer、underrun count などは
  アプリ側から十分には見えない。観測できるのは主に戻り値と粗い再生状態だけ。
- Core2 speaker では 20 ms chunk をそのまま投げるより、初回 40 ms prebuffer、
  以後 40 ms chunk の方が安定することが manual tuning で見えている。
- 同じ三重バッファ、prebuffer、retry、ログカウンタ処理が
  `tests/manual/core2_rtp_gstreamer/`、`core2_rtp_speaker/`、
  `core2_rtp_g711_gstreamer/`、`core2_rtp_g722_gstreamer/`、
  `core2_rtp_opus_gstreamer/`、VBAN speaker 系に散らばり始めている。

これは transport や codec の責務ではなく、出力デバイス固有の async queue 管理と
buffer lifetime 管理の責務である。したがって、PCMFlowUDP に閉じ込めるより、
`PCMFlowDevice` として切り出すのが自然。

---

## 2. PCMFlow ファミリー内の位置付け

```
PCMFlow
   ├─ PCMSource / PCMSink / PCMFormat / pump
   ├─ 汎用 PCM 処理、リングバッファ、フォーマット処理
   └─ デバイス固有の M5Unified 依存は持たない

PCMFlowUDP
   └─ RTP / VBAN / Raw UDP transport

PCMFlowG711 / PCMFlowG722 / PCMFlowOpus
   └─ codec

PCMFlowDevice
   ├─ M5Unified speaker/mic helper
   ├─ I2S mic/speaker helper
   └─ 将来: external codec / DAC / board-specific audio helper
```

設計原則:

- `PCMFlowDevice` は PCMFlow に依存する。
- `PCMFlowDevice` は network transport を持たない。
- `PCMFlowDevice` は codec encode/decode を持たない。
- M5Unified 依存は M5 専用ヘッダに閉じ込める。
- 汎用ヘッダを include しただけで M5Unified が必須にならない構成にする。

推奨 include 分離:

```cpp
#include <PCMFlowDevice.h>    // 汎用 interface / version / shared profile
#include <PCMFlowDeviceM5.h>  // M5Unified speaker/mic helper
```

---

## 3. 最初に作るもの

初期スコープは speaker output helper に絞る。mic helper はその後。

最初の目標:

- `M5.Speaker.playRaw()` に渡す PCM バッファの寿命を helper が保証する。
- 受け取った PCM frames を、初回 prebuffer と通常 chunk にまとめて speaker へ出す。
- `playRaw()` が `false` を返したときに retry する。
- 実機切り分けに必要な counters を公開する。
- PCMFlowUDP manual test の重複コードを減らせる API にする。

候補 API:

```cpp
class M5SpeakerBufferedPlayer
{
public:
    struct Profile
    {
        uint16_t initialPrebufferMs;
        uint16_t chunkMs;
    };

    static Profile lowLatencyProfile(); // 40 / 20 ms
    static Profile balancedProfile();   // 40 / 40 ms
    static Profile stableProfile();     // 80 / 40 ms

    bool begin(const PCMFormat &format, Profile profile = balancedProfile());
    void reset();
    void stop();

    size_t writeFrames(const int16_t *frames, size_t frameCount);
    bool flush();

    size_t fillFrames() const;
    uint32_t chunks() const;
    uint32_t waits() const;
    uint32_t gapRisks() const;
    uint32_t drops() const;
};
```

使用イメージ:

```cpp
if (rx.poll())
{
    uint8_t packet[160];
    int16_t pcm[320];
    const size_t bytes = rx.readEncoded(packet, sizeof(packet));
    const size_t frames = decoder.decode(packet, bytes, pcm, 320);
    player.writeFrames(pcm, frames);
}
```

PCMSource から直接吸う形も後で追加できる:

```cpp
size_t pumpFrom(PCMSource &source, size_t maxFrames);
```

ただし最初から PCMFlow 本体の `pump()` と密結合させない。まずは packet decode 後の
PCM frames を渡す単純 API で実機挙動を固める。

---

## 4. 実装方針

speaker helper 内部は、PCMFlowUDP の Core2 manual test で安定したパターンを
一般化する。

- helper が 3 つ以上の PCM buffer を所有する。
- `writeFrames()` は caller buffer から helper-owned buffer へコピーする。
- 初回は `initialPrebufferMs` 分たまるまで `playRaw()` しない。
- running 後は `chunkMs` 分たまったら `playRaw()` する。
- `playRaw()` が `false` の間は `delay(1)` 付きで retry する。
- retry 回数を `waits()` に積む。
- chunk 提出間隔が理論再生時間より遅れた場合、`gapRisks()` に積む。
- `reset()` は speaker stop、内部 fill、buffer index、counters を初期化する。

Core2 での既知の初期値:

| Profile | 初回 prebuffer | 通常 chunk | 用途 |
|---|---:|---:|---|
| low latency | 40 ms | 20 ms | 低遅延確認用。途切れやすい可能性あり |
| balanced | 40 ms | 40 ms | Core2 speaker manual test の現時点の標準 |
| stable | 80 ms | 40 ms | 安定性確認用 |

現時点では `balanced` を default 候補にする。

---

## 5. 観測値の考え方

M5Unified の内部 queue 状態は完全には見えない。したがって、
`PCMFlowDevice` は「正確な内部状態」ではなく「アプリ側で観測可能な状態」を提供する。

公開候補:

| Counter / value | 意味 |
|---|---|
| `fillFrames()` | helper 内で次 chunk として蓄積済みの frames |
| `chunks()` | `playRaw()` に投入できた chunk 数 |
| `waits()` | `playRaw()` が `false` を返し retry した回数 |
| `gapRisks()` | chunk 投入間隔から推定した gap risk 回数 |
| `drops()` | helper 側 capacity 不足などで捨てた frames/chunks |
| `lastSubmitMs()` | 最後に chunk を投入した `millis()` |

これらは M5Unified 内部の真の underrun count ではない。README/SPEC では、
「device helper が観測できる application-side diagnostics」と明記する。

M5Unified 側に将来 API が追加されるなら、helper の内部で使えばよい。
候補は `freeQueueSlots()`、`canAcceptRaw()`、`underrunCount()` など。
ただし `PCMFlowDevice` の立ち上げは M5Unified API 追加待ちにしない。

---

## 6. mic helper の後続スコープ

speaker helper の後、M5Unified mic / I2S mic を `PCMSource` として見せる helper を検討する。

候補:

```cpp
class M5MicSource : public PCMSource
{
public:
    bool begin(const PCMFormat &format, size_t chunkFrames);
    size_t readFrames(void *out, size_t frameCount) override;

    const PCMFormat &format() const override;
    bool isReady() const override;
    bool isEof() const override;

    uint32_t reads() const;
    uint32_t emptyReads() const;
    int32_t lastRms() const;
};
```

mic 側の論点:

- M5Unified の mic API も内部状態が見えにくい。
- streaming では取得 frames 数、RMS、zero-run、送信 packet 間隔をログ化したい。
- low latency を狙うなら `recordWav()` 系ではなく、短い frame chunk の取得 API を使う。
- board ごとに mic の sample rate / channel / gain / AEC などの制約が違う可能性がある。

mic helper は speaker helper よりボード差が大きいので、最初は M5Core2/CoreS3 の
manual test に必要な最小実装から始める。

---

## 7. リポ構成案

既存の兄弟ライブラリ規約に寄せる。

```
PCMFlowDevice/
├─ README.md / README.ja.md
├─ SPEC.md / SPEC.ja.md
├─ CHANGELOG.md
├─ LICENSE
├─ library.properties
├─ library.json
├─ keywords.txt
├─ src/
│  ├─ PCMFlowDevice.h
│  ├─ PCMFlowDeviceM5.h
│  ├─ M5SpeakerBufferedPlayer.h
│  ├─ M5SpeakerBufferedPlayer.cpp
│  ├─ M5MicSource.h
│  ├─ M5MicSource.cpp
│  └─ pcmflowdevice_version.h
├─ examples/
│  ├─ M5SpeakerFromSine/
│  └─ M5MicToSpeaker/
├─ tests/
│  ├─ smoke/
│  └─ manual/
│     ├─ core2_speaker_buffered/
│     └─ core2_mic_source/
├─ doc/
│  └─ device_library_brief.md
└─ tools/
   └─ bump_version.py
```

`library.properties` の依存候補:

```text
depends=PCMFlow
```

M5Unified は悩みどころ。Arduino Library Manager 的には optional dependency を
表現しにくいので、M5 helper を主目的にする初期リリースでは
`depends=PCMFlow, M5Unified` とする案が現実的。ただし I2S 汎用 helper も含めたいなら、
M5Unified 依存を避けるために M5 用を `PCMFlowDeviceM5` として別リポにする案もある。

初期判断:

- まず `PCMFlowDevice` は M5Unified helper を中心に立ち上げてよい。
- 汎用 I2S helper を本格的に入れる段階で、依存分割が必要か見直す。

---

## 8. PCMFlowUDP から切り出す候補

この repo で重複している、または重複し始めている処理:

- `tests/manual/core2_rtp_gstreamer/core2_rtp_gstreamer.ino`
- `tests/manual/core2_rtp_speaker/core2_rtp_speaker.ino`
- `tests/manual/core2_rtp_buffer_tuning/core2_rtp_buffer_tuning.ino`
- `tests/manual/core2_rtp_g711_gstreamer/core2_rtp_g711_gstreamer.ino`
- `tests/manual/core2_rtp_g722_gstreamer/core2_rtp_g722_gstreamer.ino`
- `tests/manual/core2_rtp_opus_gstreamer/core2_rtp_opus_gstreamer.ino`
- `tests/manual/core2_vban_speaker/core2_vban_speaker.ino`
- `tests/manual/core2_voicemeeter_to_speaker/core2_voicemeeter_to_speaker.ino`

切り出す共通処理:

- `kAudioBuffers = 3`
- helper-owned PCM buffers
- `initialPrebufferMs` / `chunkMs`
- `g_audioFill` / `g_audioIndex` / `g_playStarted`
- `submitAudio()` の `playRaw()` retry loop
- `waits` / `chunks` / `gapRisks` counters
- display / Serial stats 用の getter

PCMFlowUDP 側は切り出し後、transport と codec decode だけを書く形に戻す。

---

## 9. 立ち上げ時の注意点

- 最初から抽象化しすぎない。M5 speaker の実害を潰す小さな helper から始める。
- API は「PCM frames を渡す」形を第一にする。`PCMSource` 統合は後から足す。
- `M5.Speaker.playRaw()` に渡す buffer lifetime は helper の最重要責務として SPEC に書く。
- counters は「推定値」であり、M5Unified 内部状態ではないことを明記する。
- profiles は ms 指定にする。sample rate ごとの frames 換算は helper 内で行う。
- 8 kHz G711、16 kHz L16/G722、48 kHz Opus のすべてで同じ helper を使えるようにする。
- examples は landing page 的説明ではなく、実際に音が出る最小 sketch にする。
- manual test は Core2 と CoreS3 profile を分ける。

---

## 10. PCMFlowDevice 立ち上げ後に戻ってやること

PCMFlowDevice の初期版ができたら、PCMFlowUDP で以下を行う。

1. manual speaker 系テストに `PCMFlowDevice` を依存追加する。
2. 各 ino のローカル三重バッファ処理を `M5SpeakerBufferedPlayer` に置き換える。
3. `tests/manual/README.md` / `README.ja.md` の speaker buffer 説明を
   PCMFlowDevice 参照に更新する。
4. `SPEC.md` / `SPEC.ja.md` の「受信バッファと遅延の責務」を更新し、
   transport は packet jitter、device helper は async output queue を担当すると明記する。
5. G711/G722/Opus の manual test を再実機確認する。

---

## 11. 未決事項

- M5Unified を `PCMFlowDevice` の必須依存にするか、`PCMFlowDeviceM5` を別リポにするか。
- helper のクラス名を `M5SpeakerBufferedPlayer` にするか、`M5SpeakerSink` にするか。
- `writeFrames()` が容量不足時に block/retry するか、drop して counter を増やすか。
- `flush()` を明示 API にするか、自動 chunk submit だけで十分か。
- mic helper を初期リリースに含めるか、speaker helper の安定後にするか。

現時点の推奨:

- 初期リリースは M5 speaker helper のみ。
- クラス名は挙動が明確な `M5SpeakerBufferedPlayer`。
- `writeFrames()` は helper 内 buffer に収まる範囲をコピーし、通常は non-blocking。
  `playRaw()` submit 時だけ retry する。
- mic helper は v0.2 以降。
