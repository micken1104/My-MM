#ifndef SCAN_MARKET_H
#define SCAN_MARKET_H

#include <map>
#include <string>
#include <vector>
#include <chrono>

// 1. 市場の「今」の状態を保持する
struct MarketState {
    double imbalance;
    double mid_price;
    std::chrono::steady_clock::time_point timestamp;
};

// 2. 「1分後の答え合わせ」のために保存しておくデータ
struct TradeData {
    std::string symbol;
    double entry_price;
    double entry_imbalance;
    double entry_diff;
    double entry_volume;
    std::chrono::steady_clock::time_point entry_time;
};

// 3. 他のファイルからも関数を呼べるように宣言
void scan_market(std::map<std::string, MarketState>& history, std::vector<TradeData>& pending_checks);

// CSVへの保存と1分経ったかのチェックを行う関数
void process_pending_checks(std::vector<TradeData>& pending_checks, const std::map<std::string, double>& current_prices);

#endif