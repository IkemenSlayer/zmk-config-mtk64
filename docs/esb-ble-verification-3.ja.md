# ESB＋BLE HID 継続検証（第3回）

2026-09-24。試験ブランチ `right_left_dongle_ble` の `c7e1e88` を検証。
実機の無線・HID入力試験は行っていません。

## 今回の変更

`patches/esb-timeslot-timing.patch` を追加しました。固定SHAのESB依存に対し、既存のRADIO所有権パッチの後で適用します。

- close要求後のTIMER0、延長成功callbackでは、比較割り込みとイベントを解除し、アプリの停止callbackを呼び、MPSLにENDを返す。
- close要求後にSTARTが届いた場合はESBを開始せずENDを返す。
- callbackごとに返却actionをNONEへ初期化し、比較イベントがないTIMER0で以前のEXTENDを再利用しない。
- extension／cleanup marginの正値・順序・slot長との関係をBUILD_ASSERTで検査する。10 ms用のmarginを残してslot長だけ1 msへ縮めた設定を拒否する。

MPSLのsession closeは実行中のslotが終わるまで完了しません。元コードはcloseをqueueへ入れた時点で `m_sess_open=false` にし、以降のTIMER0を何も処理せず返していました。今回の修正は、**終了前に停止callbackを呼ぶ制御フロー**を修正するものです。停止callback内部のESBドライバが制限時間内にハードウェアを停止できることまで保証しません。

仕様参照: [固定版MPSL timeslot API](https://github.com/nrfconnect/sdk-nrfxlib/blob/dfadf17305d8f000eda9aa74a5b9ff1c5647a23e/mpsl/include/mpsl_timeslot.h)。

## ホスト上のCテスト

`scripts/test-timeslot-host.py` は実際の `timeslot.c` を取得し、includeをテスト用境界モックに置き換えてgccでコンパイルします。callbackやsession workerのロジックをPythonに書き直したモデルではありません。上流版と修正版を同じテストで実行し、上流版の指定assertion失敗も検査します。

| 合成イベント列／条件 | 上流版 | 修正版 |
|---|---|---|
| close後のSTART | ENDを返さず失敗 | 開始せずEND |
| START→close要求→TIMER0 | 停止せず失敗 | 停止callback＋END、timer解除 |
| 延長要求→close要求→延長成功 | ENDを返さず失敗 | 停止callback＋END |
| 延長要求→比較イベントなしTIMER0 | 前回のEXTENDを返して失敗 | NONE |
| 通常の延長成功 | 成功 | 成功、比較時刻を10 ms更新 |
| 通常の延長失敗 | 成功 | 成功、停止後に次slot要求 |
| lengthだけ1 ms、marginは4／2 ms | コンパイル通過 | static assertionで拒否 |

上の4失敗ケースは注入したイベント列に対する制御フローの確認です。特に延長応答前のcloseや比較イベントなしの通知が実際のMPSLで発生する頻度・可否を測定したわけではありません。モックはZephyrのscheduler、割り込み優先度、RADIOレジスタ動作、送受信、実時間を再現しません。

## 再現できた未修正の不具合

以下はテストが「不具合の再現」に成功したという意味で、FWの正常動作を示すPASSではありません。ログも `KNOWN DEFECT` と区別しています。

1. **session open失敗後にID 255でrequestする。** openを失敗させ、workerの最初の2項目を処理すると、先にqueueへ入っているMAKE_REQUESTが再openより先に実行される。上流版・今回修正版の両方で再現。
2. **closeの後にIDLEが再requestをqueueへ入れる。** CLOSE→MAKE_REQUESTの順になる。上流版・今回修正版の両方で再現。

また、固定版APIではrequestの `-NRF_EAGAIN` は「sessionがIDLEでない」、`-NRF_ENOENT` が「session未open」です。元コードがEAGAINを無効ID扱いしてclose／openする回復経路も、API定義に合わせた見直しが必要です。

## 引き続き実機試験を妨げる問題

第2回で確認した次の問題は今回の時間管理パッチで解消していません。

- START→app_esb_resume→pull_packet_from_tx_msgqがゼロレイテンシIRQ内でZephyr queueを操作する。
- send／低優先度event handlerが、slot終了で割り込まれた後にESB APIを再開できる。`m_active` の確認だけでは排他にならない。
- app_esb_resumeは初期化エラーでもactiveにし、送信queueを処理する。初期化側のesb_start_rxの戻り値も無視している。
- session workerのopen／close／retry管理と、MPSL assertから戻ってしまう経路。

対処には、session状態と操作希望を分離した直列化、およびslot所有中だけドライバを操作する受渡しが必要です。START内のqueue呼出しを1個削る、あるいは通常のirq_lockを追加するだけでは、低優先度側の競合が残ります。

USB／BT切替、切替前releaseの送達保証、USB時のBLE無線停止は未実装です。今回の結果はPhase 1の実機入力確認や1000 Hz性能保証を意味しません。

## CIと成果物

実行: [35964052505](https://github.com/IkemenSlayer/zmk-config-mtk64/actions/runs/35964052505)。
同じrunでホストテストとOLEDなし／ありのSDC＋MPSL＋ESBビルドがすべて成功しました。生成設定検査も両方で成功。取得したmapでMPSL／ESB callbackとSoftDeviceの組込みを再確認しました。詳細は同梱 `verification-3-checks.txt` に記録します。

ホスト検証の再現は、固定版 `badjeff/zmk-feature-split-esb` に `esb-timeslot-radio.patch` と `esb-timeslot-timing.patch` を適用後、Linux／gcc環境で `python3 scripts/test-timeslot-host.py <ESB checkout>`。

元の `build.yaml`、`config/boards`、`config/mtk64.keymap` はベース `a2b12d4` と同一です。左右の設定変更・書込みは行っていません。今回も書込み用UF2を成果物に含めません。
