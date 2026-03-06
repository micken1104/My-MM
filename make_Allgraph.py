import pandas as pd
import matplotlib.pyplot as plt

plt.figure(figsize=(14, 7))

symbols = ["ATOMUSDT", "ETHUSDT", "SOLUSDT", "BTCUSDT"]
colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728"] 

# 1. 全体の開始時間を取得し、全体の累積PnLを計算（ループの外で1回だけ行う）
try:
    all_csv = pd.read_csv('./data/all_trades_history.csv', header=None)
    start_ts = all_csv[0].iloc[0]  
    
    # Python側で全履歴の単発PnL(2列目)を累計する
    total_pnl_recalculated = all_csv[2].cumsum()
    
    # 全体の太い黒線をプロット
    plt.plot(all_csv[0] - start_ts, total_pnl_recalculated, label='TOTAL PnL (Recalculated)', 
             color='black', linewidth=3, zorder=10)
    
    print(f"Total Cumulative PnL recalculated: {total_pnl_recalculated.iloc[-1]:.3f}%")
except Exception as e:
    print(f"トレード履歴の読み込みに失敗しました: {e}")
    start_ts = None

# 2. 各銘柄ごとのプロット（for文の中）
if start_ts is not None:
    for symbol, color in zip(symbols, colors):
        # --- 個別銘柄のトレード損益（点線） ---
        try:
            df_t = pd.read_csv(f'./data/{symbol}_trades.csv', header=None)
            df_t = df_t[df_t[0] >= start_ts]
            
            # df_t の 4列目（単発PnL）を cumsum() する
            plt.plot(df_t[0] - start_ts, df_t[4].cumsum(), 
                     linestyle='--', color=color, alpha=0.8, label=f'{symbol} Strategy')
        except: 
            pass

        # --- 各銘柄の市場価格（実線） ---
        try:
            df_m = pd.read_csv(f'./data/{symbol}_market_data.csv')
            df_m = df_m[df_m['timestamp'] >= start_ts].reset_index(drop=True)
            if not df_m.empty:
                base_price = df_m['price'].iloc[0]
                market_pnl = (df_m['price'] / base_price - 1) * 100
                plt.plot(df_m['timestamp'] - start_ts, market_pnl, 
                         linestyle='-', color=color, alpha=0.3, linewidth=1, label=f'{symbol} Market')
        except: 
            pass

# グラフの仕上げ
plt.axhline(0, color='red', linewidth=1, linestyle='-')
plt.xlabel('Time [s] (Since First Trade)')
plt.ylabel('Return [%]')
plt.title('Strategy Performance vs Market (Normalized from First Trade)')
plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize='small')
plt.grid(True, alpha=0.3)
plt.tight_layout()

#try:
    # 学習イベントログの読み込み
    #df_ev = pd.read_csv('./data/training_events.csv', header=None, names=['timestamp', 'symbol'])
    # 開始時刻以降のイベントのみ対象
    #df_ev = df_ev[df_ev['timestamp'] >= start_ts]
    # 学習タイミングを縦線で描画
    #for _, row in df_ev.iterrows():
        # 各銘柄の色に合わせる場合は colors マップを利用
        #color_idx = symbols.index(row['symbol']) if row['symbol'] in symbols else -1
        #line_color = colors[color_idx] if color_idx != -1 else 'gray'
        #plt.axvline(x=row['timestamp'] - start_ts, color=line_color, 
        #            linestyle=':', linewidth=1.5, alpha=0.6)       
        # グラフ上部に銘柄名を表示
        # plt.text(row['timestamp'] - start_ts, plt.ylim()[1], f" Train:{row['symbol']}", 
        #         rotation=90, verticalalignment='top', fontsize=8, color=line_color, alpha=0.8)
    #print("学習イベントをプロットしました。")
#except Exception as e:
    #print(f"学習イベントの読み込みスキップ（まだ記録がない可能性があります）: {e}")

plt.show()