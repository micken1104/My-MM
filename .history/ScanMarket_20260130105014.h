#ifndef SCAN_MARKET_H
#define SCAN_MARKET_H

#include <map>
#include <string>
#include <vector>
#include <chrono>

// 市場の履歴状態
struct MarketState {
    double imbalance;
    // 必要であれば mid_price も追加可能
};

// 1分後の答え合わせ用データ
struct TradeData {
    std::string symbol;
    double entry_price;
    double entry_imbalance;
    double entry_diff;
    double entry_volume;
    std::chrono::steady_clock::time_point entry_time;
};

void scan_market(std::map<std::string, MarketState>& history, std::vector<TradeData>& pending_checks);
void check_pending_trades(const std::map<std::string, double>& current_prices, std::vector<TradeData>& pending_checks);

#endif