# mtk64ebt ESB＋BLE HID 調査・ビルド試験報告

調査日: 2026-09-24

**結論: 完成ファームウェアは未実装です。暗号バックエンドのビルド障害は回避できましたが、現行依存関係のままでは無線共存が成立する設定になりません。Phase 1 の実機入力確認に到達していないため、Phase 2〜6 の切替機能は追加していません。** ご指定の「安全に実現できない場合は無理にコードを書かず障害を報告」に従った成果物です。コンパイル成功を、ESB受信・BLE接続・1000Hz性能の確認とは扱っていません。

試験ブランチ: [IkemenSlayer/zmk-config-mtk64: right_left_dongle_ble](https://github.com/IkemenSlayer/zmk-config-mtk64/tree/right_left_dongle_ble)

元ブランチ `mentako-ya/zmk-config-mtk64:right_left_dongle_rev4` と既存リリースには変更していません。左右・ドングルの既存shield、keymap、`build.yaml` は元コミットと同一です。個人forkの試験ブランチのみ作成・pushしました。デバイスへの書込み、NVS消去、ペアリング操作は実行していません。

## 1. 検証対象

| 構成要素 | 調査・試験のリビジョン |
|---|---|
| mtk64 config | `a2b12d4405ba36461f338c4abaaa5f8a2a689e79` |
| ZMK main | `9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0` |
| Zephyr v4.1.0+zmk-fixes | `10ba6d0cb38bc3d258775d27982f707599320085` |
| zmk-feature-split-esb | `314c7cbaf4a74e1add1d6ffc8249de3e29965b8c` |
| sdk-nrf v3.1-branch+zmk-fixes | `9b3d2623fdcd9c0fd0284f860beea924568c9826` |
| nrfxlib v3.1-branch | `dfadf17305d8f000eda9aa74a5b9ff1c5647a23e` |
| Zephyrが取得したMbed TLS | `4952e1328529ee549d412b498ea71c54f30aa3b1` |
| dongle display mtk64_rev4 | `9d2818eb810297866759d84e93c3bb69dda2c3ef` |

`main` は移動するため、本報告は上記のコードに対する結果です。最終試験ブランチでは直接依存とZephyrも試験時のSHAに固定しています。ビルド環境はZMK公式の再利用可能なGitHub Actions workflowと `zmkfirmware/zmk-build-arm:stable`。コンテナはタグ指定であり、完全なbit-for-bit再現性までは保証していません。

## 2. 実際のビルド結果

[試験1: 055947e / Actions 35948061192](https://github.com/IkemenSlayer/zmk-config-mtk64/actions/runs/35948061192)

| 対象 | 結果 |
|---|---|
| 既存ドングル・OLEDなし、Studioあり | 成功 |
| 既存ドングル・OLEDあり、Studioあり | 成功 |
| 既存左Peripheral | 成功 |
| 既存右Peripheral | 成功 |
| BLE追加ドングル・OLEDなし | 暗号ライブラリのCMake生成で失敗 |
| BLE追加ドングル・OLEDあり | 暗号ライブラリのCMake生成で失敗 |

BLE試験では `ZMK_BLE=y`、`ZMK_SPLIT_ESB_USE_TIMESLOT=y`、`MPSL_TIMESLOT_SESSION_COUNT=2`、`BT_MAX_PAIRED=1`、`BT_MAX_CONN=1` を追加しました。split BLEは元の `n` のままです。

[試験2: 3e9089c / Actions 35949002007](https://github.com/IkemenSlayer/zmk-config-mtk64/actions/runs/35949002007)

さらに `NRF_SECURITY=n`、`NORDIC_SECURITY_BACKEND=n` を明示すると、OLEDなし・ありの両方がコンパイル・リンク成功しました。OLEDありでは FLASH 403600 B、RAM 138746 B。生成設定で `MBEDTLS_BUILTIN=y`、`BT_SMP=y`、`BT_SMP_SC_PAIR_ONLY=y` を確認しています。BLE暗号やSecure Connectionsを無効にして通したものではありません。

**ただし試験2のUF2は書き込まないでください。** timeslotが実際には無効、BLEコントローラも不適切なままの診断用出力です。Actionsの緑色は無線共存の合格を意味しません。実機が接続されていないため、ESB・BLE HID・Studio・RGB・OLED・バッテリー・トラックボールの動作測定は未実施です。

## 3. READMEに記載された暗号ビルド障害

実際のエラーは次の通りです。

```text
nrf/subsys/nrf_security/src/CMakeLists.txt:96:
  Cannot find source file: /programs/ssl/library/pk.c
nrf/subsys/nrf_security/src/core/nrf_oberon/CMakeLists.txt:19:
  Cannot find source file: /library/platform.c
nrf/subsys/nrf_security/src/drivers/nrf_oberon/CMakeLists.txt:41:
  Cannot find source file: /oberon/drivers/oberon_helpers.c
No SOURCES given to target: mbedcrypto / psa_core / oberon_psa_driver
```

原因の経路:

1. ZMK `app/Kconfig` の `ZMK_BLE` が `BT_SMP` を選択。
2. Zephyr `subsys/bluetooth/host/Kconfig` の `BT_ECC` 等がMbed TLS / PSA Cryptoを要求。
3. sdk-nrf `Kconfig.nrf` の `NRF_SECURITY_ENABLER` が `NRF_SECURITY` をimply。
4. Nordicの `subsys/nrf_security/Kconfig`、`CMakeLists.txt`、`configs/config_extra.cmake.in` がNordic用Mbed TLS/Oberon PSAの構成を使用。
5. 対象manifestはsdk-nrf本体を取得しますが、sdk-nrfの `west.yml` をimportしていません。NCS側が期待する `sdk-mbedtls` と `sdk-oberon-psa-crypto` の組合せにならず、ログの絶対ルートから始まる不正なソースパスに至ります。

NCS側manifestには両暗号モジュールの `ncs-v3.1.1` 指定があります。一方、本試験はZephyr側のMbed TLSを取得しています。パスだけ継ぎ足して直ったと判断できる問題ではありません。

**最小のビルド回避策として確認できたのは、両Nordicバックエンド設定を `n` にしてZephyr組込みPSA暗号を使うことです。** この回避策のペアリング・鍵保存・再接続の実機検証は未実施です。NCSの暗号ライブラリを維持する別案ではmanifestの暗号依存一式とZephyr APIの整合性確認が必要になります。

参照: [NCS Kconfig.nrf](https://github.com/badjeff/sdk-nrf/blob/9b3d2623fdcd9c0fd0284f860beea924568c9826/Kconfig.nrf)、[暗号パス設定](https://github.com/badjeff/sdk-nrf/blob/9b3d2623fdcd9c0fd0284f860beea924568c9826/subsys/nrf_security/configs/config_extra.cmake.in)、[NCS manifest](https://github.com/badjeff/sdk-nrf/blob/9b3d2623fdcd9c0fd0284f860beea924568c9826/west.yml)。

## 4. 暗号問題以外の無線共存障害

### 4.1 timeslotが有効にならない

ESBモジュール `src/split/esb/Kconfig:140` の `ZMK_SPLIT_ESB_USE_TIMESLOT` は `depends on BLE`。今回の設定では `BLE` は成立せず、指定した `y` が `n` に落とされます。READMEの自動有効化説明に対応する `default y` もありません。

```text
ZMK_SPLIT_ESB_USE_TIMESLOT was assigned the value 'y' but got the value 'n'.
unsatisfied dependencies: BLE (=n)
```

`src/split/Kconfig` の `select MPSL_DYNAMIC_INTERRUPTS if !BLE` も同じ名前を参照します。修正案は実在する `BT` / `ZMK_BLE` とSoftDevice使用条件に基づく依存・選択へ変更し、BLE併用ビルドではtimeslot無効をビルドエラーにすることです。USB専用targetへの適用は避けます。

### 4.2 BLEコントローラがMPSLと協調していない

両試験の生成設定は `BT_LL_SW_SPLIT=y`。DTSの `bt_hci_controller` も `compatible = "zephyr,bt-hci-ll-sw-split"` です。

NCS 3.1の `subsys/bluetooth/controller/Kconfig` にある `BT_LL_SOFTDEVICE` は `DT_HAS_NORDIC_BT_HCI_SDC_ENABLED` に依存します。新ドングル専用overlayで `nordic,bt-hci-sdc` を使用するHCIノードとchosenの設定を整え、生成設定で `BT_LL_SOFTDEVICE=y`、`BT_LL_SW_SPLIT=n` を確認する必要があります。

`MPSL_TIMESLOT_SESSION_COUNT=2` はセッションの上限を確保する設定であり、それだけではコントローラの切替や無線仲裁は行われません。2個のradioができるわけでもありません。

### 4.3 RADIO IRQの所有権を調整する必要がある

NCS `subsys/esb/esb.c` の `esb_init()` は、`ESB_DYNAMIC_INTERRUPTS` 時にRADIOを `ARM_IRQ_DIRECT_DYNAMIC_CONNECT` / `irq_connect_dynamic` で直接登録します。一方ESBモジュール `timeslot.c:mpsl_timeslot_callback()` は、MPSLのRADIO signalから `__ptr__radio_dynamic_irq_handler` を呼ぶ方式です。

timeslot開始時に `app_esb_resume()` → `esb_initialize()` → `esb_init()` を実行するため、RADIO IRQをMPSLが所有したままESB処理へ渡すよう、専用Kconfigでドライバの直接登録を抑制する等の統合が必要です。RADIO優先度を直接変更する `app_esb.c:esb_initialize()` も対象です。TIMER/PPI/IRQの競合を確認せず `BLE` を `BT` に置換するだけでは安全な修正になりません。

また `timeslot.c:schedule_request()` はcallbackから待ち時間付き `k_msgq_put(..., K_MSEC(1))` を呼ぶ経路を持ちます。IRQ文脈での非ブロッキング化、queue overflow時の回復、open失敗後のrequest順序、MPSL assert後の処理を合わせて検証すべきです。USB専用で通らない経路のため、既存USB動作実績をそのまま適用できません。

参照: [ESB Kconfig](https://github.com/badjeff/zmk-feature-split-esb/blob/314c7cbaf4a74e1add1d6ffc8249de3e29965b8c/src/split/esb/Kconfig)、[timeslot.c](https://github.com/badjeff/zmk-feature-split-esb/blob/314c7cbaf4a74e1add1d6ffc8249de3e29965b8c/src/split/esb/timeslot.c)、[Nordic ESBドライバ](https://github.com/badjeff/sdk-nrf/blob/9b3d2623fdcd9c0fd0284f860beea924568c9826/subsys/esb/esb.c)。

## 5. 旧バージョンで可能だった理由

ESBモジュールのZMK 0.4対応直前のmanifestはNCS/nrfxlib `v2.6.4` でした。NCS 2.6.4では `BT_LL_CHOICE` の既定が `BT_LL_SOFTDEVICE` で、現在のSDC devicetree依存条件とは異なります。その後のZephyr/NCS移行で暗号・コントローラ選択の前提が変わっています。さらに現在の `depends on BLE` を持つtimeslot設定は2026-07-06の `c4c2007` で追加されました。

旧READMEの共存実績は旧依存セットに対する記述です。本作業では旧バージョンを再ビルド・実機検証していません。旧版へ全体を戻す案は、Studioや現在のESBプロトコルとの互換性確認が必要なため採用していません。

参照: [ZMK 0.4移行](https://github.com/badjeff/zmk-feature-split-esb/commit/d44303d62e56275493c067f8d5619cf3e8699c22)、[NCS 2.6.4コントローラ設定](https://github.com/nrfconnect/sdk-nrf/blob/v2.6.4/subsys/bluetooth/controller/Kconfig)。

## 6. 切替・releaseの最小設計案（未実装）

標準 `&out OUT_USB` / `OUT_BLE` / `OUT_TOG` は `app/src/behaviors/behavior_outputs.c` から `zmk_endpoint_set_preferred_transport()` / `zmk_endpoint_toggle_preferred_transport()` に到達するため、キー操作の入口は再利用できます。ただし次の変更が必要です。

- `endpoints.c:get_selected_transport()` の自動フォールバックを新targetのみ抑制。BT選択中に未接続でもUSBに入力を出さない。
- `zmk_endpoint_clear_reports()` はkeyboard/consumerを送りますが、mouseはクリアするだけで送信しません。旧endpointへ全ボタン解放のmouse reportも送る。
- `hog.c` は送信をqueueへ積む非同期方式。空レポートをenqueueした直後に切断すると送信前に捨てられ得ます。keyboard/consumer/mouseの送信完了を追跡し、成功後に切替・切断する。USBにも完了・エラー処理を設ける。
- 切替中の入力受付を直列化し、旧宛先のpending reportと新入力を混在させない。物理的に押下中のキーを新宛先へ引き継ぐか、離すまで抑制するかを明文化する。HID reportだけのmemsetではmodifier参照カウント等の内部状態はリセットされない。
- リンク断・ホスト停止ではrelease到達の絶対保証はできない。接続中の正常切替は完了待ち、失敗時は切替中止／明示的な回復状態とし、「送ったつもり」で切断しない。

推奨順序は「入力を一時保留 → 旧endpointへ全release → 完了確認 → 出力先変更 → BLE開始または停止 → 新endpointへの入力を許可」。単純なbehavior内の固定sleepでは実装しません。

参照: [endpoints.c](https://github.com/zmkfirmware/zmk/blob/9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0/app/src/endpoints.c)、[hog.c](https://github.com/zmkfirmware/zmk/blob/9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0/app/src/hog.c)。

## 7. BLE 1台とESB子機・バッテリーの管理

`app/include/zmk/ble.h` では、`ZMK_SPLIT_BLE=n` の場合 `ZMK_BLE_PROFILE_COUNT=BT_MAX_PAIRED`。従って `BT_MAX_PAIRED=1`、`BT_MAX_CONN=1` でBLE HIDホスト1台にできます。ESB子機のためにBLE bond枠を増やす必要はありません。標準profile管理を削除せず、profile 0だけ使う方が変更を小さくできます。

`ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS=4` はESBモジュールでもバッテリー関連設定として再定義されています。名称だけを見て0にしてはいけません。一方、現行ZMKの `split/central.h` はsplit BLE無効時に `BLE_PERIPHERAL_COUNT=0` とするため、この値だけでは中央のバッテリー配列が確保されません。

今回の生成設定は `ZMK_SPLIT_ESB_PERIPHERAL_COUNT=0`。`app/src/split/central.c` はバッテリーイベントの `source` を配列indexとして直接使い、ESB側はsourceをそのまま渡します。新targetでは少なくとも `ZMK_SPLIT_ESB_PERIPHERAL_COUNT=4` とし、ID 1/2/3（左/右/foot）を収容し、受信時の境界検査も必要です。これは元構成にも存在するコード上の懸念であり、今回初めて実機障害を確認したという意味ではありません。

## 8. USBモードのBLE停止と性能

`ble.c:update_advertising()` はprofileが未接続なら広告を開始し、`disconnected()` も更新workを投入します。単発の `bt_le_adv_stop()` や切断behaviorだけでは停止を維持できません。起動・settings読込み・切断callbackを含む共通のmode gateが必要です。

USB選択中は広告禁止、接続済みならrelease送信完了後に切断。BT選択でgateを解除して広告開始。最初の実装ではMPSL自体は維持するのが比較的限定的な変更です。`bt_disable()` とMPSLの停止・直接ESBへの実行時切替はIRQ・クロック・sessionの所有権を再構築するため、初期修正へ混ぜるべきではありません。

BLE通信停止により無線占有は減らせますが、timeslot版のUSB動作は現行の直接ESB版と同一ではありません。**USB 1000Hzの性能同等性は未測定・未保証です。** 広告・接続イベント中は同じradioをBLEが使うためESB受信できない区間が発生します。既定の `ZMK_SPLIT_ESB_RETRY_INPUT_EVENT=0` ではトラックボールの損失も検証対象です。

## 9. BLE遅延の見込み

ZMK既定の接続間隔要求は6〜12単位、すなわち7.5〜15 msです（`BT_PERIPHERAL_PREF_MIN_INT/MAX_INT`）。入力から次の接続イベントまでの待ち、ESB送信待ち・再送、HID queue、OS処理が加わります。したがって旧READMEの7.5 ms＋1 msを固定遅延や保証値として扱えません。ホストの採用間隔・無線負荷によって変わります。実測の平均・p95/p99・最大値はありません。

## 10. OLED・Studio・その他

OLED側 `boards/shields/dongle_display/widgets/output_status.c` は既にendpoint/profile変更イベントを購読します。Phase 1通過後、profile番号画像を除き、選択モードを `USB` / `BT` とする小さい変更で対応できます。未接続時はselected endpointがNONEになるため、モード表示にはpreferred transportを使い、接続状態と区別します。今回表示の変更はしていません。

Studioは既存 `CONFIG_ZMK_STUDIO=y` と `studio-rpc-usb-uart` を両試験で維持しました。コンパイル確認のみで、RPC接続やキーマップ保存を実機確認したわけではありません。HID出力をBTへ切り替える際もUSBデバイス全体を停止せず、Studio用USB CDCを保持する設計が必要です。

RGB・キー・encoder・trackballの既存コードは未変更。左右のビルド成功は確認しましたが、既存UF2との無線相互接続を新ドングルで検証したわけではありません。ESBアドレス・ID・payload形式は変更していません。

## 11. 最小修正の順序と検証条件

1. 新ドングルtargetにのみZephyr PSA backend回避策を適用し、SDCのDTS/設定を整える。
2. ESB moduleのtimeslot Kconfig、NCS ESBドライバのRADIO IRQ所有権、callbackのqueue処理を専用オプションで修正。SDK全体の置換は避ける。
3. 生成 `.config` に `ZMK_BLE=y`、`ZMK_SPLIT_BLE=n`、`ZMK_SPLIT_ESB=y`、`ZMK_SPLIT_ESB_USE_TIMESLOT=y`、`BT_LL_SOFTDEVICE=y`、`BT_MAX_PAIRED=1`、`BT_MAX_CONN=1`、`MPSL_TIMESLOT_SESSION_COUNT=2` を必須化。`BT_LL_SW_SPLIT=y` ならCIを失敗させる。
4. 元の左右UF2でESB→BLEキーボード・mouse・encoderを実機確認。広告中、接続中、再接続、host sleep、queue負荷を含める。
5. その後にendpoint切替状態機械、全release完了待ち、BLE mode gateを実装。
6. USB/BT往復中のShift/Ctrl/複数キー/mouse drag/scroll/consumer keyをホスト側で記録。切断時の回復も検証。
7. 現行USB-onlyと同条件で1000Hz入力を比較し、packet loss・queue overflow・カーソル間隔・遅延分布を測る。数値で合格を判断した後にOLED/Studio/RGB/batteryを確認。

デバッグではESBログの `ZMK_SPLIT_ESB_LOG_LEVEL_DBG` とZMKログを使用できます。最終版はpacket単位printを無効にし、slot blocked/cancelled/overstayed・overflow・再送・受信数は低負荷のカウンタで測る案です。今回新しいログ機能は実装していません。

## 12. ペアリング・キー設定・復旧

完成dual FWが存在しないため、現時点で利用可能なペアリング／切替操作は提供していません。上記修正後の想定は、BTモードで `mtk64` をホストからペアリング、bondはprofile 0へ保存、ホスト変更時のみ `&bt BT_CLR` です。左右Peripheralのbond/NVSリセットは不要な設計にします。

標準入口を採用する場合のkeymap記述案:

```dts
#include <dt-bindings/zmk/outputs.h>
// 対象レイヤー内の選んだキーを次のbindingへ置換:
&out OUT_TOG
```

現行の標準 `&out` だけでは本報告の厳密なmode・mouse release・送信完了待ちは満たせません。Studioでの割当ても、修正版behaviorとmetadataがビルドされた後の検証項目です。試験用UF2にこのキーを設定して使用することは勧めません。

元構成への復旧は、ドングルをXIAOのUF2ブートローダーへ入り直し、保存済みの正常な `mtk64_DONGLE.uf2` または `mtk64_DONGLE_display.uf2` を書き戻す方法です。左右はそのままです。本作業はsettings領域やbootloaderを変更していません。`settings_reset` は必要ありません。Studioの保存keymapも消えるため、予防的な全消去は避けます。

## 13. 変更ファイルと成果物の範囲

| ファイル | 変更内容 |
|---|---|
| `.github/workflows/build.yml` | 試験専用CI。リリース公開・別repoへの自動pushを除去。最終版は手動で試験行列を選択 |
| `build-dual-probe.yaml` | 元4target＋BLE追加2targetの再現試験 |
| `build-crypto-probe.yaml` | Zephyr PSAへの切替による暗号障害切分け用2target |
| `config/west.yml` | 調査時の依存SHAを固定 |
| `docs/esb-ble-feasibility.ja.md` | 本報告 |

新たな実用 `mtk64_DONGLE_dual` shield、切替behavior、OLED変更、release制御コードは追加していません。次の開発には少なくともESBモジュール、NCS ESB IRQ統合、ZMK endpoint/HOG/BLE管理の変更が必要です。今回の到達点は、既存4targetのビルド確認、暗号ビルド障害の再現と回避、残る無線共存障害のコード上の特定です。
