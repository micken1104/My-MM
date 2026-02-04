from PIL import Image
import matplotlib.pyplot as plt
import random
import numpy as np
import math


data_sum = 1000
width = high = 20
ste = 1.1

#検証データ
DATA = []
for i in range(data_sum):
	data_comp = []
	for k in range (3):
		c = random.randint(0,255)
		data_comp.append(c)
	DATA.append(data_comp)

#基礎データ
images = []
images_data = []
for i in range(width):
	for j in range(high):
		r = random.randint(0,255)
		g = random.randint(0,255)
		b = random.randint(0,255)
		image_comp = []
		image_comp.append(r)
		image_comp.append(g)
		image_comp.append(b)
		img = Image.new("RGBA", (1, 1), (r,g,b,255))
		images_data.append(image_comp)
		images.append(img)

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
df_list = []
for i in range(len(DATA)):
	sa_list = []
	for j in range(len(images_data)):
		zet_list = []
		for k in range(3):
			zet_list.append(abs(DATA[i][k] - images_data[j][k]))
		zet = sum(zet_list)
		sa_list.append(zet)
	df_list.append(sa_list.index(min(sa_list)))
for i in range(data_sum):
		for j in range(3):
			images_data[df_list[i]][j] = (images_data[df_list[i]][j] + math.floor(DATA[i][j] * ste)) // 2
			if df_list[i] - (width + 1) > 0:
				images_data[df_list[i] - (width + 1)][j] = (images_data[df_list[i] - (width + 1)][j] + math.floor(DATA[i][j] * ste)) // 2
			if df_list[i] - (width) > 0:
				images_data[df_list[i] - (width)][j] = (images_data[df_list[i] - (width)][j] + math.floor(DATA[i][j] * ste)) // 2
			if df_list[i] - (width-1) > 0:
				images_data[df_list[i] - (width - 1)][j] = (images_data[df_list[i] - (width - 1)][j] + math.floor(DATA[i][j] * ste)) // 2
			if df_list[i] - 1 > 0:
				images_data[df_list[i] - 1][j] = (images_data[df_list[i] - 1][j] + math.floor(DATA[i][j] * ste)) // 2
			if df_list[i] + 1 < (width*high- 1):
				images_data[df_list[i] + 1][j] = (images_data[df_list[i] + 1][j] + math.floor(DATA[i][j] * ste)) // 2
			if df_list[i] + (width - 1) < (width*high - 1):
				images_data[df_list[i] + (width - 1)][j] = (images_data[df_list[i] + (width - 1)][j] + math.floor(DATA[i][j] * ste)) // 2
			if df_list[i] + (width) < (width*high-1):
				images_data[df_list[i] + (width)][j] = (images_data[df_list[i] + (width)][j] + math.floor(DATA[i][j] * ste)) // 2
			if df_list[i] + (width + 1) < (width*high - 1):
				images_data[df_list[i] + (width + 1)][j] = (images_data[df_list[i] + (width + 1)][j] + math.floor(DATA[i][j] * ste)) // 2

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
