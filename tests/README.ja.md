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

## 商標について

`vban_*` テストは PCMFlowUDP の VBAN サブセット実装を検証する。「VBAN」は VB-Audio Software のプロトコル名。詳細は [../SPEC.ja.md §14](../SPEC.ja.md#14-ライセンスと商標) を参照。
