# ESB＋BLE HID 継続検証（第2回）

追記: callback終了処理の修正とCテストの結果は [第3回](esb-ble-verification-3.ja.md) を参照してください。

2026-09-24。初回報告の後に、試験ブランチ `right_left_dongle_ble` で検証を継続しました。

**SoftDevice・MPSL timeslot・ESB・BLE HIDを実際に組み込むビルドが、OLEDなし／ありの両方で成立しました。** 前回の「timeslotが無効のままコンパイル成功」から進んでいます。ただしPhase 1の実機入力確認は未完了です。今回のパッチは統合検証用で、実用dual-mode FWではありません。

## 検証結果

| 試験 | 内容 | 結果 |
|---|---|---|
| [35950491987](https://github.com/IkemenSlayer/zmk-config-mtk64/actions/runs/35950491987) | Kconfig修正＋SDC devicetree＋Zephyr PSA | OLEDなし／ありのビルドと設定検査に成功 |
| [35950493715](https://github.com/IkemenSlayer/zmk-config-mtk64/actions/runs/35950493715) | 上記＋RADIO所有権パッチ | OLEDなし／ありのビルドと設定検査に成功 |
| [35951054638](https://github.com/IkemenSlayer/zmk-config-mtk64/actions/runs/35951054638) | HCI API互換修正＋チャンネル更新修正、該当警告をエラー化 | OLEDなし／ありのビルドと設定検査に成功 |

最初のCIは `west manifest --freeze` が未取得の非選択シミュレータ依存で停止したため、依存一覧の記録方法を修正しました。これはFWのビルド失敗には数えていません。

パッチなしのRADIO構成もコンパイル自体は通りました。したがって「重複登録は必ずリンクエラーになる」という判定はできません。割り込みの所有権はソースとリンク内容を別途確認しています。

## 今回確認できた生成設定・リンク内容

```conf
CONFIG_ZMK_BLE=y
CONFIG_ZMK_SPLIT_ESB=y
CONFIG_ZMK_SPLIT_ESB_USE_TIMESLOT=y
CONFIG_BT_LL_SOFTDEVICE=y
CONFIG_ESB_MPSL_RADIO=y
CONFIG_MPSL_TIMESLOT_SESSION_COUNT=2
CONFIG_BT_MAX_CONN=1
CONFIG_BT_MAX_PAIRED=1
CONFIG_MBEDTLS_BUILTIN=y
CONFIG_BT_SMP=y
CONFIG_BT_SMP_SC_PAIR_ONLY=y
CONFIG_ZMK_SPLIT_ESB_PERIPHERAL_COUNT=4
CONFIG_ZMK_STUDIO=y
CONFIG_ZMK_USB=y
```

`ZMK_SPLIT_BLE`、`BT_LL_SW_SPLIT`、`MPSL_DYNAMIC_INTERRUPTS`、`NRF_SECURITY` は無効。`.config` に対する自動検査で確認しています。前回の不適切な生成設定を同じ検査に渡すと、timeslot・controller・動的IRQ・子機領域の不一致を検出して失敗します。

リンクマップで `libsoftdevice_controller_peripheral.a`、`timeslot.c.obj`、`mpsl_timeslot_callback`、`mpsl_radio_isr_wrapper`、`esb_mpsl_radio_irq_handler` の有効な組込みを確認しました。シンボル名がソースにあるだけの確認ではありません。`ZERO_LATENCY_IRQS=y` とESBのTIMER2使用も確認しています。

## 実装した試験用変更

| ファイル | 内容 |
|---|---|
| `config/dual-probe.conf` | BLE 1台、timeslot、Zephyr暗号、子機ID領域、Studioを設定 |
| `config/dual-probe.overlay` | 既存HCIノードを `nordic,bt-hci-sdc` に変更 |
| `patches/esb-timeslot-kconfig.patch` | 存在しない `BLE` 依存を `BT && BT_LL_SOFTDEVICE` に修正。BLE併用時のMPSL動的IRQ選択を抑制 |
| `patches/nrf-esb-radio-owner.patch` | 専用 `ESB_MPSL_RADIO` を追加。ESBがRADIO IRQを直接登録・有効化・無効化しないようにし、MPSL callback用の入口を追加 |
| `patches/esb-timeslot-radio.patch` | 上記入口をtimeslotから呼ぶ。ESBによるRADIO IRQ優先度変更・callbackでの無効化を抑制 |
| `patches/nrf-hci-zephyr41.patch` | Zephyr 4.1に存在しない `bt_hci_cmd_alloc()` を、opcode/長さを渡す `bt_hci_cmd_create()` に変更。長さの上限も検査 |
| `patches/esb-channel-hop.patch` | `idx = ++idx % count` の未定義動作を `idx = (idx + 1U) % count` に修正 |
| `scripts/apply-dual-probe.py` | 上流SHAを確認して一時west checkoutにだけパッチを適用。違うSHAや重複適用は失敗 |
| `scripts/check-dual-config.py` | 実際の生成設定を検査。要求が暗黙に無効化されたビルドを拒否 |
| `.github/workflows/dual-radio-probe.yml` | OLED有無のCI、設定検査、`.config`・DTS・mapの保存。UF2は公開しない |
| `.gitattributes` | unified diffの文法上必要な先頭空白を通常コードの空白エラーと区別 |

試験で見つかったHCI警告は、未使用関数がリンク時に破棄されるため従来はリンク成功の陰に隠れていました。将来vendor commandを使用する時点で問題化するため、APIを合わせています。HCI呼出しやRF hoppingの実機挙動を測定したわけではありません。

パッチ対象は初回報告の固定SHAと同じです。上流repoへのpushやforkの追加作成はしていません。既存 `build.yaml`、左右・ドングルのshield、共通keymapは元コミット `a2b12d4` と同一であることを再確認しました。これら既存4targetのビルド成功記録は初回の [35948061192](https://github.com/IkemenSlayer/zmk-config-mtk64/actions/runs/35948061192) です。今回は既存4targetを再ビルドしたという意味ではありません。

## 実機へ進む前に残る問題

### 1. timeslot開始がゼロレイテンシ割り込み内でキューを操作する

経路は `mpsl_timeslot_callback(START)` → `set_timeslot_active_status(true)` → `on_timeslot_start_stop()` → `app_esb_resume()` → `pull_packet_from_tx_msgq()`。末尾で `k_msgq_peek/get` を呼びます。

nrfxlibの `mpsl_timeslot.h` はSTART/TIMER0/RADIOを高優先度のIRQ文脈と定義し、kernel API使用に注意を求めています。今回の設定では実際に `ZERO_LATENCY_IRQS=y` です。**待ち時間を `K_NO_WAIT` にするだけでは解決しません。** 高優先度callbackではハードウェア処理と事前準備済みデータの受渡しに限定し、Zephyr queue操作を通常IRQ／threadへ分離する必要があります。debugログの経路も同様に確認が必要です。

参照: [Zephyr 4.1のIRQ仕様](https://github.com/zmkfirmware/zephyr/blob/10ba6d0cb38bc3d258775d27982f707599320085/doc/kernel/services/interrupts.rst#zero-latency-interrupts)、[MPSL callback仕様](https://github.com/nrfconnect/sdk-nrfxlib/blob/dfadf17305d8f000eda9aa74a5b9ff1c5647a23e/mpsl/include/mpsl_timeslot.h)。

### 2. 通常処理とtimeslot終了の競合

`zmk_split_esb_send()` とESBの低優先度event handlerも `pull_packet_from_tx_msgq()` を呼びます。`m_active` を検査した直後にMPSLが割り込み、ESB受信を停止してBLEへ無線を返す場合、通常処理が復帰してからESB APIを呼ぶ可能性があります。単純なboolや通常の `irq_lock()` で、ゼロレイテンシIRQとの排他が成立したとは言えません。

必要なのは、送信commandの事前準備、ISR安全な受渡し、スロット中だけのESB操作を一貫させる構成です。今回のIRQ所有権パッチだけでは、この問題を解消していません。

### 3. session回復とassert処理

現行 `timeslot.c` はsession open成功前に `m_sess_open` を立て、openとrequestを別々にqueueへ投入します。open失敗後の順序、close中のIDLEによる再要求、queue overflow時の回復を確認・修正する必要があります。また `mpsl_assert_handle()` はログを出して戻るだけで、回復不能なMPSLエラー後に安全な停止／再起動を保証しません。

### 4. 無線時間配分と1000Hz性能

現在のESB要求スロットは10 ms、拡張要求marginは4 ms、次要求marginは2 ms。SDC側の既定最大connection event長は7.5 msで、ZMKの接続間隔要求は7.5〜15 msです。ホストの採用値や無線負荷によってはESBの長い要求が成立しにくくなるため、設定だけで1000Hzを保証できません。

短いスロットへ変更する際はmarginも一緒に見直す必要があります。lengthだけを1 msへ下げると、現在の減算式のタイマー値が不正になります。広告中・接続中・USB選択時のblocked/cancelled/overstayed、packet loss、queue overflow、入力遅延分布を測って調整する段階が残っています。

## 再現方法

試験ブランチのGitHub Actionsで `ESB SDC integration probe` を選び、`radio_patch=true` で実行します。検証コミットは `7f84837`。実行内容はworkflow内に保存しています。修正版CIは、暗黙の関数宣言・整数からpointerへの変換・sequence point警告をエラーとして扱います。

結果artifactには `.config`、`zephyr.dts`、`zmk.map`、依存一覧を保存します。書込み用UF2は含めません。`radio_patch=false` は未修正対照用で、最新の厳格な警告条件では互換性警告によって失敗する場合があります。表の対照実験は警告厳格化前のコミット `ef26efe` です。

## まだ確認していないこと

実機でのESB入力→BLE HID送信、ペアリング、再接続、USB時の性能、切替release、StudioのRPC操作、OLED/RGB/battery表示。USB/BT mode gateや新しい切替behaviorも未実装です。元keymapの `BT_SEL 1` 以降は1プロファイル試験では対象外で、利用するのはprofile 0のみです。

この結果でPhase 1の**ビルド成立**は確認できましたが、**実機入力の成立**は未確認です。残るIRQ文脈・競合・回復処理を解消してから実機試験へ進む必要があります。既存FWへの復旧方法と、左右を書き換えない方針は初回報告のままです。
