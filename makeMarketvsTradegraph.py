import pandas as pd
import matplotlib.pyplot as plt

# データの読み込み
try:
    all_csv = pd.read_csv('./data/all_trades_history.csv', header=None)
    # 銘柄別データから価格を取得（BTCの価格推移用）
    # C++側の save_market_data_to_csv で保存しているファイルを使います
    btc_market_csv = pd.read_csv('./data/BTCUSDT_market_data.csv')
except FileNotFoundError as e:
    print(f"ファイルが見つかりません: {e}")
    exit()

fig, ax1 = plt.subplots(figsize=(12, 6))

# --- 左軸: 合計損益 [%] ---
all_time = all_csv[0]
all_pnl = all_csv[3]
time_rel = all_time - all_time[0] # 開始からの経過秒数

ax1.plot(time_rel, all_pnl, label='Total PnL [%]', color='black', linewidth=2)
ax1.set_xlabel('Time [s]')
ax1.set_ylabel('Total PnL [%]', color='black')
ax1.tick_params(axis='y', labelcolor='black')
ax1.grid(True, which='both', linestyle=':', alpha=0.5)

# --- 右軸: BTC価格 [USDT] ---
ax2 = ax1.twinx() # 2軸目を作成
market_time = btc_market_csv['timestamp']
btc_price = btc_market_csv['price']
market_time_rel = market_time - all_time[0] # 損益グラフと時間を合わせる

ax2.plot(market_time_rel, btc_price, label='BTC Price', color='orange', linestyle='-', alpha=0.4)
ax2.set_ylabel('BTC Price [USDT]', color='orange')
ax2.tick_params(axis='y', labelcolor='orange')

# タイトルと凡例
plt.title('Trading Performance vs BTC Market Trend')
fig.tight_layout()

# 凡例をまとめる
lines1, labels1 = ax1.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper left')

plt.show()