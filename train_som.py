import pandas as pd
import numpy as np
from sklearn.preprocessing import MinMaxScaler
import sys
import os
import random

# --- 1. 銘柄を引数から取得 ---
if len(sys.argv) < 2:
    print("Usage: python train_som.py <SYMBOL>")
    sys.exit(1)

target_symbol = sys.argv[1]
support_symbol = 'BTCUSDT'
target_path = f"data/{target_symbol}_market_data.csv"
support_path = f"data/{support_symbol}_market_data.csv"

# ファイル存在チェック
if not os.path.exists(target_path) or not os.path.exists(support_path):
    print(f"Waiting for data... (Target or Support file missing)")
    sys.exit(1)

# --- 2. データの読み込み ---
df_target = pd.read_csv(target_path, names=['timestamp', 'symbol', 'imbalance', 'diff', 'volume', 'entry_price', 'exit_price', 'price_change'], header=0)
df_support = pd.read_csv(support_path, names=['timestamp', 'symbol', 'imbalance', 'diff', 'volume', 'entry_price', 'exit_price', 'price_change'], header=0)

# 型変換とソート
df_target['timestamp'] = pd.to_numeric(df_target['timestamp'])
df_support['timestamp'] = pd.to_numeric(df_support['timestamp'])
df_target = df_target.sort_values('timestamp')
df_support = df_support.sort_values('timestamp')

# --- 3. 時間軸での結合 (merge_asof) ---
# 「ATOMの各行に対して、その直近のBTCの状態」を紐付ける
combined_df = pd.merge_asof(
    df_target, 
    df_support[['timestamp', 'imbalance', 'diff', 'volume']], 
    on='timestamp', 
    direction='backward', 
    suffixes=('', '_support')
)

# BTCデータが存在しない古い行を削除
combined_df = combined_df.dropna(subset=['imbalance_support'])

# --- 4. データの十分性チェック ---
MIN_REQUIRED_DATA = 1000 
if len(combined_df) < MIN_REQUIRED_DATA:
    print(f"Error: Not enough joined data. Valid rows: {len(combined_df)}")
    sys.exit(1)

# 特徴量の整理
features = ['target_imb', 'target_diff', 'target_vol', 'support_imb', 'support_diff', 'support_vol']
combined_data = {
    'target_imb':  combined_df['imbalance'].values,
    'target_diff': combined_df['diff'].values,
    'target_vol':  combined_df['volume'].values,
    'support_imb': combined_df['imbalance_support'].values,
    'support_diff':combined_df['diff_support'].values,
    'support_vol': combined_df['volume_support'].values,
    'price_change':combined_df['price_change'].values
}
final_df = pd.DataFrame(combined_data).tail(30000).reset_index(drop=True)

# --- 5. 学習準備 ---
scaler = MinMaxScaler()
DATA_SCALED = scaler.fit_transform(final_df[features])
PRICE_CHANGES = final_df['price_change'].values

width = high = 20
# 初期重みをデータからサンプリング
if len(DATA_SCALED) >= width * high:
    images_data = np.array(random.sample(DATA_SCALED.tolist(), width * high))
else:
    indices = np.random.choice(len(DATA_SCALED), width * high, replace=True)
    images_data = DATA_SCALED[indices]

# --- 6. SOM学習 (高速版) ---
epochs = 20  # 疎通確認のため一旦20程度に
for epoch in range(epochs):
    total_prog = epoch / epochs
    ste = 0.05 * (1.0 - total_prog)
    sigma = 4.0 * (1.0 - total_prog)
    
    # グリッド計算の簡略化
    x, y = np.meshgrid(np.arange(width), np.arange(high))
    grid_coords = np.stack([x.flatten(), y.flatten()], axis=1)

    for i in range(len(DATA_SCALED)):
        diffs = images_data - DATA_SCALED[i]
        dist_sqs = np.sum(diffs**2, axis=1)
        winner_idx = np.argmin(dist_sqs)
        
        bmu_coord = grid_coords[winner_idx]
        dist_to_bmu = np.sum((grid_coords - bmu_coord)**2, axis=1)
        influence = np.exp(-dist_to_bmu / (2 * (sigma**2 + 1e-5)))
        images_data += (DATA_SCALED[i] - images_data) * (ste * influence[:, np.newaxis])

# --- 7. 期待値・リスクマップ作成と保存 ---
all_dists = np.linalg.norm(images_data[:, np.newaxis] - DATA_SCALED, axis=2)
all_winners = np.argmin(all_dists, axis=0)

node_to_changes = [[] for _ in range(width * high)]
for i, winner in enumerate(all_winners):
    node_to_changes[winner].append(PRICE_CHANGES[i])

final_expectancy = np.zeros(width * high)
node_std = np.ones(width * high) * 0.05

for idx in range(width * high):
    changes = node_to_changes[idx]
    if len(changes) > 0:
        final_expectancy[idx] = np.mean(changes)
        if len(changes) > 1:
            node_std[idx] = np.std(changes)

os.makedirs("models", exist_ok=True)
prefix = f"models/{target_symbol}_"
np.savetxt(f"{prefix}expectancy.csv", final_expectancy, delimiter=",")
np.savetxt(f"{prefix}risk_map.csv", node_std, delimiter=",")
np.savetxt(f"{prefix}map_weights.csv", images_data, delimiter=",")
pd.DataFrame({'min': scaler.data_min_, 'max': scaler.data_max_}).to_csv(f"{prefix}scaling_params.csv", index=False)

print(f"✅ {target_symbol} Model Training Complete! (Joined rows: {len(combined_df)})")