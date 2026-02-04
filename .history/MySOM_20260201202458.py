import pandas as pd
import numpy as np
import math
from sklearn.preprocessing import MinMaxScaler
import matplotlib.pyplot as plt
import random

# 1. データ読み込み（ヘッダー対応）
df = pd.read_csv('market_data.csv', 
                 names=['timestamp', 'symbol', 'imbalance', 'diff', 'volume', 'entry_price', 'exit_price', 'price_change'],
                 header=0)

# 2. 銘柄ごとに分割
target_symbol = 'ATOMUSDT'   # C++の target と合わせる
support_symbol = 'BTCUSDT'  # C++の support と合わせる

target_df = df[df['symbol'] == target_symbol].reset_index(drop=True)
support_df = df[df['symbol'] == support_symbol].reset_index(drop=True)

# 欠損考慮：最小の長さに合わせる
min_len = min(len(target_df), len(support_df))

# 3. 横持ちデータ作成
# ※この features の順番が C++ の input 配列と一致している必要があります
features = ['target_imb', 'target_diff', 'target_vol', 'support_imb', 'support_diff', 'support_vol']
combined_data = {
    'target_imb':  target_df.loc[:min_len-1, 'imbalance'].values,
    'target_diff': target_df.loc[:min_len-1, 'diff'].values,
    'target_vol':  target_df.loc[:min_len-1, 'volume'].values,
    'support_imb': support_df.loc[:min_len-1, 'imbalance'].values,
    'support_diff':support_df.loc[:min_len-1, 'diff'].values,
    'support_vol': support_df.loc[:min_len-1, 'volume'].values,
    'price_change':target_df.loc[:min_len-1, 'price_change'].values # targetの変化を予測対象にする
}

combined_df = pd.DataFrame(combined_data)
# 直近20,000〜30,000行に絞る（相場の「今」に特化させる）
combined_df = combined_df.tail(30000).reset_index(drop=True)

# 4. スケーリング
scaler = MinMaxScaler()
df_scaled = combined_df.copy()
df_scaled[features] = scaler.fit_transform(combined_df[features])

# 学習用データ準備
DATA = df_scaled[features].values.tolist()
PRICE_CHANGES = df_scaled['price_change'].values.tolist()
num_features = len(features)
width = high = 20
# DATAの中から、ノードの数（400個）だけランダムに抽出
#SOMの学習を始める際、地図の初期状態（初期の重み）をランダムに決める必要があります。真っ白な地図から始めるより、**「今あるデータの中からランダムにいくつか選んで地図の初期値にする」**ほうが学習が速く進むため、この手法がよく使われます。
images_data = random.sample(DATA, width * high)


#暴露
epochs = 50
initial_ste = 0.05    # 初期の学習率（強め）
initial_sigma = 4.0  # 初期の近傍半径（広め）
for epoch in range(epochs):
	print(f"Epoch {epoch+1}/{epochs} starting...")
    # グリッドの座標行列を事前に作成 (20x20)
x, y = np.meshgrid(np.arange(width), np.arange(high))
grid_coords = np.stack([x.flatten(), y.flatten()], axis=1)

	for i in range(len(DATA)):
		# 学習率と半径を、後半に行くほど小さくする
		# 全体の進行度（エポックも考慮）で学習率を減衰させる
        # total_progress は 0.0 から 1.0 まで進む
		total_progress = (epoch * len(DATA) + i) / (epochs * len(DATA))
		current_ste = initial_ste * (1.0 - total_progress)
		current_sigma = initial_sigma * (1.0 - total_progress)
		
		# 1. BMU (Winner) を探す (高速版)
		diffs = np.array(images_data) - DATA[i]
		dist_sqs = np.sum(diffs**2, axis=1)
		winner_idx = np.argmin(dist_sqs)
		
		# 2. 全ノードとBMUの距離を一気に計算
		bmu_coord = grid_coords[winner_idx]
		dist_to_bmu = np.sum((grid_coords - bmu_coord)**2, axis=1)
		
		# 3. ガウス近傍関数を一括適用
		influence = np.exp(-dist_to_bmu / (2 * (current_sigma**2 + 1e-5)))
		
		# 4. 重みを一括更新 (images_data: [400, 6], influence: [400, 1])
		images_data += (DATA[i] - images_data) * (current_ste * influence[:, np.newaxis])

# --- 最終結果：価格変化のグラデマップ ---
node_weights = np.zeros(width * high)
node_counts = np.ones(width * high) * 0.001

for i in range(len(DATA)):
    dists = np.sum(np.abs(np.array(images_data) - DATA[i]), axis=1)
    winner = np.argmin(dists)
    node_weights[winner] += PRICE_CHANGES[i]
    node_counts[winner] += 1

avg_change = node_weights / node_counts

# 密度マップ（データの出現回数）を正規化
density = node_counts.reshape(width, high)
density_norm = (density - density.min()) / (density.max() - density.min())

plt.figure(figsize=(10, 8))
# 背景に価格変化のヒートマップ
img = plt.imshow(avg_change.reshape(width, high), cmap='RdYlGn', interpolation='nearest')
# 密度を等高線（Contour）として重ねる
plt.contour(density_norm, colors='black', alpha=0.5, levels=5) 
plt.colorbar(img, label='Avg Price Change')
plt.title('Strategy Map with Density Contours')
plt.xticks(range(0, width, 2)); plt.yticks(range(0, high, 2))
plt.show()

# 5. シグナル抽出
node_weights = np.zeros(width * high)
node_counts = np.ones(width * high) * 0.001
for i in range(len(DATA)):
    dists = np.sum(np.abs(np.array(images_data) - DATA[i]), axis=1)
    winner = np.argmin(dists)
    node_weights[winner] += PRICE_CHANGES[i]
    node_counts[winner] += 1
avg_change = node_weights / node_counts

# シグナル定義
final_expectancy = np.where(node_counts >= 0.5, avg_change, 0.0)

# C++用に保存（名称を som_expectancy.csv などに変えると分かりやすい）
np.savetxt("som_expectancy.csv", final_expectancy.flatten(), delimiter=",")

# --- 座標ごとの標準偏差（リスク）を計算 ---
node_std = np.zeros(width * high)
for idx in range(width * high):
    # そのノードに分類されたデータのインデックスを取得
    indices = [i for i, d in enumerate(DATA) if np.argmin(np.sum(np.abs(np.array(images_data) - d), axis=1)) == idx]
    if len(indices) > 1:
        node_std[idx] = np.std([PRICE_CHANGES[i] for i in indices])
    else:
        node_std[idx] = 0.05 # データが少ない場所はデフォルトの損切幅（0.05%など）

# C++用に損切用マップとして保存
np.savetxt("som_risk_map.csv", node_std.flatten(), delimiter=",")


# 6. C++用ファイルの保存
# 地図の重み
np.savetxt("som_map_weights.csv", np.array(images_data), delimiter=",")
# シグナル
# スケーリング定数
scaling_params = pd.DataFrame({
    'min': scaler.data_min_,
    'max': scaler.data_max_
})
scaling_params.to_csv("som_scaling_params.csv", index=False)

print("C++用モデルデータの保存完了！")