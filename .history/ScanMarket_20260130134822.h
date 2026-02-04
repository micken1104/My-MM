#ifndef SCAN_MARKET_H
#define SCAN_MARKET_H

#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <nlohmann/json.hpp> // これを追加

// 市場の履歴状態
struct MarketState {
    double imbalance = 0.0;
    double diff = 0.0;
    double volume = 0.0;
    double last_price = 0.0; // 追加しておくと便利
    std::chrono::steady_clock::time_point timestamp;
};

// 1分後の答え合わせ用データ
struct TradeData {
    std::string symbol;
    double entry_price;
    double entry_imbalance;
    double entry_diff;
    double entry_volume;
    std::chrono::steady_clock::time_point entry_time;
    
    // --- トレード評価用に追加 ---
    double lot_size = 0.0;
    std::string side = ""; // "LONG" or "SHORT"
};

// nlohmann::json を明示的に指定
void process_ws_data(const nlohmann::json& j, std::map<std::string, MarketState>& history, std::vector<TradeData>& pending_checks);
void scan_market(std::map<std::string, MarketState>& history, std::vector<TradeData>& pending_checks);
void check_pending_trades(const std::map<std::string, double>& current_prices, std::vector<TradeData>& pending_checks);

#endif