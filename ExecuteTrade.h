#ifndef EXECUTETRADE_H
#define EXECUTETRADE_H

#include <string>
#include <vector>
#include <chrono>
#include <algorithm>

/**
 * @brief トレードの制約条件（利確・損切・保有時間）を管理する構造体
 */
struct TradingConstraints {
    double tp_rate;      // 利確しきい値 (例: 0.0025 = 0.25%)
    double sl_rate;      // 損切しきい値 (例: -0.0008)
    int max_hold_sec;    // 最大保有秒数
    double default_risk; // デフォルトのリスク値
};

/**
 * @brief エントリー情報および注文状態を管理する構造体
 */
struct TradeData {
    std::string symbol;         // 銘柄名
    std::string side;           // "LONG" または "SHORT"
    double entry_price;         // エントリー価格
    double lot_size;            // 注文数量（USD換算）
    double dynamic_sl;          // 動的に計算された損切幅
    
    // データ収集用メンバ
    double entry_imbalance;     // エントリー時のインバランス
    double entry_diff;          // エントリー時のインバランス変化量
    double entry_volume;        // エントリー時の出来高
    long long entry_timestamp;  // システム時刻（UNIXタイムスタンプ）
    
    // 時間管理
    std::chrono::steady_clock::time_point entry_time; // エントリー時刻
};

/**
 * @brief 期待値に基づきトレードを実行（注文をキューに追加）する
 * * @param expectancy 推論された期待値
 * @param current_price 現在価格
 * @param symbol 銘柄名
 * @param pending_trades 執行中トレードのリスト
 * @param local_risk SOMノード固有のリスク（標準偏差）
 */
void execute_trade(double expectancy, double current_price, std::string symbol, 
                   std::vector<TradeData>& pending_trades, double local_risk);

#endif // EXECUTETRADE_H