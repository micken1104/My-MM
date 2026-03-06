import pandas as pd
import os

def analyze_trade_quality():
    symbols = ["ATOMUSDT", "ETHUSDT", "SOLUSDT", "BTCUSDT"]
    summary = []

    for symbol in symbols:
        file_path = f'./data/{symbol}_trades.csv'
        if not os.path.exists(file_path):
            continue
            
        # CSV読み込み (timestamp, symbol, entry, exit, pnl, reason)
        df = pd.read_csv(file_path, header=None, 
                         names=['ts', 'symbol', 'entry', 'exit', 'pnl', 'reason'])
        
        total_trades = len(df)
        tp_count = len(df[df['reason'].str.contains('TP', na=False)])
        sl_count = len(df[df['reason'].str.contains('SL', na=False)])
        tu_count = len(df[df['reason'].str.contains('Time Up', na=False)])
        
        # Time Up時の平均損益（ここがプラスなら、保持時間を延ばす価値あり）
        tu_pnl_avg = df[df['reason'].str.contains('Time Up', na=False)]['pnl'].mean()
        
        summary.append({
            'Symbol': symbol,
            'Total': total_trades,
            'TP%': round(tp_count / total_trades * 100, 1),
            'SL%': round(sl_count / total_trades * 100, 1),
            'TimeUp%': round(tu_count / total_trades * 100, 1),
            'TU_PnL_Avg': round(tu_pnl_avg, 4)
        })

    df_summary = pd.DataFrame(summary)
    print("\n=== Trade Quality Analysis ===")
    print(df_summary.to_string(index=False))
    print("==============================\n")

if __name__ == "__main__":
    analyze_trade_quality()