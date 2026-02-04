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
df_list = []
for i in range(len(DATA)):
	# 1. 勝者ノード（BMU）を探す
	sa_list = []
	for j in range(len(images_data)):
		zet = sum([abs(DATA[i][k] - images_data[j][k]) for k in range(3)])
		sa_list.append(zet)
	winner_idx = sa_list.index(min(sa_list))

	# 2. 更新対象のリスト（勝者とその周辺）
	neighbors = [winner_idx, winner_idx-1, winner_idx+1, winner_idx-width, winner_idx+width]
	for idx in neighbors:
		if 0 <= idx < (width * high):
			for k in range(3):
			# 更新式：現在の値 + (ターゲットとの差分 * 学習率)
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
