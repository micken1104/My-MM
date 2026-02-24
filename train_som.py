#!/usr/bin/env python3
"""
シンプルなSOM（自己組織化マップ）トレーニングスクリプト
C++から出力される市場データを使用してSOMモデルを訓練する

新しいCSV形式: timestamp,symbol,imbalance,imbalance_change

使い方: python train_som.py <symbol>
例: python train_som.py ETHUSDT
"""

import pandas as pd
import numpy as np
import sys
import os
from sklearn.preprocessing import MinMaxScaler
import random

# ==================== 設定 ====================
MIN_REQUIRED_DATA = 500  # 学習に必要な最小データ数
SOM_WIDTH = 20           # SOMグリッドの幅
SOM_HEIGHT = 20          # SOMグリッドの高さ
EPOCHS = 20              # 学習エポック数

# ==================== 1. コマンドライン引数の取得 ====================
if len(sys.argv) < 2:
    print("使用法: python train_som.py <symbol>")
    print("例: python train_som.py ETHUSDT")
    sys.exit(1)

target_symbol = sys.argv[1]
support_symbol = 'BTCUSDT'

# ==================== 2. ファイル名の準備 ====================
data_dir = "data"
models_dir = "models"

target_market_path = f"{data_dir}/{target_symbol}_market_data.csv"
support_market_path = f"{data_dir}/{support_symbol}_market_data.csv"

# ==================== 3. ファイル存在確認 ====================
if not os.path.exists(data_dir):
    print(f"Error: {data_dir} directory not found")
    sys.exit(1)

if not os.path.exists(target_market_path):
    print(f"Error: Market data file not found: {target_market_path}")
    sys.exit(1)

if not os.path.exists(support_market_path):
    print(f"Error: Support market file not found: {support_market_path}")
    sys.exit(1)

# ==================== 4. データの読み込み ====================
# total_depth, price, btc_price, volatility, btc_corr が追加されている
cols = ['timestamp', 'symbol', 'imbalance', 'imbalance_change', 'total_depth', 'price', 'btc_price', 'volatility', 'btc_corr']

df_target = pd.read_csv(target_market_path, names=cols, header=0)
df_support = pd.read_csv(support_market_path, names=cols, header=0)

# タイムスタンプを数値に変換してソート
df_target['timestamp'] = pd.to_numeric(df_target['timestamp'], errors='coerce')
df_support['timestamp'] = pd.to_numeric(df_support['timestamp'], errors='coerce')
df_target = df_target.sort_values('timestamp').reset_index(drop=True)
df_support = df_support.sort_values('timestamp').reset_index(drop=True)

# ==================== 5. データの結合（時間軸での） ====================
# 対象シンボルの各タイムスタンプに対して、最も近い直前のBTC状態を紐付ける
combined_df = pd.merge_asof(
    df_target,
    df_support[['timestamp', 'imbalance', 'imbalance_change']].rename({
        'imbalance': 'btc_imbalance',
        'imbalance_change': 'btc_imbalance_change'
    }, axis=1),
    on='timestamp',
    direction='backward'
)

# BTCデータが存在しない行を削除（古いデータ行）
combined_df = combined_df.dropna(subset=['btc_imbalance'])

# 1. 未来のデータを「今」の行に持ってくる (30行 = 約30秒先と仮定)
# imbalance_change を 30個分上にずらす
combined_df['future_pnl'] = (combined_df['price'].shift(-30) - combined_df['price']) / combined_df['price']
# 2. 特徴量の準備セクションで、price_changes を future_pnl に変える
# feature_data を作る前に shift するので、dropna が必要
combined_df = combined_df.dropna(subset=['btc_imbalance', 'future_pnl'])

# ==================== 6. データの十分性チェック ====================
if len(combined_df) < MIN_REQUIRED_DATA:
    print(f"警告: {target_symbol} は結合後 {len(combined_df)} 行です")
    print(f"      必要な最小値: {MIN_REQUIRED_DATA} 行")
    sys.exit(1)

# ==================== 7. 特徴量の準備 ====================
features = [
    'imbalance', 
    'imbalance_change', 
    'btc_imbalance', 
    'btc_imbalance_change',
    'total_depth',    # 追加：板の厚み
    'volatility',     # 追加：相場の荒れ具合
    'btc_corr'        # 追加：BTCとの連動性
]

# 最新のデータを使用
working_df = combined_df.tail(30000).copy()

# データテーブルの構築（最新30000行を使用）
feature_data = working_df[features].reset_index(drop=True)

# インバランス変化を疑似価格変動として使用
price_changes = working_df['future_pnl'].values * 1000

# ==================== 8. 特徴量の正規化 ====================
scaler = MinMaxScaler()
data_scaled = scaler.fit_transform(feature_data)

# ==================== 9. SOMの初期化 ====================
width = SOM_WIDTH
height = SOM_HEIGHT
neurons_count = width * height

# 初期重みをデータからランダムサンプリング
if len(data_scaled) >= neurons_count:
    som_weights = np.array(random.sample(data_scaled.tolist(), neurons_count))
else:
    # データが不足している場合は繰り返しサンプリング
    indices = np.random.choice(len(data_scaled), neurons_count, replace=True)
    som_weights = data_scaled[indices]

# グリッド座標の準備
x_coord, y_coord = np.meshgrid(np.arange(width), np.arange(height))
grid_coords = np.stack([x_coord.flatten(), y_coord.flatten()], axis=1)

# ==================== 10. SOM学習ループ ====================
print(f"訓練開始: {target_symbol}")
print(f"  - データサイズ: {len(feature_data)} 行")
print(f"  - 特徴量: {features}")
print(f"  - SOMグリッド: {width} x {height} = {neurons_count} ニューロン")
print(f"  - エポック: {EPOCHS}")

for epoch in range(EPOCHS):
    # 進行度に基づいて学習率と近傍半径を減衰
    progress = epoch / float(EPOCHS)
    learning_rate = 0.03 * (1.0 - progress)
    sigma = 3.0 * (1.0 - progress)
    
    # 各データポイントに対して学習
    for i in range(len(data_scaled)):
        # 1. 最も近いニューロン（BMU = Best Matching Unit）を探索
        differences = som_weights - data_scaled[i]
        distances_squared = np.sum(differences ** 2, axis=1)
        winner_idx = np.argmin(distances_squared)
        bmu_coord = grid_coords[winner_idx]
        
        # 2. 全ニューロンについて距離と影響度を計算
        dist_to_bmu = np.sum((grid_coords - bmu_coord) ** 2, axis=1)
        influence = np.exp(-dist_to_bmu / (2 * (sigma ** 2 + 1e-5)))
        
        # 3. ニューロン重みを更新
        weight_update = learning_rate * influence[:, np.newaxis]
        som_weights += weight_update * (data_scaled[i] - som_weights)
    
    # エポック進捗表示
    if (epoch + 1) % 5 == 0:
        print(f"  エポック {epoch + 1}/{EPOCHS}")

# ==================== 11. 期待値とリスクマップの計算 ====================
# 各データポイントがどのニューロンに割り当てられるかを計算
all_distances = np.linalg.norm(som_weights[:, np.newaxis] - data_scaled, axis=2)
all_winners = np.argmin(all_distances, axis=0)

# ニューロンごとに対応する価格変動をグループ化
node_pnl = [[] for _ in range(neurons_count)]
for data_idx, neuron_idx in enumerate(all_winners):
    if data_idx < len(price_changes):
        node_pnl[neuron_idx].append(price_changes[data_idx])

# 期待値とリスク（標準偏差）を計算
expectancy_map = np.zeros(neurons_count)
risk_map = np.ones(neurons_count) * 0.05

for neuron_idx in range(neurons_count):
    pnl_values = node_pnl[neuron_idx]
    if len(pnl_values) > 0:
        expectancy_map[neuron_idx] = np.mean(pnl_values)
        if len(pnl_values) > 1:
            risk_map[neuron_idx] = np.std(pnl_values)

# ==================== 12. モデルの保存 ====================
os.makedirs(models_dir, exist_ok=True)
prefix = f"{models_dir}/{target_symbol}_"

# SOMの重みを保存
np.savetxt(f"{prefix}map_weights.csv", som_weights, delimiter=",")

# 期待値を保存
np.savetxt(f"{prefix}expectancy.csv", expectancy_map, delimiter=",")

# リスクマップ（リスク＝標準偏差）を保存
np.savetxt(f"{prefix}risk_map.csv", risk_map, delimiter=",")

# スケーリングパラメータを保存（特徴正規化用）
scaling_params = pd.DataFrame({
    'feature': features,
    'min': scaler.data_min_,
    'max': scaler.data_max_
})
scaling_params.to_csv(f"{prefix}scaling_params.csv", index=False)

# ==================== 13. 完了メッセージ ====================
print(f"\n✅ {target_symbol} の訓練が完了しました")
print(f"  - 処理した市場データ: {len(feature_data)} 行")
print(f"  - 期待値が正（有利）なニューロン: {np.sum(expectancy_map > 0)} 個")
print(f"  - 期待値の平均: {np.mean(expectancy_map):.6f}")
print(f"  - 保存されたファイル:")
print(f"    - {prefix}map_weights.csv (SOM重み)")
print(f"    - {prefix}expectancy.csv (期待値マップ)")
print(f"    - {prefix}risk_map.csv (リスクマップ)")
print(f"    - {prefix}scaling_params.csv (正規化パラメータ)")