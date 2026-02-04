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

width = high = 20;
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
initial_ste = 0.5    # 初期の学習率（強め）
initial_sigma = 3.0  # 初期の近傍半径（広め）
for i in range(len(DATA)):
	# 学習率と半径を、後半に行くほど小さくする
	# rate = 1.0 - (i / len(DATA))
    current_ste = initial_ste * rate
    current_sigma = initial_sigma * rate
	
	# 1. 勝者ノード（BMU）を探す
	sa_list = []
	for j in range(len(images_data)):
		# マンハッタン距離
		zet = sum([abs(DATA[i][k] - images_data[j][k]) for k in range(3)])
		sa_list.append(zet)
	winner_idx = sa_list.index(min(sa_list))

	# 勝者ノードのインデックスから(x, y)を逆算
	winner_x = winner_idx // high
	winner_y = winner_idx % high

	# 近傍ノードをループで回す（上下左右1マスずつなど）
	for dx in [-1, 0, 1]:
		for dy in [-1, 0, 1]:
			nx, ny = winner_x + dx, winner_y + dy
			
			# ここで(x, y)が範囲内（0〜19）か判定
			if 0 <= nx < width and 0 <= ny < high:
				idx = nx * high + ny
				# 更新式：現在の値 + (ターゲットとの差分 * 学習率)
				for k in range(3):
					images_data[idx][k] += (DATA[i][k] - images_data[idx][k]) * ste


# 各マスに価格変化を蓄積する用の配列
node_weights = np.zeros(width * high)
node_counts = np.ones(width * high) * 0.001 # 0除算防止

for i in range(len(DATA)):
    # 各データが最終的にどのノードに落ちるか判定
    zet_list = [sum([abs(DATA[i][k] - images_data[j][k]) for k in range(3)]) for j in range(len(images_data))]
    winner = zet_list.index(min(zet_list))
    node_weights[winner] += PRICE_CHANGES[i]
    node_counts[winner] += 1

# 平均変化率
avg_change = node_weights / node_counts

# 可視化：2D配列に変換してヒートマップ表示
plt.figure(figsize=(10, 8))
plt.imshow(node_counts.reshape(width, high), cmap='Blues')
plt.colorbar(label='Data Count')
plt.title('Where the data landed')
plt.show()