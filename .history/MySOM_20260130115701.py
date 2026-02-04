from PIL import Image
import matplotlib.pyplot as plt
import random
import numpy as np
import math
import pandas as pd
from sklearn.preprocessing import MinMaxScaler


# ヘッダーなしCSVの読み込み
df = pd.read_csv('market_data.csv', names=['symbol', 'imbalance', 'diff', 'volume', 'price_change'])

# 銘柄ごとにフィルタリング
symbol_dfs = {
    'BTC': df[df['symbol'] == 'BTCUSDT'].reset_index(drop=True),
    'ETH': df[df['symbol'] == 'ETHUSDT'].reset_index(drop=True)
}

# 最小行数を合わせる（欠損対策）
min_len = min(len(symbol_dfs['BTC']), len(symbol_dfs['ETH']))

# 横持ちデータ作成
combined_data = {
    'btc_imbalance': symbol_dfs['BTC'].loc[:min_len-1, 'imbalance'].values,
    'btc_diff':      symbol_dfs['BTC'].loc[:min_len-1, 'diff'].values,
    'btc_volume':    symbol_dfs['BTC'].loc[:min_len-1, 'volume'].values,
    'eth_imbalance': symbol_dfs['ETH'].loc[:min_len-1, 'imbalance'].values,
    'eth_diff':      symbol_dfs['ETH'].loc[:min_len-1, 'diff'].values,
    'eth_volume':    symbol_dfs['ETH'].loc[:min_len-1, 'volume'].values,
    'price_change':  symbol_dfs['BTC'].loc[:min_len-1, 'price_change'].values
}
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

# --- 気になる座標の数値を逆引きする関数 ---
def inspect_node(x, y):
    idx = x * high + y
    weights = images_data[idx]
    print(f"--- Node ({x}, {y}) Statistics ---")
    for name, val in zip(features, weights):
        # スケーラーを逆変換して生の値に戻す（オプション。今は正規化後の値が表示されます）
        print(f"{name}: {val:.4f}")
    print(f"Avg Price Change: {avg_change[idx]:.6f}")
    print(f"Sample Count: {node_counts[idx]:.0f}")

# --- 戦略の抽出とバックテスト ---
PROFIT_THRESHOLD = 0.05  # 期待値0.05%以上で買い
LOSS_THRESHOLD = -0.05    # 期待値-0.05%以下で売り
MIN_SAMPLES = 1           # 少なくとも1回はデータが落ちていること

signals = {}
wallet = 1000000
history = []

# 1. 全ノードを走査してシグナルマップを作成
for x in range(width):
    for y in range(high):
        idx = x * high + y
        if node_counts[idx] >= MIN_SAMPLES:
            if avg_change[idx] >= PROFIT_THRESHOLD:
                signals[(x, y)] = "BUY"
            elif avg_change[idx] <= LOSS_THRESHOLD:
                signals[(x, y)] = "SELL"

# 2. バックテスト（資産推移の確認）
for i in range(len(DATA)):
    dists = np.sum(np.abs(np.array(images_data) - DATA[i]), axis=1)
    winner = np.argmin(dists)
    wx, wy = winner // high, winner % high
    
    if (wx, wy) in signals:
        sig = signals[(wx, wy)]
        change = PRICE_CHANGES[i] / 100.0
        if sig == "BUY": wallet *= (1.0 + change)
        elif sig == "SELL": wallet *= (1.0 - change)
    history.append(wallet)

print(f"Final Wallet: {wallet:,.0f} JPY")

# --- C++用の書き出し ---
# 1. 地図の重み (400行 x 6列)
np.savetxt("som_map_weights.csv", np.array(images_data), delimiter=",")

# 2. シグナルフラグ (20x20の行列)
signal_matrix = np.zeros((width, high))
for (x, y), sig in signals.items():
    if sig == "BUY": signal_matrix[x, y] = 1
    if sig == "SELL": signal_matrix[x, y] = -1
np.savetxt("som_signals.csv", signal_matrix, delimiter=",")

print("C++用の地図データを保存しました。")