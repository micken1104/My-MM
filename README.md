# SOM仮想トレードボット README

このプロジェクトは、Binanceの板情報（bookTicker）を使って、SOM（自己組織化マップ）で短期の価格優位性を推定し、仮想売買を行うシステムです。

「WebSocket -> 特徴量計算 -> SOM推論 -> 執行」という流れは維持したまま、以下を強化しています。

- LONG/SHORTの両建て対応
- ボラティリティ適応TP/SL
- 手数料込みPnL評価
- 時間帯フィルター
- risk_mapを使ったリスク制御
- 学習データの時系列加重
- ホールドアウト検証レポート

## 1. まず全体像

### 1.1 何をしているボットか

1. Binanceから4銘柄（ATOMUSDT, BTCUSDT, ETHUSDT, SOLUSDT）のbookTickerを受信
2. 1秒ごとに市場特徴量を更新・保存
3. SOMで「今の状態に似た過去パターン」を探し、期待値を取得
4. フィルターを通過したら仮想ポジションを建てる
5. TP/SL/Time Upで決済し、CSVに記録
6. 30分ごとに再学習し、モデルをリロード

### 1.2 データフロー

```text
WebSocket (Binance bookTicker)
        -> 特徴量計算
    -> data/current/*_market_data.csv
        -> SOM推論 (expectancy + risk)
        -> エントリー判定 (LONG / SHORT)
        -> 決済判定 (TP / SL / Time Up)
    -> data/current/*_trades.csv, data/current/all_trades_history.csv
    -> 30分ごと再学習 (train_som.py)
```

### 1.3 旧仕様との分離

- 旧仕様の履歴は既存の `data/` と `models/` に残します
- 現行仕様の出力先は `data/current/` と `models/current/` です
- グラフ作成と分析スクリプトは、既定で `data/current/` を見るようにしています
- 旧履歴を見たい場合は、旧ファイルをそのまま参照してください

## 2. LONGとSHORT（最重要）

金融では2方向のポジションが取れます。

- LONG（買い）: 価格上昇で利益、下落で損失
- SHORT（売り）: 価格下落で利益、上昇で損失

このボットは現在、次の条件で両対応しています。

- LONG: 期待値が正方向閾値を上回る
- SHORT: 期待値が負方向閾値を下回る
    - ボラ高: expectancy < -0.30
    - ボラ低: expectancy < -0.45

これにより、下落トレンド局面でも機会を拾えるようになり、市場の片側だけを見る問題を回避します。

## 3. なぜTP/SLを「ボラティリティ×定数」で決めるのか

## 3.1 直感

ボラティリティ（標準偏差）は「その銘柄がどれくらい動くか」の尺度です。

- 静かな相場（例: $\sigma=0.05\%$）でTPを固定0.2%にすると、到達しにくくTime Upが増える
- 荒い相場（例: $\sigma=0.15\%$）なら0.2%は届きやすい

固定TP/SLは「相場の今の動き」を無視するため、環境依存で機能不全になりやすいです。

## 3.2 このボットの設定

- $TP = \sigma \times 2.0$（ただし $0.10\% \le TP \le 0.50\%$）
- $SL = \sigma \times 1.5$（ただし $0.08\% \le SL \le 0.35\%$）
- 最大保有時間: 180秒

実装上はクランプで暴走を抑えているため、異常に小さい/大きいTP/SLを防げます。

## 3.3 統計的な見方（注意点付き）

正規分布を仮定すると、

- $\pm 1\sigma$ に約68%
- $\pm 2\sigma$ に約95%

が収まります。よって「何σを狙うか」で、到達頻度の目安を持てます。

ただし実際の高頻度市場は完全な正規分布ではありません（裾が太い、非対称など）。
したがって「厳密な確率」ではなく「比較可能な共通尺度」として使うのが実務的です。

## 4. エントリー/決済ロジック

## 4.1 エントリー前フィルター

- クールダウン: 同銘柄は決済後30秒は再エントリー禁止
- 板厚みフィルター: depthが薄すぎると見送り
- 最低ボラフィルター: 凪相場を回避
- 時間帯フィルター（UTC）:
    - ATOMUSDT: 2, 13, 15時
    - BTCUSDT: 9, 16, 20時
    - ETHUSDT: 7, 15, 19時
    - SOLUSDT: 0, 8, 9時
- 1銘柄1ポジション

## 4.2 決済条件

- TP到達
- SL到達
- Time Up（180秒）

LONG/SHORTは対称に実装されており、判定は以下です。

- LONGの粗利益率: $(close-entry)/entry \times 100$
- SHORTの粗利益率: $(entry-close)/entry \times 100$

## 4.3 手数料込みPnL

- 往復手数料: 0.04% + 0.04% = 0.08%
- net_pnl = gross_pnl - 0.08

CSVには `gross_pnl` と `net_pnl` の両方を記録し、実運用に近い評価ができます。

## 5. SOM評価の考え方

SOMの各ノードには次が保存されます。

- expectancy_map: そのノードでの平均リターン
- risk_map: そのノードでのリターン標準偏差

推論時は、期待値だけでなくリスクも使って足切りします。

- risk >= 0.15 なら見送り
- |expectancy| / risk < 2.0 なら見送り

この設計で「見かけ上の期待値が高いだけの不安定ノード」を避けます。

## 6. 学習の改善点

## 6.1 時系列加重

学習データは最新30000行を使用し、そのうち直近10000行を追加複製（2倍重み）します。

狙いは「古い地合いより最近の地合いを重視する」ことです。

## 6.2 ホールドアウト検証

- 時系列順に train 80% / validation 20% に分割
- 各ノードで train期待値と validation期待値の乖離を計算
- 乖離が大きいノードを警告
- `models/SYMBOL_validation_report.csv` を出力

これにより、再学習ごとに過学習気味のノードを監視できます。

## 7. 出力ファイル

- `data/current/SYMBOL_market_data.csv`
    - timestamp, symbol, imbalance, imbalance_change, total_depth, price, btc_price, volatility, btc_corr
- `data/current/SYMBOL_trades.csv`
    - ts, symbol, entry, exit, pnl(net), reason, side, gross_pnl, net_pnl, hold_sec, tp_rate, sl_rate
- `data/current/all_trades_history.csv`
    - ts, symbol, side, entry, exit, gross_pnl, net_pnl, total_net_pnl, reason, hold_sec
- `models/current/SYMBOL_validation_report.csv`
    - ノードごとのtrain/validation乖離レポート

## 8. セットアップ

### 8.1 前提

- Windows 10/11
- Visual Studio Build Tools or Visual Studio Community
- CMake
- Python 3.9+

### 8.2 Python

```bash
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements-train.txt
```

### 8.3 学習スクリプト（CPU）

この構成では `train_som.py` を使って CPU で学習します。
`main.cpp` は `.venv\Scripts\python.exe` で `train_som.py` を実行します。

### 8.4 C++依存（vcpkg）

```bash
vcpkg install fmt nlohmann-json ixwebsocket
```

### 8.5 ビルドと実行

```bash
cmake --build build --config Release
build\Release\My-MM.exe
```

### 8.6 インストール確認

```powershell
.venv\Scripts\python.exe -c "import numpy, pandas, sklearn; print('ok')"
```

`ok` が出れば学習環境は準備完了です。

## 9. よくある詰まりポイント

### 9.1 取引が出ない

- 収集直後は学習データ不足
- 時間帯フィルター外の時間に実行している
- riskフィルターで落ちている

### 9.2 Time Upがまだ多い

- ボラ推定窓と相場の実勢がズレている
- TP係数が高すぎる可能性
- 期待値閾値が厳しすぎて、動かない局面だけ残っている可能性

### 9.3 学習が不安定

- validation_reportで `warn_gap=1` が多いノードを確認
- 特徴量の分布変化（regime shift）を疑う

### 9.4 `train_som.py` が実行できない

- `.venv\Scripts\python.exe` が存在するか確認してください
- `requirements-train.txt` が入っているか確認してください
- `main.cpp` は `.venv` の Python で `train_som.py` を実行します

## 10. 免責

このプロジェクトは研究・学習目的の仮想売買システムです。実運用前に必ず追加検証を実施してください。

