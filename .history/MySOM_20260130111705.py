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
fig, ax = plt.subplots(width, high, figsize=(10, 10))
fig.subplots_adjust(hspace=0, wspace=0)
for i in range(width):
    for j in range(high):
        ax[i, j].xaxis.set_major_locator(plt.NullLocator())
        ax[i, j].yaxis.set_major_locator(plt.NullLocator())
        ax[i, j].imshow(images[width*i+j])
plt.show()

#暴露
ste = 0.1 # 学習率（最初は小さめから）
for i in range(len(DATA)):
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

#最終表示
Fimages = []
for i in range(width*high):
	r = images_data[i][0]
	g = images_data[i][1]
	b = images_data[i][2]
	Fimg = Image.new("RGB", (1, 1), (r,g,b))
	Fimages.append(Fimg)
fig, ax = plt.subplots(width, high, figsize=(10, 10))
fig.subplots_adjust(hspace=0, wspace=0)
for i in range(width):
    for j in range(high):
        ax[i, j].xaxis.set_major_locator(plt.NullLocator())
        ax[i, j].yaxis.set_major_locator(plt.NullLocator())
        ax[i, j].imshow(Fimages[width*i+j])
plt.show()
