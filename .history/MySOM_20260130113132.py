from PIL import Image
import matplotlib.pyplot as plt
import random
import numpy as np
import math
import pandas as pd
from sklearn.preprocessing import MinMaxScaler


df = pd.read_csv('market_data.csv', names=['symbol', 'imbalance', 'diff', 'volume', 'price_change'])

scaler = MinMaxScaler()
features = ['imbalance', 'diff', 'volume']
df_scaled = df.copy()
df_scaled[features] = scaler.fit_transform(df[features])
print(df_scaled.head())

width = high = 20
epochs = 5

# df_scaledから学習用の3次元リストを作る
DATA = df_scaled[['imbalance', 'diff', 'volume']].values.tolist()
data_sum = len(DATA)
# 価格変化（色用）を取っておく
PRICE_CHANGES = df_scaled['price_change'].values.tolist()

#基礎データ
images = []
images_data = np.random.rand(width * high, 3).tolist()

#基礎表示
# 修正後の基礎表示
# --- 基礎表示 (初期状態のランダムなマップ) ---
plt.figure(figsize=(8, 8))
# images_data (1次元) を (width, high, RGB) の3次元に変形して表示
initial_map = np.array(images_data).reshape(width, high, 3)
plt.imshow(initial_map)
plt.title("Initial Random Map")
plt.axis('off')
plt.show()

#暴露
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
					for k in range(3):
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

plt.figure(figsize=(10, 8))
# RdYlGnは「赤(下落) - 黄(停滞) - 緑(上昇)」のグラデーション
plt.imshow(avg_change.reshape(width, high), cmap='RdYlGn')
plt.colorbar(label='Avg Price Change')
plt.title('Market Strategy Heatmap (Red: Fall, Green: Rise)')
plt.show()