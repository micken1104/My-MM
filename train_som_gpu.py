import pandas as pd
import numpy as np
import sys
import os
import torch
from sklearn.preprocessing import MinMaxScaler

# ==================== RTX 50 シリーズ最適化設定 ====================
os.environ["PYTORCH_CUDA_ALLOC_CONF"] = "expandable_segments:True"
if not torch.cuda.is_available():
    print("Error: CUDA GPU (NVIDIA, PCIe) が見つかりません。.venv_gpu のPyTorch CUDA環境を確認してください。")
    sys.exit(1)

gpu_index = int(os.environ.get("SOM_CUDA_DEVICE", "0"))
torch.cuda.set_device(gpu_index)
DEVICE = torch.device(f"cuda:{gpu_index}")

if DEVICE.type == 'cuda':
    torch.backends.cuda.matmul.allow_tf32 = True
    torch.backends.cudnn.allow_tf32 = True
    # sm_120 (Blackwell) のための警告抑制フラグ（環境によって使用）
    # os.environ["TORCH_CUDA_ARCH_LIST"] = "9.0" 

# ==================== パラメータ設定 ====================
MIN_REQUIRED_DATA = 500
SOM_WIDTH = 20
SOM_HEIGHT = 20
EPOCHS = 20
BATCH_SIZE = 1000  # GPUを遊ばせないためのバッチサイズ
INITIAL_LR = 0.1
INITIAL_SIGMA = max(SOM_WIDTH, SOM_HEIGHT) / 2.0

# ==================== 1. 引数とファイル準備 ====================
if len(sys.argv) < 2:
    print("使用法: python train_som_gpu.py <symbol>")
    sys.exit(1)

target_symbol = sys.argv[1]
support_symbol = 'BTCUSDT'
data_dir, models_dir = "data", "models"

target_path = f"{data_dir}/{target_symbol}_market_data.csv"
support_path = f"{data_dir}/{support_symbol}_market_data.csv"

# ==================== 2. データ読み込み & 結合 ====================
cols = ['timestamp', 'symbol', 'imbalance', 'imbalance_change', 'total_depth', 'price', 'btc_price', 'volatility', 'btc_corr']

try:
    df_t = pd.read_csv(target_path, names=cols, header=0)
    df_s = pd.read_csv(support_path, names=cols, header=0)
except FileNotFoundError as e:
    print(f"Error: {e}")
    sys.exit(1)

df_t['timestamp'] = pd.to_numeric(df_t['timestamp'], errors='coerce')
df_s['timestamp'] = pd.to_numeric(df_s['timestamp'], errors='coerce')

combined_df = pd.merge_asof(
    df_t.sort_values('timestamp'),
    df_s.sort_values('timestamp')[['timestamp', 'imbalance', 'imbalance_change']].rename(
        {'imbalance': 'btc_imbalance', 'imbalance_change': 'btc_imbalance_change'}, axis=1),
    on='timestamp', direction='backward'
).dropna(subset=['btc_imbalance'])

# 未来のPNL計算 (30秒先)
combined_df['future_pnl'] = (combined_df['price'].shift(-30) - combined_df['price']) / combined_df['price']
combined_df = combined_df.dropna(subset=['future_pnl']).tail(10000)  # 最新1万行に絞る

if len(combined_df) < MIN_REQUIRED_DATA:
    print(f"データ不足: {len(combined_df)}行")
    sys.exit(1)

# ==================== 3. 特徴量準備 & GPU転送 ====================
features = ['imbalance', 'imbalance_change', 'btc_imbalance', 'btc_imbalance_change', 'total_depth', 'volatility', 'btc_corr']
scaler = MinMaxScaler()
data_scaled = scaler.fit_transform(combined_df[features].values)
price_changes = combined_df['future_pnl'].values * 1000

# 全てGPUへ転送
data_tensor = torch.FloatTensor(data_scaled).to(DEVICE)
price_tensor = torch.FloatTensor(price_changes).to(DEVICE)

# SOM初期化
neurons_count = SOM_WIDTH * SOM_HEIGHT
indices = np.random.choice(len(data_scaled), neurons_count, replace=True)
som_weights = torch.FloatTensor(data_scaled[indices]).to(DEVICE)

x, y = torch.meshgrid(torch.arange(SOM_WIDTH), torch.arange(SOM_HEIGHT), indexing='ij')
grid_coords = torch.stack([x.flatten(), y.flatten()], dim=1).float().to(DEVICE)

# ==================== 4. 爆速GPU学習ループ (Batch SOM) ====================
print(f"🚀 訓練開始 (Device: {DEVICE}, GPU: {torch.cuda.get_device_name(gpu_index)}): {target_symbol}")

for epoch in range(EPOCHS):
    prog = epoch / float(EPOCHS)
    lr = INITIAL_LR * np.exp(-prog * 3.0)
    sigma = INITIAL_SIGMA * np.exp(-prog * 3.0)
    sigma_sq = 2 * (sigma**2) + 1e-5

    # バッチ処理
    perm = torch.randperm(data_tensor.size(0))
    for i in range(0, data_tensor.size(0), BATCH_SIZE):
        batch_idx = perm[i:i + BATCH_SIZE]
        samples = data_tensor[batch_idx]

        # BMU探索 (一括)
        dists = torch.cdist(samples, som_weights, p=2)
        bmu_indices = torch.argmin(dists, dim=1)
        bmu_locs = grid_coords[bmu_indices]

        # 影響度計算 (全サンプル x 全ニューロン)
        # dist_sq: (Batch, Neurons)
        dist_sq = torch.cdist(bmu_locs, grid_coords, p=2)**2
        influence = torch.exp(-dist_sq / sigma_sq)

        # 重み更新 (行列演算で一括)
        # numerator: (Neurons, Dim), denominator: (Neurons, 1)
        numerator = torch.matmul(influence.t(), samples)
        denominator = influence.sum(dim=0).unsqueeze(1) + 1e-8
        
        target_w = numerator / denominator
        som_weights = (1 - lr) * som_weights + lr * target_w

    if (epoch + 1) % 5 == 0:
        print(f"  Epoch {epoch + 1}/{EPOCHS} completed")

# ==================== 5. 期待値計算 (GPU完結) ====================
print("📊 期待値を計算中...")
with torch.no_grad():
    # 各データがどのニューロンに落ちるか一括判定
    final_dists = torch.cdist(data_tensor, som_weights, p=2)
    winners = torch.argmin(final_dists, dim=1)

# 集計のためにCPUへ
winners_cpu = winners.cpu().numpy()
price_cpu = price_tensor.cpu().numpy()

expectancy_map = np.zeros(neurons_count)
risk_map = np.full(neurons_count, 0.05)

for i in range(neurons_count):
    node_pnl = price_cpu[winners_cpu == i]
    if len(node_pnl) > 0:
        expectancy_map[i] = np.mean(node_pnl)
        if len(node_pnl) > 1:
            risk_map[i] = np.std(node_pnl)

# ==================== 6. 保存 ====================
os.makedirs(models_dir, exist_ok=True)
prefix = f"{models_dir}/{target_symbol}_"

np.savetxt(f"{prefix}map_weights.csv", som_weights.cpu().numpy(), delimiter=",")
np.savetxt(f"{prefix}expectancy.csv", expectancy_map, delimiter=",")
np.savetxt(f"{prefix}risk_map.csv", risk_map, delimiter=",")

pd.DataFrame({
    'feature': features,
    'min': scaler.data_min_,
    'max': scaler.data_max_
}).to_csv(f"{prefix}scaling_params.csv", index=False)

print(f"\n✅ 訓練完了: {target_symbol}")