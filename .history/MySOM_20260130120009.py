import pandas as pd
import numpy as np
import math
from sklearn.preprocessing import MinMaxScaler
import matplotlib.pyplot as plt
import random

# 1. データ読み込み（ヘッダーなし対応）
df = pd.read_csv('market_data.csv', names=['symbol', 'imbalance', 'diff', 'volume', 'price_change'])

# 2. 銘柄ごとに分割
btc_df = df[df['symbol'] == 'BTCUSDT'].reset_index(drop=True)
eth_df = df[df['symbol'] == 'ETHUSDT'].reset_index(drop=True)

# 欠損考慮：最小の長さに合わせる
min_len = min(len(btc_df), len(eth_df))

# 3. 横持ちデータ作成
# ※この features の順番が C++ の input 配列と一致している必要があります
features = ['btc_imbalance', 'btc_diff', 'btc_volume', 'eth_imbalance', 'eth_diff', 'eth_volume']
combined_data = {
    'btc_imbalance': btc_df.loc[:min_len-1, 'imbalance'].values,
    'btc_diff':      btc_df.loc[:min_len-1, 'diff'].values,
    'btc_volume':    btc_df.loc[:min_len-1, 'volume'].values,
    'eth_imbalance': eth_df.loc[:min_len-1, 'imbalance'].values,
    'eth_diff':      eth_df.loc[:min_len-1, 'diff'].values,
    'eth_volume':    eth_df.loc[:min_len-1, 'volume'].values,
    'price_change':  btc_df.loc[:min_len-1, 'price_change'].values # BTCの変化をターゲットに
}

combined_df = pd.DataFrame(combined_data)

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
images_data = random.sample(DATA, width * high)


#暴露
epochs = 5
initial_ste = 0.1    # 初期の学習率（強め）
initial_sigma = 3.0  # 初期の近傍半径（広め）
for epoch in range(epochs):
	print(f"Epoch {epoch+1}/{epochs} starting...")

	for i in range(len(DATA)):
		# 学習率と半径を、後半に行くほど小さくする
		# 全体の進行度（エポックも考慮）で学習率を減衰させる
        # total_progress は 0.0 から 1.0 まで進む
		total_progress = (epoch * len(DATA) + i) / (epochs * len(DATA))
		current_ste = initial_ste * (1.0 - total_progress)
		current_sigma = initial_sigma * (1.0 - total_progress)
		
		# 1. 勝者ノード（BMU）を探す
		dists = np.sum(np.abs(np.array(images_data) - DATA[i]), axis=1)
		winner_idx = np.argmin(dists)

		# 勝者ノードのインデックスから(x, y)を逆算
		winner_x = winner_idx // high
		winner_y = winner_idx % high

		# 近傍ノードをループで回す（上下左右1マスずつなど）
		r_limit = math.ceil(current_sigma)
		for dx in range(-r_limit, r_limit + 1):
			for dy in range(-r_limit, r_limit + 1):
				nx, ny = winner_x + dx, winner_y + dy
				
				# ここで(x, y)が範囲内（0〜19）か判定
				if 0 <= nx < width and 0 <= ny < high:
					# 勝者からの距離に応じた減衰（ガウス分布）
					dist_sq = dx**2 + dy**2
					influence = math.exp(-dist_sq / (2 * (current_sigma**2 + 1e-5)))

					idx = nx * high + ny
					# 更新式：現在の値 + (ターゲットとの差分 * 学習率)
					for k in range(num_features):
						images_data[idx][k] += (DATA[i][k] - images_data[idx][k]) * current_ste * influence


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
PROFIT_THRESHOLD = 0.05
signals = np.zeros((width, high))
for i in range(width * high):
    if node_counts[i] >= 1: # 1回以上出現
        if avg_change[i] >= PROFIT_THRESHOLD: signals[i // high, i % high] = 1
        elif avg_change[i] <= -PROFIT_THRESHOLD: signals[i // high, i % high] = -1

# 6. C++用ファイルの保存
# 地図の重み
np.savetxt("som_map_weights.csv", np.array(images_data), delimiter=",")
# シグナル
np.savetxt("som_signals.csv", signals.flatten(), delimiter=",")
# スケーリング定数
scaling_params = pd.DataFrame({
    'min': scaler.data_min_,
    'max': scaler.data_max_
})
scaling_params.to_csv("som_scaling_params.csv", index=False)

print("C++用モデルデータの保存完了！")