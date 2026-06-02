"""
目的:
    M5Stack Core2 manual テスト基盤の最小動作を確認する。

手動である理由:
    LCD 表示と物理ボタン A/B/C の確認が必要なため。

必要なハードウェア:
    - M5Stack Core2
    - USB-C ケーブル
    - 2.4 GHz Wi-Fi AP

セットアップ手順:
    1. `.env` に Core2 のシリアルポートと WIFI_SSID / WIFI_PASSWORD を設定する。
    2. 実行: uv run --env-file .env pytest manual/core2_smoke/core2_smoke.py -v -s --profile m5stack_core2
    3. LCD に IP アドレスが表示されたら、Core2 の A/B/C ボタンを順に押す。
"""

import re


def test_core2_smoke(dut):
    """
    合格の条件:
        - Serial に CORE2-READY が出る。
        - LCD に IP アドレスが表示される。
        - A/B/C ボタン押下ごとに Serial に BUTTON A/B/C が出る。
        - 最後に CORE2-SMOKE done が出る。
    """
    match = dut.expect(
        [
            re.compile(rb"CORE2-READY ip=((\d{1,3}\.){3}\d{1,3}) rssi=(-?\d+)"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=90,
    )

    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connection failed: {match.group(1).decode()}")

    ip = match.group(1).decode()
    print(f"\nCore2 ready: ip={ip}")
    print("Confirm the same IP is visible on the Core2 LCD, then press A, B, and C.")

    dut.expect("BUTTON A", timeout=60)
    dut.expect("BUTTON B", timeout=60)
    dut.expect("BUTTON C", timeout=60)
    dut.expect("CORE2-SMOKE done", timeout=10)
