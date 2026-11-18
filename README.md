# BonDriver_GVMVPVZ_T (v1.0.0)

I-O DATA **GV-MVP/VZ**（3波対応 USB チューナー / `VID_04BB&PID_052B` / NXP SAA7164F + SONY SMT-LJxxx）を、**純正カーネルドライバを使わず純ユーザーモード USB（`libusbK` / `libusb0.dll`）のみ**で **EDCB**・**KonomiTV**・**Mirakurun**・TVTest 等の x64 BonDriver 対応アプリケーションから利用できるようにしたスタンドアロン x64 BonDriver です。

Windows 11（24H2 / 26H2 等）で旧来の純正カーネルドライバ（`PhilMAS64_052B04BB.sys`）による BSOD が発生する環境でも、カーネルドライバを一切読み込まずに安全に **地デジ（ISDB-T）・BS・CS110（ISDB-S）** の 3 波を受信できます。

> **重要**: 本リポジトリには I-O DATA 純正ファームウェア（`NXP7164F_052B04BB.bin`）・ドライバ・mAgicTV 関連ファイルおよび `libusb0.dll` は一切含まれていません。ご自身の正規環境からファームウェアをご用意ください。

---

## 初版リリース (`v1.0.0`) の主な特徴

1. **完全ユーザーモード USB 動作（純正ドライバ不要・BSOD 回避）**
   - `libusbK` + `libusb0.dll` 経由で直接 USB 制御するため、純正 BDA ドライバのインストール不要。
   - コールドブート状態（`bcdDevice != 0x0100`）を検知すると、配置済みのファームウェア `NXP7164F_052B04BB.bin` を 2 段階 DFU で自動ロードします。
2. **IVDRM 相互認証・WatchDog・AES-128-CBC TS 復号を内蔵**
   - 1024-bit Diffie-Hellman 鍵交換と AES-128-ECB 相互認証（`unit=9`）を Windows CNG (`bcrypt.lib`) で実装。
   - バックグラウンドスレッドによる 2 秒周期の WatchDog パケット送信で、5 秒タイムアウトによるファームウェアロックを防止。
   - TS ストリーム内の鍵パケット（`FORIVFROMNXP`）によるランダム周期のセッション鍵ローテーションにリアルタイム追従し、AES-128-CBC で 188B TS パケットを復号します。
3. **3波対応（地デジ / BS / CS110）と高速・安定したチャンネル切替**
   - `Space 0` (`UHF 13ch..62ch` 50スロット) / `Space 1` (`BS1/TS0..BS23/TS2` 28スロット) / `Space 2` (`ND2..ND24` 12スロット) に対応。
   - `EP 0x83` 専用の高優先度非同期リーダースレッド（16×64KB URB リング + `usb_reap_async_nocancel`）と AES-128-CBC 復号スレッドを分離し、高ビットレート BS/CS でもドロップを防止します。
   - `SetChannel()` の連続切替時にクリーンなセッション再初期化を行うことで、BS ↔ 地デジ ↔ CS110 を跨いだ連続選局が安定動作します。
4. **ソフトウェア自動リセット＆復旧機構**
   - `OpenTuner()` 時に `usb_clear_halt(0x81/0x01)` を実行し、異常時は `libusbK.dll` (`UsbK_ResetDevice`) による USB リセット＋コールド再初期化を自動実行します。
   - `OpenSession` 失敗時は `WatchDogInit (cs=6)` による状態リセット＆リトライを自動で行います。
5. **本体内蔵 B-CAS カードによるハードウェア MULTI2 自動復号**
   - 外付け IC カードリーダーやソフト B25 デコーダは不要です（EDCB・Mirakurun `decoder: ~` でそのまま視聴・録画可能）。
   - コールドブート時のみ内部 B-CAS カードの初期化（`INT 0x30`）を実行し、`status != 0` のウォーム起動では GPIO リセット・`INT` 再送をスキップすることで高速オープンと安定動作を両立しています。
6. **単一ソース・外部 SDK 依存ゼロの x64 ビルド**
   - DirectShow / BDA SDK 不要、C++ ソース 1 ファイル（`src/BonDriver_GVMVPVZ_T.cpp`）と Windows 標準 `bcrypt.lib` のみで静的 CRT (`/MT`) ビルド可能。

---

## ディレクトリ構成

```text
.
├── release/                         # 初版リリース v1.0.0 配布用バイナリ・ZIP
│   ├── BonDriver_GVMVPVZ_T.dll      # Release x64 ビルド済み BonDriver (静的 CRT)
│   └── BonDriver_GVMVPVZ_T_v1.0.0_x64.zip
├── configs/
│   └── mirakurun/
│       ├── tuners.sample.yml        # Mirakurun チューナー設定サンプル
│       └── channels.sample.yml      # Mirakurun チャンネル設定サンプル (東京スカイツリー例)
├── src/
│   └── BonDriver_GVMVPVZ_T.cpp      # スタンドアロン BonDriver ソースコード
├── build.bat                        # x64 Release ビルドスクリプト
├── .gitignore
├── LICENSE
└── README.md
```

---

## 導入手順（Mirakurun + KonomiTV 例）

### 1. USB ドライバ（libusbK / libusb0.dll）の準備
1. GV-MVP/VZ（本体に B-CAS カードを挿入済み）を PC に接続します。
2. [Zadig](https://zadig.akeo.ie/) 等を使用して、GV-MVP/VZ（`VID_04BB&PID_052B`）のドライバを **`libusbK`** に置き換えます。
3. x64 版の `libusb0.dll` および `libusbK.dll`（自動リセット機能で使用）が `C:\Windows\System32\` または実行ファイルと同じフォルダにあることを確認してください（Zadig で `libusbK` を導入すると通常自動で配置されます）。

### 2. ファームウェアと BonDriver の配置
1. ご自身の GV-MVP/VZ 添付メディアまたは公式ドライバパッケージ（`Drivers\VZ\x64\` 等）から **`NXP7164F_052B04BB.bin`** を取り出します。
2. 以下の 2 ファイルを同じフォルダ（例: `C:\DTV\BonDriver\`）へ配置します。
   - `release/BonDriver_GVMVPVZ_T.dll`
   - `NXP7164F_052B04BB.bin`

> **ファームウェア検索順**:
> 1. 環境変数 `GVMVPVZ_FW_PATH` に指定されたフルパス
> 2. `BonDriver_GVMVPVZ_T.dll` と同じフォルダの `NXP7164F_052B04BB.bin`
> 3. `C:\DTV\BonDriver\NXP7164F_052B04BB.bin`

### 3. EDCB / KonomiTV / Mirakurun 設定例

#### EDCB + KonomiTV（推奨）
- `C:\DTV\EDCB\BonDriver\BonDriver_GVMVPVZ_T.dll` と `NXP7164F_052B04BB.bin` を配置します。
- 本ドライバの `GetTunerName()` は `GVMVPVZ` を返すため、EDCB のチャンネル設定ファイル名は `BonDriver_GVMVPVZ_T(GVMVPVZ).ChSet4.txt` になります。
- `EpgDataCap_Bon.exe` 等でチャンネルスキャン（地デジ・BS・CS110）を実行すれば、KonomiTV（`backend: 'EDCB'`）から 3 波すべてを視聴・録画できます。

#### Mirakurun 設定例

`%USERPROFILE%\.Mirakurun\tuners.yml`（[configs/mirakurun/tuners.sample.yml](configs/mirakurun/tuners.sample.yml)）:

```yaml
- name: GV-MVP-VZ
  types:
    - GR
    - BS
    - CS
  command: C:\DTV\BonRecTest\x64\BonRecTest.exe --driver C:\DTV\BonDriver\BonDriver_GVMVPVZ_T.dll --space <space> --channel <channel> --output -
  decoder: ~
  isDisabled: false
```

`%USERPROFILE%\.Mirakurun\channels.yml`（[configs/mirakurun/channels.sample.yml](configs/mirakurun/channels.sample.yml)）には、`space: 0`（UHF 物理ch - 13）・`space: 1`（BS スロット 0..27）・`space: 2`（CS110 スロット 0..11）を設定してください。

※ 呼び出し元（`EpgDataCap_Bon.exe` / `BonRecTest.exe` 等）は **x64 版** を使用してください。

---

## ソースからのビルド方法

- **必要環境**: Visual Studio 2022（または Build Tools、C++ デスクトップ開発 / Windows SDK）
- リポジトリ直下で以下を実行すると `release\BonDriver_GVMVPVZ_T.dll` が生成されます。

```bat
build.bat
```

### デバッグ出力（任意）
通常運用時はログファイルを生成しません。トラブルシューティング時のみ環境変数 `GVMVPVZ_DEBUG=1` を設定して実行すると、標準エラー出力 (`stderr`) に初期化・選局・WatchDog・TS鍵ローテーションの診断ログを出力します。

---

## 検証済み環境・テスト実績

- **OS**: Windows 11 (x64 / 26H2)
- **チューナー / ドライバ**: I-O DATA GV-MVP/VZ（`VID_04BB&PID_052B` / `libusbK` + `libusb0.dll` / 内蔵 B-CAS カード）
- **配信スタック**: `BonDriver_GVMVPVZ_T.dll` (x64) → `EDCB` (`EpgTimerSrv` / `EpgDataCap_Bon`) または `Mirakurun` → `KonomiTV 0.14.1` (`QSVEncC` 1080p)
- **3波チャンネル受信・スキャン**: 地デジ 20 サービス・BS 31 サービス・CS110 50 サービス（計 101 サービス）を EDCB に登録し、KonomiTV 上で BS（`NHK BS`）および地デジ（`NHK総合`）のライブ配信を確認済み。
- **3波連続切替テスト**: `BS15/TS1 (NHK BS)` → `BS1/TS0 (BS朝日)` → `UHF 27ch (NHK総合)` → `CS ND2` のプロセス内連続切替および復号受信を確認済み。
- **長時間連続録画テスト**: NHK総合 (27ch) にて **310.5 秒（5 分 10 秒 / 638.95 MB）連続受信**、終始 16.3〜16.5 Mbps 安定、TS 同期エラー 0、IVDRM 鍵ローテーション 16 回すべて自動追従、フルセグ映像・音声復号率 100%。

---

## 注意事項

- `OpenTuner()` には `usb_clear_halt`・`UsbK_ResetDevice`・`WatchDogInit` による自動復旧機構を内蔵していますが、受信プロセスの強制終了等でデバイス内部の WatchDog ロックアウト（`cs8=6`）状態に入った場合のみ、管理者権限での `pnputil /restart-device` または USB ケーブルの再接続が必要になる場合があります。
- 本ソフトウェアの利用によって生じた損害等について、作者・関係者は一切の責任を負いません。

## ライセンス

[MIT License](LICENSE)
