from PIL import Image
import matplotlib.pyplot as plt
import random
import numpy as np
import math
import pandas as pd
from sklearn.preprocessing import MinMaxScaler


# 1. まずは単純に読み込む（列名は元の market_data.csv に合わせる）
df = pd.read_csv('market_data.csv', names=['symbol', 'imbalance', 'diff', 'volume', 'price_change'])

# 分析したい銘柄リスト（ここを増やすだけでOK）
target_symbols = ['BTCUSDT', 'ETHUSDT'] 

# 各銘柄のデータを抽出して辞書に格納
symbol_dfs = {sym: df[df['symbol'] == sym].reset_index(drop=True) for sym in target_symbols}

# 全銘柄で共通して存在するインデックス範囲を計算
min_len = min(len(s_df) for s_df in symbol_dfs.values())

# SOMに食わせる「横持ち」データを作成
combined_data = {}
features = []

for sym in target_symbols:
    prefix = sym.split('USDT')[0].lower() # 'btc', 'eth' など
    combined_data[f'{prefix}_imbalance'] = symbol_dfs[sym].loc[:min_len-1, 'imbalance'].values
    combined_data[f'{prefix}_diff'] = symbol_dfs[sym].loc[:min_len-1, 'diff'].values
    combined_data[f'{prefix}_volume'] = symbol_dfs[sym].loc[:min_len-1, 'volume'].values
    # 特徴量リストに追加
    features.extend([f'{prefix}_imbalance', f'{prefix}_diff', f'{prefix}_volume'])

# BTCの価格変化をターゲットにする
combined_data['price_change'] = symbol_dfs['BTCUSDT'].loc[:min_len-1, 'price_change'].values

combined_df = pd.DataFrame(combined_data)

# --- 4. スケーラーにかける ---
scaler = MinMaxScaler()
df_scaled = combined_df.copy()
df_scaled[features] = scaler.fit_transform(combined_df[features])
print(df_scaled.head())

width = high = 20
epochs = 5

# df_scaledから学習用のn次元リストを作る
DATA = df_scaled[features].values.tolist()
num_features = len(features)
data_sum = len(DATA)
# 価格変化（色用）を取っておく
PRICE_CHANGES = df_scaled['price_change'].values.tolist()

#基礎データ
images = []
images_data = np.random.rand(width * high, num_features).tolist()


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